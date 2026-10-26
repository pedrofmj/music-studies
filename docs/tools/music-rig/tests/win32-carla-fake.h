#ifndef MUSIC_RIG_WIN32_CARLA_FAKE_H
#define MUSIC_RIG_WIN32_CARLA_FAKE_H

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT 4U
#define MUSIC_RIG_WIN32_CARLA_FAKE_REQUIRED_OPTIONS UINT32_C(0x090)

#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_HOST_INIT UINT32_C(0x001)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_CALLBACK UINT32_C(0x002)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_OPTION UINT32_C(0x004)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_INIT UINT32_C(0x008)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_PROJECT UINT32_C(0x010)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_EXTERNAL_REFRESH UINT32_C(0x020)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_INTERNAL_REFRESH UINT32_C(0x040)
#define MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_CLOSE UINT32_C(0x080)

typedef struct music_rig_win32_carla_fake_status {
    uint32_t struct_size;
    uint32_t faults;
    uint32_t lifecycle;
    uint32_t standalone_init_calls;
    uint32_t engine_option_calls;
    uint32_t engine_init_calls;
    uint32_t project_load_calls;
    uint32_t external_refresh_calls;
    uint32_t internal_refresh_calls;
    uint32_t last_error_calls;
    uint32_t option_masks[MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT];
    uint32_t connection_mask;
    uint32_t active_mask;
    uint32_t deactivated_mask;
    uint32_t connection_calls;
    uint32_t client_callbacks;
    uint32_t port_callbacks;
    uint32_t activation_calls;
    uint32_t deactivation_calls;
    uint32_t gain_calls;
    uint32_t reverb_calls;
    uint32_t note_calls;
    uint32_t note_on_calls;
    uint32_t note_off_calls;
    uint32_t idle_calls;
    uint32_t engine_close_calls;
    uint32_t plugin_info_calls;
    uint32_t parameter_info_calls;
    uint32_t parameter_range_calls;
    uint32_t wrong_thread_api_calls;
} music_rig_win32_carla_fake_status;

typedef int (__cdecl *music_rig_win32_carla_fake_query_fn)(
    music_rig_win32_carla_fake_status *status,
    size_t status_capacity
);

#endif
