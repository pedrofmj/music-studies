#ifndef MUSIC_RIG_RUNTIME_H
#define MUSIC_RIG_RUNTIME_H

#include "music_rig/core.h"
#include "music_rig/protocol.h"
#include "music_rig/state.h"
#include "music_rig/storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_RUNTIME_ABI_VERSION UINT32_C(2)

typedef enum music_rig_runtime_lifecycle {
    MUSIC_RIG_RUNTIME_UNINITIALIZED = 0,
    MUSIC_RIG_RUNTIME_INITIALIZED = 1,
    MUSIC_RIG_RUNTIME_RUNNING = 2,
    MUSIC_RIG_RUNTIME_STOPPED = 3,
    MUSIC_RIG_RUNTIME_FAILED = 4
} music_rig_runtime_lifecycle;

typedef enum music_rig_control_poll {
    MUSIC_RIG_CONTROL_REQUEST = 0,
    MUSIC_RIG_CONTROL_IDLE = 1,
    MUSIC_RIG_CONTROL_STOP = 2,
    MUSIC_RIG_CONTROL_ERROR = 3
} music_rig_control_poll;

typedef struct music_rig_runtime_state {
    uint32_t schema_version;
    music_rig_runtime_lifecycle lifecycle;
    uint64_t generation_id;
    uint8_t definition_fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    music_rig_output_mode output_mode;
    uint64_t started_at_ns;
    uint64_t stopped_at_ns;
} music_rig_runtime_state;

typedef struct music_rig_runtime_metrics {
    uint64_t loop_iterations;
    uint64_t idle_polls;
    uint64_t control_waits;
    uint64_t control_requests;
    uint64_t status_requests;
    uint64_t invalid_requests;
    uint64_t control_responses;
    uint64_t generation_publications;
    uint64_t generation_conflicts;
    uint64_t state_restores;
    uint64_t state_fallbacks;
    uint64_t state_writes;
    uint64_t adapter_failures;
} music_rig_runtime_metrics;

typedef struct music_rig_clock_adapter {
    void *context;
    uint64_t (*now_ns)(void *context);
} music_rig_clock_adapter;

typedef struct music_rig_control_adapter {
    void *context;
    music_rig_result (*start)(void *context);
    music_rig_control_poll (*poll)(
        void *context,
        music_rig_protocol_request *request
    );
    music_rig_result (*wait)(void *context);
    music_rig_result (*respond)(
        void *context,
        const music_rig_protocol_response *response
    );
    music_rig_result (*stop)(void *context);
} music_rig_control_adapter;

typedef struct music_rig_platform_interfaces {
    uint32_t abi_version;
    music_rig_clock_adapter clock;
    music_rig_control_adapter control;
    music_rig_storage_adapter storage;
} music_rig_platform_interfaces;

typedef struct music_rig_runtime_config {
    const music_rig_generation *initial_generation;
    const uint8_t *definition_fingerprint;
    size_t definition_fingerprint_size;
    music_rig_output_mode output_mode;
} music_rig_runtime_config;

/* Caller-owned storage keeps initialization deterministic and allocation-free. */
typedef struct music_rig_runtime {
    music_rig_runtime_state state;
    music_rig_runtime_metrics metrics;
    music_rig_generation initial_generation;
    music_rig_generation_slot generations;
    music_rig_platform_interfaces interfaces;
    bool control_started;
} music_rig_runtime;

music_rig_result music_rig_runtime_init(
    music_rig_runtime *runtime,
    const music_rig_runtime_config *config,
    const music_rig_platform_interfaces *interfaces
);

music_rig_result music_rig_runtime_publish_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *next_generation,
    uint64_t expected_generation
);

music_rig_result music_rig_runtime_run(music_rig_runtime *runtime);

music_rig_result music_rig_runtime_persist_state(music_rig_runtime *runtime);

const music_rig_runtime_state *music_rig_runtime_get_state(
    const music_rig_runtime *runtime
);

const music_rig_runtime_metrics *music_rig_runtime_get_metrics(
    const music_rig_runtime *runtime
);

#endif
