#ifndef MUSIC_RIG_RUNTIME_H
#define MUSIC_RIG_RUNTIME_H

#include "music_rig/core.h"
#include "music_rig/definition.h"
#include "music_rig/device_ports.h"
#include "music_rig/protocol.h"
#include "music_rig/state.h"
#include "music_rig/storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_RUNTIME_ABI_VERSION UINT32_C(7)
#define MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_RUNTIME_COMMIT_GENERATION_CAPACITY \
    (MUSIC_RIG_RETIRED_GENERATION_CAPACITY + (size_t)1)

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
    uint64_t list_requests;
    uint64_t validate_requests;
    uint64_t dry_run_requests;
    uint64_t unsupported_requests;
    uint64_t invalid_requests;
    uint64_t control_responses;
    uint64_t generation_publications;
    uint64_t generation_conflicts;
    uint64_t commit_requests;
    uint64_t commit_successes;
    uint64_t commit_rollbacks;
    uint64_t commit_rollback_failures;
    uint64_t generation_reclamations;
    uint64_t generation_backpressure;
    uint64_t port_identity_conflicts;
    uint64_t state_restores;
    uint64_t state_fallbacks;
    uint64_t state_writes;
    uint64_t output_enablements;
    uint64_t output_preparations;
    uint64_t output_adoptions;
    uint64_t output_adoption_failures;
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

typedef struct music_rig_output_adoption_adapter {
    uint32_t abi_version;
    void *context;
    music_rig_result (*prepare)(
        void *context,
        const music_rig_generation *generation
    );
    music_rig_result (*confirm)(
        void *context,
        const music_rig_generation *generation
    );
} music_rig_output_adoption_adapter;

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
    const char *active_rig_profile;
    const music_rig_prepared_definition *prepared_definitions;
    size_t prepared_definition_count;
    music_rig_output_mode output_mode;
    const music_rig_output_adoption_adapter *output_adoption;
} music_rig_runtime_config;

/* Caller-owned storage keeps initialization deterministic and allocation-free. */
typedef struct music_rig_runtime {
    music_rig_runtime_state state;
    music_rig_runtime_metrics metrics;
    music_rig_generation initial_generation;
    music_rig_generation committed_generations[
        MUSIC_RIG_RUNTIME_COMMIT_GENERATION_CAPACITY
    ];
    bool committed_generation_in_use[
        MUSIC_RIG_RUNTIME_COMMIT_GENERATION_CAPACITY
    ];
    const music_rig_generation *control_generation;
    music_rig_generation_slot generations;
    music_rig_device_port_catalogue device_ports;
    music_rig_platform_interfaces interfaces;
    music_rig_output_adoption_adapter output_adoption;
    char active_rig_profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    char initial_rig_profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    const music_rig_compiled_tables *initial_tables;
    const music_rig_compiled_tables *base_tables;
    const music_rig_prepared_definition *prepared_definitions;
    size_t prepared_definition_count;
    music_rig_compiled_tables device_override_tables[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    bool device_override_table_in_use[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    music_rig_persisted_device_override device_overrides[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    uint32_t device_override_count;
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

/* Explicitly arms a prepared backend before the runtime control loop starts. */
music_rig_result music_rig_runtime_enable_output(
    music_rig_runtime *runtime,
    const music_rig_output_adoption_adapter *adapter
);

const music_rig_generation *music_rig_runtime_reclaim_generation(
    music_rig_runtime *runtime
);

music_rig_result music_rig_runtime_run(music_rig_runtime *runtime);

music_rig_result music_rig_runtime_dispatch(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
);

music_rig_result music_rig_runtime_persist_state(music_rig_runtime *runtime);

const music_rig_runtime_state *music_rig_runtime_get_state(
    const music_rig_runtime *runtime
);

const music_rig_runtime_metrics *music_rig_runtime_get_metrics(
    const music_rig_runtime *runtime
);

const music_rig_device_port_catalogue *music_rig_runtime_get_device_ports(
    const music_rig_runtime *runtime
);

#endif
