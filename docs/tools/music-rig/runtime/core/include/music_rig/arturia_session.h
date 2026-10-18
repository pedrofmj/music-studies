#ifndef MUSIC_RIG_ARTURIA_SESSION_H
#define MUSIC_RIG_ARTURIA_SESSION_H

#include "music_rig/arturia_control_state.h"
#include "music_rig/arturia_shared_sf2.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_ARTURIA_MASTER_VOLUME_CC UINT8_C(114)
#define MUSIC_RIG_ARTURIA_MASTER_MUTE_CC UINT8_C(115)
#define MUSIC_RIG_ARTURIA_MODULATION_CC UINT8_C(1)
#define MUSIC_RIG_ARTURIA_SUSTAIN_CC UINT8_C(64)

typedef struct music_rig_arturia_engine_ops {
    music_rig_result (*activate_patch)(void *context, uint16_t patch_index);
    music_rig_result (*deactivate_patch)(void *context, uint16_t patch_index);
    music_rig_result (*set_patch_volume)(
        void *context,
        uint16_t patch_index,
        uint8_t value
    );
    music_rig_result (*set_patch_reverb)(
        void *context,
        uint16_t patch_index,
        uint8_t value
    );
    music_rig_result (*send_note)(
        void *context,
        uint16_t patch_index,
        bool note_on,
        uint8_t note,
        uint8_t velocity
    );
    music_rig_result (*send_controller)(
        void *context,
        uint16_t patch_index,
        uint8_t controller,
        uint8_t value
    );
    music_rig_result (*send_pitch_bend)(
        void *context,
        uint16_t patch_index,
        uint16_t value
    );
    music_rig_result (*set_master_volume)(void *context, uint8_t value);
    music_rig_result (*set_master_mute)(void *context, bool muted);
} music_rig_arturia_engine_ops;

/*
 * Caller-owned session state. All operations, including MIDI processing, use
 * fixed storage. Engine callbacks are invoked synchronously by the caller.
 */
typedef struct music_rig_arturia_session {
    music_rig_arturia_engine_ops engine;
    void *engine_context;
    music_rig_arturia_control_state control_state;
    music_rig_arturia_shared_sf2_plan plan;
    uint8_t master_volume;
    bool master_muted;
    bool mute_button_down;
    bool initialized;
    bool rollback_incomplete;
} music_rig_arturia_session;

/*
 * Initializes the engine master controls and selects the initial profile.
 * patch_indexes is indexed by Arturia layer (layer 1 is element 0).
 */
music_rig_result music_rig_arturia_session_init(
    music_rig_arturia_session *session,
    const music_rig_arturia_engine_ops *engine,
    void *engine_context,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    uint8_t initial_master_volume
);

music_rig_result music_rig_arturia_session_select_profile(
    music_rig_arturia_session *session,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
);

/* Accepts one complete MIDI message; unsupported/system messages are not sent. */
music_rig_result music_rig_arturia_session_process_midi(
    music_rig_arturia_session *session,
    const uint8_t *message,
    size_t message_size
);

#endif
