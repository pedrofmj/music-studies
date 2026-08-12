#ifndef MUSIC_RIG_DIAGNOSTICS_H
#define MUSIC_RIG_DIAGNOSTICS_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_DIAGNOSTIC_SINK_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_DIAGNOSTIC_CODE_CAPACITY ((size_t)65)
#define MUSIC_RIG_DIAGNOSTIC_MESSAGE_CAPACITY ((size_t)257)

typedef enum music_rig_diagnostic_severity {
    MUSIC_RIG_DIAGNOSTIC_INVALID = 0,
    MUSIC_RIG_DIAGNOSTIC_INFO = 1,
    MUSIC_RIG_DIAGNOSTIC_WARNING = 2,
    MUSIC_RIG_DIAGNOSTIC_ERROR = 3
} music_rig_diagnostic_severity;

typedef struct music_rig_diagnostic_record {
    uint64_t timestamp_ns;
    uint64_t suppressed_before;
    music_rig_diagnostic_severity severity;
    const char *code;
    const char *message;
} music_rig_diagnostic_record;

typedef struct music_rig_diagnostic_sink {
    uint32_t abi_version;
    void *context;
    music_rig_result (*write)(
        void *context,
        const music_rig_diagnostic_record *record
    );
} music_rig_diagnostic_sink;

typedef struct music_rig_diagnostic_metrics {
    uint64_t attempted;
    uint64_t emitted;
    uint64_t suppressed;
    uint64_t sink_failures;
} music_rig_diagnostic_metrics;

/* Single control-thread state. Never call from a real-time callback. */
typedef struct music_rig_diagnostics {
    music_rig_diagnostic_sink sink;
    music_rig_diagnostic_metrics metrics;
    uint64_t interval_ns;
    uint64_t window_started_ns;
    uint64_t last_timestamp_ns;
    uint64_t suppressed_pending;
    uint32_t burst;
    uint32_t emitted_in_window;
    bool started;
} music_rig_diagnostics;

music_rig_result music_rig_diagnostics_init(
    music_rig_diagnostics *diagnostics,
    uint64_t interval_ns,
    uint32_t burst,
    const music_rig_diagnostic_sink *sink
);

music_rig_result music_rig_diagnostics_emit(
    music_rig_diagnostics *diagnostics,
    uint64_t timestamp_ns,
    music_rig_diagnostic_severity severity,
    const char *code,
    const char *message
);

const music_rig_diagnostic_metrics *music_rig_diagnostics_get_metrics(
    const music_rig_diagnostics *diagnostics
);

#endif
