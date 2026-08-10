#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "music_rig/core.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define GENERATION_COUNT 10000U
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
    static music_rig_generation generations[GENERATION_COUNT];
    music_rig_generation_slot slot;
    callback_context context;
    LARGE_INTEGER frequency;
    HANDLE callback_thread;
    uint32_t generation_index;
    uint64_t maximum_commit_ns = UINT64_C(0);
    DWORD wait_result;

    for (generation_index = 0U;
         generation_index < GENERATION_COUNT;
         ++generation_index) {
        generations[generation_index].id = (uint64_t)generation_index +
            UINT64_C(1);
        generations[generation_index].mapping = &generations[generation_index];
    }

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
        LARGE_INTEGER started;
        LARGE_INTEGER finished;
        uint64_t commit_ns;

        if (!QueryPerformanceCounter(&started) ||
            music_rig_generation_slot_publish(
                &slot,
                &generations[generation_index]
            ) != MUSIC_RIG_RESULT_OK ||
            !QueryPerformanceCounter(&finished) ||
            !elapsed_ns(started, finished, frequency, &commit_ns)) {
            fputs("Windows generation publication failed\n", stderr);
            CloseHandle(callback_thread);
            return 1;
        }
        if (commit_ns > maximum_commit_ns) {
            maximum_commit_ns = commit_ns;
        }
    }

    wait_result = WaitForSingleObject(callback_thread, ADOPTION_TIMEOUT_MS);
    CloseHandle(callback_thread);
    if (wait_result != WAIT_OBJECT_0 ||
        InterlockedCompareExchange(&context.failed, 0L, 0L) != 0L ||
        music_rig_generation_slot_adopted(&slot) !=
            &generations[GENERATION_COUNT - 1U]) {
        fputs("Windows synthetic callback adoption failed\n", stderr);
        return 1;
    }
    if (maximum_commit_ns >= COMMIT_LIMIT_NS) {
        fputs("Windows control commit exceeded 20 ms\n", stderr);
        return 1;
    }

    printf(
        "{\"schema\":\"music-studies/generation-publication-spike/v1\","
        "\"adapter\":\"windows-synthetic-callback\","
        "\"iterations\":%u,\"atomic_pointer_lock_free\":true,"
        "\"maximum_control_commit_ns\":%" PRIu64 ",\"failures\":0}\n",
        GENERATION_COUNT - 1U,
        maximum_commit_ns
    );
    return 0;
}
