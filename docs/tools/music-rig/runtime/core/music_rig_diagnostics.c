#include "music_rig/diagnostics.h"

#include <string.h>

static void increment(uint64_t *counter)
{
    if (*counter != UINT64_MAX) {
        *counter += UINT64_C(1);
    }
}

static bool bounded_printable(
    const char *value,
    size_t capacity,
    bool code
)
{
    size_t index;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (index = 0U; index < capacity; ++index) {
        unsigned char byte = (unsigned char)value[index];

        if (byte == '\0') {
            return true;
        }
        if (byte < (unsigned char)0x20 || byte > (unsigned char)0x7e ||
            (code && !((byte >= (unsigned char)'a' &&
                        byte <= (unsigned char)'z') ||
                       (byte >= (unsigned char)'0' &&
                        byte <= (unsigned char)'9') ||
                       byte == (unsigned char)'.' ||
                       byte == (unsigned char)'-'))) {
            return false;
        }
    }
    return false;
}

music_rig_result music_rig_diagnostics_init(
    music_rig_diagnostics *diagnostics,
    uint64_t interval_ns,
    uint32_t burst,
    const music_rig_diagnostic_sink *sink
)
{
    if (diagnostics == NULL || interval_ns == UINT64_C(0) || burst == 0U ||
        sink == NULL ||
        sink->abi_version != MUSIC_RIG_DIAGNOSTIC_SINK_ABI_VERSION ||
        sink->write == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(diagnostics, 0, sizeof(*diagnostics));
    diagnostics->sink = *sink;
    diagnostics->interval_ns = interval_ns;
    diagnostics->burst = burst;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_diagnostics_emit(
    music_rig_diagnostics *diagnostics,
    uint64_t timestamp_ns,
    music_rig_diagnostic_severity severity,
    const char *code,
    const char *message
)
{
    music_rig_diagnostic_record record;
    music_rig_result result;

    if (diagnostics == NULL ||
        severity < MUSIC_RIG_DIAGNOSTIC_INFO ||
        severity > MUSIC_RIG_DIAGNOSTIC_ERROR ||
        !bounded_printable(code, MUSIC_RIG_DIAGNOSTIC_CODE_CAPACITY, true) ||
        !bounded_printable(
            message,
            MUSIC_RIG_DIAGNOSTIC_MESSAGE_CAPACITY,
            false
        ) ||
        (diagnostics->started &&
         timestamp_ns < diagnostics->last_timestamp_ns)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    increment(&diagnostics->metrics.attempted);
    if (!diagnostics->started ||
        timestamp_ns - diagnostics->window_started_ns >=
            diagnostics->interval_ns) {
        diagnostics->window_started_ns = timestamp_ns;
        diagnostics->emitted_in_window = 0U;
        diagnostics->started = true;
    }
    diagnostics->last_timestamp_ns = timestamp_ns;

    if (diagnostics->emitted_in_window >= diagnostics->burst) {
        increment(&diagnostics->metrics.suppressed);
        increment(&diagnostics->suppressed_pending);
        return MUSIC_RIG_RESULT_OK;
    }
    diagnostics->emitted_in_window += 1U;

    record.timestamp_ns = timestamp_ns;
    record.suppressed_before = diagnostics->suppressed_pending;
    record.severity = severity;
    record.code = code;
    record.message = message;
    result = diagnostics->sink.write(diagnostics->sink.context, &record);
    if (result != MUSIC_RIG_RESULT_OK) {
        increment(&diagnostics->metrics.sink_failures);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }

    diagnostics->suppressed_pending = UINT64_C(0);
    increment(&diagnostics->metrics.emitted);
    return MUSIC_RIG_RESULT_OK;
}

const music_rig_diagnostic_metrics *music_rig_diagnostics_get_metrics(
    const music_rig_diagnostics *diagnostics
)
{
    return diagnostics == NULL ? NULL : &diagnostics->metrics;
}
