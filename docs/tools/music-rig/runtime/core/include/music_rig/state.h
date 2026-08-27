#ifndef MUSIC_RIG_STATE_H
#define MUSIC_RIG_STATE_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_RUNTIME_STATE_VERSION UINT32_C(3)
#define MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE ((size_t)800)
#define MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE ((size_t)32)
#define MUSIC_RIG_PERSISTED_PROFILE_CAPACITY ((size_t)65)
#define MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY ((size_t)5)
#define MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_FRAME_SIZE ((size_t)130)

typedef struct music_rig_persisted_device_override {
    char device_slot[MUSIC_RIG_PERSISTED_PROFILE_CAPACITY];
    char profile[MUSIC_RIG_PERSISTED_PROFILE_CAPACITY];
} music_rig_persisted_device_override;

typedef enum music_rig_output_mode {
    MUSIC_RIG_OUTPUT_SUPPRESSED = 0,
    MUSIC_RIG_OUTPUT_ENABLED = 1
} music_rig_output_mode;

typedef struct music_rig_persisted_state {
    uint64_t generation_id;
    uint8_t definition_fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    music_rig_output_mode output_mode;
    char active_rig_profile[MUSIC_RIG_PERSISTED_PROFILE_CAPACITY];
    uint32_t device_override_count;
    music_rig_persisted_device_override device_overrides[
        MUSIC_RIG_PERSISTED_DEVICE_OVERRIDE_CAPACITY
    ];
} music_rig_persisted_state;

music_rig_result music_rig_state_encode(
    const music_rig_persisted_state *state,
    uint8_t *output,
    size_t output_size
);

music_rig_result music_rig_state_decode(
    const uint8_t *input,
    size_t input_size,
    music_rig_persisted_state *state
);

#endif
