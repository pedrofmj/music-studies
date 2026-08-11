#include "music_rig/core.h"

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define GENERATION_COUNT UINT64_C(10000)
#define GENERATION_POOL_CAPACITY 4U
#define COMMIT_LIMIT_NS UINT64_C(20000000)
#define ADOPTION_TIMEOUT_NS UINT64_C(5000000000)

typedef struct callback_context {
    music_rig_generation_slot *slot;
    atomic_int failed;
} callback_context;

static uint64_t monotonic_ns(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &value) != 0) {
        return UINT64_C(0);
    }

    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
        (uint64_t)value.tv_nsec;
}

static void *synthetic_callback(void *opaque)
{
    callback_context *context = opaque;
    uint64_t previous_id = UINT64_C(0);

    for (;;) {
        const music_rig_generation *generation =
            music_rig_generation_slot_adopt(context->slot);

        if (generation == NULL || generation->id < previous_id) {
            atomic_store_explicit(&context->failed, 1, memory_order_relaxed);
            return NULL;
        }
        previous_id = generation->id;
        if (generation->id == GENERATION_COUNT) {
            return NULL;
        }
    }
}

int main(void)
{
    static music_rig_generation generations[GENERATION_POOL_CAPACITY];
    bool available[GENERATION_POOL_CAPACITY] = {false, true, true, true};
    music_rig_generation_slot slot;
    callback_context context;
    pthread_t callback_thread;
    uint64_t generation_id;
    uint64_t maximum_commit_ns = UINT64_C(0);
    uint64_t adoption_started_ns;
    uint64_t reclaimed_count = UINT64_C(0);
    size_t maximum_retired = 0U;

    generations[0].id = UINT64_C(1);
    generations[0].mapping = &generations[0];

    if (music_rig_generation_slot_init(&slot, &generations[0]) !=
        MUSIC_RIG_RESULT_OK ||
        !music_rig_generation_slot_is_lock_free(&slot)) {
        fputs("lock-free generation slot unavailable\n", stderr);
        return 1;
    }

    context.slot = &slot;
    atomic_init(&context.failed, 0);
    if (pthread_create(
            &callback_thread,
            NULL,
            synthetic_callback,
            &context
        ) != 0) {
        fputs("could not start synthetic callback\n", stderr);
        return 1;
    }

    for (generation_id = UINT64_C(2);
         generation_id <= GENERATION_COUNT;
         ++generation_id) {
        size_t generation_index = GENERATION_POOL_CAPACITY;
        uint64_t commit_started_ns = monotonic_ns();
        uint64_t commit_finished_ns;
        uint64_t commit_ns;

        while (generation_index == GENERATION_POOL_CAPACITY) {
            const music_rig_generation *reclaimed;
            size_t index;

            while ((reclaimed = music_rig_generation_slot_reclaim(&slot)) !=
                NULL) {
                size_t reclaimed_index = (size_t)(reclaimed - generations);

                if (reclaimed_index >= GENERATION_POOL_CAPACITY ||
                    available[reclaimed_index]) {
                    fputs("invalid reclaimed generation\n", stderr);
                    return 1;
                }
                available[reclaimed_index] = true;
                reclaimed_count += UINT64_C(1);
            }
            for (index = 0U; index < GENERATION_POOL_CAPACITY; ++index) {
                if (available[index]) {
                    generation_index = index;
                    break;
                }
            }
            if (generation_index == GENERATION_POOL_CAPACITY) {
                if (monotonic_ns() - commit_started_ns >
                    ADOPTION_TIMEOUT_NS) {
                    fputs("generation pool reclamation timed out\n", stderr);
                    return 1;
                }
                sched_yield();
            }
        }

        generations[generation_index].id = generation_id;
        generations[generation_index].mapping = &generations[generation_index];
        commit_started_ns = monotonic_ns();

        if (music_rig_generation_slot_publish(
                &slot,
                &generations[generation_index]
            ) != MUSIC_RIG_RESULT_OK) {
            fputs("generation publication failed\n", stderr);
            return 1;
        }
        available[generation_index] = false;

        commit_finished_ns = monotonic_ns();
        commit_ns = commit_finished_ns - commit_started_ns;
        if (commit_ns > maximum_commit_ns) {
            maximum_commit_ns = commit_ns;
        }
        if (music_rig_generation_slot_retired_count(&slot) >
            maximum_retired) {
            maximum_retired = music_rig_generation_slot_retired_count(&slot);
        }
    }

    adoption_started_ns = monotonic_ns();
    while (music_rig_generation_slot_adopted(&slot)->id != GENERATION_COUNT) {
        if (monotonic_ns() - adoption_started_ns > ADOPTION_TIMEOUT_NS) {
            fputs("synthetic callback adoption timed out\n", stderr);
            return 1;
        }
        sched_yield();
    }

    if (pthread_join(callback_thread, NULL) != 0 ||
        atomic_load_explicit(&context.failed, memory_order_relaxed) != 0) {
        fputs("synthetic callback observed an invalid generation\n", stderr);
        return 1;
    }
    for (;;) {
        const music_rig_generation *reclaimed =
            music_rig_generation_slot_reclaim(&slot);

        if (reclaimed == NULL) {
            break;
        }
        reclaimed_count += UINT64_C(1);
    }
    if (reclaimed_count != GENERATION_COUNT - UINT64_C(1) ||
        music_rig_generation_slot_retired_count(&slot) != 0U ||
        maximum_retired >= GENERATION_POOL_CAPACITY) {
        fputs("bounded generation reclamation proof failed\n", stderr);
        return 1;
    }
    if (maximum_commit_ns >= COMMIT_LIMIT_NS) {
        fputs("control commit exceeded 20 ms\n", stderr);
        return 1;
    }

    printf(
        "{\"schema\":\"music-studies/generation-publication-spike/v2\","
        "\"iterations\":%" PRIu64 ",\"atomic_pointer_lock_free\":true,"
        "\"storage_slots\":%u,\"reclaimed\":%" PRIu64 ","
        "\"maximum_retired\":%zu,"
        "\"maximum_control_commit_ns\":%" PRIu64 ",\"failures\":0}\n",
        GENERATION_COUNT - UINT64_C(1),
        GENERATION_POOL_CAPACITY,
        reclaimed_count,
        maximum_retired,
        maximum_commit_ns
    );
    return 0;
}
