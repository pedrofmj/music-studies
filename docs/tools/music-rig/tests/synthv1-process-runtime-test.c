#include "compiled-tables-fixture.h"
#include "music_rig/runtime.h"
#include "music_rig/synthv1_preset_adapter.h"
#include "music_rig/synthv1_process.h"

#include <stdio.h>
#include <string.h>

typedef struct storage_mock {
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    size_t size;
    bool exists;
} storage_mock;

typedef struct output_mock {
    unsigned int prepare_calls;
    unsigned int confirm_calls;
    unsigned int rollback_calls;
    unsigned int confirm_failures;
} output_mock;

static uint64_t now_ns(void *opaque)
{
    static uint64_t value = 0U;
    (void)opaque;
    value += UINT64_C(100);
    return value;
}

static music_rig_result storage_read(
    void *opaque, music_rig_storage_object object, uint8_t *output,
    size_t capacity, size_t *size
)
{
    storage_mock *storage = opaque;
    if (object != MUSIC_RIG_STORAGE_RUNTIME_STATE || !storage->exists) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (capacity < storage->size) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, storage->frame, storage->size);
    *size = storage->size;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result storage_replace(
    void *opaque, music_rig_storage_object object, const uint8_t *input,
    size_t size
)
{
    storage_mock *storage = opaque;
    if (object != MUSIC_RIG_STORAGE_RUNTIME_STATE ||
        size != sizeof(storage->frame)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memcpy(storage->frame, input, size);
    storage->size = size;
    storage->exists = true;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result control_start(void *opaque)
{
    (void)opaque;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_control_poll control_poll(
    void *opaque, music_rig_protocol_request *request
)
{
    (void)opaque;
    (void)request;
    return MUSIC_RIG_CONTROL_STOP;
}

static music_rig_result control_wait(void *opaque)
{
    (void)opaque;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result control_respond(
    void *opaque, const music_rig_protocol_response *response
)
{
    (void)opaque;
    (void)response;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result control_stop(void *opaque)
{
    (void)opaque;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_prepare(
    void *opaque, const music_rig_generation *generation
)
{
    output_mock *output = opaque;
    (void)generation;
    output->prepare_calls += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_confirm(
    void *opaque, const music_rig_generation *generation
)
{
    output_mock *output = opaque;
    (void)generation;
    output->confirm_calls += 1U;
    if (output->confirm_failures != 0U) {
        output->confirm_failures -= 1U;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_rollback(
    void *opaque, const music_rig_generation *generation
)
{
    output_mock *output = opaque;
    (void)generation;
    output->rollback_calls += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_adopted(
    void *opaque, const music_rig_generation *generation,
    uint64_t *adopted_at_ns
)
{
    (void)opaque;
    *adopted_at_ns = generation->id;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_protocol_request request(void)
{
    music_rig_protocol_request value = {0};
    value.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    value.operation = (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL;
    value.request_id = UINT64_C(1);
    value.expected_generation = UINT64_C(1);
    fixture_copy(value.profile, "multilevel-volume-mixed-pads");
    return value;
}

int main(int argc, char **argv)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {0x72};
    static const char *const required[] = {
        "DCO1_BALANCE", "DCF1_CUTOFF", "OUT1_VOLUME"
    };
    static music_rig_compiled_tables alternate_tables;
    static music_rig_compiled_definition alternate_definition;
    static music_rig_prepared_definition prepared;
    const music_rig_generation initial = {UINT64_C(1), NULL};
    music_rig_compiled_tables initial_tables;
    music_rig_generation initial_generation;
    music_rig_runtime runtime;
    music_rig_runtime_config config = {0};
    music_rig_platform_interfaces interfaces = {0};
    music_rig_output_adoption_adapter output = {0};
    music_rig_synthv1_preset_adapter preset = {0};
    music_rig_prepared_engine_adapter prepared_engine = {0};
    music_rig_synthv1_process process = {0};
    storage_mock storage = {0};
    output_mock output_state = {0};
    music_rig_protocol_request value;
    music_rig_protocol_response response;
    bool real_process = strcmp(argv[1], "/definitely/missing/synthv1_jack") != 0;
    music_rig_result dispatch_result;

    if (argc != 4 || init_compiled_tables_fixture(&initial_tables) !=
            MUSIC_RIG_RESULT_OK ||
        init_alternate_prepared_definition_fixture(
            &alternate_tables, &alternate_definition, &prepared
        ) != MUSIC_RIG_RESULT_OK) {
        return 2;
    }
    initial_generation.id = initial.id;
    initial_generation.mapping = &initial_tables;
    if (music_rig_synthv1_process_init(
            &process, argv[1], "s2-synthv1-runtime", argv[3]
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_synthv1_preset_adapter_init(
            &preset, argv[2], required, sizeof(required) / sizeof(required[0])
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_synthv1_preset_adapter_interfaces(
            &preset, &prepared_engine
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    music_rig_synthv1_preset_adapter_set_process_hooks(
        &preset, &process,
        music_rig_synthv1_process_start,
        music_rig_synthv1_process_stop
    );
    interfaces.abi_version = MUSIC_RIG_RUNTIME_ABI_VERSION;
    interfaces.clock.now_ns = now_ns;
    interfaces.control.context = NULL;
    interfaces.control.start = control_start;
    interfaces.control.poll = control_poll;
    interfaces.control.wait = control_wait;
    interfaces.control.respond = control_respond;
    interfaces.control.stop = control_stop;
    interfaces.storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    interfaces.storage.context = &storage;
    interfaces.storage.read = storage_read;
    interfaces.storage.atomic_replace = storage_replace;
    output.abi_version = MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION;
    output.context = &output_state;
    output.prepare = output_prepare;
    output.confirm = output_confirm;
    output.rollback = output_rollback;
    output.adopted = output_adopted;
    config.initial_generation = &initial_generation;
    config.definition_fingerprint = fingerprint;
    config.definition_fingerprint_size = sizeof(fingerprint);
    config.active_rig_profile = "full-live-rack";
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    config.prepared_engine = &prepared_engine;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.output_adoption = &output;
    value = request();
    {
        music_rig_result init_result = music_rig_runtime_init(
            &runtime, &config, &interfaces
        );
        if (init_result != MUSIC_RIG_RESULT_OK) {
            fprintf(stderr, "synthv1 process runtime initialization failed: %d\n",
                (int)init_result);
            return 1;
        }
    }
    output_state.confirm_failures = 1U;
    dispatch_result = music_rig_runtime_dispatch(&runtime, &value, &response);
    if (dispatch_result != MUSIC_RIG_RESULT_OK ||
        response.result_code != (uint32_t)(real_process
            ? MUSIC_RIG_RESULT_ADAPTER_FAILURE
            : MUSIC_RIG_RESULT_INVALID_DATA) ||
        music_rig_synthv1_process_is_healthy(&process) ||
        (real_process && (
            response.rollback_status != (uint32_t)MUSIC_RIG_ROLLBACK_SUCCEEDED ||
            output_state.rollback_calls != 1U
        )) ||
        (!real_process && response.rollback_status !=
            (uint32_t)MUSIC_RIG_ROLLBACK_NOT_REQUIRED)) {
        fprintf(stderr, "synthv1 process runtime rollback failed: dispatch=%d result=%u rollback=%u process=%d output-rollbacks=%u\n",
            (int)dispatch_result, response.result_code, response.rollback_status,
            music_rig_synthv1_process_is_healthy(&process),
            output_state.rollback_calls);
        (void)music_rig_synthv1_process_stop(&process);
        return 1;
    }
    puts("Synthv1 process runtime tests: PASS");
    return 0;
}
