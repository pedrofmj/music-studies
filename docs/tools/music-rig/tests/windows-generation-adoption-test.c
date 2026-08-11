#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "music_rig/core.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define GENERATION_COUNT 10000U
#define GENERATION_POOL_CAPACITY 4U
#define COMMIT_LIMIT_NS UINT64_C(20000000)
#define ADOPTION_TIMEOUT_MS 5000U

typedef struct callback_context {
    music_rig_generation_slot *slot;
    volatile LONG failed;
} callback_context;

static int elapsed_ns(
    LARGE_INTEGER started,
    LARGE_INTEGER finished,
    LARGE_INTEGER frequency,
    uint64_t *value
)
{
    uint64_t ticks;
    uint64_t ticks_per_second;

    if (finished.QuadPart < started.QuadPart || frequency.QuadPart <= 0 ||
        value == NULL) {
        return 0;
    }

    ticks = (uint64_t)(finished.QuadPart - started.QuadPart);
    ticks_per_second = (uint64_t)frequency.QuadPart;
    *value = ticks / ticks_per_second * UINT64_C(1000000000) +
        ticks % ticks_per_second * UINT64_C(1000000000) / ticks_per_second;
    return 1;
}

static DWORD WINAPI synthetic_callback(LPVOID opaque)
{
    callback_context *context = opaque;
    uint64_t previous_id = UINT64_C(0);

    for (;;) {
        const music_rig_generation *generation =
            music_rig_generation_slot_adopt(context->slot);

        if (generation == NULL || generation->id < previous_id) {
            InterlockedExchange(&context->failed, 1L);
            return 1U;
        }
        previous_id = generation->id;
        if (generation->id == (uint64_t)GENERATION_COUNT) {
            return 0U;
        }
        SwitchToThread();
    }
}

int main(void)
{
    static music_rig_generation generations[GENERATION_POOL_CAPACITY];
    bool available[GENERATION_POOL_CAPACITY] = {false, true, true, true};
    music_rig_generation_slot slot;
    callback_context context;
    LARGE_INTEGER frequency;
    HANDLE callback_thread;
    uint32_t generation_index;
    uint64_t maximum_commit_ns = UINT64_C(0);
    uint64_t reclaimed_count = UINT64_C(0);
    size_t maximum_retired = 0U;
    DWORD wait_result;

    generations[0].id = UINT64_C(1);
    generations[0].mapping = &generations[0];

    if (!QueryPerformanceFrequency(&frequency) ||
        music_rig_generation_slot_init(&slot, &generations[0]) !=
            MUSIC_RIG_RESULT_OK ||
        !music_rig_generation_slot_is_lock_free(&slot)) {
        fputs("lock-free Windows generation slot unavailable\n", stderr);
        return 1;
    }

    context.slot = &slot;
    context.failed = 0L;
    callback_thread = CreateThread(
        NULL,
        0U,
        synthetic_callback,
        &context,
        0U,
        NULL
    );
    if (callback_thread == NULL) {
        fputs("could not start Windows synthetic callback\n", stderr);
        return 1;
    }

    for (generation_index = 1U;
         generation_index < GENERATION_COUNT;
         ++generation_index) {
        size_t storage_index = GENERATION_POOL_CAPACITY;
        LARGE_INTEGER started;
        LARGE_INTEGER finished;
        uint64_t commit_ns;
        ULONGLONG pool_wait_started = GetTickCount64();

        while (storage_index == GENERATION_POOL_CAPACITY) {
            const music_rig_generation *reclaimed;
            size_t index;

            while ((reclaimed = music_rig_generation_slot_reclaim(&slot)) !=
                NULL) {
                size_t reclaimed_index = (size_t)(reclaimed - generations);

                if (reclaimed_index >= GENERATION_POOL_CAPACITY ||
                    available[reclaimed_index]) {
                    fputs("invalid Windows reclaimed generation\n", stderr);
                    CloseHandle(callback_thread);
                    return 1;
                }
                available[reclaimed_index] = true;
                reclaimed_count += UINT64_C(1);
            }
            for (index = 0U; index < GENERATION_POOL_CAPACITY; ++index) {
                if (available[index]) {
                    storage_index = index;
                    break;
                }
            }
            if (storage_index == GENERATION_POOL_CAPACITY) {
                if (GetTickCount64() - pool_wait_started >
                    (ULONGLONG)ADOPTION_TIMEOUT_MS) {
                    fputs("Windows generation pool reclamation timed out\n",
                        stderr);
                    CloseHandle(callback_thread);
                    return 1;
                }
                SwitchToThread();
            }
        }

        generations[storage_index].id = (uint64_t)generation_index +
            UINT64_C(1);
        generations[storage_index].mapping = &generations[storage_index];

        if (!QueryPerformanceCounter(&started) ||
            music_rig_generation_slot_publish(
                &slot,
                &generations[storage_index]
            ) != MUSIC_RIG_RESULT_OK ||
            !QueryPerformanceCounter(&finished) ||
            !elapsed_ns(started, finished, frequency, &commit_ns)) {
            fputs("Windows generation publication failed\n", stderr);
            CloseHandle(callback_thread);
            return 1;
        }
        available[storage_index] = false;
        if (commit_ns > maximum_commit_ns) {
            maximum_commit_ns = commit_ns;
        }
        if (music_rig_generation_slot_retired_count(&slot) >
            maximum_retired) {
            maximum_retired = music_rig_generation_slot_retired_count(&slot);
        }
    }

    wait_result = WaitForSingleObject(callback_thread, ADOPTION_TIMEOUT_MS);
    CloseHandle(callback_thread);
    if (wait_result != WAIT_OBJECT_0 ||
        InterlockedCompareExchange(&context.failed, 0L, 0L) != 0L ||
        music_rig_generation_slot_adopted(&slot)->id !=
            (uint64_t)GENERATION_COUNT) {
        fputs("Windows synthetic callback adoption failed\n", stderr);
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
    if (reclaimed_count != (uint64_t)GENERATION_COUNT - UINT64_C(1) ||
        music_rig_generation_slot_retired_count(&slot) != 0U ||
        maximum_retired >= GENERATION_POOL_CAPACITY) {
        fputs("bounded Windows generation reclamation proof failed\n", stderr);
        return 1;
    }
    if (maximum_commit_ns >= COMMIT_LIMIT_NS) {
        fputs("Windows control commit exceeded 20 ms\n", stderr);
        return 1;
    }

    printf(
        "{\"schema\":\"music-studies/generation-publication-spike/v2\","
        "\"adapter\":\"windows-synthetic-callback\","
        "\"iterations\":%u,\"atomic_pointer_lock_free\":true,"
        "\"storage_slots\":%u,\"reclaimed\":%" PRIu64 ","
        "\"maximum_retired\":%zu,"
        "\"maximum_control_commit_ns\":%" PRIu64 ",\"failures\":0}\n",
        GENERATION_COUNT - 1U,
        GENERATION_POOL_CAPACITY,
        reclaimed_count,
        maximum_retired,
        maximum_commit_ns
    );
    return 0;
}
