#include "music_rig/win32_carla_host.h"

#include "music_rig/arturia_carla_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#define CARLA_NAME_CAPACITY 256U
#define CARLA_ERROR_CAPACITY 512U
#define CARLA_CLIENT_CAPACITY 512U
#define CARLA_PORT_CAPACITY 2048U
#define CARLA_CONNECTION_CAPACITY 2048U
#define CARLA_ENUMERATION_ATTEMPTS 200U
#define CARLA_PLUGIN_TYPE_SF2 10
#define CARLA_PARAMETER_REVERB_LEVEL 3U
#define CARLA_OPTION_SKIP_NOTES UINT32_C(0x400)
#define CARLA_OPTION_SEND_CONTROL_CHANGES UINT32_C(0x010)
#define CARLA_OPTION_SEND_PITCH_BEND UINT32_C(0x080)
#define CARLA_ENGINE_OPTION_PROCESS_MODE 1
#define CARLA_ENGINE_PROCESS_MODE_PATCHBAY 3
#define CARLA_CALLBACK_PATCHBAY_CLIENT_ADDED 20
#define CARLA_CALLBACK_PATCHBAY_CLIENT_REMOVED 21
#define CARLA_CALLBACK_PATCHBAY_CLIENT_RENAMED 22
#define CARLA_CALLBACK_PATCHBAY_PORT_ADDED 24
#define CARLA_CALLBACK_PATCHBAY_PORT_REMOVED 25
#define CARLA_CALLBACK_PATCHBAY_PORT_CHANGED 26
#define CARLA_CALLBACK_PATCHBAY_CONNECTION_ADDED 27
#define CARLA_CALLBACK_PATCHBAY_CONNECTION_REMOVED 28
#define CARLA_CALLBACK_ENGINE_STOPPED 30
#define CARLA_CALLBACK_ENGINE_QUIT 41
#define CARLA_PATCHBAY_PORT_IS_INPUT UINT32_C(0x01)
#define CARLA_PATCHBAY_PORT_TYPE_MIDI UINT32_C(0x08)
#define CARLA_MIDI_QUEUE_MASK \
    (MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY - 1U)

/* Struct layouts and function signatures follow Carla 2.5.10 (3834c7f). */
typedef struct carla_plugin_info_2510 {
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
} carla_plugin_info_2510;

typedef struct carla_parameter_info_2510 {
    const char *name;
    const char *symbol;
    const char *unit;
    const char *comment;
    const char *group_name;
    uint32_t scale_point_count;
} carla_parameter_info_2510;

typedef struct carla_parameter_ranges_2510 {
    float default_value;
    float minimum;
    float maximum;
    float step;
    float step_small;
    float step_large;
} carla_parameter_ranges_2510;

typedef void (__cdecl *carla_engine_callback_2510)(
    void *context,
    int action,
    uint32_t plugin_id,
    int value1,
    int value2,
    int value3,
    float valuef,
    const char *value_string
);

typedef void *(__cdecl *carla_standalone_host_init_fn)(void);
typedef bool (__cdecl *carla_engine_init_fn)(
    void *handle, const char *driver, const char *client
);
typedef bool (__cdecl *carla_engine_close_fn)(void *handle);
typedef void (__cdecl *carla_engine_idle_fn)(void *handle);
typedef bool (__cdecl *carla_load_project_fn)(
    void *handle, const char *path
);
typedef uint32_t (__cdecl *carla_get_current_plugin_count_fn)(void *handle);
typedef const carla_plugin_info_2510 *(__cdecl *carla_get_plugin_info_fn)(
    void *handle, uint32_t plugin_id
);
typedef uint32_t (__cdecl *carla_get_parameter_count_fn)(
    void *handle, uint32_t plugin_id
);
typedef const carla_parameter_info_2510 *(
    __cdecl *carla_get_parameter_info_fn
)(void *handle, uint32_t plugin_id, uint32_t parameter_id);
typedef const carla_parameter_ranges_2510 *(
    __cdecl *carla_get_parameter_ranges_fn
)(void *handle, uint32_t plugin_id, uint32_t parameter_id);
typedef void (__cdecl *carla_set_active_fn)(
    void *handle, uint32_t plugin_id, bool active
);
typedef void (__cdecl *carla_set_volume_fn)(
    void *handle, uint32_t plugin_id, float value
);
typedef void (__cdecl *carla_set_parameter_value_fn)(
    void *handle, uint32_t plugin_id, uint32_t parameter_id, float value
);
typedef void (__cdecl *carla_set_option_fn)(
    void *handle, uint32_t plugin_id, uint32_t option, bool enabled
);
typedef void (__cdecl *carla_set_engine_callback_fn)(
    void *handle, carla_engine_callback_2510 callback, void *context
);
typedef void (__cdecl *carla_set_engine_option_fn)(
    void *handle, int option, int value, const char *value_string
);
typedef bool (__cdecl *carla_patchbay_refresh_fn)(
    void *handle, bool external
);
typedef bool (__cdecl *carla_patchbay_connect_fn)(
    void *handle,
    bool external,
    uint32_t source_group,
    uint32_t source_port,
    uint32_t destination_group,
    uint32_t destination_port
);
typedef const char *(__cdecl *carla_get_last_error_fn)(void *handle);

typedef struct carla_api_2510 {
    carla_standalone_host_init_fn standalone_host_init;
    carla_engine_init_fn engine_init;
    carla_engine_close_fn engine_close;
    carla_engine_idle_fn engine_idle;
    carla_load_project_fn load_project;
    carla_get_current_plugin_count_fn get_current_plugin_count;
    carla_get_plugin_info_fn get_plugin_info;
    carla_get_parameter_count_fn get_parameter_count;
    carla_get_parameter_info_fn get_parameter_info;
    carla_get_parameter_ranges_fn get_parameter_ranges;
    carla_set_active_fn set_active;
    carla_set_volume_fn set_volume;
    carla_set_parameter_value_fn set_parameter_value;
    carla_set_option_fn set_option;
    carla_set_engine_callback_fn set_engine_callback;
    carla_set_engine_option_fn set_engine_option;
    carla_patchbay_refresh_fn patchbay_refresh;
    carla_patchbay_connect_fn patchbay_connect;
    carla_get_last_error_fn get_last_error;
} carla_api_2510;

typedef struct carla_client_record {
    bool used;
    uint32_t group_id;
    int plugin_id;
    char name[CARLA_NAME_CAPACITY];
} carla_client_record;

typedef struct carla_port_record {
    bool used;
    uint32_t group_id;
    uint32_t port_id;
    uint32_t hints;
    char name[CARLA_NAME_CAPACITY];
} carla_port_record;

typedef struct carla_connection_record {
    bool used;
    uint32_t connection_id;
    uint32_t source_group;
    uint32_t source_port;
    uint32_t destination_group;
    uint32_t destination_port;
} carla_connection_record;

typedef struct carla_patch_record {
    bool found;
    bool midi_connected;
    uint32_t plugin_id;
    uint32_t reverb_parameter;
    uint8_t fader;
} carla_patch_record;

typedef struct carla_midi_message {
    uint8_t kind;
    uint8_t size;
    uint8_t bytes[3];
    uint8_t active_layer_count;
    uint16_t catalog_patch_count;
    uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    uint8_t active_layers[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
} carla_midi_message;

#define CARLA_MIDI_QUEUE_EVENT_MESSAGE UINT8_C(0)
#define CARLA_MIDI_QUEUE_EVENT_PROFILE UINT8_C(1)

struct music_rig_win32_carla_host {
    HMODULE library;
    carla_api_2510 api;
    void *carla_handle;
    CRITICAL_SECTION callback_lock;
    bool callback_lock_initialized;
    bool callback_registered;
    bool engine_started;
    bool project_open;
    DWORD owner_thread;
    LONG callback_fault;
    LONG routing_established;
    char error[CARLA_ERROR_CAPACITY];
    char dll_path[32768];
    char midi_client_name[CARLA_NAME_CAPACITY];
    char midi_port_name[CARLA_NAME_CAPACITY];
    const char *const *opening_patch_names;
    uint16_t opening_patch_count;
    music_rig_win32_carla_raw_midi_output_fn raw_midi_output;
    void *raw_midi_output_context;
    carla_midi_message midi_queue[MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY];
    _Atomic size_t midi_queue_head;
    _Atomic size_t midi_queue_tail;
    _Atomic uint32_t midi_queue_overflows;
    _Atomic bool midi_queue_accepting;
    carla_client_record clients[CARLA_CLIENT_CAPACITY];
    carla_port_record ports[CARLA_PORT_CAPACITY];
    carla_connection_record connections[CARLA_CONNECTION_CAPACITY];
    carla_patch_record patches[MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY];
    size_t patch_count;
    uint8_t master_volume;
    bool master_muted;
    music_rig_arturia_carla_host portable;
};

_Static_assert(
    (MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY &
        CARLA_MIDI_QUEUE_MASK) == 0U,
    "Carla MIDI queue capacity must be a power of two"
);

static bool copy_bounded(
    char *destination, size_t capacity, const char *source, size_t *length
)
{
    size_t index;

    if (destination == NULL || source == NULL || capacity == 0U) {
        return false;
    }
    for (index = 0U; index < capacity; ++index) {
        if (source[index] == '\0') {
            if (index == 0U) {
                return false;
            }
            memcpy(destination, source, index + 1U);
            if (length != NULL) {
                *length = index;
            }
            return true;
        }
    }
    return false;
}

static bool valid_text(const char *text, size_t capacity)
{
    size_t index;

    if (text == NULL) {
        return false;
    }
    for (index = 0U; index < capacity; ++index) {
        if (text[index] == '\0') {
            return index != 0U;
        }
    }
    return false;
}

static void set_error(music_rig_win32_carla_host *host, const char *message)
{
    size_t index;

    if (host == NULL || message == NULL) {
        return;
    }
    for (index = 0U; index + 1U < sizeof(host->error) &&
         message[index] != '\0'; ++index) {
        host->error[index] = message[index];
    }
    host->error[index] = '\0';
}

static void copy_error_output(
    char *destination, size_t capacity, const char *message
)
{
    size_t index;

    if (destination == NULL || capacity == 0U) {
        return;
    }
    destination[0] = '\0';
    if (message == NULL) {
        return;
    }
    for (index = 0U; index + 1U < capacity && message[index] != '\0'; ++index) {
        destination[index] = message[index];
    }
    destination[index] = '\0';
}

static bool load_api(music_rig_win32_carla_host *host);

music_rig_result music_rig_win32_carla_host_probe_library(
    const wchar_t *carla_dll_path,
    char *error_message,
    size_t error_capacity
)
{
    music_rig_win32_carla_host *probe;
    music_rig_result result = MUSIC_RIG_RESULT_OK;

    copy_error_output(error_message, error_capacity, "");
    if (carla_dll_path == NULL || carla_dll_path[0] == L'\0') {
        copy_error_output(error_message, error_capacity,
            "A Carla DLL path is required");
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    probe = calloc(1U, sizeof(*probe));
    if (probe == NULL) {
        copy_error_output(error_message, error_capacity,
            "Could not allocate Carla API probe state");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    probe->library = LoadLibraryExW(
        carla_dll_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH
    );
    if (probe->library == NULL) {
        set_error(probe, "Could not load Carla DLL or one of its dependencies");
        result = MUSIC_RIG_RESULT_NOT_FOUND;
    } else if (!load_api(probe)) {
        set_error(probe, "Carla DLL is missing a required 2.5.10 Host API export");
        result = MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    if (probe->library != NULL && !FreeLibrary(probe->library)) {
        if (probe->error[0] == '\0') {
            set_error(probe, "FreeLibrary failed after Carla API probe");
        }
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    copy_error_output(error_message, error_capacity, probe->error);
    free(probe);
    return result;
}

static void capture_carla_error(
    music_rig_win32_carla_host *host, const char *fallback
)
{
    const char *message = NULL;

    if (host != NULL && host->api.get_last_error != NULL &&
        host->carla_handle != NULL) {
        message = host->api.get_last_error(host->carla_handle);
    }
    set_error(host, message != NULL && message[0] != '\0' ? message : fallback);
}

static bool load_symbol(
    HMODULE library, FARPROC *symbol, const char *name
)
{
    *symbol = GetProcAddress(library, name);
    return *symbol != NULL;
}

static bool load_api(music_rig_win32_carla_host *host)
{
    FARPROC symbol;

#define LOAD_CARLA_SYMBOL(member, export_name) \
    do { \
        if (!load_symbol(host->library, &symbol, export_name)) { \
            return false; \
        } \
        _Static_assert(sizeof(symbol) == sizeof(host->api.member), \
            "Carla function pointers must match FARPROC size"); \
        memcpy(&host->api.member, &symbol, sizeof(symbol)); \
    } while (0)

    LOAD_CARLA_SYMBOL(standalone_host_init, "carla_standalone_host_init");
    LOAD_CARLA_SYMBOL(engine_init, "carla_engine_init");
    LOAD_CARLA_SYMBOL(engine_close, "carla_engine_close");
    LOAD_CARLA_SYMBOL(engine_idle, "carla_engine_idle");
    LOAD_CARLA_SYMBOL(load_project, "carla_load_project");
    LOAD_CARLA_SYMBOL(get_current_plugin_count,
        "carla_get_current_plugin_count");
    LOAD_CARLA_SYMBOL(get_plugin_info, "carla_get_plugin_info");
    LOAD_CARLA_SYMBOL(get_parameter_count, "carla_get_parameter_count");
    LOAD_CARLA_SYMBOL(get_parameter_info, "carla_get_parameter_info");
    LOAD_CARLA_SYMBOL(get_parameter_ranges, "carla_get_parameter_ranges");
    LOAD_CARLA_SYMBOL(set_active, "carla_set_active");
    LOAD_CARLA_SYMBOL(set_volume, "carla_set_volume");
    LOAD_CARLA_SYMBOL(set_parameter_value, "carla_set_parameter_value");
    LOAD_CARLA_SYMBOL(set_option, "carla_set_option");
    LOAD_CARLA_SYMBOL(set_engine_callback, "carla_set_engine_callback");
    LOAD_CARLA_SYMBOL(set_engine_option, "carla_set_engine_option");
    LOAD_CARLA_SYMBOL(patchbay_refresh, "carla_patchbay_refresh");
    LOAD_CARLA_SYMBOL(patchbay_connect, "carla_patchbay_connect");
    LOAD_CARLA_SYMBOL(get_last_error, "carla_get_last_error");
#undef LOAD_CARLA_SYMBOL
    return true;
}

static void callback_fault(music_rig_win32_carla_host *host, LONG fault)
{
    (void)InterlockedCompareExchange(&host->callback_fault, fault, 0);
}

static bool parse_uint32(const char **cursor, uint32_t *value)
{
    uint32_t parsed = 0U;
    const char *position = *cursor;
    bool has_digit = false;

    while (*position >= '0' && *position <= '9') {
        uint32_t digit = (uint32_t)(*position - '0');

        if (parsed > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
        position += 1;
        has_digit = true;
    }
    if (!has_digit) {
        return false;
    }
    *cursor = position;
    *value = parsed;
    return true;
}

static bool parse_connection(
    const char *text,
    uint32_t *source_group,
    uint32_t *source_port,
    uint32_t *destination_group,
    uint32_t *destination_port
)
{
    const char *cursor = text;

    if (cursor == NULL || !parse_uint32(&cursor, source_group) ||
        *cursor != ':') {
        return false;
    }
    cursor += 1;
    if (!parse_uint32(&cursor, source_port) || *cursor != ':') {
        return false;
    }
    cursor += 1;
    if (!parse_uint32(&cursor, destination_group) || *cursor != ':') {
        return false;
    }
    cursor += 1;
    return parse_uint32(&cursor, destination_port) && *cursor == '\0';
}

static carla_client_record *client_slot(
    music_rig_win32_carla_host *host, uint32_t group_id
)
{
    carla_client_record *free_slot = NULL;
    size_t index;

    for (index = 0U; index < CARLA_CLIENT_CAPACITY; ++index) {
        carla_client_record *record = &host->clients[index];

        if (record->used && record->group_id == group_id) {
            return record;
        }
        if (!record->used && free_slot == NULL) {
            free_slot = record;
        }
    }
    return free_slot;
}

static carla_port_record *port_slot(
    music_rig_win32_carla_host *host, uint32_t group_id, uint32_t port_id
)
{
    carla_port_record *free_slot = NULL;
    size_t index;

    for (index = 0U; index < CARLA_PORT_CAPACITY; ++index) {
        carla_port_record *record = &host->ports[index];

        if (record->used && record->group_id == group_id &&
            record->port_id == port_id) {
            return record;
        }
        if (!record->used && free_slot == NULL) {
            free_slot = record;
        }
    }
    return free_slot;
}

static void __cdecl carla_callback(
    void *context,
    int action,
    uint32_t plugin_id,
    int value1,
    int value2,
    int value3,
    float valuef,
    const char *value_string
)
{
    music_rig_win32_carla_host *host = context;
    char copied_name[CARLA_NAME_CAPACITY];
    size_t copied_name_length = 0U;
    bool needs_name = action == CARLA_CALLBACK_PATCHBAY_CLIENT_ADDED ||
        action == CARLA_CALLBACK_PATCHBAY_CLIENT_RENAMED ||
        action == CARLA_CALLBACK_PATCHBAY_PORT_ADDED ||
        action == CARLA_CALLBACK_PATCHBAY_PORT_CHANGED;

    (void)value3;
    (void)valuef;
    if (host == NULL || !host->callback_lock_initialized) {
        return;
    }
    if (action == CARLA_CALLBACK_ENGINE_STOPPED ||
        action == CARLA_CALLBACK_ENGINE_QUIT) {
        callback_fault(host, 5L);
        return;
    }
    if (InterlockedCompareExchange(&host->routing_established, 0, 0) != 0L &&
        (action == CARLA_CALLBACK_PATCHBAY_CLIENT_REMOVED ||
            action == CARLA_CALLBACK_PATCHBAY_PORT_REMOVED ||
            action == CARLA_CALLBACK_PATCHBAY_CONNECTION_REMOVED)) {
        callback_fault(host, 4L);
    }
    if (needs_name && !copy_bounded(
            copied_name, sizeof(copied_name), value_string,
            &copied_name_length
        )) {
        callback_fault(host, 1L);
        return;
    }
    if (action == CARLA_CALLBACK_PATCHBAY_PORT_ADDED ||
        action == CARLA_CALLBACK_PATCHBAY_PORT_CHANGED) {
        if (value1 < 0 || value2 < 0) {
            callback_fault(host, 1L);
            return;
        }
    }
    if (action == CARLA_CALLBACK_PATCHBAY_CLIENT_ADDED && value2 < -1) {
        callback_fault(host, 1L);
        return;
    }
    if (action == CARLA_CALLBACK_PATCHBAY_CONNECTION_ADDED) {
        uint32_t source_group;
        uint32_t source_port;
        uint32_t destination_group;
        uint32_t destination_port;

        if (!parse_connection(value_string, &source_group, &source_port,
                &destination_group, &destination_port)) {
            callback_fault(host, 1L);
            return;
        }
        if (!TryEnterCriticalSection(&host->callback_lock)) {
            callback_fault(host, 3L);
            return;
        }
        {
            carla_connection_record *slot = NULL;
            size_t index;

            for (index = 0U; index < CARLA_CONNECTION_CAPACITY; ++index) {
                carla_connection_record *record = &host->connections[index];

                if (record->used && record->connection_id == plugin_id) {
                    slot = record;
                    break;
                }
                if (!record->used && slot == NULL) {
                    slot = record;
                }
            }
            if (slot == NULL) {
                callback_fault(host, 2L);
            } else {
                slot->used = true;
                slot->connection_id = plugin_id;
                slot->source_group = source_group;
                slot->source_port = source_port;
                slot->destination_group = destination_group;
                slot->destination_port = destination_port;
            }
        }
        LeaveCriticalSection(&host->callback_lock);
        return;
    }
    if (action != CARLA_CALLBACK_PATCHBAY_CLIENT_ADDED &&
        action != CARLA_CALLBACK_PATCHBAY_CLIENT_REMOVED &&
        action != CARLA_CALLBACK_PATCHBAY_CLIENT_RENAMED &&
        action != CARLA_CALLBACK_PATCHBAY_PORT_ADDED &&
        action != CARLA_CALLBACK_PATCHBAY_PORT_REMOVED &&
        action != CARLA_CALLBACK_PATCHBAY_PORT_CHANGED &&
        action != CARLA_CALLBACK_PATCHBAY_CONNECTION_REMOVED) {
        return;
    }
    if (!TryEnterCriticalSection(&host->callback_lock)) {
        callback_fault(host, 3L);
        return;
    }
    if (action == CARLA_CALLBACK_PATCHBAY_CLIENT_ADDED) {
        carla_client_record *record = client_slot(host, plugin_id);

        if (record == NULL) {
            callback_fault(host, 2L);
        } else {
            record->used = true;
            record->group_id = plugin_id;
            record->plugin_id = value2;
            memcpy(record->name, copied_name, copied_name_length + 1U);
        }
    } else if (action == CARLA_CALLBACK_PATCHBAY_CLIENT_REMOVED) {
        size_t index;

        for (index = 0U; index < CARLA_CLIENT_CAPACITY; ++index) {
            if (host->clients[index].used &&
                host->clients[index].group_id == plugin_id) {
                host->clients[index].used = false;
            }
        }
        for (index = 0U; index < CARLA_PORT_CAPACITY; ++index) {
            if (host->ports[index].used &&
                host->ports[index].group_id == plugin_id) {
                host->ports[index].used = false;
            }
        }
        for (index = 0U; index < CARLA_CONNECTION_CAPACITY; ++index) {
            carla_connection_record *connection = &host->connections[index];

            if (connection->used &&
                (connection->source_group == plugin_id ||
                    connection->destination_group == plugin_id)) {
                connection->used = false;
            }
        }
    } else if (action == CARLA_CALLBACK_PATCHBAY_CLIENT_RENAMED) {
        carla_client_record *record = client_slot(host, plugin_id);

        if (record == NULL || !record->used) {
            callback_fault(host, 1L);
        } else {
            memcpy(record->name, copied_name, copied_name_length + 1U);
        }
    } else if (action == CARLA_CALLBACK_PATCHBAY_PORT_ADDED ||
        action == CARLA_CALLBACK_PATCHBAY_PORT_CHANGED) {
        carla_port_record *record = port_slot(
            host, plugin_id, (uint32_t)value1
        );

        if (record == NULL) {
            callback_fault(host, 2L);
        } else {
            record->used = true;
            record->group_id = plugin_id;
            record->port_id = (uint32_t)value1;
            record->hints = (uint32_t)value2;
            memcpy(record->name, copied_name, copied_name_length + 1U);
        }
    } else if (action == CARLA_CALLBACK_PATCHBAY_PORT_REMOVED) {
        if (value1 < 0) {
            callback_fault(host, 1L);
        } else {
            size_t index;

            for (index = 0U; index < CARLA_PORT_CAPACITY; ++index) {
                if (host->ports[index].used &&
                    host->ports[index].group_id == plugin_id &&
                    host->ports[index].port_id == (uint32_t)value1) {
                    host->ports[index].used = false;
                }
            }
            for (index = 0U; index < CARLA_CONNECTION_CAPACITY; ++index) {
                carla_connection_record *connection =
                    &host->connections[index];

                if (connection->used &&
                    ((connection->source_group == plugin_id &&
                        connection->source_port == (uint32_t)value1) ||
                        (connection->destination_group == plugin_id &&
                            connection->destination_port ==
                                (uint32_t)value1))) {
                    connection->used = false;
                }
            }
        }
    } else if (action == CARLA_CALLBACK_PATCHBAY_CONNECTION_REMOVED) {
        size_t index;

        for (index = 0U; index < CARLA_CONNECTION_CAPACITY; ++index) {
            if (host->connections[index].used &&
                host->connections[index].connection_id == plugin_id) {
                host->connections[index].used = false;
            }
        }
    }
    LeaveCriticalSection(&host->callback_lock);
}

static music_rig_result callback_status(music_rig_win32_carla_host *host)
{
    LONG fault = InterlockedCompareExchange(&host->callback_fault, 0, 0);

    if (fault == 0L) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (fault == 1L) {
        set_error(host, "Carla callback contained an invalid or oversized identifier");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (fault == 2L) {
        set_error(host, "Carla callback record capacity was exceeded");
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    if (fault == 4L) {
        set_error(host, "Carla removed a client, port, or MIDI connection after routing setup");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (fault == 5L) {
        set_error(host, "Carla engine stopped or quit unexpectedly");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    set_error(host, "Carla callback lock was contended; routing data was rejected");
    return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static music_rig_result inspect_patch(
    music_rig_win32_carla_host *host,
    const char *patch_name,
    uint32_t *plugin_id,
    uint32_t *reverb_parameter
)
{
    uint32_t count = host->api.get_current_plugin_count(host->carla_handle);
    uint32_t found_id = UINT32_MAX;
    uint32_t index;

    for (index = 0U; index < count; ++index) {
        const carla_plugin_info_2510 *info = host->api.get_plugin_info(
            host->carla_handle, index
        );

        if (info != NULL && info->name != NULL &&
            strcmp(info->name, patch_name) == 0) {
            if (found_id != UINT32_MAX) {
                set_error(host, "Carla returned duplicate exact plugin instance names");
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
            found_id = index;
            if (info->type != CARLA_PLUGIN_TYPE_SF2) {
                set_error(host, "Requested Carla plugin is not an SF2 instance");
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
    }
    if (found_id == UINT32_MAX) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (found_id > UINT16_MAX) {
        set_error(host, "Carla plugin id exceeds the portable host id range");
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    {
        uint32_t parameter_count = host->api.get_parameter_count(
            host->carla_handle, found_id
        );
        uint32_t matched_parameter = UINT32_MAX;

        for (index = 0U; index < parameter_count; ++index) {
            const carla_parameter_info_2510 *info =
                host->api.get_parameter_info(
                    host->carla_handle, found_id, index
                );

            if (info != NULL && info->name != NULL &&
                strcmp(info->name, "Reverb Level") == 0) {
                const carla_parameter_ranges_2510 *ranges =
                    host->api.get_parameter_ranges(
                        host->carla_handle, found_id, index
                    );

                if (matched_parameter != UINT32_MAX || ranges == NULL ||
                    ranges->minimum != 0.0f || ranges->maximum != 1.0f) {
                    set_error(host,
                        "Carla SF2 Reverb Level parameter has an unexpected range");
                    return MUSIC_RIG_RESULT_INVALID_DATA;
                }
                matched_parameter = index;
            }
        }
        if (matched_parameter != CARLA_PARAMETER_REVERB_LEVEL) {
            set_error(host,
                "Carla SF2 plugin does not expose Reverb Level at ABI parameter 3");
            return MUSIC_RIG_RESULT_UNSUPPORTED;
        }
        *plugin_id = found_id;
        *reverb_parameter = matched_parameter;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result find_group_for_plugin(
    music_rig_win32_carla_host *host,
    uint32_t plugin_id,
    uint32_t *group_id
)
{
    size_t index;
    size_t matches = 0U;

    EnterCriticalSection(&host->callback_lock);
    for (index = 0U; index < CARLA_CLIENT_CAPACITY; ++index) {
        const carla_client_record *record = &host->clients[index];

        if (record->used && record->plugin_id >= 0 &&
            (uint32_t)record->plugin_id == plugin_id) {
            *group_id = record->group_id;
            matches += 1U;
        }
    }
    LeaveCriticalSection(&host->callback_lock);
    if (matches == 0U) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (matches != 1U) {
        set_error(host, "Multiple Carla patchbay groups identify one plugin id");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result find_midi_source(
    music_rig_win32_carla_host *host,
    uint32_t *group_id,
    uint32_t *port_id
)
{
    uint32_t source_group = UINT32_MAX;
    size_t client_matches = 0U;
    size_t port_matches = 0U;
    size_t index;

    EnterCriticalSection(&host->callback_lock);
    for (index = 0U; index < CARLA_CLIENT_CAPACITY; ++index) {
        const carla_client_record *record = &host->clients[index];

        if (record->used && record->plugin_id == -1 &&
            strcmp(record->name, host->midi_client_name) == 0) {
            source_group = record->group_id;
            client_matches += 1U;
        }
    }
    if (client_matches == 1U) {
        for (index = 0U; index < CARLA_PORT_CAPACITY; ++index) {
            const carla_port_record *record = &host->ports[index];

            if (record->used && record->group_id == source_group &&
                strcmp(record->name, host->midi_port_name) == 0 &&
                (record->hints & CARLA_PATCHBAY_PORT_TYPE_MIDI) != 0U &&
                (record->hints & CARLA_PATCHBAY_PORT_IS_INPUT) == 0U) {
                *port_id = record->port_id;
                port_matches += 1U;
            }
        }
    }
    LeaveCriticalSection(&host->callback_lock);
    if (client_matches > 1U || port_matches > 1U) {
        set_error(host, "Exact external MIDI source names are ambiguous");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (client_matches == 0U || port_matches == 0U) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    *group_id = source_group;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result find_midi_destination(
    music_rig_win32_carla_host *host,
    uint32_t plugin_id,
    uint32_t *group_id,
    uint32_t *port_id
)
{
    music_rig_result result = find_group_for_plugin(host, plugin_id, group_id);
    size_t matches = 0U;
    size_t index;

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    EnterCriticalSection(&host->callback_lock);
    for (index = 0U; index < CARLA_PORT_CAPACITY; ++index) {
        const carla_port_record *record = &host->ports[index];

        if (record->used && record->group_id == *group_id &&
            (record->hints & CARLA_PATCHBAY_PORT_TYPE_MIDI) != 0U &&
            (record->hints & CARLA_PATCHBAY_PORT_IS_INPUT) != 0U) {
            *port_id = record->port_id;
            matches += 1U;
        }
    }
    LeaveCriticalSection(&host->callback_lock);
    if (matches == 0U) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (matches != 1U) {
        set_error(host, "SF2 patch has multiple MIDI event-input ports");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result resolve_patchbay_route(
    music_rig_win32_carla_host *host,
    uint32_t plugin_id,
    uint32_t *source_group,
    uint32_t *source_port,
    uint32_t *destination_group,
    uint32_t *destination_port
)
{
    music_rig_result result = find_midi_source(
        host, source_group, source_port
    );

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    return find_midi_destination(
        host, plugin_id, destination_group, destination_port
    );
}

static music_rig_result check_enumeration_ready(
    music_rig_win32_carla_host *host,
    const char *const patch_names[],
    uint16_t patch_count
)
{
    uint16_t index;

    for (index = 0U; index < patch_count; ++index) {
        uint32_t plugin_id;
        uint32_t reverb_parameter;
        uint32_t source_group;
        uint32_t source_port;
        uint32_t destination_group;
        uint32_t destination_port;
        music_rig_result result = inspect_patch(
            host, patch_names[index], &plugin_id, &reverb_parameter
        );

        (void)reverb_parameter;
        if (result != MUSIC_RIG_RESULT_OK) {
            return result;
        }
        result = resolve_patchbay_route(
            host,
            plugin_id,
            &source_group,
            &source_port,
            &destination_group,
            &destination_port
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            return result;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result dispose_native(
    music_rig_win32_carla_host *host
)
{
    music_rig_result result = MUSIC_RIG_RESULT_OK;

    if (host->carla_handle != NULL && host->callback_registered) {
        host->api.set_engine_callback(host->carla_handle, NULL, NULL);
        host->callback_registered = false;
    }
    if (host->carla_handle != NULL && host->api.engine_close != NULL) {
        if (!host->api.engine_close(host->carla_handle)) {
            if (host->error[0] == '\0') {
                capture_carla_error(host, "Carla engine close failed");
            }
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        host->carla_handle = NULL;
    }
    host->engine_started = false;
    if (host->callback_lock_initialized) {
        DeleteCriticalSection(&host->callback_lock);
        host->callback_lock_initialized = false;
    }
    if (host->library != NULL) {
        if (!FreeLibrary(host->library)) {
            if (result == MUSIC_RIG_RESULT_OK) {
                set_error(host, "FreeLibrary failed for the Carla DLL");
            }
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        host->library = NULL;
    }
    memset(&host->api, 0, sizeof(host->api));
    host->project_open = false;
    return result;
}

static music_rig_result open_project(
    void *context,
    const char *project_path,
    const char *driver,
    const char *client_name
)
{
    music_rig_win32_carla_host *host = context;
    wchar_t *wide_dll_path = NULL;
    wchar_t *absolute_dll_path = NULL;
    int wide_length;
    DWORD absolute_length;
    DWORD absolute_written;
    uint16_t index;
    music_rig_result result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;

    host->owner_thread = GetCurrentThreadId();
    wide_length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, host->dll_path, -1, NULL, 0
    );
    if (wide_length <= 0 || (size_t)wide_length > 32768U) {
        set_error(host, "Carla DLL path is not valid UTF-8");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    wide_dll_path = malloc((size_t)wide_length * sizeof(*wide_dll_path));
    if (wide_dll_path == NULL) {
        set_error(host, "Could not allocate the Carla DLL path buffer");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, host->dll_path,
            -1, wide_dll_path, wide_length) == 0) {
        set_error(host, "Could not convert the Carla DLL path to UTF-16");
        result = MUSIC_RIG_RESULT_INVALID_DATA;
        goto failed;
    }
    absolute_length = GetFullPathNameW(wide_dll_path, 0U, NULL, NULL);
    if (absolute_length == 0U || absolute_length > 32768U) {
        set_error(host, "Could not resolve the Carla DLL path");
        goto failed;
    }
    absolute_dll_path = malloc(
        (size_t)absolute_length * sizeof(*absolute_dll_path)
    );
    if (absolute_dll_path == NULL) {
        set_error(host, "Could not allocate the absolute Carla DLL path");
        goto failed;
    }
    absolute_written = GetFullPathNameW(
        wide_dll_path, absolute_length, absolute_dll_path, NULL
    );
    if (absolute_written == 0U || absolute_written >= absolute_length) {
        set_error(host, "Could not resolve the Carla DLL path");
        goto failed;
    }
    host->library = LoadLibraryExW(
        absolute_dll_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH
    );
    if (host->library == NULL) {
        set_error(host, "Could not load the requested Carla DLL");
        goto failed;
    }
    if (!load_api(host)) {
        set_error(host, "Carla DLL is missing a required 2.5.10 Host API export");
        result = MUSIC_RIG_RESULT_UNSUPPORTED;
        goto failed;
    }
    if (!InitializeCriticalSectionAndSpinCount(&host->callback_lock, 0U)) {
        set_error(host, "Could not initialize the Carla callback lock");
        goto failed;
    }
    host->callback_lock_initialized = true;
    host->carla_handle = host->api.standalone_host_init();
    if (host->carla_handle == NULL) {
        set_error(host, "carla_standalone_host_init returned NULL");
        goto failed;
    }
    host->api.set_engine_callback(
        host->carla_handle, carla_callback, host
    );
    host->callback_registered = true;
    host->api.set_engine_option(
        host->carla_handle,
        CARLA_ENGINE_OPTION_PROCESS_MODE,
        CARLA_ENGINE_PROCESS_MODE_PATCHBAY,
        NULL
    );
    if (!host->api.engine_init(host->carla_handle, driver, client_name)) {
        capture_carla_error(host, "carla_engine_init failed");
        goto failed;
    }
    host->engine_started = true;
    if (!host->api.load_project(host->carla_handle, project_path)) {
        capture_carla_error(host, "carla_load_project failed");
        goto failed;
    }
    if (!host->api.patchbay_refresh(host->carla_handle, true) ||
        !host->api.patchbay_refresh(host->carla_handle, false)) {
        capture_carla_error(host, "Carla patchbay refresh failed");
        goto failed;
    }
    for (index = 0U; index < CARLA_ENUMERATION_ATTEMPTS; ++index) {
        result = callback_status(host);
        if (result != MUSIC_RIG_RESULT_OK) {
            goto failed;
        }
        host->api.engine_idle(host->carla_handle);
        result = check_enumeration_ready(
            host, host->opening_patch_names, host->opening_patch_count
        );
        if (result == MUSIC_RIG_RESULT_OK) {
            host->project_open = true;
            free(absolute_dll_path);
            free(wide_dll_path);
            return MUSIC_RIG_RESULT_OK;
        }
        if (result != MUSIC_RIG_RESULT_NOT_FOUND) {
            goto failed;
        }
        Sleep(10U);
    }
    set_error(host,
        "Carla did not report the exact requested MIDI and SF2 patchbay ports");
    result = MUSIC_RIG_RESULT_NOT_FOUND;

failed:
    free(absolute_dll_path);
    free(wide_dll_path);
    (void)dispose_native(host);
    return result;
}

static music_rig_result close_project(void *context)
{
    return dispose_native(context);
}

static void idle(void *context)
{
    music_rig_win32_carla_host *host = context;

    host->api.engine_idle(host->carla_handle);
}

static carla_patch_record *patch_by_id(
    music_rig_win32_carla_host *host, uint16_t plugin_id
)
{
    size_t index;

    for (index = 0U; index < host->patch_count; ++index) {
        if (host->patches[index].found &&
            host->patches[index].plugin_id == plugin_id) {
            return &host->patches[index];
        }
    }
    return NULL;
}

static music_rig_result find_patch(
    void *context, const char *patch_name, uint16_t *backend_patch_id
)
{
    music_rig_win32_carla_host *host = context;
    uint32_t plugin_id;
    uint32_t reverb_parameter;
    size_t index;
    music_rig_result result;

    if (patch_name == NULL || backend_patch_id == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = callback_status(host);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = inspect_patch(
        host, patch_name, &plugin_id, &reverb_parameter
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    if (patch_by_id(host, (uint16_t)plugin_id) != NULL) {
        set_error(host, "The same Carla SF2 instance was requested more than once");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    for (index = 0U; index < MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY; ++index) {
        carla_patch_record *record = &host->patches[index];

        if (!record->found) {
            record->found = true;
            record->plugin_id = plugin_id;
            record->reverb_parameter = reverb_parameter;
            record->fader = 0U;
            host->patch_count += 1U;
            *backend_patch_id = (uint16_t)plugin_id;
            return MUSIC_RIG_RESULT_OK;
        }
    }
    return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
}

static music_rig_result connect_midi_input(
    void *context, uint16_t backend_patch_id
)
{
    music_rig_win32_carla_host *host = context;
    carla_patch_record *patch = patch_by_id(host, backend_patch_id);
    uint32_t source_group;
    uint32_t source_port;
    uint32_t destination_group;
    uint32_t destination_port;
    size_t existing = 0U;
    size_t index;
    music_rig_result result;
    const carla_plugin_info_2510 *info;

    if (patch == NULL || patch->midi_connected) {
        set_error(host, "Duplicate or unknown Carla MIDI patch connection request");
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    (void)InterlockedExchange(&host->routing_established, 1L);
    result = callback_status(host);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = resolve_patchbay_route(
        host,
        patch->plugin_id,
        &source_group,
        &source_port,
        &destination_group,
        &destination_port
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    host->api.set_option(
        host->carla_handle,
        patch->plugin_id,
        CARLA_OPTION_SEND_CONTROL_CHANGES,
        true
    );
    host->api.set_option(
        host->carla_handle,
        patch->plugin_id,
        CARLA_OPTION_SEND_PITCH_BEND,
        true
    );
    host->api.set_option(
        host->carla_handle,
        patch->plugin_id,
        CARLA_OPTION_SKIP_NOTES,
        false
    );
    info = host->api.get_plugin_info(host->carla_handle, patch->plugin_id);
    if (info == NULL ||
        (info->options_enabled & CARLA_OPTION_SKIP_NOTES) != 0U ||
        (info->options_enabled & CARLA_OPTION_SEND_CONTROL_CHANGES) == 0U ||
        (info->options_enabled & CARLA_OPTION_SEND_PITCH_BEND) == 0U) {
        set_error(host,
            "Carla did not enable loopback control/pitch or disable note filtering");
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    EnterCriticalSection(&host->callback_lock);
    for (index = 0U; index < CARLA_CONNECTION_CAPACITY; ++index) {
        const carla_connection_record *connection =
            &host->connections[index];

        if (connection->used && connection->source_group == source_group &&
            connection->source_port == source_port &&
            connection->destination_group == destination_group &&
            connection->destination_port == destination_port) {
            existing += 1U;
        }
    }
    LeaveCriticalSection(&host->callback_lock);
    if (existing > 1U) {
        set_error(host, "Carla patchbay already contains duplicate MIDI routes");
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (existing == 0U && !host->api.patchbay_connect(
            host->carla_handle,
            true,
            source_group,
            source_port,
            destination_group,
            destination_port
        )) {
        capture_carla_error(host, "Carla failed to connect the raw MIDI source");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    patch->midi_connected = true;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result activate_patch(void *context, uint16_t plugin_id)
{
    music_rig_win32_carla_host *host = context;

    if (patch_by_id(host, plugin_id) == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    host->api.set_active(host->carla_handle, plugin_id, true);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result deactivate_patch(void *context, uint16_t plugin_id)
{
    music_rig_win32_carla_host *host = context;

    if (patch_by_id(host, plugin_id) == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    host->api.set_active(host->carla_handle, plugin_id, false);
    return MUSIC_RIG_RESULT_OK;
}

static void apply_patch_gain(
    music_rig_win32_carla_host *host, carla_patch_record *patch
)
{
    float gain = (float)patch->fader / 127.0f;

    if (host->master_muted) {
        gain = 0.0f;
    } else {
        gain *= (float)host->master_volume / 127.0f;
    }
    host->api.set_volume(host->carla_handle, patch->plugin_id, gain);
}

static music_rig_result set_patch_volume(
    void *context, uint16_t plugin_id, uint8_t value
)
{
    music_rig_win32_carla_host *host = context;
    carla_patch_record *patch = patch_by_id(host, plugin_id);

    if (patch == NULL || value > 127U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    patch->fader = value;
    apply_patch_gain(host, patch);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result set_patch_reverb(
    void *context, uint16_t plugin_id, uint8_t value
)
{
    music_rig_win32_carla_host *host = context;
    carla_patch_record *patch = patch_by_id(host, plugin_id);

    if (patch == NULL || value > 127U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    host->api.set_parameter_value(
        host->carla_handle,
        patch->plugin_id,
        patch->reverb_parameter,
        (float)value / 127.0f
    );
    return MUSIC_RIG_RESULT_OK;
}

static bool is_first_active_route(
    const music_rig_win32_carla_host *host, uint16_t plugin_id
);

static music_rig_result output_raw_midi(
    music_rig_win32_carla_host *host,
    const uint8_t message[3]
);

static music_rig_result send_note(
    void *context,
    uint16_t plugin_id,
    bool note_on,
    uint8_t note,
    uint8_t velocity
)
{
    music_rig_win32_carla_host *host = context;
    uint8_t message[3];

    if (patch_by_id(host, plugin_id) == NULL || note > 127U ||
        velocity > 127U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!is_first_active_route(host, plugin_id)) {
        return MUSIC_RIG_RESULT_OK;
    }
    message[0] = note_on ? UINT8_C(0x90) : UINT8_C(0x80);
    message[1] = note;
    message[2] = note_on ? velocity : 0U;
    return output_raw_midi(host, message);
}

static bool is_first_active_route(
    const music_rig_win32_carla_host *host, uint16_t plugin_id
)
{
    const music_rig_arturia_shared_sf2_plan *plan =
        &host->portable.session.plan;
    uint16_t patch_index;

    if (plan->route_count == 0U) {
        return false;
    }
    patch_index = plan->routes[0].patch_index;
    return patch_index < host->portable.patch_count &&
        host->portable.backend_patch_ids[patch_index] == plugin_id;
}

static music_rig_result output_raw_midi(
    music_rig_win32_carla_host *host,
    const uint8_t message[3]
)
{
    music_rig_result result = host->raw_midi_output(
        host->raw_midi_output_context, message, 3U
    );

    if (result < MUSIC_RIG_RESULT_OK ||
        result > MUSIC_RIG_RESULT_BUFFER_TOO_SMALL) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        set_error(host, "Filtered raw MIDI output callback failed");
    }
    return result;
}

static music_rig_result send_controller(
    void *context,
    uint16_t plugin_id,
    uint8_t controller,
    uint8_t value
)
{
    music_rig_win32_carla_host *host = context;
    carla_patch_record *patch = patch_by_id(host, plugin_id);
    uint8_t message[3];

    if (patch == NULL || !patch->midi_connected) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (!is_first_active_route(host, plugin_id)) {
        return MUSIC_RIG_RESULT_OK;
    }
    message[0] = UINT8_C(0xb0);
    message[1] = controller;
    message[2] = value;
    return output_raw_midi(host, message);
}

static music_rig_result send_pitch_bend(
    void *context, uint16_t plugin_id, uint16_t value
)
{
    music_rig_win32_carla_host *host = context;
    carla_patch_record *patch = patch_by_id(host, plugin_id);
    uint8_t message[3];

    if (patch == NULL || !patch->midi_connected || value > UINT16_C(16383)) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (!is_first_active_route(host, plugin_id)) {
        return MUSIC_RIG_RESULT_OK;
    }
    message[0] = UINT8_C(0xe0);
    message[1] = (uint8_t)(value & UINT16_C(0x7f));
    message[2] = (uint8_t)((value >> 7U) & UINT16_C(0x7f));
    return output_raw_midi(host, message);
}

static music_rig_result set_master_volume(void *context, uint8_t value)
{
    music_rig_win32_carla_host *host = context;
    size_t index;

    if (value > 127U || host->patch_count == 0U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    host->master_volume = value;
    for (index = 0U; index < MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY; ++index) {
        if (host->patches[index].found) {
            apply_patch_gain(host, &host->patches[index]);
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result set_master_mute(void *context, bool muted)
{
    music_rig_win32_carla_host *host = context;
    size_t index;

    if (host->patch_count == 0U) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    host->master_muted = muted;
    for (index = 0U; index < MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY; ++index) {
        if (host->patches[index].found) {
            apply_patch_gain(host, &host->patches[index]);
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result validate_options(
    const music_rig_win32_carla_host_options *options
)
{
    char checked[CARLA_NAME_CAPACITY];
    size_t index;

    if (options == NULL || options->patch_names == NULL ||
        options->patch_count == 0U ||
        options->patch_count > MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY ||
        options->patch_indexes == NULL || options->catalog_patch_count == 0U ||
        options->initial_master_volume > 127U ||
        options->raw_midi_output == NULL ||
        !copy_bounded(checked, sizeof(checked), options->midi_client_name, NULL) ||
        !copy_bounded(checked, sizeof(checked), options->midi_port_name, NULL)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!valid_text(options->carla_dll_path, 32768U) ||
        !valid_text(options->project_path, 32768U) ||
        !valid_text(options->driver_name, 32768U) ||
        !valid_text(options->client_name, 32768U)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < options->patch_count; ++index) {
        if (!copy_bounded(
                checked, sizeof(checked), options->patch_names[index], NULL
            )) {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static const music_rig_arturia_carla_ops portable_ops = {
    open_project,
    close_project,
    idle,
    find_patch,
    connect_midi_input,
    activate_patch,
    deactivate_patch,
    set_patch_volume,
    set_patch_reverb,
    send_note,
    send_controller,
    send_pitch_bend,
    set_master_volume,
    set_master_mute
};

music_rig_result music_rig_win32_carla_host_open(
    music_rig_win32_carla_host **host_output,
    const music_rig_win32_carla_host_options *options,
    char *error_message,
    size_t error_capacity
)
{
    music_rig_win32_carla_host *host;
    music_rig_result result;

    if (host_output == NULL) {
        copy_error_output(error_message, error_capacity,
            "Host output pointer is required");
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *host_output = NULL;
    copy_error_output(error_message, error_capacity, "");
    result = validate_options(options);
    if (result != MUSIC_RIG_RESULT_OK) {
        copy_error_output(error_message, error_capacity,
            "Invalid Carla host options or identifier exceeds its limit");
        return result;
    }
    host = calloc(1U, sizeof(*host));
    if (host == NULL) {
        copy_error_output(error_message, error_capacity,
            "Could not allocate the Carla host state");
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    atomic_init(&host->midi_queue_head, 0U);
    atomic_init(&host->midi_queue_tail, 0U);
    atomic_init(&host->midi_queue_overflows, UINT32_C(0));
    atomic_init(&host->midi_queue_accepting, false);
    if (!atomic_is_lock_free(&host->midi_queue_head) ||
        !atomic_is_lock_free(&host->midi_queue_tail) ||
        !atomic_is_lock_free(&host->midi_queue_overflows) ||
        !atomic_is_lock_free(&host->midi_queue_accepting)) {
        copy_error_output(error_message, error_capacity,
            "Carla MIDI queue atomics are not lock-free");
        free(host);
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    if (!copy_bounded(host->dll_path, sizeof(host->dll_path),
            options->carla_dll_path, NULL) ||
        !copy_bounded(host->midi_client_name, sizeof(host->midi_client_name),
            options->midi_client_name, NULL) ||
        !copy_bounded(host->midi_port_name, sizeof(host->midi_port_name),
            options->midi_port_name, NULL)) {
        set_error(host, "Carla path or MIDI identifier exceeds its storage limit");
        result = MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        goto failed;
    }
    host->opening_patch_names = options->patch_names;
    host->opening_patch_count = options->patch_count;
    host->raw_midi_output = options->raw_midi_output;
    host->raw_midi_output_context = options->raw_midi_output_context;
    host->master_volume = options->initial_master_volume;
    result = music_rig_arturia_carla_host_open(
        &host->portable,
        &portable_ops,
        host,
        options->project_path,
        options->driver_name,
        options->client_name,
        options->patch_names,
        options->patch_count,
        options->patch_indexes,
        options->catalog_patch_count,
        options->active_layers,
        options->active_layer_count,
        options->initial_master_volume
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)dispose_native(host);
        if (host->error[0] == '\0') {
            set_error(host, "Carla host initialization failed");
        }
        goto failed;
    }
    result = callback_status(host);
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)music_rig_arturia_carla_host_close(&host->portable);
        (void)dispose_native(host);
        goto failed;
    }
    atomic_store_explicit(
        &host->midi_queue_accepting, true, memory_order_release
    );
    *host_output = host;
    return MUSIC_RIG_RESULT_OK;

failed:
    copy_error_output(error_message, error_capacity, host->error);
    free(host);
    return result;
}

static bool host_is_valid(const music_rig_win32_carla_host *host)
{
    return host != NULL && host->portable.project_open &&
        host->owner_thread == GetCurrentThreadId();
}

music_rig_result music_rig_win32_carla_host_idle(
    music_rig_win32_carla_host *host
)
{
    music_rig_result result;

    if (!host_is_valid(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = music_rig_arturia_carla_host_idle(&host->portable);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    return callback_status(host);
}

music_rig_result music_rig_win32_carla_host_select_profile(
    music_rig_win32_carla_host *host,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
)
{
    music_rig_result result;

    if (!host_is_valid(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = callback_status(host);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = music_rig_arturia_carla_host_select_profile(
        &host->portable,
        patch_indexes,
        catalog_patch_count,
        active_layers,
        active_layer_count
    );
    return result == MUSIC_RIG_RESULT_OK ? callback_status(host) : result;
}

music_rig_result music_rig_win32_carla_host_process_midi(
    music_rig_win32_carla_host *host,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_result result;

    if (!host_is_valid(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = callback_status(host);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = music_rig_arturia_carla_host_process_midi(
        &host->portable, message, message_size
    );
    return result == MUSIC_RIG_RESULT_OK ? callback_status(host) : result;
}

music_rig_result music_rig_win32_carla_host_enqueue_midi(
    music_rig_win32_carla_host *host,
    const uint8_t *message,
    size_t message_size
)
{
    uint8_t command;
    size_t head;
    size_t tail;
    carla_midi_message *queued;

    if (host == NULL || message == NULL || message_size == 0U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (message[0] < UINT8_C(0x80) || message[0] >= UINT8_C(0xf0) ||
        (message[0] & UINT8_C(0x0f)) != 0U) {
        return MUSIC_RIG_RESULT_OK;
    }
    command = message[0] & UINT8_C(0xf0);
    if (command != UINT8_C(0x80) && command != UINT8_C(0x90) &&
        command != UINT8_C(0xb0) && command != UINT8_C(0xe0)) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (message_size != 3U) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (message[1] > UINT8_C(127) || message[2] > UINT8_C(127)) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (!atomic_load_explicit(
            &host->midi_queue_accepting, memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    head = atomic_load_explicit(&host->midi_queue_head, memory_order_relaxed);
    tail = atomic_load_explicit(&host->midi_queue_tail, memory_order_acquire);
    if (head - tail >= MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY) {
        (void)atomic_fetch_add_explicit(
            &host->midi_queue_overflows, UINT32_C(1), memory_order_relaxed
        );
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    queued = &host->midi_queue[head & CARLA_MIDI_QUEUE_MASK];
    memset(queued, 0, sizeof(*queued));
    queued->kind = CARLA_MIDI_QUEUE_EVENT_MESSAGE;
    queued->size = (uint8_t)message_size;
    memcpy(queued->bytes, message, message_size);
    atomic_store_explicit(&host->midi_queue_head, head + 1U,
        memory_order_release);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_win32_carla_host_enqueue_profile(
    music_rig_win32_carla_host *host,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
)
{
    music_rig_arturia_shared_sf2_plan plan;
    size_t head;
    size_t tail;
    carla_midi_message *queued;
    music_rig_result result;

    if (host == NULL || patch_indexes == NULL || active_layers == NULL ||
        active_layer_count > UINT8_MAX ||
        catalog_patch_count != host->portable.patch_count) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!atomic_load_explicit(
            &host->midi_queue_accepting, memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = music_rig_arturia_shared_sf2_plan_build(
        patch_indexes, catalog_patch_count, active_layers, active_layer_count,
        &plan
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    head = atomic_load_explicit(&host->midi_queue_head, memory_order_relaxed);
    tail = atomic_load_explicit(&host->midi_queue_tail, memory_order_acquire);
    if (head - tail >= MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY) {
        (void)atomic_fetch_add_explicit(
            &host->midi_queue_overflows, UINT32_C(1), memory_order_relaxed
        );
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    queued = &host->midi_queue[head & CARLA_MIDI_QUEUE_MASK];
    memset(queued, 0, sizeof(*queued));
    queued->kind = CARLA_MIDI_QUEUE_EVENT_PROFILE;
    queued->active_layer_count = (uint8_t)active_layer_count;
    queued->catalog_patch_count = catalog_patch_count;
    memcpy(queued->patch_indexes, patch_indexes, sizeof(queued->patch_indexes));
    memcpy(queued->active_layers, active_layers, active_layer_count);
    atomic_store_explicit(&host->midi_queue_head, head + 1U,
        memory_order_release);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_win32_carla_host_drain_midi(
    music_rig_win32_carla_host *host,
    size_t *message_count
)
{
    size_t count = 0U;
    music_rig_result first_error = MUSIC_RIG_RESULT_OK;

    if (message_count == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *message_count = 0U;
    if (!host_is_valid(host)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    for (;;) {
        size_t tail = atomic_load_explicit(
            &host->midi_queue_tail, memory_order_relaxed
        );
        size_t head = atomic_load_explicit(
            &host->midi_queue_head, memory_order_acquire
        );
        carla_midi_message message;
        music_rig_result result;

        if (tail == head) {
            break;
        }
        message = host->midi_queue[tail & CARLA_MIDI_QUEUE_MASK];
        atomic_store_explicit(&host->midi_queue_tail, tail + 1U,
            memory_order_release);
        if (message.kind == CARLA_MIDI_QUEUE_EVENT_MESSAGE) {
            result = music_rig_win32_carla_host_process_midi(
                host, message.bytes, (size_t)message.size
            );
        } else if (message.kind == CARLA_MIDI_QUEUE_EVENT_PROFILE) {
            result = music_rig_win32_carla_host_select_profile(
                host, message.patch_indexes, message.catalog_patch_count,
                message.active_layers, message.active_layer_count
            );
        } else {
            result = MUSIC_RIG_RESULT_INVALID_DATA;
        }
        if (result != MUSIC_RIG_RESULT_OK && first_error == MUSIC_RIG_RESULT_OK) {
            first_error = result;
        }
        count += 1U;
    }
    *message_count = count;
    return first_error;
}

uint64_t music_rig_win32_carla_host_midi_queue_overflows(
    const music_rig_win32_carla_host *host
)
{
    return host == NULL ? UINT64_C(0) : (uint64_t)atomic_load_explicit(
        &host->midi_queue_overflows, memory_order_relaxed
    );
}

const char *music_rig_win32_carla_host_last_error(
    const music_rig_win32_carla_host *host
)
{
    return host == NULL ? NULL : host->error;
}

music_rig_result music_rig_win32_carla_host_close(
    music_rig_win32_carla_host **host_pointer
)
{
    music_rig_win32_carla_host *host;
    music_rig_result result;

    if (host_pointer == NULL || *host_pointer == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    host = *host_pointer;
    if (host->owner_thread != GetCurrentThreadId()) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    atomic_store_explicit(
        &host->midi_queue_accepting, false, memory_order_release
    );
    result = music_rig_arturia_carla_host_close(&host->portable);
    if (host->library != NULL || host->carla_handle != NULL ||
        host->callback_lock_initialized) {
        music_rig_result cleanup = dispose_native(host);

        if (result == MUSIC_RIG_RESULT_OK) {
            result = cleanup;
        }
    }
    free(host);
    *host_pointer = NULL;
    return result;
}
