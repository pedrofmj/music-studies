#include "music_rig/runtime.h"

#include "music_rig/control.h"

#include <stdint.h>
#include <string.h>

_Static_assert(
    MUSIC_RIG_PERSISTED_PROFILE_CAPACITY ==
        MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY,
    "persistent and protocol profile identifiers must have equal capacity"
);

static void increment(uint64_t *counter)
{
    if (*counter != UINT64_MAX) {
        *counter += UINT64_C(1);
    }
}

static bool bounded_profile(const char *value)
{
    size_t index;

    if (value == NULL) {
        return false;
    }
    for (index = 0; index < MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY; ++index) {
        if (value[index] == '\0') {
            return index != 0;
        }
    }
    return false;
}

static void copy_profile(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static int device_override_index(
    const music_rig_runtime *runtime,
    const char *device_slot
)
{
    uint32_t index;

    for (index = 0U; index < runtime->device_override_count; ++index) {
        if (strcmp(runtime->device_overrides[index].device_slot,
                device_slot) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static const music_rig_compiled_tables *device_profile_tables(
    const music_rig_runtime *runtime,
    const char *device_slot,
    const char *profile
)
{
    size_t index;
    uint16_t profile_index;

    for (index = 0U; index < runtime->prepared_definition_count; ++index) {
        const music_rig_prepared_definition *prepared =
            &runtime->prepared_definitions[index];

        if (music_rig_compiled_profile_index(
                prepared->tables, device_slot, &profile_index
            ) == MUSIC_RIG_RESULT_OK && strcmp(
                prepared->tables->device_profiles[profile_index].profile,
                profile
            ) == 0) {
            return prepared->tables;
        }
    }
    return NULL;
}

static int free_device_override_table(const music_rig_runtime *runtime)
{
    size_t index;

    for (index = 0U; index < MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY;
         ++index) {
        if (!runtime->device_override_table_in_use[index]) {
            return (int)index;
        }
    }
    return -1;
}

static void release_device_override_table(
    music_rig_runtime *runtime,
    const music_rig_compiled_tables *tables
)
{
    size_t index;

    for (index = 0U; index < MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY;
         ++index) {
        if (&runtime->device_override_tables[index] == tables) {
            runtime->device_override_table_in_use[index] = false;
            return;
        }
    }
}

static const music_rig_compiled_tables *profile_tables(
    const music_rig_runtime *runtime,
    const char *profile
)
{
    size_t index;

    if (strcmp(profile, runtime->initial_rig_profile) == 0) {
        return runtime->initial_tables;
    }
    for (index = 0U; index < runtime->prepared_definition_count; ++index) {
        if (strcmp(
                profile,
                runtime->prepared_definitions[index].definition->
                    active_rig_profile
            ) == 0) {
            return runtime->prepared_definitions[index].tables;
        }
    }
    return NULL;
}

static bool interfaces_are_valid(
    const music_rig_platform_interfaces *interfaces
)
{
    return interfaces != NULL &&
        interfaces->abi_version == MUSIC_RIG_RUNTIME_ABI_VERSION &&
        interfaces->clock.now_ns != NULL &&
        interfaces->control.start != NULL &&
        interfaces->control.poll != NULL &&
        interfaces->control.wait != NULL &&
        interfaces->control.respond != NULL &&
        interfaces->control.stop != NULL &&
        interfaces->storage.abi_version == MUSIC_RIG_STORAGE_ABI_VERSION &&
        interfaces->storage.read != NULL &&
        interfaces->storage.atomic_replace != NULL;
}

static bool output_adoption_is_valid(
    const music_rig_output_adoption_adapter *adapter
)
{
    return adapter != NULL &&
        adapter->abi_version ==
            MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION &&
        adapter->prepare != NULL && adapter->confirm != NULL;
}

static music_rig_result prepare_output_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *generation
)
{
    music_rig_result result;

    increment(&runtime->metrics.output_preparations);
    result = runtime->output_adoption.prepare(
        runtime->output_adoption.context,
        generation
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        increment(&runtime->metrics.output_adoption_failures);
        if (result == MUSIC_RIG_RESULT_ADAPTER_FAILURE) {
            increment(&runtime->metrics.adapter_failures);
        }
    }
    return result;
}

static music_rig_result confirm_output_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *generation
)
{
    music_rig_result result = runtime->output_adoption.confirm(
        runtime->output_adoption.context,
        generation
    );

    if (result == MUSIC_RIG_RESULT_OK) {
        increment(&runtime->metrics.output_adoptions);
    } else {
        increment(&runtime->metrics.output_adoption_failures);
        if (result == MUSIC_RIG_RESULT_ADAPTER_FAILURE) {
            increment(&runtime->metrics.adapter_failures);
        }
    }
    return result;
}

static music_rig_generation *allocate_commit_generation(
    music_rig_runtime *runtime,
    uint64_t generation_id,
    const music_rig_compiled_tables *tables
);

static bool release_commit_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *generation
);

static music_rig_result restore_state(music_rig_runtime *runtime)
{
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    size_t frame_size = 0;
    music_rig_persisted_state persisted;
    const music_rig_compiled_tables *tables;
    const char *profile;
    music_rig_result result;

    result = runtime->interfaces.storage.read(
        runtime->interfaces.storage.context,
        MUSIC_RIG_STORAGE_RUNTIME_STATE,
        frame,
        sizeof(frame),
        &frame_size
    );
    if (result == MUSIC_RIG_RESULT_NOT_FOUND) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (result == MUSIC_RIG_RESULT_BUFFER_TOO_SMALL) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        increment(&runtime->metrics.adapter_failures);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }

    result = music_rig_state_decode(frame, frame_size, &persisted);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    if (memcmp(
            persisted.definition_fingerprint,
            runtime->state.definition_fingerprint,
            MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
        ) != 0 ||
        persisted.generation_id < runtime->initial_generation.id) {
        increment(&runtime->metrics.state_fallbacks);
        return MUSIC_RIG_RESULT_OK;
    }

    profile = persisted.active_rig_profile[0] == 0
        ? runtime->initial_rig_profile
        : persisted.active_rig_profile;
    tables = profile_tables(runtime, profile);
    if (tables == NULL) {
        increment(&runtime->metrics.state_fallbacks);
        return MUSIC_RIG_RESULT_OK;
    }

    runtime->base_tables = tables;
    runtime->device_override_count = 0U;
    {
        const music_rig_compiled_tables *current = tables;
    for (uint32_t index = 0U; index < persisted.device_override_count; ++index) {
        const music_rig_compiled_tables *source = device_profile_tables(
            runtime,
            persisted.device_overrides[index].device_slot,
            persisted.device_overrides[index].profile
        );
        int table_index = free_device_override_table(runtime);

        if (source == NULL || table_index < 0 ||
            music_rig_compiled_tables_compose_device(
                current,
                source,
                persisted.device_overrides[index].device_slot,
                &runtime->device_override_tables[table_index]
            ) != MUSIC_RIG_RESULT_OK) {
            memset(runtime->device_override_table_in_use, 0,
                sizeof(runtime->device_override_table_in_use));
            runtime->device_override_count = 0U;
            increment(&runtime->metrics.state_fallbacks);
            return MUSIC_RIG_RESULT_OK;
        }
        runtime->device_override_table_in_use[table_index] = true;
        runtime->device_overrides[index] = persisted.device_overrides[index];
        runtime->device_override_count += 1U;
        current = &runtime->device_override_tables[table_index];
    }
    }
    if (runtime->device_override_count != 0U) {
        tables = &runtime->device_override_tables[
            runtime->device_override_count - 1U
        ];
    }

    runtime->initial_generation.id = persisted.generation_id;
    runtime->initial_generation.mapping = tables;
    runtime->state.generation_id = persisted.generation_id;
    copy_profile(runtime->active_rig_profile, profile);
    increment(&runtime->metrics.state_restores);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result finish(
    music_rig_runtime *runtime,
    music_rig_result result
)
{
    if (runtime->control_started) {
        if (runtime->interfaces.control.stop(
                runtime->interfaces.control.context
            ) != MUSIC_RIG_RESULT_OK) {
            increment(&runtime->metrics.adapter_failures);
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        runtime->control_started = false;
    }

    runtime->state.stopped_at_ns = runtime->interfaces.clock.now_ns(
        runtime->interfaces.clock.context
    );
    runtime->state.lifecycle = result == MUSIC_RIG_RESULT_OK
        ? MUSIC_RIG_RUNTIME_STOPPED
        : MUSIC_RIG_RUNTIME_FAILED;
    return result;
}

static music_rig_result adapter_failure(music_rig_runtime *runtime)
{
    increment(&runtime->metrics.adapter_failures);
    return finish(runtime, MUSIC_RIG_RESULT_ADAPTER_FAILURE);
}

static void classify_request(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    const music_rig_protocol_response *response
)
{
    if (response->result_code == (uint32_t)MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        increment(&runtime->metrics.invalid_requests);
    }
    if (response->result_code == (uint32_t)MUSIC_RIG_RESULT_UNSUPPORTED) {
        increment(&runtime->metrics.unsupported_requests);
    }
    if (response->result_code ==
        (uint32_t)MUSIC_RIG_RESULT_GENERATION_CONFLICT) {
        increment(&runtime->metrics.generation_conflicts);
    }
    if (request->flags == MUSIC_RIG_REQUEST_DRY_RUN) {
        increment(&runtime->metrics.dry_run_requests);
    }
    if (request->operation == (uint32_t)MUSIC_RIG_OPERATION_STATUS) {
        increment(&runtime->metrics.status_requests);
    } else if (request->operation ==
        (uint32_t)MUSIC_RIG_OPERATION_LIST_PROFILES) {
        increment(&runtime->metrics.list_requests);
    } else if (request->operation ==
        (uint32_t)MUSIC_RIG_OPERATION_VALIDATE_ACTIVE) {
        increment(&runtime->metrics.validate_requests);
    }
}

music_rig_result music_rig_runtime_init(
    music_rig_runtime *runtime,
    const music_rig_runtime_config *config,
    const music_rig_platform_interfaces *interfaces
)
{
    music_rig_result result;
    music_rig_control_snapshot snapshot;

    if (runtime == NULL || config == NULL ||
        config->initial_generation == NULL ||
        config->initial_generation->id == UINT64_C(0) ||
        config->definition_fingerprint == NULL ||
        config->definition_fingerprint_size !=
            MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE ||
        !bounded_profile(config->active_rig_profile) ||
        !interfaces_are_valid(interfaces) ||
        config->prepared_definition_count >
            MUSIC_RIG_PREPARED_DEFINITION_CAPACITY ||
        (config->prepared_definition_count != 0U &&
            config->prepared_definitions == NULL)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (config->output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED &&
        (config->output_mode != MUSIC_RIG_OUTPUT_ENABLED ||
         !output_adoption_is_valid(config->output_adoption))) {
        return config->output_mode == MUSIC_RIG_OUTPUT_ENABLED
            ? MUSIC_RIG_RESULT_INVALID_ARGUMENT
            : MUSIC_RIG_RESULT_UNSUPPORTED;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->interfaces = *interfaces;
    runtime->initial_generation = *config->initial_generation;
    runtime->initial_tables = config->initial_generation->mapping;
    runtime->base_tables = runtime->initial_tables;
    runtime->prepared_definitions = config->prepared_definitions;
    runtime->prepared_definition_count = config->prepared_definition_count;
    runtime->state.schema_version = MUSIC_RIG_RUNTIME_STATE_VERSION;
    runtime->state.generation_id = config->initial_generation->id;
    memcpy(
        runtime->state.definition_fingerprint,
        config->definition_fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    copy_profile(runtime->active_rig_profile, config->active_rig_profile);
    copy_profile(runtime->initial_rig_profile, config->active_rig_profile);
    runtime->state.output_mode = config->output_mode;
    if (config->output_mode == MUSIC_RIG_OUTPUT_ENABLED) {
        runtime->output_adoption = *config->output_adoption;
    }

    result = music_rig_device_port_catalogue_build(
        runtime->initial_tables,
        &runtime->device_ports
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    snapshot.generation_id = runtime->state.generation_id;
    snapshot.active_rig_profile = runtime->initial_rig_profile;
    snapshot.tables = runtime->initial_tables;
    snapshot.output_mode = runtime->state.output_mode;
    result = music_rig_control_prepared_definitions_validate(
        &snapshot,
        runtime->prepared_definitions,
        runtime->prepared_definition_count
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = restore_state(runtime);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    if (runtime->state.output_mode == MUSIC_RIG_OUTPUT_ENABLED) {
        result = prepare_output_generation(runtime, &runtime->initial_generation);
        if (result == MUSIC_RIG_RESULT_OK) {
            result = confirm_output_generation(
                runtime, &runtime->initial_generation
            );
        }
        if (result != MUSIC_RIG_RESULT_OK) {
            memset(&runtime->output_adoption, 0,
                sizeof(runtime->output_adoption));
            return result;
        }
    }
    result = music_rig_generation_slot_init(
        &runtime->generations,
        &runtime->initial_generation
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    runtime->control_generation = &runtime->initial_generation;
    runtime->state.lifecycle = MUSIC_RIG_RUNTIME_INITIALIZED;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_runtime_publish_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *next_generation,
    uint64_t expected_generation
)
{
    music_rig_device_port_catalogue next_ports;
    music_rig_result result;

    if (runtime == NULL || next_generation == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (runtime->state.lifecycle != MUSIC_RIG_RUNTIME_INITIALIZED &&
        runtime->state.lifecycle != MUSIC_RIG_RUNTIME_RUNNING) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (expected_generation != UINT64_C(0) &&
        expected_generation != runtime->state.generation_id) {
        increment(&runtime->metrics.generation_conflicts);
        return MUSIC_RIG_RESULT_GENERATION_CONFLICT;
    }
    result = music_rig_device_port_catalogue_build(
        next_generation->mapping,
        &next_ports
    );
    if (result != MUSIC_RIG_RESULT_OK ||
        !music_rig_device_port_catalogues_match(
            &runtime->device_ports,
            &next_ports
        )) {
        increment(&runtime->metrics.port_identity_conflicts);
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }

    if (runtime->state.output_mode == MUSIC_RIG_OUTPUT_ENABLED) {
        result = prepare_output_generation(runtime, next_generation);
        if (result != MUSIC_RIG_RESULT_OK) {
            return result;
        }
    }

    result = music_rig_generation_slot_publish(
        &runtime->generations,
        next_generation
    );
    if (result == MUSIC_RIG_RESULT_GENERATION_CONFLICT) {
        increment(&runtime->metrics.generation_conflicts);
        return result;
    }
    if (result == MUSIC_RIG_RESULT_INVALID_STATE) {
        increment(&runtime->metrics.generation_backpressure);
        return result;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    runtime->state.generation_id = next_generation->id;
    runtime->control_generation = next_generation;
    increment(&runtime->metrics.generation_publications);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_runtime_enable_output(
    music_rig_runtime *runtime,
    const music_rig_output_adoption_adapter *adapter
)
{
    music_rig_result result;

    if (runtime == NULL || !output_adoption_is_valid(adapter)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (runtime->state.lifecycle != MUSIC_RIG_RUNTIME_INITIALIZED ||
        runtime->state.output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }

    runtime->output_adoption = *adapter;
    result = prepare_output_generation(runtime, runtime->control_generation);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = confirm_output_generation(
            runtime,
            runtime->control_generation
        );
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        memset(
            &runtime->output_adoption,
            0,
            sizeof(runtime->output_adoption)
        );
        return result;
    }

    runtime->state.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    increment(&runtime->metrics.output_enablements);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_generation *allocate_commit_generation(
    music_rig_runtime *runtime,
    uint64_t generation_id,
    const music_rig_compiled_tables *tables
)
{
    size_t index;

    for (index = 0U;
         index < MUSIC_RIG_RUNTIME_COMMIT_GENERATION_CAPACITY;
         ++index) {
        if (!runtime->committed_generation_in_use[index]) {
            runtime->committed_generation_in_use[index] = true;
            runtime->committed_generations[index].id = generation_id;
            runtime->committed_generations[index].mapping = tables;
            return &runtime->committed_generations[index];
        }
    }
    return NULL;
}

static bool release_commit_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *generation
)
{
    size_t index;

    for (index = 0U;
         index < MUSIC_RIG_RUNTIME_COMMIT_GENERATION_CAPACITY;
         ++index) {
        if (generation == &runtime->committed_generations[index]) {
            runtime->committed_generation_in_use[index] = false;
            return true;
        }
    }
    return false;
}

static size_t available_commit_generations(const music_rig_runtime *runtime)
{
    size_t available = 0U;
    size_t index;

    for (index = 0U;
         index < MUSIC_RIG_RUNTIME_COMMIT_GENERATION_CAPACITY;
         ++index) {
        if (!runtime->committed_generation_in_use[index]) {
            available += 1U;
        }
    }
    return available;
}

const music_rig_generation *music_rig_runtime_reclaim_generation(
    music_rig_runtime *runtime
)
{
    const music_rig_generation *generation;

    if (runtime == NULL ||
        (runtime->state.lifecycle != MUSIC_RIG_RUNTIME_INITIALIZED &&
         runtime->state.lifecycle != MUSIC_RIG_RUNTIME_RUNNING)) {
        return NULL;
    }

    for (;;) {
        generation = music_rig_generation_slot_reclaim(
            &runtime->generations
        );
        if (generation == NULL) {
            return NULL;
        }
        increment(&runtime->metrics.generation_reclamations);
        release_device_override_table(runtime, generation->mapping);
        if (generation != &runtime->initial_generation &&
            !release_commit_generation(runtime, generation)) {
            return generation;
        }
    }
}

music_rig_result music_rig_runtime_run(music_rig_runtime *runtime)
{
    if (runtime == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (runtime->state.lifecycle != MUSIC_RIG_RUNTIME_INITIALIZED) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }

    runtime->state.started_at_ns = runtime->interfaces.clock.now_ns(
        runtime->interfaces.clock.context
    );
    if (runtime->interfaces.control.start(
            runtime->interfaces.control.context
        ) != MUSIC_RIG_RESULT_OK) {
        increment(&runtime->metrics.adapter_failures);
        runtime->state.stopped_at_ns = runtime->interfaces.clock.now_ns(
            runtime->interfaces.clock.context
        );
        runtime->state.lifecycle = MUSIC_RIG_RUNTIME_FAILED;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }

    runtime->control_started = true;
    runtime->state.lifecycle = MUSIC_RIG_RUNTIME_RUNNING;
    for (;;) {
        music_rig_protocol_request request;
        music_rig_control_poll poll_result;

        memset(&request, 0, sizeof(request));
        increment(&runtime->metrics.loop_iterations);
        poll_result = runtime->interfaces.control.poll(
            runtime->interfaces.control.context,
            &request
        );

        if (poll_result == MUSIC_RIG_CONTROL_REQUEST) {
            music_rig_protocol_response response;

            if (music_rig_runtime_dispatch(
                    runtime,
                    &request,
                    &response
                ) != MUSIC_RIG_RESULT_OK) {
                return adapter_failure(runtime);
            }
            if (runtime->interfaces.control.respond(
                    runtime->interfaces.control.context,
                    &response
                ) != MUSIC_RIG_RESULT_OK) {
                return adapter_failure(runtime);
            }
            increment(&runtime->metrics.control_responses);
        } else if (poll_result == MUSIC_RIG_CONTROL_IDLE) {
            increment(&runtime->metrics.idle_polls);
            if (runtime->interfaces.control.wait(
                    runtime->interfaces.control.context
                ) != MUSIC_RIG_RESULT_OK) {
                return adapter_failure(runtime);
            }
            increment(&runtime->metrics.control_waits);
        } else if (poll_result == MUSIC_RIG_CONTROL_STOP) {
            return finish(runtime, MUSIC_RIG_RESULT_OK);
        } else {
            return adapter_failure(runtime);
        }
    }
}

static music_rig_result plan_global_switch(
    music_rig_runtime *runtime,
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response,
    const music_rig_compiled_tables **target_tables
)
{
    music_rig_protocol_request plan_request = *request;
    music_rig_control_snapshot plan_snapshot = *snapshot;
    music_rig_result result;
    bool target_was_active;
    size_t index;

    plan_request.flags = MUSIC_RIG_REQUEST_DRY_RUN;
    *target_tables = profile_tables(runtime, request->profile);
    if (*target_tables == NULL) {
        return music_rig_control_dispatch_prepared(
            snapshot,
            runtime->prepared_definitions,
            runtime->prepared_definition_count,
            &plan_request,
            response
        );
    }

    target_was_active =
        strcmp(request->profile, runtime->active_rig_profile) == 0;
    if (!target_was_active &&
        strcmp(request->profile, runtime->initial_rig_profile) == 0) {
        plan_snapshot.active_rig_profile = runtime->initial_rig_profile;
        plan_snapshot.tables = runtime->initial_tables;
    }
    result = music_rig_control_dispatch_prepared(
        &plan_snapshot,
        runtime->prepared_definitions,
        runtime->prepared_definition_count,
        &plan_request,
        response
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    copy_profile(response->active_rig_profile, runtime->active_rig_profile);
    if (!target_was_active) {
        for (index = 0U; index < response->profile_count; ++index) {
            response->profiles[index].flags &= ~MUSIC_RIG_PROFILE_ACTIVE;
        }
    }
    if (request->flags != MUSIC_RIG_REQUEST_DRY_RUN) {
        response->flags &= ~MUSIC_RIG_RESPONSE_DRY_RUN;
    }
    return MUSIC_RIG_RESULT_OK;
}

static void mark_response_profiles_active(
    music_rig_protocol_response *response
)
{
    size_t index;

    for (index = 0U; index < response->profile_count; ++index) {
        response->profiles[index].flags |= MUSIC_RIG_PROFILE_ACTIVE;
    }
}

static void set_commit_failure(
    music_rig_protocol_response *response,
    music_rig_result result
)
{
    response->result_code = (uint32_t)result;
    response->flags &= ~(MUSIC_RIG_RESPONSE_DRY_RUN |
        MUSIC_RIG_RESPONSE_VALID | MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY);
}

typedef struct music_rig_commit_transaction {
    const music_rig_generation *previous_generation;
    music_rig_generation *generation;
    const music_rig_compiled_tables *previous_base_tables;
    music_rig_persisted_device_override previous_overrides[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    uint32_t previous_override_count;
} music_rig_commit_transaction;

static music_rig_result commit_global_switch(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    const music_rig_compiled_tables *target_tables,
    music_rig_protocol_response *response,
    uint64_t started_ns,
    uint64_t *commit_duration_ns
)
{
    const music_rig_generation *previous_generation =
        runtime->control_generation;
    music_rig_generation *commit_generation;
    music_rig_generation *rollback_generation;
    char previous_profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    const music_rig_compiled_tables *previous_base_tables;
    music_rig_persisted_device_override previous_overrides[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    uint32_t previous_override_count;
    music_rig_result result;
    uint64_t published_ns;
    size_t retired_count;

    increment(&runtime->metrics.commit_requests);
    if (response->result_code != (uint32_t)MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (response->readiness == (uint32_t)MUSIC_RIG_READINESS_COLD) {
        set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_STATE);
        return MUSIC_RIG_RESULT_OK;
    }
    if (strcmp(request->profile, runtime->active_rig_profile) == 0) {
        response->flags |= MUSIC_RIG_RESPONSE_VALID |
            MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
        mark_response_profiles_active(response);
        return MUSIC_RIG_RESULT_OK;
    }

    retired_count = music_rig_generation_slot_retired_count(
        &runtime->generations
    );
    if (runtime->state.generation_id > UINT64_MAX - UINT64_C(2)) {
        increment(&runtime->metrics.generation_conflicts);
        set_commit_failure(response, MUSIC_RIG_RESULT_GENERATION_CONFLICT);
        return MUSIC_RIG_RESULT_OK;
    }
    if (retired_count >
            MUSIC_RIG_RETIRED_GENERATION_CAPACITY - (size_t)2 ||
        available_commit_generations(runtime) < 2U) {
        increment(&runtime->metrics.generation_backpressure);
        set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_STATE);
        return MUSIC_RIG_RESULT_OK;
    }

    copy_profile(previous_profile, runtime->active_rig_profile);
    previous_base_tables = runtime->base_tables;
    previous_override_count = runtime->device_override_count;
    memcpy(previous_overrides, runtime->device_overrides,
        sizeof(previous_overrides));
    commit_generation = allocate_commit_generation(
        runtime,
        runtime->state.generation_id + UINT64_C(1),
        target_tables
    );
    if (commit_generation == NULL) {
        increment(&runtime->metrics.generation_backpressure);
        set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_STATE);
        return MUSIC_RIG_RESULT_OK;
    }

    result = music_rig_runtime_publish_generation(
        runtime,
        commit_generation,
        request->expected_generation
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        release_commit_generation(runtime, commit_generation);
        set_commit_failure(response, result);
        return MUSIC_RIG_RESULT_OK;
    }
    published_ns = runtime->interfaces.clock.now_ns(
        runtime->interfaces.clock.context
    );
    *commit_duration_ns = published_ns >= started_ns
        ? published_ns - started_ns
        : UINT64_C(0);

    if (runtime->state.output_mode == MUSIC_RIG_OUTPUT_ENABLED &&
        confirm_output_generation(runtime, commit_generation) !=
            MUSIC_RIG_RESULT_OK) {
        set_commit_failure(response, MUSIC_RIG_RESULT_ADAPTER_FAILURE);
        return MUSIC_RIG_RESULT_OK;
    }

    copy_profile(runtime->active_rig_profile, request->profile);
    runtime->base_tables = target_tables;
    runtime->device_override_count = 0U;
    result = music_rig_runtime_persist_state(runtime);
    if (result == MUSIC_RIG_RESULT_OK) {
        response->resulting_generation = runtime->state.generation_id;
        copy_profile(
            response->active_rig_profile,
            runtime->active_rig_profile
        );
        response->flags |= MUSIC_RIG_RESPONSE_VALID |
            MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
        mark_response_profiles_active(response);
        increment(&runtime->metrics.commit_successes);
        return MUSIC_RIG_RESULT_OK;
    }

    rollback_generation = allocate_commit_generation(
        runtime,
        runtime->state.generation_id + UINT64_C(1),
        (const music_rig_compiled_tables *)previous_generation->mapping
    );
    if (rollback_generation != NULL &&
        music_rig_runtime_publish_generation(
            runtime,
            rollback_generation,
            runtime->state.generation_id
        ) == MUSIC_RIG_RESULT_OK) {
        copy_profile(runtime->active_rig_profile, previous_profile);
        runtime->base_tables = previous_base_tables;
        runtime->device_override_count = previous_override_count;
        memcpy(runtime->device_overrides, previous_overrides,
            sizeof(previous_overrides));
        response->resulting_generation = runtime->state.generation_id;
        copy_profile(
            response->active_rig_profile,
            runtime->active_rig_profile
        );
        if (music_rig_runtime_persist_state(runtime) == MUSIC_RIG_RESULT_OK) {
            response->rollback_status =
                (uint32_t)MUSIC_RIG_ROLLBACK_SUCCEEDED;
            increment(&runtime->metrics.commit_rollbacks);
        } else {
            response->rollback_status =
                (uint32_t)MUSIC_RIG_ROLLBACK_FAILED;
            increment(&runtime->metrics.commit_rollback_failures);
        }
    } else {
        if (rollback_generation != NULL) {
            release_commit_generation(runtime, rollback_generation);
        }
        response->resulting_generation = runtime->state.generation_id;
        copy_profile(
            response->active_rig_profile,
            runtime->active_rig_profile
        );
        response->rollback_status = (uint32_t)MUSIC_RIG_ROLLBACK_FAILED;
        increment(&runtime->metrics.commit_rollback_failures);
    }
    set_commit_failure(response, MUSIC_RIG_RESULT_ADAPTER_FAILURE);
    return MUSIC_RIG_RESULT_OK;
}

static void restore_device_transaction(
    music_rig_runtime *runtime,
    const music_rig_persisted_device_override *overrides,
    uint32_t override_count,
    const music_rig_generation *previous_generation,
    music_rig_protocol_response *response
)
{
    music_rig_generation *rollback_generation;

    memcpy(runtime->device_overrides, overrides,
        sizeof(runtime->device_overrides));
    runtime->device_override_count = override_count;
    rollback_generation = allocate_commit_generation(
        runtime,
        runtime->state.generation_id + UINT64_C(1),
        (const music_rig_compiled_tables *)previous_generation->mapping
    );
    if (rollback_generation != NULL && music_rig_runtime_publish_generation(
            runtime, rollback_generation, runtime->state.generation_id
        ) == MUSIC_RIG_RESULT_OK && music_rig_runtime_persist_state(runtime) ==
            MUSIC_RIG_RESULT_OK) {
        response->rollback_status = (uint32_t)MUSIC_RIG_ROLLBACK_SUCCEEDED;
        increment(&runtime->metrics.commit_rollbacks);
    } else {
        if (rollback_generation != NULL) {
            release_commit_generation(runtime, rollback_generation);
        }
        response->rollback_status = (uint32_t)MUSIC_RIG_ROLLBACK_FAILED;
        increment(&runtime->metrics.commit_rollback_failures);
    }
    set_commit_failure(response, MUSIC_RIG_RESULT_ADAPTER_FAILURE);
}

static music_rig_result commit_device_switch(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    music_rig_commit_transaction transaction;
    music_rig_persisted_device_override previous_overrides[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    const music_rig_generation *previous_generation = runtime->control_generation;
    const music_rig_compiled_tables *source;
    music_rig_compiled_tables *composed;
    music_rig_generation *generation;
    int override_index;
    int table_index;
    uint32_t previous_count;
    music_rig_result result;

    increment(&runtime->metrics.commit_requests);
    if (response->result_code != (uint32_t)MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_OK;
    }
    {
        uint16_t active_index;

        if (music_rig_compiled_profile_index(
                runtime->control_generation->mapping,
                request->device_slot,
                &active_index
            ) == MUSIC_RIG_RESULT_OK && strcmp(
                ((const music_rig_compiled_tables *)
                    runtime->control_generation->mapping)->device_profiles[
                    active_index
                ].profile,
                request->profile
            ) == 0) {
            response->flags &= ~MUSIC_RIG_RESPONSE_DRY_RUN;
            response->flags |= MUSIC_RIG_RESPONSE_VALID |
                MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
            response->resulting_generation = runtime->state.generation_id;
            return MUSIC_RIG_RESULT_OK;
        }
    }
    source = device_profile_tables(
        runtime, request->device_slot, request->profile
    );
    if (source == NULL) {
        set_commit_failure(response, MUSIC_RIG_RESULT_NOT_FOUND);
        return MUSIC_RIG_RESULT_OK;
    }
    override_index = device_override_index(runtime, request->device_slot);
    if (override_index < 0) {
        if (runtime->device_override_count >=
                MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY) {
            set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_STATE);
            return MUSIC_RIG_RESULT_OK;
        }
        override_index = (int)runtime->device_override_count;
    }
    table_index = free_device_override_table(runtime);
    if (table_index < 0) {
        set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_STATE);
        return MUSIC_RIG_RESULT_OK;
    }
    runtime->device_override_table_in_use[table_index] = true;
    composed = &runtime->device_override_tables[table_index];
    result = music_rig_compiled_tables_compose_device(
        runtime->control_generation->mapping,
        source,
        request->device_slot,
        composed
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        runtime->device_override_table_in_use[table_index] = false;
        set_commit_failure(response, result);
        return MUSIC_RIG_RESULT_OK;
    }
    memcpy(previous_overrides, runtime->device_overrides,
        sizeof(previous_overrides));
    previous_count = runtime->device_override_count;
    transaction.previous_generation = previous_generation;
    transaction.previous_base_tables = runtime->base_tables;
    transaction.previous_override_count = previous_count;
    memcpy(transaction.previous_overrides, previous_overrides,
        sizeof(previous_overrides));
    generation = allocate_commit_generation(runtime,
        runtime->state.generation_id + UINT64_C(1), composed);
    if (generation == NULL || music_rig_runtime_publish_generation(
            runtime, generation, request->expected_generation
        ) != MUSIC_RIG_RESULT_OK) {
        if (generation != NULL) release_commit_generation(runtime, generation);
        runtime->device_override_table_in_use[table_index] = false;
        set_commit_failure(response, MUSIC_RIG_RESULT_GENERATION_CONFLICT);
        return MUSIC_RIG_RESULT_OK;
    }
    if ((uint32_t)override_index == runtime->device_override_count) {
        runtime->device_override_count += 1U;
    }
    copy_profile(runtime->device_overrides[override_index].device_slot,
        request->device_slot);
    copy_profile(runtime->device_overrides[override_index].profile,
        request->profile);
    if (runtime->state.output_mode == MUSIC_RIG_OUTPUT_ENABLED &&
        confirm_output_generation(runtime, generation) != MUSIC_RIG_RESULT_OK) {
        restore_device_transaction(runtime, transaction.previous_overrides,
            transaction.previous_override_count, transaction.previous_generation,
            response);
        return MUSIC_RIG_RESULT_OK;
    }
    if (music_rig_runtime_persist_state(runtime) != MUSIC_RIG_RESULT_OK) {
        restore_device_transaction(
            runtime, previous_overrides, previous_count,
            previous_generation, response
        );
        return MUSIC_RIG_RESULT_OK;
    }
    copy_profile(response->selected_device_slot, request->device_slot);
    copy_profile(response->selected_profile, request->profile);
    response->resulting_generation = runtime->state.generation_id;
    response->flags &= ~MUSIC_RIG_RESPONSE_DRY_RUN;
    response->flags |= MUSIC_RIG_RESPONSE_VALID |
        MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
    increment(&runtime->metrics.commit_successes);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result reset_device_override(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    music_rig_persisted_device_override previous_overrides[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
    const music_rig_generation *previous_generation =
        runtime->control_generation;
    music_rig_compiled_tables *composed;
    music_rig_generation *generation;
    int override_index = device_override_index(runtime, request->device_slot);
    int table_index;
    uint32_t previous_count;

    increment(&runtime->metrics.commit_requests);
    if (response->result_code != (uint32_t)MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (override_index < 0) {
        response->flags &= ~MUSIC_RIG_RESPONSE_DRY_RUN;
        response->flags |= MUSIC_RIG_RESPONSE_VALID |
            MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
        return MUSIC_RIG_RESULT_OK;
    }
    table_index = free_device_override_table(runtime);
    if (table_index < 0) {
        set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_STATE);
        return MUSIC_RIG_RESULT_OK;
    }
    runtime->device_override_table_in_use[table_index] = true;
    composed = &runtime->device_override_tables[table_index];
    if (music_rig_compiled_tables_compose_device(
            runtime->control_generation->mapping,
            runtime->base_tables,
            request->device_slot,
            composed
        ) != MUSIC_RIG_RESULT_OK) {
        runtime->device_override_table_in_use[table_index] = false;
        set_commit_failure(response, MUSIC_RIG_RESULT_INVALID_DATA);
        return MUSIC_RIG_RESULT_OK;
    }
    memcpy(previous_overrides, runtime->device_overrides,
        sizeof(previous_overrides));
    previous_count = runtime->device_override_count;
    generation = allocate_commit_generation(runtime,
        runtime->state.generation_id + UINT64_C(1), composed);
    if (generation == NULL || music_rig_runtime_publish_generation(
            runtime, generation, request->expected_generation
        ) != MUSIC_RIG_RESULT_OK) {
        if (generation != NULL) release_commit_generation(runtime, generation);
        runtime->device_override_table_in_use[table_index] = false;
        set_commit_failure(response, MUSIC_RIG_RESULT_GENERATION_CONFLICT);
        return MUSIC_RIG_RESULT_OK;
    }
    for (uint32_t index = (uint32_t)override_index;
         index + 1U < runtime->device_override_count; ++index) {
        runtime->device_overrides[index] = runtime->device_overrides[index + 1U];
    }
    runtime->device_override_count -= 1U;
    if (music_rig_runtime_persist_state(runtime) != MUSIC_RIG_RESULT_OK) {
        restore_device_transaction(
            runtime, previous_overrides, previous_count,
            previous_generation, response
        );
        return MUSIC_RIG_RESULT_OK;
    }
    response->resulting_generation = runtime->state.generation_id;
    response->flags &= ~MUSIC_RIG_RESPONSE_DRY_RUN;
    response->flags |= MUSIC_RIG_RESPONSE_VALID |
        MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
    increment(&runtime->metrics.commit_successes);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_runtime_dispatch(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    music_rig_control_snapshot snapshot;
    const music_rig_compiled_tables *target_tables = NULL;
    music_rig_result result;
    uint64_t started_ns;
    uint64_t finished_ns;
    uint64_t commit_duration_ns = UINT64_MAX;

    if (runtime == NULL || request == NULL || response == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (runtime->state.lifecycle != MUSIC_RIG_RUNTIME_INITIALIZED &&
        runtime->state.lifecycle != MUSIC_RIG_RUNTIME_RUNNING) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }

    started_ns = runtime->interfaces.clock.now_ns(
        runtime->interfaces.clock.context
    );
    snapshot.generation_id = runtime->state.generation_id;
    snapshot.active_rig_profile = runtime->active_rig_profile;
    snapshot.tables = runtime->control_generation->mapping;
    snapshot.output_mode = runtime->state.output_mode;
    if (request->operation ==
        (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL) {
        result = plan_global_switch(
            runtime,
            &snapshot,
            request,
            response,
            &target_tables
        );
        if (result == MUSIC_RIG_RESULT_OK &&
            request->flags != MUSIC_RIG_REQUEST_DRY_RUN) {
            result = commit_global_switch(
                runtime,
                request,
                target_tables,
                response,
                started_ns,
                &commit_duration_ns
            );
        }
    } else {
        if (request->flags == MUSIC_RIG_REQUEST_DRY_RUN ||
            (request->operation !=
                (uint32_t)MUSIC_RIG_OPERATION_SWITCH_DEVICE &&
             request->operation !=
                (uint32_t)MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE)) {
            result = music_rig_control_dispatch_prepared(
                &snapshot,
                runtime->prepared_definitions,
                runtime->prepared_definition_count,
                request,
                response
            );
        } else {
            music_rig_protocol_request plan_request = *request;
            plan_request.flags = MUSIC_RIG_REQUEST_DRY_RUN;
            result = music_rig_control_dispatch_prepared(
                &snapshot,
                runtime->prepared_definitions,
                runtime->prepared_definition_count,
                &plan_request,
                response
            );
            if (result == MUSIC_RIG_RESULT_OK &&
                request->operation ==
                    (uint32_t)MUSIC_RIG_OPERATION_SWITCH_DEVICE) {
                result = commit_device_switch(runtime, request, response);
            }
            if (result == MUSIC_RIG_RESULT_OK &&
                request->operation ==
                    (uint32_t)MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE) {
                result = reset_device_override(runtime, request, response);
            }
        }
    }
    finished_ns = runtime->interfaces.clock.now_ns(
        runtime->interfaces.clock.context
    );

    increment(&runtime->metrics.control_requests);
    if (result != MUSIC_RIG_RESULT_OK) {
        increment(&runtime->metrics.invalid_requests);
        return result;
    }
    response->control_duration_ns = commit_duration_ns != UINT64_MAX
        ? commit_duration_ns
        : (finished_ns >= started_ns
            ? finished_ns - started_ns
            : UINT64_C(0));
    classify_request(runtime, request, response);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_runtime_persist_state(music_rig_runtime *runtime)
{
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    music_rig_persisted_state persisted;
    music_rig_result result;

    if (runtime == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (runtime->state.lifecycle != MUSIC_RIG_RUNTIME_INITIALIZED &&
        runtime->state.lifecycle != MUSIC_RIG_RUNTIME_RUNNING) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }

    memset(&persisted, 0, sizeof(persisted));
    persisted.generation_id = runtime->state.generation_id;
    memcpy(
        persisted.definition_fingerprint,
        runtime->state.definition_fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    persisted.output_mode = runtime->state.output_mode;
    copy_profile(
        persisted.active_rig_profile,
        runtime->active_rig_profile
    );
    persisted.device_override_count = runtime->device_override_count;
    memcpy(persisted.device_overrides, runtime->device_overrides,
        sizeof(persisted.device_overrides));
    result = music_rig_state_encode(&persisted, frame, sizeof(frame));
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = runtime->interfaces.storage.atomic_replace(
        runtime->interfaces.storage.context,
        MUSIC_RIG_STORAGE_RUNTIME_STATE,
        frame,
        sizeof(frame)
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        increment(&runtime->metrics.adapter_failures);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    increment(&runtime->metrics.state_writes);
    return MUSIC_RIG_RESULT_OK;
}

const music_rig_runtime_state *music_rig_runtime_get_state(
    const music_rig_runtime *runtime
)
{
    return runtime == NULL ? NULL : &runtime->state;
}

const music_rig_runtime_metrics *music_rig_runtime_get_metrics(
    const music_rig_runtime *runtime
)
{
    return runtime == NULL ? NULL : &runtime->metrics;
}

const music_rig_device_port_catalogue *music_rig_runtime_get_device_ports(
    const music_rig_runtime *runtime
)
{
    return runtime == NULL ? NULL : &runtime->device_ports;
}
