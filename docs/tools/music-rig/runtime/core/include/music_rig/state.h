#ifndef MUSIC_RIG_STATE_H
#define MUSIC_RIG_STATE_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_RUNTIME_STATE_VERSION UINT32_C(1)
#define MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE ((size_t)64)
#define MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE ((size_t)32)

typedef enum music_rig_output_mode {
    MUSIC_RIG_OUTPUT_SUPPRESSED = 0,
    MUSIC_RIG_OUTPUT_ENABLED = 1
} music_rig_output_mode;

typedef struct music_rig_persisted_state {
    uint64_t generation_id;
    uint8_t definition_fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    music_rig_output_mode output_mode;
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
