#include "music_rig/runtime.h"

#include <stdint.h>
#include <string.h>

static void increment(uint64_t *counter)
{
    if (*counter != UINT64_MAX) {
        *counter += UINT64_C(1);
    }
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

static music_rig_result restore_state(music_rig_runtime *runtime)
{
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    size_t frame_size = 0;
    music_rig_persisted_state persisted;
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

    runtime->initial_generation.id = persisted.generation_id;
    runtime->state.generation_id = persisted.generation_id;
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

static void dispatch_request(
    music_rig_runtime *runtime,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    music_rig_result result = MUSIC_RIG_RESULT_OK;

    response->protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    response->request_id = request->request_id;
    response->previous_generation = runtime->state.generation_id;
    response->resulting_generation = runtime->state.generation_id;

    increment(&runtime->metrics.control_requests);
    if (request->protocol_version != MUSIC_RIG_PROTOCOL_VERSION ||
        request->operation != (uint32_t)MUSIC_RIG_OPERATION_STATUS ||
        request->request_id == UINT64_C(0)) {
        increment(&runtime->metrics.invalid_requests);
        result = MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    } else {
        increment(&runtime->metrics.status_requests);
        if (request->expected_generation != UINT64_C(0) &&
            request->expected_generation != runtime->state.generation_id) {
            increment(&runtime->metrics.generation_conflicts);
            result = MUSIC_RIG_RESULT_GENERATION_CONFLICT;
        }
    }
    response->result_code = (uint32_t)result;
}

music_rig_result music_rig_runtime_init(
    music_rig_runtime *runtime,
    const music_rig_runtime_config *config,
    const music_rig_platform_interfaces *interfaces
)
{
    music_rig_result result;

    if (runtime == NULL || config == NULL ||
        config->initial_generation == NULL ||
        config->initial_generation->id == UINT64_C(0) ||
        config->definition_fingerprint == NULL ||
        config->definition_fingerprint_size !=
            MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE ||
        !interfaces_are_valid(interfaces)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (config->output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED) {
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->interfaces = *interfaces;
    runtime->initial_generation = *config->initial_generation;
    runtime->state.schema_version = MUSIC_RIG_RUNTIME_STATE_VERSION;
    runtime->state.generation_id = config->initial_generation->id;
    memcpy(
        runtime->state.definition_fingerprint,
        config->definition_fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    runtime->state.output_mode = config->output_mode;
    result = restore_state(runtime);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = music_rig_generation_slot_init(
        &runtime->generations,
        &runtime->initial_generation
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    runtime->state.lifecycle = MUSIC_RIG_RUNTIME_INITIALIZED;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_runtime_publish_generation(
    music_rig_runtime *runtime,
    const music_rig_generation *next_generation,
    uint64_t expected_generation
)
{
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

    result = music_rig_generation_slot_publish(
        &runtime->generations,
        next_generation
    );
    if (result == MUSIC_RIG_RESULT_GENERATION_CONFLICT) {
        increment(&runtime->metrics.generation_conflicts);
        return result;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    runtime->state.generation_id = next_generation->id;
    increment(&runtime->metrics.generation_publications);
    return MUSIC_RIG_RESULT_OK;
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

            dispatch_request(runtime, &request, &response);
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

    persisted.generation_id = runtime->state.generation_id;
    memcpy(
        persisted.definition_fingerprint,
        runtime->state.definition_fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    persisted.output_mode = runtime->state.output_mode;
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
