#ifndef MUSIC_RIG_CURRENT_SMK25_H
#define MUSIC_RIG_CURRENT_SMK25_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_CURRENT_SMK25_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_CURRENT_SMK25_SNAPSHOT_VERSION UINT32_C(1)
#define MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT ((size_t)8)
#define MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT ((size_t)128)
#define MUSIC_RIG_CURRENT_SMK25_CHORD_CAPACITY ((size_t)16)
#define MUSIC_RIG_CURRENT_SMK25_BEHAVIOR_STORAGE_MAX ((size_t)4096)
#define MUSIC_RIG_CURRENT_SMK25_SNAPSHOT_STORAGE_MAX ((size_t)2048)

typedef enum music_rig_current_smk25_control_type {
    MUSIC_RIG_CURRENT_SMK25_CONTROL_NONE = 0,
    MUSIC_RIG_CURRENT_SMK25_CONTROL_CC = 1,
    MUSIC_RIG_CURRENT_SMK25_CONTROL_NOTE = 2
} music_rig_current_smk25_control_type;

typedef enum music_rig_current_smk25_pad_behavior {
    MUSIC_RIG_CURRENT_SMK25_PAD_VALUE = 0,
    MUSIC_RIG_CURRENT_SMK25_PAD_TOGGLE = 1
} music_rig_current_smk25_pad_behavior;

typedef struct music_rig_current_smk25_control {
    music_rig_current_smk25_control_type type;
    uint8_t number;
} music_rig_current_smk25_control;

typedef struct music_rig_current_smk25_config {
    uint8_t channel;
    music_rig_current_smk25_control_type pad_type;
    music_rig_current_smk25_pad_behavior pad_behavior;
    uint8_t pad_on_minimum;
    uint8_t pads[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    uint8_t pad_channels[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    uint8_t knobs[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    uint8_t knob_channels[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    music_rig_current_smk25_control play;
    music_rig_current_smk25_control stop;
    uint8_t default_chord[MUSIC_RIG_CURRENT_SMK25_CHORD_CAPACITY];
    size_t default_chord_count;
} music_rig_current_smk25_config;

typedef struct music_rig_current_smk25_layer_state {
    bool enabled;
    bool paused;
    bool pad_down;
    uint8_t last_velocity[MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT];
    uint8_t active_velocity[MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT];
} music_rig_current_smk25_layer_state;

typedef struct music_rig_current_smk25_snapshot {
    uint32_t version;
    uint8_t knob_values[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    bool enabled[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    bool paused[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    uint8_t last_velocity
        [MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT]
        [MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT];
} music_rig_current_smk25_snapshot;

/* Caller-owned single-thread state. Treat config as immutable after init. */
typedef struct music_rig_current_smk25_behavior {
    uint32_t abi_version;
    music_rig_current_smk25_config config;
    music_rig_current_smk25_layer_state
        layers[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    bool physical_notes[MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT];
    size_t physical_note_count;
    uint8_t knob_values[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    int32_t output_connection_counts[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    uint32_t generation;
} music_rig_current_smk25_behavior;

typedef void (*music_rig_current_smk25_emit_fn)(
    void *context,
    size_t layer,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
);

void music_rig_current_smk25_config_init(
    music_rig_current_smk25_config *config
);

music_rig_result music_rig_current_smk25_init(
    music_rig_current_smk25_behavior *behavior,
    const music_rig_current_smk25_config *config
);

music_rig_result music_rig_current_smk25_restore_legacy_text(
    music_rig_current_smk25_behavior *behavior,
    const char *text,
    size_t text_size
);

music_rig_result music_rig_current_smk25_snapshot_read(
    const music_rig_current_smk25_behavior *behavior,
    music_rig_current_smk25_snapshot *snapshot
);

music_rig_result music_rig_current_smk25_snapshot_restore(
    music_rig_current_smk25_behavior *behavior,
    const music_rig_current_smk25_snapshot *snapshot
);

music_rig_result music_rig_current_smk25_process_midi(
    music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    size_t message_size,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
);

music_rig_result music_rig_current_smk25_output_connections(
    music_rig_current_smk25_behavior *behavior,
    size_t layer,
    int32_t connection_count,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
);

music_rig_result music_rig_current_smk25_stop(
    music_rig_current_smk25_behavior *behavior,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
);

music_rig_result music_rig_current_smk25_play(
    music_rig_current_smk25_behavior *behavior,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
);

#endif
