#ifndef MUSIC_RIG_ARTURIA_CARLA_HOST_H
#define MUSIC_RIG_ARTURIA_CARLA_HOST_H

#include "music_rig/arturia_session.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY ((size_t)256)

typedef struct music_rig_arturia_carla_ops {
    music_rig_result (*open_project)(
        void *context,
        const char *project_path,
        const char *driver,
        const char *client_name
    );
    music_rig_result (*close_project)(void *context);
    void (*idle)(void *context);
    music_rig_result (*find_patch)(
        void *context,
        const char *patch_name,
        uint16_t *backend_patch_id
    );
    /* Routes raw controls/pitch while suppressing raw notes sent via send_note. */
    music_rig_result (*connect_midi_input)(
        void *context,
        uint16_t backend_patch_id
    );
    music_rig_result (*activate_patch)(void *context, uint16_t backend_patch_id);
    music_rig_result (*deactivate_patch)(void *context, uint16_t backend_patch_id);
    music_rig_result (*set_patch_volume)(
        void *context,
        uint16_t backend_patch_id,
        uint8_t value
    );
    music_rig_result (*set_patch_reverb)(
        void *context,
        uint16_t backend_patch_id,
        uint8_t value
    );
    music_rig_result (*send_note)(
        void *context,
        uint16_t backend_patch_id,
        bool note_on,
        uint8_t note,
        uint8_t velocity
    );
    music_rig_result (*send_controller)(
        void *context,
        uint16_t backend_patch_id,
        uint8_t controller,
        uint8_t value
    );
    music_rig_result (*send_pitch_bend)(
        void *context,
        uint16_t backend_patch_id,
        uint16_t value
    );
    music_rig_result (*set_master_volume)(void *context, uint8_t value);
    music_rig_result (*set_master_mute)(void *context, bool muted);
} music_rig_arturia_carla_ops;

typedef struct music_rig_arturia_carla_host {
    music_rig_arturia_carla_ops ops;
    void *ops_context;
    uint16_t backend_patch_ids[MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY];
    uint16_t patch_count;
    bool project_open;
    music_rig_arturia_session session;
} music_rig_arturia_carla_host;

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
);

music_rig_result music_rig_arturia_carla_host_select_profile(
    music_rig_arturia_carla_host *host,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
);

music_rig_result music_rig_arturia_carla_host_process_midi(
    music_rig_arturia_carla_host *host,
    const uint8_t *message,
    size_t message_size
);

/* Call regularly on the same serialized host thread as all other Carla ops. */
music_rig_result music_rig_arturia_carla_host_idle(
    music_rig_arturia_carla_host *host
);

music_rig_result music_rig_arturia_carla_host_close(
    music_rig_arturia_carla_host *host
);

#endif
