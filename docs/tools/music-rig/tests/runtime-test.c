#include "music_rig/runtime.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <string.h>

#define MOCK_EVENT_CAPACITY 8U
#define MOCK_RESPONSE_CAPACITY 8U

static music_rig_compiled_tables default_tables;

typedef struct mock_adapter {
    music_rig_control_poll events[MOCK_EVENT_CAPACITY];
    music_rig_protocol_request requests[MOCK_EVENT_CAPACITY];
    music_rig_protocol_response responses[MOCK_RESPONSE_CAPACITY];
    uint8_t state_frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    size_t event_count;
    size_t event_index;
    size_t response_count;
    size_t state_size;
    uint64_t now_ns;
    unsigned int start_calls;
    unsigned int wait_calls;
    unsigned int respond_calls;
    unsigned int stop_calls;
    unsigned int state_read_calls;
    unsigned int state_replace_calls;
    unsigned int state_replace_failures;
    bool state_exists;
    music_rig_result start_result;
    music_rig_result wait_result;
    music_rig_result respond_result;
    music_rig_result stop_result;
    music_rig_result state_read_result;
    music_rig_result state_replace_result;
    unsigned int output_prepare_calls;
    unsigned int output_confirm_calls;
    music_rig_result output_prepare_result;
    music_rig_result output_confirm_result;
    unsigned int output_confirm_failures;
    unsigned int output_rollback_calls;
    music_rig_result output_rollback_result;
} mock_adapter;

static music_rig_result mock_output_prepare(
    void *opaque, const music_rig_generation *generation
)
{
    mock_adapter *adapter = opaque;
    (void)generation;
    adapter->output_prepare_calls += 1U;
    return adapter->output_prepare_result;
}

static music_rig_result mock_output_confirm(
    void *opaque, const music_rig_generation *generation
)
{
    mock_adapter *adapter = opaque;
    (void)generation;
    adapter->output_confirm_calls += 1U;
    if (adapter->output_confirm_failures != 0U) {
        adapter->output_confirm_failures -= 1U;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return adapter->output_confirm_result;
}

static music_rig_result mock_output_rollback(
    void *opaque, const music_rig_generation *generation
)
{
    mock_adapter *adapter = opaque;
    (void)generation;
    adapter->output_rollback_calls += 1U;
    return adapter->output_rollback_result;
}


static uint64_t mock_now_ns(void *opaque)
{
    mock_adapter *adapter = opaque;

    adapter->now_ns += UINT64_C(10);
    return adapter->now_ns;
}

static music_rig_result mock_start(void *opaque)
{
    mock_adapter *adapter = opaque;

    adapter->start_calls += 1U;
    return adapter->start_result;
}

static music_rig_control_poll mock_poll(
    void *opaque,
    music_rig_protocol_request *request
)
{
    mock_adapter *adapter = opaque;
    size_t index = adapter->event_index;

    if (index >= adapter->event_count) {
        return MUSIC_RIG_CONTROL_ERROR;
    }
    adapter->event_index += 1U;
    if (adapter->events[index] == MUSIC_RIG_CONTROL_REQUEST) {
        *request = adapter->requests[index];
    }
    return adapter->events[index];
}

static music_rig_result mock_wait(void *opaque)
{
    mock_adapter *adapter = opaque;

    adapter->wait_calls += 1U;
    return adapter->wait_result;
}

static music_rig_result mock_respond(
    void *opaque,
    const music_rig_protocol_response *response
)
{
    mock_adapter *adapter = opaque;

    adapter->respond_calls += 1U;
    if (adapter->respond_result != MUSIC_RIG_RESULT_OK) {
        return adapter->respond_result;
    }
    if (adapter->response_count >= MOCK_RESPONSE_CAPACITY) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    adapter->responses[adapter->response_count] = *response;
    adapter->response_count += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result mock_stop(void *opaque)
{
    mock_adapter *adapter = opaque;

    adapter->stop_calls += 1U;
    return adapter->stop_result;
}

static music_rig_result mock_storage_read(
    void *opaque,
    music_rig_storage_object object,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
)
{
    mock_adapter *adapter = opaque;

    adapter->state_read_calls += 1U;
    if (object != MUSIC_RIG_STORAGE_RUNTIME_STATE) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (adapter->state_read_result != MUSIC_RIG_RESULT_OK) {
        return adapter->state_read_result;
    }
    if (!adapter->state_exists) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (output_capacity < adapter->state_size) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, adapter->state_frame, adapter->state_size);
    *output_size = adapter->state_size;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result mock_storage_replace(
    void *opaque,
    music_rig_storage_object object,
    const uint8_t *input,
    size_t input_size
)
{
    mock_adapter *adapter = opaque;

    adapter->state_replace_calls += 1U;
    if (adapter->state_replace_failures != 0U) {
        adapter->state_replace_failures -= 1U;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (adapter->state_replace_result != MUSIC_RIG_RESULT_OK) {
        return adapter->state_replace_result;
    }
    if (object != MUSIC_RIG_STORAGE_RUNTIME_STATE ||
        input_size != sizeof(adapter->state_frame)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memcpy(adapter->state_frame, input, input_size);
    adapter->state_size = input_size;
    adapter->state_exists = true;
    return MUSIC_RIG_RESULT_OK;
}

static void init_mock(mock_adapter *adapter)
{
    memset(adapter, 0, sizeof(*adapter));
    adapter->start_result = MUSIC_RIG_RESULT_OK;
    adapter->wait_result = MUSIC_RIG_RESULT_OK;
    adapter->respond_result = MUSIC_RIG_RESULT_OK;
    adapter->stop_result = MUSIC_RIG_RESULT_OK;
    adapter->state_read_result = MUSIC_RIG_RESULT_OK;
    adapter->state_replace_result = MUSIC_RIG_RESULT_OK;
    adapter->output_prepare_result = MUSIC_RIG_RESULT_OK;
    adapter->output_confirm_result = MUSIC_RIG_RESULT_OK;
    adapter->output_rollback_result = MUSIC_RIG_RESULT_OK;
}

static music_rig_platform_interfaces interfaces_for(mock_adapter *adapter)
{
    music_rig_platform_interfaces interfaces;

    interfaces.abi_version = MUSIC_RIG_RUNTIME_ABI_VERSION;
    interfaces.clock.context = adapter;
    interfaces.clock.now_ns = mock_now_ns;
    interfaces.control.context = adapter;
    interfaces.control.start = mock_start;
    interfaces.control.poll = mock_poll;
    interfaces.control.wait = mock_wait;
    interfaces.control.respond = mock_respond;
    interfaces.control.stop = mock_stop;
    interfaces.storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    interfaces.storage.context = adapter;
    interfaces.storage.read = mock_storage_read;
    interfaces.storage.atomic_replace = mock_storage_replace;
    return interfaces;
}

static music_rig_runtime_config config_for(
    const music_rig_generation *generation,
    const uint8_t *fingerprint
)
{
    music_rig_runtime_config config;

    memset(&config, 0, sizeof(config));
    config.initial_generation = generation;
    config.definition_fingerprint = fingerprint;
    config.definition_fingerprint_size =
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE;
    config.active_rig_profile = "full-live-rack";
    config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    return config;
}

static music_rig_protocol_request request(
    uint64_t request_id,
    uint64_t expected_generation,
    uint32_t operation
)
{
    music_rig_protocol_request value;

    memset(&value, 0, sizeof(value));
    value.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    value.operation = operation;
    value.request_id = request_id;
    value.expected_generation = expected_generation;
    return value;
}

static int test_lifecycle_and_metrics(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    static music_rig_compiled_tables tables;
    static music_rig_compiled_tables next_tables;
    music_rig_generation initial;
    music_rig_generation next;
    music_rig_runtime runtime;
    mock_adapter adapter;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;
    const music_rig_runtime_state *state;
    const music_rig_runtime_metrics *metrics;

    if (init_compiled_tables_fixture(&tables) != MUSIC_RIG_RESULT_OK ||
        init_compiled_tables_fixture(&next_tables) != MUSIC_RIG_RESULT_OK) {
        fputs("runtime table fixture failed\n", stderr);
        return 1;
    }
    fixture_copy(next_tables.device_profiles[0].profile, "organ");
    fixture_copy(next_tables.ownership[0].owners[0].profile, "organ");
    if (music_rig_compiled_tables_prepare(
            &next_tables,
            UINT32_C(2),
            UINT32_C(2),
            UINT32_C(2),
            UINT32_C(2)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("next runtime table fixture failed\n", stderr);
        return 1;
    }
    initial.id = UINT64_C(7);
    initial.mapping = &tables;
    next.id = UINT64_C(8);
    next.mapping = &next_tables;
    init_mock(&adapter);
    adapter.event_count = 5U;
    adapter.events[0] = MUSIC_RIG_CONTROL_IDLE;
    adapter.events[1] = MUSIC_RIG_CONTROL_REQUEST;
    adapter.requests[1] = request(
        UINT64_C(11),
        UINT64_C(8),
        (uint32_t)MUSIC_RIG_OPERATION_STATUS
    );
    adapter.events[2] = MUSIC_RIG_CONTROL_REQUEST;
    adapter.requests[2] = request(
        UINT64_C(12),
        UINT64_C(7),
        (uint32_t)MUSIC_RIG_OPERATION_STATUS
    );
    adapter.events[3] = MUSIC_RIG_CONTROL_REQUEST;
    adapter.requests[3] = request(
        UINT64_C(13),
        UINT64_C(0),
        (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL
    );
    fixture_copy(adapter.requests[3].profile, "full-live-rack");
    adapter.events[4] = MUSIC_RIG_CONTROL_STOP;
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);

    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_get_state(&runtime)->lifecycle !=
            MUSIC_RIG_RUNTIME_INITIALIZED ||
        music_rig_runtime_get_state(&runtime)->generation_id != UINT64_C(7) ||
        music_rig_runtime_get_state(&runtime)->output_mode !=
            MUSIC_RIG_OUTPUT_SUPPRESSED ||
        memcmp(
            music_rig_runtime_get_state(&runtime)->definition_fingerprint,
            fingerprint,
            sizeof(fingerprint)
        ) != 0) {
        fputs("runtime initialization failed\n", stderr);
        return 1;
    }
    if (music_rig_runtime_publish_generation(
            &runtime,
            &next,
            UINT64_C(6)
        ) != MUSIC_RIG_RESULT_GENERATION_CONFLICT ||
        music_rig_runtime_publish_generation(
            &runtime,
            &next,
            UINT64_C(7)
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_publish_generation(
            &runtime,
            &next,
            UINT64_C(8)
        ) != MUSIC_RIG_RESULT_GENERATION_CONFLICT ||
        music_rig_runtime_persist_state(&runtime) != MUSIC_RIG_RESULT_OK) {
        fputs("runtime generation publication failed\n", stderr);
        return 1;
    }
    if (music_rig_runtime_run(&runtime) != MUSIC_RIG_RESULT_OK) {
        fputs("runtime loop failed\n", stderr);
        return 1;
    }

    state = music_rig_runtime_get_state(&runtime);
    metrics = music_rig_runtime_get_metrics(&runtime);
    if (state->lifecycle != MUSIC_RIG_RUNTIME_STOPPED ||
        state->generation_id != UINT64_C(8) ||
        state->started_at_ns != UINT64_C(10) ||
        state->stopped_at_ns != UINT64_C(80) ||
        adapter.start_calls != 1U || adapter.wait_calls != 1U ||
        adapter.respond_calls != 3U || adapter.stop_calls != 1U ||
        adapter.response_count != 3U || adapter.state_read_calls != 1U ||
        adapter.state_replace_calls != 1U) {
        fputs("runtime lifecycle state is incorrect\n", stderr);
        return 1;
    }
    if (adapter.responses[0].result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK ||
        adapter.responses[0].request_id != UINT64_C(11) ||
        adapter.responses[0].previous_generation != UINT64_C(8) ||
        adapter.responses[0].resulting_generation != UINT64_C(8) ||
        adapter.responses[1].result_code !=
            (uint32_t)MUSIC_RIG_RESULT_GENERATION_CONFLICT ||
        adapter.responses[2].result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK ||
        adapter.responses[0].profile_count != UINT32_C(2) ||
        strcmp(adapter.responses[0].profiles[0].profile, "organ") != 0 ||
        adapter.responses[0].control_duration_ns != UINT64_C(10)) {
        fputs("runtime responses are incorrect\n", stderr);
        return 1;
    }
    if (metrics->loop_iterations != UINT64_C(5) ||
        metrics->idle_polls != UINT64_C(1) ||
        metrics->control_waits != UINT64_C(1) ||
        metrics->control_requests != UINT64_C(3) ||
        metrics->status_requests != UINT64_C(2) ||
        metrics->list_requests != UINT64_C(0) ||
        metrics->validate_requests != UINT64_C(0) ||
        metrics->dry_run_requests != UINT64_C(0) ||
        metrics->unsupported_requests != UINT64_C(0) ||
        metrics->invalid_requests != UINT64_C(0) ||
        metrics->control_responses != UINT64_C(3) ||
        metrics->generation_publications != UINT64_C(1) ||
        metrics->generation_conflicts != UINT64_C(3) ||
        metrics->commit_requests != UINT64_C(1) ||
        metrics->commit_successes != UINT64_C(0) ||
        metrics->commit_rollbacks != UINT64_C(0) ||
        metrics->commit_rollback_failures != UINT64_C(0) ||
        metrics->generation_reclamations != UINT64_C(0) ||
        metrics->generation_backpressure != UINT64_C(0) ||
        metrics->port_identity_conflicts != UINT64_C(0) ||
        metrics->state_restores != UINT64_C(0) ||
        metrics->state_fallbacks != UINT64_C(0) ||
        metrics->state_writes != UINT64_C(1) ||
        metrics->adapter_failures != UINT64_C(0)) {
        fputs("runtime metrics are incorrect\n", stderr);
        return 1;
    }
    if (music_rig_runtime_run(&runtime) != MUSIC_RIG_RESULT_INVALID_STATE ||
        music_rig_runtime_persist_state(&runtime) !=
            MUSIC_RIG_RESULT_INVALID_STATE ||
        music_rig_runtime_publish_generation(
            &runtime,
            &next,
            UINT64_C(8)
        ) != MUSIC_RIG_RESULT_INVALID_STATE) {
        fputs("stopped runtime accepted work\n", stderr);
        return 1;
    }
    return 0;
}

static int test_prepared_definition_catalogue(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x33
    };
    static music_rig_compiled_tables alternate_tables;
    const music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_compiled_definition alternate_definition;
    music_rig_prepared_definition prepared;
    music_rig_persisted_state persisted;
    music_rig_protocol_request value;
    music_rig_protocol_response response;
    music_rig_runtime runtime;
    mock_adapter adapter;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;

    if (init_alternate_prepared_definition_fixture(
            &alternate_tables,
            &alternate_definition,
            &prepared
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("prepared runtime fixture failed\n", stderr);
        return 1;
    }
    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_OK) {
        fputs("prepared runtime initialization failed\n", stderr);
        return 1;
    }

    value = request(
        UINT64_C(31),
        UINT64_C(1),
        (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL
    );
    value.flags = MUSIC_RIG_REQUEST_DRY_RUN;
    fixture_copy(value.profile, "multilevel-volume-mixed-pads");
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK ||
        response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
        response.previous_generation != UINT64_C(1) ||
        response.resulting_generation != UINT64_C(1) ||
        response.profile_count != UINT32_C(2) ||
        strcmp(
            response.selected_profile,
            "multilevel-volume-mixed-pads"
        ) != 0 ||
        runtime.state.generation_id != UINT64_C(1)) {
        fputs("prepared runtime dry-run failed\n", stderr);
        return 1;
    }

    value.flags = UINT32_C(0);
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK ||
        response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
        response.previous_generation != UINT64_C(1) ||
        response.resulting_generation != UINT64_C(2) ||
        response.control_duration_ns != UINT64_C(10) ||
        response.rollback_status !=
            (uint32_t)MUSIC_RIG_ROLLBACK_NOT_REQUIRED ||
        strcmp(
            response.active_rig_profile,
            "multilevel-volume-mixed-pads"
        ) != 0 ||
        strcmp(
            runtime.active_rig_profile,
            "multilevel-volume-mixed-pads"
        ) != 0 ||
        runtime.control_generation->mapping != &alternate_tables ||
        runtime.metrics.commit_requests != UINT64_C(1) ||
        runtime.metrics.commit_successes != UINT64_C(1) ||
        runtime.metrics.state_writes != UINT64_C(1)) {
        fputs("prepared global commit failed\n", stderr);
        return 1;
    }

    value = request(
        UINT64_C(32),
        UINT64_C(2),
        (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL
    );
    fixture_copy(value.profile, "full-live-rack");
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK ||
        response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
        response.previous_generation != UINT64_C(2) ||
        response.resulting_generation != UINT64_C(3) ||
        strcmp(response.active_rig_profile, "full-live-rack") != 0 ||
        runtime.control_generation->mapping != &default_tables ||
        runtime.metrics.commit_requests != UINT64_C(2) ||
        runtime.metrics.commit_successes != UINT64_C(2) ||
        runtime.metrics.state_writes != UINT64_C(2)) {
        fputs("base Rig Profile commit failed\n", stderr);
        return 1;
    }

    memset(&persisted, 0, sizeof(persisted));
    persisted.generation_id = UINT64_C(10);
    memcpy(
        persisted.definition_fingerprint,
        fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    persisted.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    fixture_copy(
        persisted.active_rig_profile,
        "multilevel-volume-mixed-pads"
    );
    if (music_rig_state_encode(
            &persisted,
            adapter.state_frame,
            sizeof(adapter.state_frame)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("prepared persisted state fixture failed\n", stderr);
        return 1;
    }
    adapter.state_size = sizeof(adapter.state_frame);
    adapter.state_exists = true;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        runtime.state.generation_id != UINT64_C(10) ||
        strcmp(
            runtime.active_rig_profile,
            "multilevel-volume-mixed-pads"
        ) != 0 ||
        runtime.control_generation->mapping != &alternate_tables ||
        runtime.metrics.state_restores != UINT64_C(1)) {
        fputs("prepared Rig Profile state was not restored\n", stderr);
        return 1;
    }


    value = request(
        UINT64_C(33),
        UINT64_C(10),
        (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL
    );
    value.flags = MUSIC_RIG_REQUEST_DRY_RUN;
    fixture_copy(value.profile, "full-live-rack");
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK ||
        response.result_code != (uint32_t)MUSIC_RIG_RESULT_OK ||
        response.previous_generation != UINT64_C(10) ||
        response.resulting_generation != UINT64_C(10) ||
        strcmp(
            response.active_rig_profile,
            "multilevel-volume-mixed-pads"
        ) != 0 ||
        (response.profiles[0].flags & MUSIC_RIG_PROFILE_ACTIVE) !=
            UINT32_C(0)) {
        fputs("base Rig Profile dry-run after restore failed\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    adapter.state_replace_failures = 1U;
    interfaces = interfaces_for(&adapter);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    value = request(
        UINT64_C(34),
        UINT64_C(1),
        (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL
    );
    fixture_copy(value.profile, "multilevel-volume-mixed-pads");
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK ||
        response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        response.previous_generation != UINT64_C(1) ||
        response.resulting_generation != UINT64_C(3) ||
        response.rollback_status !=
            (uint32_t)MUSIC_RIG_ROLLBACK_SUCCEEDED ||
        strcmp(response.active_rig_profile, "full-live-rack") != 0 ||
        runtime.control_generation->mapping != &default_tables ||
        runtime.metrics.generation_publications != UINT64_C(2) ||
        runtime.metrics.commit_rollbacks != UINT64_C(1) ||
        runtime.metrics.commit_rollback_failures != UINT64_C(0) ||
        runtime.metrics.state_writes != UINT64_C(1) ||
        runtime.metrics.adapter_failures != UINT64_C(1) ||
        adapter.state_replace_calls != 2U) {
        fputs("global commit persistence rollback failed\n", stderr);
        return 1;
    }
    if (music_rig_state_decode(
            adapter.state_frame,
            adapter.state_size,
            &persisted
        ) != MUSIC_RIG_RESULT_OK ||
        persisted.generation_id != UINT64_C(3) ||
        strcmp(persisted.active_rig_profile, "full-live-rack") != 0) {
        fputs("rolled-back global state was not durable\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    adapter.state_replace_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    interfaces = interfaces_for(&adapter);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    value.request_id = UINT64_C(35);
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK ||
        response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        response.rollback_status !=
            (uint32_t)MUSIC_RIG_ROLLBACK_FAILED ||
        response.resulting_generation != UINT64_C(3) ||
        strcmp(runtime.active_rig_profile, "full-live-rack") != 0 ||
        runtime.control_generation->mapping != &default_tables ||
        runtime.metrics.commit_rollback_failures != UINT64_C(1) ||
        runtime.metrics.adapter_failures != UINT64_C(2)) {
        fputs("global commit rollback failure was hidden\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    persisted.generation_id = UINT64_C(9);
    fixture_copy(persisted.active_rig_profile, "missing-profile");
    if (music_rig_state_encode(
            &persisted,
            adapter.state_frame,
            sizeof(adapter.state_frame)
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    adapter.state_size = sizeof(adapter.state_frame);
    adapter.state_exists = true;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        runtime.state.generation_id != UINT64_C(1) ||
        strcmp(runtime.active_rig_profile, "full-live-rack") != 0 ||
        runtime.metrics.state_fallbacks != UINT64_C(1)) {
        fputs("missing persisted Rig Profile did not fall back\n", stderr);
        return 1;
    }

    config.prepared_definition_count =
        MUSIC_RIG_PREPARED_DEFINITION_CAPACITY + 1U;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("oversized prepared runtime catalogue was accepted\n", stderr);
        return 1;
    }
    return 0;
}

static int test_generation_reclamation_and_ports(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x50
    };
    static music_rig_compiled_tables changed_slots;
    music_rig_generation generations[
        MUSIC_RIG_RETIRED_GENERATION_CAPACITY + 2U
    ];
    music_rig_runtime runtime;
    mock_adapter adapter;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;
    const music_rig_device_port_catalogue *ports;
    const music_rig_generation *reclaimed;
    size_t index;

    for (index = 0U;
         index < MUSIC_RIG_RETIRED_GENERATION_CAPACITY + 2U;
         ++index) {
        generations[index].id = (uint64_t)index + UINT64_C(1);
        generations[index].mapping = &default_tables;
    }
    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    generations[3].mapping = &default_tables;
    config = config_for(&generations[0], fingerprint);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_OK) {
        fputs("reclamation runtime initialization failed\n", stderr);
        return 1;
    }
    ports = music_rig_runtime_get_device_ports(&runtime);
    if (ports == NULL || ports->count != 4U ||
        strcmp(ports->ports[0].id,
            "device.arturia-main.midi-input") != 0 ||
        music_rig_runtime_get_device_ports(NULL) != NULL) {
        fputs("runtime stable port catalogue is incorrect\n", stderr);
        return 1;
    }

    if (music_rig_runtime_publish_generation(
            &runtime,
            &generations[1],
            UINT64_C(1)
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_reclaim_generation(&runtime) != NULL ||
        music_rig_generation_slot_adopt(&runtime.generations) !=
            &generations[1] ||
        music_rig_runtime_reclaim_generation(&runtime) != NULL ||
        runtime.metrics.generation_reclamations != UINT64_C(1)) {
        fputs("runtime initial generation reclamation failed\n", stderr);
        return 1;
    }
    if (music_rig_runtime_publish_generation(
            &runtime,
            &generations[2],
            UINT64_C(2)
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_generation_slot_adopt(&runtime.generations) !=
            &generations[2]) {
        fputs("runtime external generation publication failed\n", stderr);
        return 1;
    }
    reclaimed = music_rig_runtime_reclaim_generation(&runtime);
    if (reclaimed != &generations[1] ||
        runtime.metrics.generation_reclamations != UINT64_C(2)) {
        fputs("runtime external generation was not reclaimable\n", stderr);
        return 1;
    }

    changed_slots = default_tables;
    fixture_copy(changed_slots.device_profiles[0].slot, "arturia-secondary");
    fixture_copy(changed_slots.input_bindings[0].slot, "arturia-secondary");
    fixture_copy(changed_slots.ownership[0].owners[0].slot,
        "arturia-secondary");
    if (music_rig_compiled_tables_prepare(
            &changed_slots,
            UINT32_C(2), UINT32_C(2), UINT32_C(2), UINT32_C(2)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("changed-slot runtime fixture failed\n", stderr);
        return 1;
    }
    generations[3].id = UINT64_C(4);
    generations[3].mapping = &changed_slots;
    if (music_rig_runtime_publish_generation(
            &runtime,
            &generations[3],
            UINT64_C(3)
        ) != MUSIC_RIG_RESULT_INVALID_DATA ||
        runtime.state.generation_id != UINT64_C(3) ||
        runtime.metrics.port_identity_conflicts != UINT64_C(1)) {
        fputs("runtime accepted changed device-slot ports\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    generations[3].mapping = &default_tables;
    config = config_for(&generations[0], fingerprint);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_OK) {
        fputs("backpressure runtime initialization failed\n", stderr);
        return 1;
    }
    for (index = 1U;
         index <= MUSIC_RIG_RETIRED_GENERATION_CAPACITY;
         ++index) {
        if (music_rig_runtime_publish_generation(
                &runtime,
                &generations[index],
                (uint64_t)index
            ) != MUSIC_RIG_RESULT_OK) {
            fputs("runtime retirement ring filled too early\n", stderr);
            return 1;
        }
    }
    if (music_rig_runtime_publish_generation(
            &runtime,
            &generations[MUSIC_RIG_RETIRED_GENERATION_CAPACITY + 1U],
            (uint64_t)MUSIC_RIG_RETIRED_GENERATION_CAPACITY + UINT64_C(1)
        ) != MUSIC_RIG_RESULT_INVALID_STATE ||
        runtime.state.generation_id !=
            (uint64_t)MUSIC_RIG_RETIRED_GENERATION_CAPACITY + UINT64_C(1) ||
        runtime.metrics.generation_backpressure != UINT64_C(1) ||
        music_rig_runtime_reclaim_generation(NULL) != NULL) {
        fputs("runtime generation backpressure failed\n", stderr);
        return 1;
    }
    return 0;
}

static int test_persisted_state(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x30, 0x31, 0x32, 0x33
    };
    static const uint8_t changed_fingerprint[
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    ] = {0x40};
    const music_rig_generation initial = {UINT64_C(7), &default_tables};
    const music_rig_generation next = {UINT64_C(8), &default_tables};
    music_rig_runtime runtime;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;
    music_rig_persisted_state older;
    mock_adapter adapter;

    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_publish_generation(
            &runtime,
            &next,
            UINT64_C(7)
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_persist_state(&runtime) != MUSIC_RIG_RESULT_OK) {
        fputs("runtime state setup failed\n", stderr);
        return 1;
    }

    interfaces = interfaces_for(&adapter);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        runtime.state.generation_id != UINT64_C(8) ||
        runtime.metrics.state_restores != UINT64_C(1)) {
        fputs("qualified runtime state was not restored\n", stderr);
        return 1;
    }

    config = config_for(&initial, changed_fingerprint);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        runtime.state.generation_id != UINT64_C(7) ||
        runtime.metrics.state_fallbacks != UINT64_C(1)) {
        fputs("changed definition did not fall back safely\n", stderr);
        return 1;
    }

    adapter.state_frame[20] ^= UINT8_C(1);
    config = config_for(&initial, fingerprint);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("corrupt runtime state was accepted\n", stderr);
        return 1;
    }

    memset(&older, 0, sizeof(older));
    older.generation_id = UINT64_C(6);
    memcpy(
        older.definition_fingerprint,
        fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    older.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    fixture_copy(older.active_rig_profile, "full-live-rack");
    if (music_rig_state_encode(
            &older,
            adapter.state_frame,
            sizeof(adapter.state_frame)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("older runtime state fixture failed\n", stderr);
        return 1;
    }
    adapter.state_size = sizeof(adapter.state_frame);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        runtime.state.generation_id != UINT64_C(7) ||
        runtime.metrics.state_fallbacks != UINT64_C(1)) {
        fputs("older runtime state did not fall back safely\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    adapter.state_read_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    interfaces = interfaces_for(&adapter);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        runtime.metrics.adapter_failures != UINT64_C(1)) {
        fputs("runtime state read failure was hidden\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    adapter.state_replace_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    interfaces = interfaces_for(&adapter);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_persist_state(&runtime) !=
            MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        runtime.metrics.adapter_failures != UINT64_C(1)) {
        fputs("runtime state replace failure was hidden\n", stderr);
        return 1;
    }
    return 0;
}

static int run_failure_case(
    const char *label,
    mock_adapter *adapter,
    uint64_t expected_requests,
    unsigned int expected_stop_calls
)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x10
    };
    const music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_runtime runtime;
    music_rig_platform_interfaces interfaces = interfaces_for(adapter);
    music_rig_runtime_config config = config_for(&initial, fingerprint);

    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_runtime_run(&runtime) != MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        runtime.state.lifecycle != MUSIC_RIG_RUNTIME_FAILED ||
        runtime.metrics.control_requests != expected_requests ||
        runtime.metrics.adapter_failures != UINT64_C(1) ||
        adapter->stop_calls != expected_stop_calls) {
        fprintf(stderr, "%s failure path is incorrect\n", label);
        return 1;
    }
    return 0;
}

static int test_adapter_failures(void)
{
    mock_adapter adapter;

    init_mock(&adapter);
    adapter.start_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    if (run_failure_case("start", &adapter, UINT64_C(0), 0U) != 0) {
        return 1;
    }

    init_mock(&adapter);
    adapter.event_count = 1U;
    adapter.events[0] = MUSIC_RIG_CONTROL_IDLE;
    adapter.wait_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    if (run_failure_case("wait", &adapter, UINT64_C(0), 1U) != 0) {
        return 1;
    }

    init_mock(&adapter);
    adapter.event_count = 1U;
    adapter.events[0] = MUSIC_RIG_CONTROL_REQUEST;
    adapter.requests[0] = request(
        UINT64_C(20),
        UINT64_C(1),
        (uint32_t)MUSIC_RIG_OPERATION_STATUS
    );
    adapter.respond_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    if (run_failure_case("respond", &adapter, UINT64_C(1), 1U) != 0) {
        return 1;
    }

    init_mock(&adapter);
    adapter.event_count = 1U;
    adapter.events[0] = MUSIC_RIG_CONTROL_ERROR;
    if (run_failure_case("poll", &adapter, UINT64_C(0), 1U) != 0) {
        return 1;
    }

    init_mock(&adapter);
    adapter.event_count = 1U;
    adapter.events[0] = MUSIC_RIG_CONTROL_STOP;
    adapter.stop_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    if (run_failure_case("stop", &adapter, UINT64_C(0), 1U) != 0) {
        return 1;
    }
    return 0;
}

static int test_invalid_configuration(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x20
    };
    const music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_runtime runtime;
    mock_adapter adapter;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;

    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("enabled output mode without adapter was accepted\n", stderr);
        return 1;
    }

    config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    interfaces.abi_version += UINT32_C(1);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_runtime_init(NULL, &config, &interfaces) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_runtime_get_state(NULL) != NULL ||
        music_rig_runtime_get_metrics(NULL) != NULL) {
        fputs("invalid runtime configuration was accepted\n", stderr);
        return 1;
    }

    interfaces = interfaces_for(&adapter);
    interfaces.storage.abi_version += UINT32_C(1);
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid storage adapter ABI was accepted\n", stderr);
        return 1;
    }

    interfaces = interfaces_for(&adapter);
    {
        const music_rig_generation missing_tables = {UINT64_C(1), NULL};

        config = config_for(&missing_tables, fingerprint);
        if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
            fputs("runtime accepted a generation without tables\n", stderr);
            return 1;
        }
    }
    return 0;
}

static int test_enabled_output_initialization(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x40
    };
    const music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_runtime runtime;
    mock_adapter adapter;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;
    music_rig_output_adoption_adapter output;

    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    memset(&output, 0, sizeof(output));
    output.abi_version = MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION;
    output.context = &adapter;
    output.prepare = mock_output_prepare;
    output.confirm = mock_output_confirm;
    output.rollback = mock_output_rollback;
    output.rollback = mock_output_rollback;
    config = config_for(&initial, fingerprint);
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.output_adoption = &output;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK ||
        adapter.output_prepare_calls != 1U ||
        adapter.output_confirm_calls != 1U ||
        runtime.state.output_mode != MUSIC_RIG_OUTPUT_ENABLED) {
        fputs("enabled output initialization failed\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    adapter.output_prepare_result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.output_adoption = &output;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        adapter.output_prepare_calls != 1U ||
        adapter.output_confirm_calls != 0U) {
        fputs("failed output preparation was not rejected\n", stderr);
        return 1;
    }
    return 0;
}

static int test_output_confirmation_failure_is_fail_closed(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x52
    };
    const music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_runtime runtime;
    mock_adapter adapter;
    music_rig_platform_interfaces interfaces;
    music_rig_runtime_config config;
    music_rig_output_adoption_adapter output;

    init_mock(&adapter);
    adapter.output_confirm_failures = 1U;
    interfaces = interfaces_for(&adapter);
    memset(&output, 0, sizeof(output));
    output.abi_version = MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION;
    output.context = &adapter;
    output.prepare = mock_output_prepare;
    output.confirm = mock_output_confirm;
    output.rollback = mock_output_rollback;
    config = config_for(&initial, fingerprint);
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.output_adoption = &output;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
        MUSIC_RIG_RESULT_ADAPTER_FAILURE || adapter.output_confirm_calls != 1U ||
        adapter.output_rollback_calls != 0U) {
        fputs("output confirmation failure was accepted\n", stderr);
        return 1;
    }
    return 0;
}

static int test_device_override_transactions(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x31
    };
    static music_rig_compiled_tables alternate_tables;
    static music_rig_compiled_definition alternate_definition;
    static music_rig_prepared_definition prepared;
    music_rig_compiled_tables initial_tables;
    music_rig_generation initial;
    music_rig_runtime runtime;
    music_rig_runtime_config config;
    music_rig_platform_interfaces interfaces;
    music_rig_protocol_request switch_request;
    music_rig_protocol_request reset_request;
    music_rig_protocol_response response;
    mock_adapter adapter;

    if (init_compiled_tables_fixture(&initial_tables) != MUSIC_RIG_RESULT_OK ||
        init_alternate_prepared_definition_fixture(
            &alternate_tables, &alternate_definition, &prepared
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    initial.id = UINT64_C(1);
    initial.mapping = &initial_tables;
    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    switch_request = request(UINT64_C(21), UINT64_C(1),
        MUSIC_RIG_OPERATION_SWITCH_DEVICE);
    fixture_copy(switch_request.device_slot, "smc-mixer-main");
    fixture_copy(switch_request.profile, "multilevel-volume");
    if (music_rig_runtime_dispatch(&runtime, &switch_request, &response) !=
            MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK || response.resulting_generation !=
            UINT64_C(2) || runtime.device_override_count != 1U ||
        strcmp(runtime.device_overrides[0].profile, "multilevel-volume") != 0) {
        fputs("device override commit failed\n", stderr);
        return 1;
    }
    switch_request.request_id = UINT64_C(22);
    switch_request.expected_generation = UINT64_C(2);
    if (music_rig_runtime_dispatch(&runtime, &switch_request, &response) !=
            MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK || response.resulting_generation !=
            UINT64_C(2) || runtime.device_override_count != 1U) {
        fputs("idempotent device switch changed state\n", stderr);
        return 1;
    }
    {
        music_rig_runtime restored;
        if (music_rig_runtime_init(&restored, &config, &interfaces) !=
                MUSIC_RIG_RESULT_OK || restored.device_override_count != 1U ||
            strcmp(restored.device_overrides[0].device_slot,
                "smc-mixer-main") != 0 ||
            strcmp(((const music_rig_compiled_tables *)
                restored.initial_generation.mapping)->device_profiles[1].profile,
                "multilevel-volume") != 0) {
            fputs("device override state restore failed\n", stderr);
            return 1;
        }
    }
    switch_request.request_id = UINT64_C(23);
    switch_request.expected_generation = UINT64_C(1);
    if (music_rig_runtime_dispatch(&runtime, &switch_request, &response) !=
            MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_GENERATION_CONFLICT ||
        runtime.device_override_count != 1U) {
        fputs("device override generation guard failed\n", stderr);
        return 1;
    }
    reset_request = request(UINT64_C(24), UINT64_C(2),
        MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE);
    fixture_copy(reset_request.device_slot, "smc-mixer-main");
    if (music_rig_runtime_dispatch(&runtime, &reset_request, &response) !=
            MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK || response.resulting_generation !=
            UINT64_C(3) || runtime.device_override_count != 0U ||
        strcmp(((const music_rig_compiled_tables *)
            runtime.control_generation->mapping)->device_profiles[1].profile,
            "eight-band-eq") != 0) {
        fputs("device override reset failed\n", stderr);
        return 1;
    }

    init_mock(&adapter);
    adapter.state_replace_failures = 1U;
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK || music_rig_runtime_dispatch(
                &runtime, &switch_request, &response
            ) != MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        response.rollback_status != (uint32_t)MUSIC_RIG_ROLLBACK_SUCCEEDED ||
        runtime.device_override_count != 0U) {
        fputs("device override persistence rollback failed\n", stderr);
        return 1;
    }
    if (runtime.state.generation_id != UINT64_C(3) ||
        runtime.control_generation->mapping == NULL ||
        adapter.state_size != sizeof(adapter.state_frame) ||
        !adapter.state_exists) {
        fputs("device rollback state was not retained\n", stderr);
        return 1;
    }
    init_mock(&adapter);
    adapter.state_replace_failures = 3U;
    interfaces = interfaces_for(&adapter);
    config = config_for(&initial, fingerprint);
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK || music_rig_runtime_dispatch(
                &runtime, &switch_request, &response
            ) != MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        response.rollback_status != (uint32_t)MUSIC_RIG_ROLLBACK_FAILED ||
        runtime.metrics.commit_rollback_failures == 0U) {
        fputs("device rollback failure was not reported\n", stderr);
        return 1;
    }
    return 0;
}

static int test_output_enabled_device_switch(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x53
    };
    static music_rig_compiled_tables alternate_tables;
    static music_rig_compiled_definition alternate_definition;
    static music_rig_prepared_definition prepared;
    music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_runtime runtime;
    music_rig_runtime_config config;
    music_rig_platform_interfaces interfaces;
    music_rig_output_adoption_adapter output;
    music_rig_protocol_request value;
    music_rig_protocol_response response;
    mock_adapter adapter;

    if (init_alternate_prepared_definition_fixture(
            &alternate_tables, &alternate_definition, &prepared
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    memset(&output, 0, sizeof(output));
    output.abi_version = MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION;
    output.context = &adapter;
    output.prepare = mock_output_prepare;
    output.confirm = mock_output_confirm;
    output.rollback = mock_output_rollback;
    config = config_for(&initial, fingerprint);
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.output_adoption = &output;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    value = request(UINT64_C(41), UINT64_C(1),
        MUSIC_RIG_OPERATION_SWITCH_DEVICE);
    fixture_copy(value.device_slot, "smc-mixer-main");
    fixture_copy(value.profile, "multilevel-volume");
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK || adapter.output_prepare_calls != 2U ||
        adapter.output_confirm_calls != 2U || runtime.device_override_count != 1U) {
        fputs("output-enabled device switch failed\n", stderr);
        return 1;
    }
    if (runtime.state.generation_id != UINT64_C(2) ||
        response.resulting_generation != UINT64_C(2) ||
        adapter.output_prepare_calls != 2U ||
        adapter.output_confirm_calls != 2U) {
        fputs("output-enabled device transaction accounting failed\n", stderr);
        return 1;
    }
    return 0;
}

static int test_output_enabled_global_switch(void)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x54
    };
    static music_rig_compiled_tables alternate_tables;
    static music_rig_compiled_definition alternate_definition;
    static music_rig_prepared_definition prepared;
    music_rig_generation initial = {UINT64_C(1), &default_tables};
    music_rig_runtime runtime;
    music_rig_runtime_config config;
    music_rig_platform_interfaces interfaces;
    music_rig_output_adoption_adapter output;
    music_rig_protocol_request value;
    music_rig_protocol_response response;
    mock_adapter adapter;

    if (init_alternate_prepared_definition_fixture(
            &alternate_tables, &alternate_definition, &prepared
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    init_mock(&adapter);
    interfaces = interfaces_for(&adapter);
    memset(&output, 0, sizeof(output));
    output.abi_version = MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION;
    output.context = &adapter;
    output.prepare = mock_output_prepare;
    output.confirm = mock_output_confirm;
    output.rollback = mock_output_rollback;
    config = config_for(&initial, fingerprint);
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.output_adoption = &output;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    value = request(UINT64_C(42), UINT64_C(1),
        MUSIC_RIG_OPERATION_SWITCH_GLOBAL);
    fixture_copy(value.profile, "multilevel-volume-mixed-pads");
    if (music_rig_runtime_dispatch(&runtime, &value, &response) !=
            MUSIC_RIG_RESULT_OK || response.result_code !=
            (uint32_t)MUSIC_RIG_RESULT_OK || adapter.output_prepare_calls != 2U ||
        adapter.output_confirm_calls != 2U ||
        strcmp(runtime.active_rig_profile, "multilevel-volume-mixed-pads") != 0) {
        fputs("output-enabled global switch failed\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (init_compiled_tables_fixture(&default_tables) !=
            MUSIC_RIG_RESULT_OK ||
        test_lifecycle_and_metrics() != 0 ||
        test_prepared_definition_catalogue() != 0 ||
        test_generation_reclamation_and_ports() != 0 ||
        test_persisted_state() != 0 ||
        test_adapter_failures() != 0 ||
        test_invalid_configuration() != 0 ||
        test_enabled_output_initialization() != 0 ||
        test_output_confirmation_failure_is_fail_closed() != 0 ||
        test_device_override_transactions() != 0 ||
        test_output_enabled_device_switch() != 0 ||
        test_output_enabled_global_switch() != 0) {
        return 1;
    }
    return 0;
}
