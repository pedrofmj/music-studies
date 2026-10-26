#include "win32-carla-fake.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define FAKE_EXPORT __declspec(dllexport)
#define FAKE_CARLA_PLUGIN_TYPE_SF2 10
#define FAKE_ENGINE_OPTION_PROCESS_MODE 1
#define FAKE_ENGINE_PROCESS_MODE_PATCHBAY 3
#define FAKE_CALLBACK_PATCHBAY_CLIENT_ADDED 20
#define FAKE_CALLBACK_PATCHBAY_PORT_ADDED 24
#define FAKE_CALLBACK_PATCHBAY_CONNECTION_ADDED 27
#define FAKE_PATCHBAY_PORT_IS_INPUT UINT32_C(0x01)
#define FAKE_PATCHBAY_PORT_TYPE_MIDI UINT32_C(0x08)
#define FAKE_EXTERNAL_GROUP 40U
#define FAKE_EXTERNAL_PORT 7U
#define FAKE_PLUGIN_GROUP_BASE 100U

typedef struct fake_carla_plugin_info {
    int type;
    int category;
    uint32_t hints;
    uint32_t options_available;
    uint32_t options_enabled;
    const char *filename;
    const char *name;
    const char *label;
    const char *maker;
    const char *copyright;
    const char *icon_name;
    int64_t unique_id;
} fake_carla_plugin_info;

typedef struct fake_carla_parameter_info {
    const char *name;
    const char *symbol;
    const char *unit;
    const char *comment;
    const char *group_name;
    uint32_t scale_point_count;
} fake_carla_parameter_info;

typedef struct fake_carla_parameter_ranges {
    float default_value;
    float minimum;
    float maximum;
    float step;
    float step_small;
    float step_large;
} fake_carla_parameter_ranges;

typedef void (__cdecl *fake_carla_engine_callback)(
    void *context,
    int action,
    uint32_t plugin_id,
    int value1,
    int value2,
    int value3,
    float valuef,
    const char *value_string
);

typedef struct fake_carla_state {
    music_rig_win32_carla_fake_status status;
    fake_carla_engine_callback callback;
    void *callback_context;
    fake_carla_plugin_info plugins[MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT];
    uint32_t options[MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT];
    bool engine_started;
    bool project_loaded;
    bool callback_unregistered;
    DWORD owner_thread_id;
    char last_error[96];
} fake_carla_state;

static fake_carla_state fake;

static const char *const plugin_names[MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT] = {
    "patch-0", "patch-1", "patch-2", "patch-3"
};

static const fake_carla_parameter_info parameter_info[4] = {
    {"Parameter 0", "parameter0", "", "", "", 0U},
    {"Parameter 1", "parameter1", "", "", "", 0U},
    {"Parameter 2", "parameter2", "", "", "", 0U},
    {"Reverb Level", "reverb_level", "", "", "", 0U}
};

static const fake_carla_parameter_ranges reverb_ranges = {
    0.0f, 0.0f, 1.0f, 0.01f, 0.001f, 0.1f
};

static void initialize_plugins(void)
{
    uint32_t index;

    for (index = 0U; index < MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT; ++index) {
        fake.plugins[index].type = FAKE_CARLA_PLUGIN_TYPE_SF2;
        fake.plugins[index].options_available =
            MUSIC_RIG_WIN32_CARLA_FAKE_REQUIRED_OPTIONS;
        fake.plugins[index].name = plugin_names[index];
    }
}

static void set_fault(uint32_t fault)
{
    fake.status.faults |= fault;
    (void)snprintf(fake.last_error, sizeof(fake.last_error),
        "fake Carla contract violation 0x%08lx", (unsigned long)fault);
}

static bool valid_handle(void *handle)
{
    if (handle != &fake) {
        return false;
    }
    if (GetCurrentThreadId() != fake.owner_thread_id) {
        fake.status.wrong_thread_api_calls += 1U;
        return false;
    }
    return true;
}

static void emit_callback(
    int action,
    uint32_t plugin_id,
    int value1,
    int value2,
    const char *value_string
)
{
    if (fake.callback == NULL) {
        set_fault(UINT32_C(0x01));
        return;
    }
    fake.callback(fake.callback_context, action, plugin_id, value1, value2,
        0, 0.0f, value_string);
}

static void emit_client(uint32_t group_id, int plugin_id, const char *name)
{
    emit_callback(FAKE_CALLBACK_PATCHBAY_CLIENT_ADDED, group_id, 0, plugin_id,
        name);
    fake.status.client_callbacks += 1U;
}

static void emit_port(
    uint32_t group_id,
    uint32_t port_id,
    uint32_t hints,
    const char *name
)
{
    emit_callback(FAKE_CALLBACK_PATCHBAY_PORT_ADDED, group_id, (int)port_id,
        (int)hints, name);
    fake.status.port_callbacks += 1U;
}

static bool expected_startup(uint32_t required)
{
    if ((fake.status.lifecycle & required) != required) {
        set_fault(UINT32_C(0x02));
        return false;
    }
    return true;
}

static bool all_required_options_are_set(void)
{
    uint32_t index;

    for (index = 0U; index < MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT; ++index) {
        if (fake.options[index] !=
                MUSIC_RIG_WIN32_CARLA_FAKE_REQUIRED_OPTIONS) {
            return false;
        }
    }
    return true;
}

FAKE_EXPORT void *__cdecl carla_standalone_host_init(void)
{
    memset(&fake, 0, sizeof(fake));
    fake.status.struct_size = (uint32_t)sizeof(fake.status);
    fake.status.lifecycle = MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_HOST_INIT;
    fake.status.standalone_init_calls = 1U;
    fake.owner_thread_id = GetCurrentThreadId();
    initialize_plugins();
    return &fake;
}

FAKE_EXPORT void __cdecl carla_set_engine_callback(
    void *handle,
    fake_carla_engine_callback callback,
    void *context
)
{
    if (!valid_handle(handle)) {
        set_fault(UINT32_C(0x04));
        return;
    }
    if (callback == NULL) {
        if (!fake.engine_started || fake.callback == NULL) {
            set_fault(UINT32_C(0x08));
        }
        fake.callback = NULL;
        fake.callback_context = NULL;
        fake.callback_unregistered = true;
        return;
    }
    if (fake.callback != NULL || fake.engine_started || context == NULL) {
        set_fault(UINT32_C(0x10));
        return;
    }
    fake.callback = callback;
    fake.callback_context = context;
    fake.status.lifecycle |= MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_CALLBACK;
}

FAKE_EXPORT void __cdecl carla_set_engine_option(
    void *handle,
    int option,
    int value,
    const char *value_string
)
{
    (void)value_string;
    if (!valid_handle(handle) || fake.engine_started ||
        option != FAKE_ENGINE_OPTION_PROCESS_MODE ||
        value != FAKE_ENGINE_PROCESS_MODE_PATCHBAY) {
        set_fault(UINT32_C(0x20));
        return;
    }
    fake.status.lifecycle |=
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_OPTION;
    fake.status.engine_option_calls += 1U;
}

FAKE_EXPORT bool __cdecl carla_engine_init(
    void *handle,
    const char *driver,
    const char *client
)
{
    if (!valid_handle(handle) || fake.engine_started || driver == NULL ||
        client == NULL || strcmp(driver, "Offline") != 0 ||
        strcmp(client, "music-rig-offline") != 0 ||
        !expected_startup(
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_HOST_INIT |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_CALLBACK |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_OPTION
        )) {
        set_fault(UINT32_C(0x40));
        return false;
    }
    fake.engine_started = true;
    fake.status.lifecycle |= MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_INIT;
    fake.status.engine_init_calls += 1U;
    return true;
}

FAKE_EXPORT bool __cdecl carla_load_project(void *handle, const char *path)
{
    if (!valid_handle(handle) || !fake.engine_started || fake.project_loaded ||
        path == NULL || strcmp(path, "offline-project.carxp") != 0 ||
        !expected_startup(
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_INIT
        )) {
        set_fault(UINT32_C(0x80));
        return false;
    }
    fake.project_loaded = true;
    fake.status.lifecycle |= MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_PROJECT;
    fake.status.project_load_calls += 1U;
    return true;
}

FAKE_EXPORT uint32_t __cdecl carla_get_current_plugin_count(void *handle)
{
    if (!valid_handle(handle) || !fake.project_loaded) {
        set_fault(UINT32_C(0x100));
        return 0U;
    }
    return MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT;
}

FAKE_EXPORT const fake_carla_plugin_info *__cdecl carla_get_plugin_info(
    void *handle,
    uint32_t plugin_id
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT || !fake.project_loaded) {
        set_fault(UINT32_C(0x200));
        return NULL;
    }
    fake.status.plugin_info_calls += 1U;
    fake.plugins[plugin_id].options_enabled = fake.options[plugin_id];
    return &fake.plugins[plugin_id];
}

FAKE_EXPORT uint32_t __cdecl carla_get_parameter_count(
    void *handle,
    uint32_t plugin_id
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT) {
        set_fault(UINT32_C(0x400));
        return 0U;
    }
    return 4U;
}

FAKE_EXPORT const fake_carla_parameter_info *__cdecl carla_get_parameter_info(
    void *handle,
    uint32_t plugin_id,
    uint32_t parameter_id
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT || parameter_id >= 4U) {
        set_fault(UINT32_C(0x800));
        return NULL;
    }
    fake.status.parameter_info_calls += 1U;
    return &parameter_info[parameter_id];
}

FAKE_EXPORT const fake_carla_parameter_ranges *__cdecl
carla_get_parameter_ranges(void *handle, uint32_t plugin_id, uint32_t parameter_id)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT || parameter_id != 3U) {
        set_fault(UINT32_C(0x1000));
        return NULL;
    }
    fake.status.parameter_range_calls += 1U;
    return &reverb_ranges;
}

FAKE_EXPORT void __cdecl carla_set_active(
    void *handle,
    uint32_t plugin_id,
    bool active
)
{
    uint32_t bit;

    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT ||
        fake.status.connection_mask != UINT32_C(0x0f) ||
        !all_required_options_are_set() ||
        fake.status.lifecycle != (
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_HOST_INIT |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_CALLBACK |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_OPTION |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_INIT |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_PROJECT |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_EXTERNAL_REFRESH |
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_INTERNAL_REFRESH)) {
        set_fault(UINT32_C(0x2000));
        return;
    }
    bit = UINT32_C(1) << plugin_id;
    if (active) {
        fake.status.active_mask |= bit;
        fake.status.activation_calls += 1U;
    } else {
        fake.status.active_mask &= ~bit;
        fake.status.deactivated_mask |= bit;
        fake.status.deactivation_calls += 1U;
    }
}

FAKE_EXPORT void __cdecl carla_set_volume(
    void *handle,
    uint32_t plugin_id,
    float value
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT || value < 0.0f ||
        value > 1.0f) {
        set_fault(UINT32_C(0x4000));
        return;
    }
    fake.status.gain_calls += 1U;
}

FAKE_EXPORT void __cdecl carla_set_parameter_value(
    void *handle,
    uint32_t plugin_id,
    uint32_t parameter_id,
    float value
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT || parameter_id != 3U ||
        value < 0.0f || value > 1.0f) {
        set_fault(UINT32_C(0x8000));
        return;
    }
    fake.status.reverb_calls += 1U;
}

FAKE_EXPORT void __cdecl carla_set_option(
    void *handle,
    uint32_t plugin_id,
    uint32_t option,
    bool enabled
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT ||
        (option != UINT32_C(0x010) && option != UINT32_C(0x080) &&
            option != UINT32_C(0x400))) {
        set_fault(UINT32_C(0x10000));
        return;
    }
    if (enabled) {
        fake.options[plugin_id] |= option;
    } else {
        fake.options[plugin_id] &= ~option;
    }
    fake.status.option_masks[plugin_id] = fake.options[plugin_id];
}

FAKE_EXPORT void __cdecl carla_send_midi_note(
    void *handle,
    uint32_t plugin_id,
    uint8_t channel,
    uint8_t note,
    uint8_t velocity
)
{
    if (!valid_handle(handle) || plugin_id >=
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT || channel != 0U ||
        note > 127U || velocity > 127U ||
        (fake.options[plugin_id] &
            MUSIC_RIG_WIN32_CARLA_FAKE_REQUIRED_OPTIONS) !=
            MUSIC_RIG_WIN32_CARLA_FAKE_REQUIRED_OPTIONS) {
        set_fault(UINT32_C(0x20000));
        return;
    }
    fake.status.note_calls += 1U;
    if (velocity == 0U) {
        fake.status.note_off_calls += 1U;
    } else {
        fake.status.note_on_calls += 1U;
    }
}

FAKE_EXPORT void __cdecl carla_engine_idle(void *handle)
{
    if (!valid_handle(handle) || !fake.engine_started ||
        !fake.project_loaded) {
        set_fault(UINT32_C(0x40000));
        return;
    }
    fake.status.idle_calls += 1U;
}

FAKE_EXPORT bool __cdecl carla_engine_close(void *handle)
{
    if (!valid_handle(handle) || !fake.engine_started ||
        !fake.callback_unregistered || fake.callback != NULL) {
        set_fault(UINT32_C(0x80000));
        return false;
    }
    fake.engine_started = false;
    fake.status.engine_close_calls += 1U;
    fake.status.lifecycle |= MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_CLOSE;
    return true;
}

FAKE_EXPORT bool __cdecl carla_patchbay_refresh(void *handle, bool external)
{
    uint32_t index;

    if (!valid_handle(handle) || !fake.project_loaded || fake.callback == NULL) {
        set_fault(UINT32_C(0x100000));
        return false;
    }
    if (external) {
        if ((fake.status.lifecycle &
                MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_EXTERNAL_REFRESH) != 0U) {
            set_fault(UINT32_C(0x200000));
            return false;
        }
        emit_client(FAKE_EXTERNAL_GROUP, -1, "MIDI Services Loopback");
        emit_port(FAKE_EXTERNAL_GROUP, FAKE_EXTERNAL_PORT,
            FAKE_PATCHBAY_PORT_TYPE_MIDI, "MIDI In");
        fake.status.lifecycle |=
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_EXTERNAL_REFRESH;
        fake.status.external_refresh_calls += 1U;
        return true;
    }
    if ((fake.status.lifecycle &
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_EXTERNAL_REFRESH) == 0U ||
        (fake.status.lifecycle &
            MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_INTERNAL_REFRESH) != 0U) {
        set_fault(UINT32_C(0x400000));
        return false;
    }
    for (index = 0U; index < MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT; ++index) {
        emit_client(FAKE_PLUGIN_GROUP_BASE + index, (int)index,
            plugin_names[index]);
        emit_port(FAKE_PLUGIN_GROUP_BASE + index, 0U,
            FAKE_PATCHBAY_PORT_IS_INPUT | FAKE_PATCHBAY_PORT_TYPE_MIDI,
            "MIDI In");
    }
    fake.status.lifecycle |=
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_INTERNAL_REFRESH;
    fake.status.internal_refresh_calls += 1U;
    return true;
}

FAKE_EXPORT bool __cdecl carla_patchbay_connect(
    void *handle,
    bool external,
    uint32_t source_group,
    uint32_t source_port,
    uint32_t destination_group,
    uint32_t destination_port
)
{
    uint32_t plugin_id;
    uint32_t bit;
    char connection[64];
    int written;

    if (!valid_handle(handle) || !external || source_group !=
            FAKE_EXTERNAL_GROUP || source_port != FAKE_EXTERNAL_PORT ||
        destination_group < FAKE_PLUGIN_GROUP_BASE || destination_port != 0U) {
        set_fault(UINT32_C(0x800000));
        return false;
    }
    plugin_id = destination_group - FAKE_PLUGIN_GROUP_BASE;
    if (plugin_id >= MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT) {
        set_fault(UINT32_C(0x1000000));
        return false;
    }
    bit = UINT32_C(1) << plugin_id;
    if ((fake.status.connection_mask & bit) != 0U) {
        set_fault(UINT32_C(0x2000000));
        return false;
    }
    written = snprintf(connection, sizeof(connection), "%lu:%lu:%lu:%lu",
        (unsigned long)source_group, (unsigned long)source_port,
        (unsigned long)destination_group, (unsigned long)destination_port);
    if (written <= 0 || (size_t)written >= sizeof(connection)) {
        set_fault(UINT32_C(0x4000000));
        return false;
    }
    fake.status.connection_mask |= bit;
    fake.status.connection_calls += 1U;
    emit_callback(FAKE_CALLBACK_PATCHBAY_CONNECTION_ADDED, plugin_id + 1U,
        0, 0, connection);
    return fake.status.faults == 0U;
}

FAKE_EXPORT const char *__cdecl carla_get_last_error(void *handle)
{
    if (!valid_handle(handle)) {
        return "invalid fake Carla handle";
    }
    fake.status.last_error_calls += 1U;
    return fake.last_error;
}

FAKE_EXPORT int __cdecl music_rig_win32_carla_fake_get_status(
    music_rig_win32_carla_fake_status *status,
    size_t status_capacity
)
{
    if (status == NULL || status_capacity < sizeof(fake.status)) {
        return 0;
    }
    if (fake.status.struct_size == 0U) {
        fake.status.struct_size = (uint32_t)sizeof(fake.status);
    }
    *status = fake.status;
    return 1;
}
