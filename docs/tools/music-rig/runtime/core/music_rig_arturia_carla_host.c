#include "music_rig/arturia_carla_host.h"

#include <string.h>

static bool valid_host(const music_rig_arturia_carla_host *host)
{
    return host != NULL && host->project_open;
}

static uint16_t backend_patch_id(
    const music_rig_arturia_carla_host *host,
    uint16_t patch_index
)
{
    return patch_index < host->patch_count
        ? host->backend_patch_ids[patch_index] : UINT16_MAX;
}

static music_rig_result activate_patch(void *context, uint16_t patch)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.activate_patch != NULL
        ? host->ops.activate_patch(host->ops_context, backend)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result deactivate_patch(void *context, uint16_t patch)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.deactivate_patch != NULL
        ? host->ops.deactivate_patch(host->ops_context, backend)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result set_patch_volume(
    void *context, uint16_t patch, uint8_t value
)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.set_patch_volume != NULL
        ? host->ops.set_patch_volume(host->ops_context, backend, value)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result set_patch_reverb(
    void *context, uint16_t patch, uint8_t value
)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.set_patch_reverb != NULL
        ? host->ops.set_patch_reverb(host->ops_context, backend, value)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result send_note(
    void *context, uint16_t patch, bool note_on, uint8_t note, uint8_t velocity
)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.send_note != NULL
        ? host->ops.send_note(host->ops_context, backend, note_on, note, velocity)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result send_controller(
    void *context, uint16_t patch, uint8_t controller, uint8_t value
)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.send_controller != NULL
        ? host->ops.send_controller(host->ops_context, backend, controller, value)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result send_pitch_bend(
    void *context, uint16_t patch, uint16_t value
)
{
    music_rig_arturia_carla_host *host = context;
    uint16_t backend = valid_host(host) ? backend_patch_id(host, patch) : UINT16_MAX;

    return backend != UINT16_MAX && host->ops.send_pitch_bend != NULL
        ? host->ops.send_pitch_bend(host->ops_context, backend, value)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result set_master_volume(void *context, uint8_t value)
{
    music_rig_arturia_carla_host *host = context;

    return valid_host(host) && host->ops.set_master_volume != NULL
        ? host->ops.set_master_volume(host->ops_context, value)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result set_master_mute(void *context, bool muted)
{
    music_rig_arturia_carla_host *host = context;

    return valid_host(host) && host->ops.set_master_mute != NULL
        ? host->ops.set_master_mute(host->ops_context, muted)
        : MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_arturia_engine_ops session_ops(void)
{
    music_rig_arturia_engine_ops ops = {
        activate_patch,
        deactivate_patch,
        set_patch_volume,
        set_patch_reverb,
        send_note,
        send_controller,
        send_pitch_bend,
        set_master_volume,
        set_master_mute,
    };

    return ops;
}

music_rig_result music_rig_arturia_carla_host_open(
    music_rig_arturia_carla_host *host,
    const music_rig_arturia_carla_ops *ops,
    void *ops_context,
    const char *project_path,
    const char *driver,
    const char *client_name,
    const char *const patch_names[],
    uint16_t patch_count,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    uint8_t initial_master_volume
)
{
    uint16_t index;
    music_rig_result result;
    music_rig_arturia_engine_ops engine;

    if (host == NULL || ops == NULL || project_path == NULL ||
        driver == NULL || client_name == NULL || patch_names == NULL ||
        patch_count == 0U || patch_count > MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY ||
        catalog_patch_count != patch_count || patch_indexes == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (ops->open_project == NULL || ops->close_project == NULL ||
        ops->idle == NULL || ops->find_patch == NULL ||
        ops->connect_midi_input == NULL || ops->deactivate_patch == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(host, 0, sizeof(*host));
    host->ops = *ops;
    host->ops_context = ops_context;
    result = host->ops.open_project(
        ops_context, project_path, driver, client_name
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    host->project_open = true;
    host->patch_count = patch_count;
    for (index = 0U; index < patch_count; ++index) {
        if (patch_names[index] == NULL) {
            result = MUSIC_RIG_RESULT_NOT_FOUND;
            goto close_failed_open;
        }
        result = host->ops.find_patch(
            ops_context, patch_names[index], &host->backend_patch_ids[index]
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            goto close_failed_open;
        }
        result = host->ops.connect_midi_input(
            ops_context, host->backend_patch_ids[index]
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            goto close_failed_open;
        }
    }
    for (index = 0U; index < patch_count; ++index) {
        result = host->ops.deactivate_patch(
            ops_context, host->backend_patch_ids[index]
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            goto close_failed_open;
        }
    }
    engine = session_ops();
    result = music_rig_arturia_session_init(
        &host->session,
        &engine,
        host,
        patch_indexes,
        catalog_patch_count,
        active_layers,
        active_layer_count,
        initial_master_volume
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        goto close_failed_open;
    }
    return result;

close_failed_open:
    (void)host->ops.close_project(ops_context);
    memset(host, 0, sizeof(*host));
    return result;
}

music_rig_result music_rig_arturia_carla_host_select_profile(
    music_rig_arturia_carla_host *host,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
)
{
    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (catalog_patch_count != host->patch_count) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return music_rig_arturia_session_select_profile(
        &host->session,
        patch_indexes,
        catalog_patch_count,
        active_layers,
        active_layer_count
    );
}

music_rig_result music_rig_arturia_carla_host_process_midi(
    music_rig_arturia_carla_host *host,
    const uint8_t *message,
    size_t message_size
)
{
    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    return music_rig_arturia_session_process_midi(
        &host->session, message, message_size
    );
}

music_rig_result music_rig_arturia_carla_host_idle(
    music_rig_arturia_carla_host *host
)
{
    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    host->ops.idle(host->ops_context);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_arturia_carla_host_close(
    music_rig_arturia_carla_host *host
)
{
    music_rig_result result;

    if (host == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!host->project_open) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = host->ops.close_project(host->ops_context);
    memset(host, 0, sizeof(*host));
    return result;
}
