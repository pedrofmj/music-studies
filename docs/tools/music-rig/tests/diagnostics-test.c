#include "music_rig/diagnostics.h"

#include <stdio.h>
#include <string.h>

#define RECORD_CAPACITY 8U

typedef struct mock_sink {
    music_rig_diagnostic_record records[RECORD_CAPACITY];
    char codes[RECORD_CAPACITY][MUSIC_RIG_DIAGNOSTIC_CODE_CAPACITY];
    char messages[RECORD_CAPACITY][MUSIC_RIG_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t count;
    music_rig_result result;
} mock_sink;

static music_rig_result write_record(
    void *opaque,
    const music_rig_diagnostic_record *record
)
{
    mock_sink *sink = opaque;

    if (sink->result != MUSIC_RIG_RESULT_OK) {
        return sink->result;
    }
    if (sink->count >= RECORD_CAPACITY) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    sink->records[sink->count] = *record;
    memcpy(sink->codes[sink->count], record->code, strlen(record->code) + 1U);
    memcpy(sink->messages[sink->count], record->message,
        strlen(record->message) + 1U);
    sink->records[sink->count].code = sink->codes[sink->count];
    sink->records[sink->count].message = sink->messages[sink->count];
    sink->count += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static int test_rate_limit(void)
{
    mock_sink mock = {0};
    music_rig_diagnostic_sink sink = {
        MUSIC_RIG_DIAGNOSTIC_SINK_ABI_VERSION,
        &mock,
        write_record
    };
    music_rig_diagnostics diagnostics;
    const music_rig_diagnostic_metrics *metrics;

    if (music_rig_diagnostics_init(
            &diagnostics,
            UINT64_C(100),
            UINT32_C(2),
            &sink
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(10), MUSIC_RIG_DIAGNOSTIC_INFO,
            "runtime.started", "ready"
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(11), MUSIC_RIG_DIAGNOSTIC_WARNING,
            "runtime.retry", "retry one"
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(12), MUSIC_RIG_DIAGNOSTIC_WARNING,
            "runtime.retry", "suppressed"
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(110), MUSIC_RIG_DIAGNOSTIC_ERROR,
            "runtime.failed", "stopped"
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("diagnostic rate-limit setup failed\n", stderr);
        return 1;
    }
    metrics = music_rig_diagnostics_get_metrics(&diagnostics);
    if (mock.count != 3U || metrics == NULL ||
        metrics->attempted != UINT64_C(4) ||
        metrics->emitted != UINT64_C(3) ||
        metrics->suppressed != UINT64_C(1) ||
        metrics->sink_failures != UINT64_C(0) ||
        mock.records[2].suppressed_before != UINT64_C(1) ||
        strcmp(mock.records[2].code, "runtime.failed") != 0 ||
        mock.records[2].severity != MUSIC_RIG_DIAGNOSTIC_ERROR) {
        fputs("diagnostic rate-limit result is incorrect\n", stderr);
        return 1;
    }
    return 0;
}

static int test_failures(void)
{
    mock_sink mock = {0};
    music_rig_diagnostic_sink sink = {
        MUSIC_RIG_DIAGNOSTIC_SINK_ABI_VERSION,
        &mock,
        write_record
    };
    music_rig_diagnostics diagnostics;
    char long_code[MUSIC_RIG_DIAGNOSTIC_CODE_CAPACITY];

    memset(long_code, 'a', sizeof(long_code));
    if (music_rig_diagnostics_init(NULL, UINT64_C(1), UINT32_C(1), &sink) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_diagnostics_init(
            &diagnostics, UINT64_C(1), UINT32_C(1), &sink
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(2), MUSIC_RIG_DIAGNOSTIC_INFO,
            long_code, "invalid"
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(2), MUSIC_RIG_DIAGNOSTIC_INFO,
            "Runtime.Invalid", "invalid"
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(2), MUSIC_RIG_DIAGNOSTIC_INFO,
            "runtime.invalid", "line\nbreak"
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid diagnostic input was accepted\n", stderr);
        return 1;
    }

    mock.result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    if (music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(2), MUSIC_RIG_DIAGNOSTIC_INFO,
            "runtime.failed", "sink failed"
        ) != MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        diagnostics.metrics.sink_failures != UINT64_C(1) ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(1), MUSIC_RIG_DIAGNOSTIC_INFO,
            "runtime.old", "time moved backward"
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_diagnostics_get_metrics(NULL) != NULL) {
        fputs("diagnostic failure contract is incorrect\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_rate_limit() != 0) {
        return 1;
    }
    return test_failures();
}
