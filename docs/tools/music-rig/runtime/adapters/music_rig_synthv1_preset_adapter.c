#include "music_rig/synthv1_preset_adapter.h"

#include <stdio.h>
#include <string.h>

#define PRESET_DOCUMENT_CAPACITY ((size_t)131072)

static music_rig_result read_preset(
    const music_rig_synthv1_preset_adapter *adapter,
    char *document,
    size_t capacity
)
{
    FILE *file;
    size_t size;

    file = fopen(adapter->preset_path, "rb");
    if (file == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    size = fread(document, 1U, capacity - 1U, file);
    if (ferror(file) != 0 || fclose(file) != 0 || size == 0U) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    document[size] = '\0';
    return strstr(document, "<!DOCTYPE synthv1>") == NULL
        ? MUSIC_RIG_RESULT_INVALID_DATA
        : MUSIC_RIG_RESULT_OK;
}

static music_rig_result preset_stage(
    void *opaque, const music_rig_generation *candidate
)
{
    music_rig_synthv1_preset_adapter *adapter = opaque;
    char document[PRESET_DOCUMENT_CAPACITY];
    (void)candidate;

    if (read_preset(adapter, document, sizeof(document)) !=
        MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    adapter->staged = true;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result preset_validate(
    void *opaque, const music_rig_generation *candidate
)
{
    music_rig_synthv1_preset_adapter *adapter = opaque;
    char document[PRESET_DOCUMENT_CAPACITY];
    size_t index;

    (void)candidate;
    if (!adapter->staged || read_preset(
            adapter, document, sizeof(document)
        ) != MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    for (index = 0U; index < adapter->required_parameter_count; ++index) {
        char marker[256];
        int written = snprintf(
            marker,
            sizeof(marker),
            "name=\"%s\"",
            adapter->required_parameters[index]
        );
        if (written <= 0 || (size_t)written >= sizeof(marker) ||
            strstr(document, marker) == NULL) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result preset_arm(
    void *opaque, const music_rig_generation *candidate
)
{
    music_rig_synthv1_preset_adapter *adapter = opaque;
    (void)candidate;

    if (!adapter->staged) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (adapter->process_start != NULL) {
        if (adapter->process_start(
                adapter->process_context,
                adapter->preset_path
            ) != MUSIC_RIG_RESULT_OK) {
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        adapter->process_started = true;
    }
    adapter->armed = true;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result preset_commit(
    void *opaque, const music_rig_generation *candidate
)
{
    music_rig_synthv1_preset_adapter *adapter = opaque;
    (void)candidate;

    if (!adapter->armed ||
        (adapter->process_start != NULL && !adapter->process_started)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    adapter->committed = true;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result preset_rollback(
    void *opaque, const music_rig_generation *previous
)
{
    music_rig_synthv1_preset_adapter *adapter = opaque;
    (void)previous;

    if (adapter->process_started && adapter->process_stop != NULL &&
        adapter->process_stop(adapter->process_context) != MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    adapter->process_started = false;
    adapter->committed = false;
    adapter->armed = false;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result preset_discard(
    void *opaque, const music_rig_generation *candidate
)
{
    music_rig_synthv1_preset_adapter *adapter = opaque;
    (void)candidate;

    if (adapter->process_started && adapter->process_stop != NULL &&
        adapter->process_stop(adapter->process_context) != MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    adapter->process_started = false;
    adapter->staged = false;
    adapter->armed = false;
    adapter->committed = false;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_synthv1_preset_adapter_init(
    music_rig_synthv1_preset_adapter *adapter,
    const char *preset_path,
    const char *const *required_parameters,
    size_t required_parameter_count
)
{
    if (adapter == NULL || preset_path == NULL ||
        required_parameters == NULL || required_parameter_count == 0U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->preset_path = preset_path;
    adapter->required_parameters = required_parameters;
    adapter->required_parameter_count = required_parameter_count;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_synthv1_preset_adapter_interfaces(
    music_rig_synthv1_preset_adapter *adapter,
    music_rig_prepared_engine_adapter *interfaces
)
{
    if (adapter == NULL || interfaces == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(interfaces, 0, sizeof(*interfaces));
    interfaces->abi_version = MUSIC_RIG_PREPARED_ENGINE_ADAPTER_ABI_VERSION;
    interfaces->context = adapter;
    interfaces->stage = preset_stage;
    interfaces->validate = preset_validate;
    interfaces->arm = preset_arm;
    interfaces->commit = preset_commit;
    interfaces->rollback = preset_rollback;
    interfaces->discard = preset_discard;
    return MUSIC_RIG_RESULT_OK;
}

void music_rig_synthv1_preset_adapter_set_process_hooks(
    music_rig_synthv1_preset_adapter *adapter,
    void *context,
    music_rig_synthv1_process_start_callback start,
    music_rig_synthv1_process_stop_callback stop
)
{
    if (adapter == NULL) {
        return;
    }
    adapter->process_context = context;
    adapter->process_start = start;
    adapter->process_stop = stop;
}
