#include "music_rig/core.h"

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define GENERATION_COUNT UINT64_C(10000)
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
    static music_rig_generation generations[GENERATION_COUNT];
    music_rig_generation_slot slot;
    callback_context context;
    pthread_t callback_thread;
    uint64_t generation_id;
    uint64_t maximum_commit_ns = UINT64_C(0);
    uint64_t adoption_started_ns;

    for (generation_id = UINT64_C(1);
         generation_id <= GENERATION_COUNT;
         ++generation_id) {
        generations[generation_id - UINT64_C(1)].id = generation_id;
        generations[generation_id - UINT64_C(1)].mapping =
            &generations[generation_id - UINT64_C(1)];
    }

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
        uint64_t commit_started_ns = monotonic_ns();
        uint64_t commit_finished_ns;
        uint64_t commit_ns;

        if (music_rig_generation_slot_publish(
                &slot,
                &generations[generation_id - UINT64_C(1)]
            ) != MUSIC_RIG_RESULT_OK) {
            fputs("generation publication failed\n", stderr);
            return 1;
        }

        commit_finished_ns = monotonic_ns();
        commit_ns = commit_finished_ns - commit_started_ns;
        if (commit_ns > maximum_commit_ns) {
            maximum_commit_ns = commit_ns;
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
    if (maximum_commit_ns >= COMMIT_LIMIT_NS) {
        fputs("control commit exceeded 20 ms\n", stderr);
        return 1;
    }

    printf(
        "{\"schema\":\"music-studies/generation-publication-spike/v1\","
        "\"iterations\":%" PRIu64 ",\"atomic_pointer_lock_free\":true,"
        "\"maximum_control_commit_ns\":%" PRIu64 ",\"failures\":0}\n",
        GENERATION_COUNT - UINT64_C(1),
        maximum_commit_ns
    );
    return 0;
}
