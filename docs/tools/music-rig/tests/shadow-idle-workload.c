#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "music_rig/definition.h"
#include "music_rig/definition_json.h"
#include "music_rig/device_midi_shadow.h"
#include "music_rig/file_storage.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#define DOCUMENT_CAPACITY ((size_t)131072)
#define MAXIMUM_DURATION_MS UINT32_C(86400000)

static uint8_t definition_document[DOCUMENT_CAPACITY];
static music_rig_compiled_tables definition_tables;
static music_rig_device_midi_shadow shadow;

static int parse_duration(const char *text, uint32_t *duration_ms)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || duration_ms == NULL || text[0] == '\0') {
        return 0;
    }
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || end == NULL || *end != '\0' ||
        value == 0UL || value > (unsigned long)MAXIMUM_DURATION_MS) {
        return 0;
    }
    *duration_ms = (uint32_t)value;
    return 1;
}

static music_rig_result load_shadow(
    const char *path,
    const char *fingerprint,
    music_rig_generation *generation,
    music_rig_generation_slot *generations
)
{
    music_rig_file_storage file_storage = {0};
    music_rig_storage_adapter storage = {0};
    music_rig_definition_decoder decoder;
    music_rig_compiled_definition definition = {0};
    music_rig_device_midi_shadow_config config;
    uint8_t expected[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    music_rig_result result;
    size_t index;

    result = music_rig_definition_fingerprint_parse(
        fingerprint,
        strlen(fingerprint),
        expected,
        sizeof(expected)
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_file_storage_init(
            &file_storage,
            path,
            NULL,
            &storage
        );
    }
    decoder.context = NULL;
    decoder.decode = music_rig_definition_json_decode;
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_definition_load(
            &storage,
            &decoder,
            definition_document,
            sizeof(definition_document),
            expected,
            sizeof(expected),
            &definition,
            &definition_tables
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_definition_generation_init(
            &definition,
            &definition_tables,
            generation
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_generation_slot_init(generations, generation);
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = generations;
    for (index = 0U;
         result == MUSIC_RIG_RESULT_OK &&
            index < definition_tables.device_profile_count;
         ++index) {
        const char *preset =
            definition_tables.device_profiles[index].hardware_preset;

        if (strcmp(preset, "arturia-current-rack") == 0) {
            config.behaviors[index] =
                MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA;
        } else if (strcmp(preset, "smk25-current-pad-layers") == 0) {
            config.behaviors[index] =
                MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25;
        }
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_device_midi_shadow_init(&shadow, &config);
    }
    if (result == MUSIC_RIG_RESULT_OK &&
        music_rig_device_midi_shadow_slot_count(&shadow) != 5U) {
        result = MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return result;
}

static int write_ready_file(const char *path)
{
    FILE *stream = NULL;
    int result;

    if (path == NULL) {
        return 1;
    }
#if defined(_MSC_VER)
    if (fopen_s(&stream, path, "wb") != 0) {
        stream = NULL;
    }
#else
    stream = fopen(path, "wb");
#endif
    if (stream == NULL) {
        return 0;
    }
    result = fputc('1', stream) == '1' && fflush(stream) == 0 &&
        fclose(stream) == 0;
    return result;
}

#if defined(_WIN32)
static int idle_wait(
    uint32_t duration_ms,
    uint64_t *duration_ns,
    uint64_t *wait_calls
)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER started;
    LARGE_INTEGER finished;
    HANDLE event;
    DWORD wait_result;
    DWORD wait_ms;
    uint64_t deadline_ticks;
    uint64_t remaining_ticks;
    uint64_t requested_ticks;
    uint64_t started_ticks;
    uint64_t ticks;
    uint64_t ticks_per_second;

    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&started)) {
        return 0;
    }
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (event == NULL) {
        return 0;
    }
    ticks_per_second = (uint64_t)frequency.QuadPart;
    started_ticks = (uint64_t)started.QuadPart;
    requested_ticks = (
        (uint64_t)duration_ms * ticks_per_second + UINT64_C(999)
    ) / UINT64_C(1000);
    if (requested_ticks == UINT64_C(0)) {
        requested_ticks = UINT64_C(1);
    }
    deadline_ticks = started_ticks + requested_ticks;
    *wait_calls = UINT64_C(0);
    for (;;) {
        if (!QueryPerformanceCounter(&finished) ||
            finished.QuadPart < started.QuadPart) {
            CloseHandle(event);
            return 0;
        }
        if ((uint64_t)finished.QuadPart >= deadline_ticks) {
            break;
        }
        remaining_ticks = deadline_ticks - (uint64_t)finished.QuadPart;
        wait_ms = (DWORD)(
            remaining_ticks / ticks_per_second * UINT64_C(1000) +
            (remaining_ticks % ticks_per_second * UINT64_C(1000) +
                ticks_per_second - UINT64_C(1)) /
                ticks_per_second
        );
        if (wait_ms == 0U) {
            wait_ms = 1U;
        }
        *wait_calls += UINT64_C(1);
        wait_result = WaitForSingleObject(event, wait_ms);
        if (wait_result != WAIT_TIMEOUT) {
            CloseHandle(event);
            return 0;
        }
    }
    CloseHandle(event);
    ticks = (uint64_t)(finished.QuadPart - started.QuadPart);
    *duration_ns = ticks / ticks_per_second * UINT64_C(1000000000) +
        ticks % ticks_per_second * UINT64_C(1000000000) / ticks_per_second;
    return 1;
}
#else
static int idle_wait(
    uint32_t duration_ms,
    uint64_t *duration_ns,
    uint64_t *wait_calls
)
{
    struct timespec started;
    struct timespec finished;
    struct timespec remaining;
    uint64_t deadline_ns;
    uint64_t finished_ns;
    uint64_t started_ns;

    if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) {
        return 0;
    }
    started_ns =
        (uint64_t)started.tv_sec * UINT64_C(1000000000) +
        (uint64_t)started.tv_nsec;
    deadline_ns = started_ns + (uint64_t)duration_ms * UINT64_C(1000000);
    *wait_calls = UINT64_C(0);
    for (;;) {
        if (clock_gettime(CLOCK_MONOTONIC, &finished) != 0) {
            return 0;
        }
        finished_ns =
            (uint64_t)finished.tv_sec * UINT64_C(1000000000) +
            (uint64_t)finished.tv_nsec;
        if (finished_ns >= deadline_ns) {
            break;
        }
        remaining.tv_sec = (time_t)(
            (deadline_ns - finished_ns) / UINT64_C(1000000000)
        );
        remaining.tv_nsec = (long)(
            (deadline_ns - finished_ns) % UINT64_C(1000000000)
        );
        *wait_calls += UINT64_C(1);
        if (nanosleep(&remaining, NULL) != 0 && errno != EINTR) {
            return 0;
        }
    }
    *duration_ns = finished_ns - started_ns;
    return 1;
}
#endif

static void print_usage(const char *program)
{
    fprintf(
        stderr,
        "Usage: %s --definition PATH --expected-fingerprint SHA256 "
        "--duration-ms N [--ready-file PATH]\n",
        program
    );
}

int main(int argc, char **argv)
{
    music_rig_generation generation = {0};
    music_rig_generation_slot generations;
    const music_rig_device_midi_shadow_metrics *metrics;
    const char *ready_file = NULL;
    uint32_t duration_ms;
    uint64_t duration_ns;
    uint64_t wait_calls;
    music_rig_result result;

    if ((argc != 7 && argc != 9) ||
        strcmp(argv[1], "--definition") != 0 ||
        strcmp(argv[3], "--expected-fingerprint") != 0 ||
        strcmp(argv[5], "--duration-ms") != 0 ||
        !parse_duration(argv[6], &duration_ms) ||
        (argc == 9 && strcmp(argv[7], "--ready-file") != 0)) {
        print_usage(argv[0]);
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (argc == 9) {
        ready_file = argv[8];
    }
    result = load_shadow(argv[2], argv[4], &generation, &generations);
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "shadow workload load failed: result %d\n", (int)result);
        return (int)result;
    }
    if (!write_ready_file(ready_file) ||
        !idle_wait(duration_ms, &duration_ns, &wait_calls)) {
        fputs("shadow workload wait failed\n", stderr);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    metrics = music_rig_device_midi_shadow_metrics_read(&shadow);
    if (metrics == NULL || metrics->cycles != UINT64_C(0) ||
        metrics->input_events != UINT64_C(0) ||
        metrics->mapping_decisions != UINT64_C(0) ||
        metrics->suppressed_midi_events != UINT64_C(0)) {
        fputs("shadow workload was not idle\n", stderr);
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    printf(
        "{\"schema\":\"music-studies/shadow-idle-workload/v1\","
        "\"platform\":\"%s\",\"generation\":%" PRIu64 ","
        "\"device_profiles\":%zu,\"output_mode\":\"suppressed\","
        "\"requested_duration_ms\":%" PRIu32 ","
        "\"observed_duration_ns\":%" PRIu64 ","
        "\"wait_primitive\":\"%s\",\"wait_calls\":%" PRIu64 ","
        "\"control_requests\":0,\"midi_events\":0,\"cycles\":0,"
        "\"mapping_decisions\":0,\"suppressed_midi_events\":0}\n",
#if defined(_WIN32)
        "windows",
#else
        "linux",
#endif
        generation.id,
        music_rig_device_midi_shadow_slot_count(&shadow),
        duration_ms,
        duration_ns,
#if defined(_WIN32)
        "WaitForSingleObject-event-timeout",
#else
        "nanosleep-monotonic-timeout",
#endif
        wait_calls
    );
    return MUSIC_RIG_RESULT_OK;
}
