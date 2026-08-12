#ifndef MUSIC_RIG_CURRENT_ARTURIA_H
#define MUSIC_RIG_CURRENT_ARTURIA_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_CURRENT_ARTURIA_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_CURRENT_ARTURIA_SNAPSHOT_VERSION UINT32_C(1)
#define MUSIC_RIG_CURRENT_ARTURIA_BEHAVIOR_STORAGE_MAX ((size_t)64)
#define MUSIC_RIG_CURRENT_ARTURIA_SNAPSHOT_STORAGE_MAX ((size_t)16)

typedef struct music_rig_current_arturia_config {
    uint8_t channel;
    uint8_t volume_input_cc;
    uint8_t button_input_cc;
    uint8_t mute_output_cc;
    uint8_t volume_output_cc;
} music_rig_current_arturia_config;

typedef struct music_rig_current_arturia_snapshot {
    uint32_t version;
    uint8_t volume;
    uint8_t mute;
} music_rig_current_arturia_snapshot;

/* Caller-owned single-thread state. Treat config as immutable after init. */
typedef struct music_rig_current_arturia_behavior {
    uint32_t abi_version;
    music_rig_current_arturia_config config;
    uint8_t volume;
    uint8_t mute;
    bool button_down;
    int32_t output_connection_count;
    uint32_t generation;
    float audio_gain;
} music_rig_current_arturia_behavior;

typedef struct music_rig_current_arturia_decision {
    bool handled;
    bool output_ready;
    uint8_t output[3];
} music_rig_current_arturia_decision;

void music_rig_current_arturia_config_init(
    music_rig_current_arturia_config *config
);

music_rig_result music_rig_current_arturia_init(
    music_rig_current_arturia_behavior *behavior,
    const music_rig_current_arturia_config *config,
    int32_t initial_volume,
    int32_t initial_mute
);

music_rig_result music_rig_current_arturia_restore_legacy_text(
    music_rig_current_arturia_behavior *behavior,
    const char *text,
    size_t text_size,
    int32_t fallback_volume
);

music_rig_result music_rig_current_arturia_snapshot_read(
    const music_rig_current_arturia_behavior *behavior,
    music_rig_current_arturia_snapshot *snapshot
);

music_rig_result music_rig_current_arturia_snapshot_restore(
    music_rig_current_arturia_behavior *behavior,
    const music_rig_current_arturia_snapshot *snapshot
);

music_rig_result music_rig_current_arturia_process_midi(
    music_rig_current_arturia_behavior *behavior,
    const uint8_t *message,
    size_t message_size,
    music_rig_current_arturia_decision *decision
);

music_rig_result music_rig_current_arturia_output_connections(
    music_rig_current_arturia_behavior *behavior,
    int32_t connection_count,
    music_rig_current_arturia_decision *decision
);

music_rig_result music_rig_current_arturia_apply_audio_frame(
    music_rig_current_arturia_behavior *behavior,
    float ramp_step,
    float input_left,
    float input_right,
    float *output_left,
    float *output_right
);

#endif
