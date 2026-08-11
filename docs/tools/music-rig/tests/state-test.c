#include "music_rig/state.h"

#include <stdio.h>
#include <string.h>

static int test_round_trip_and_corruption(void)
{
    music_rig_persisted_state source;
    music_rig_persisted_state decoded;
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    uint8_t corrupted[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    size_t index;

    memset(&source, 0, sizeof(source));
    source.generation_id = UINT64_C(42);
    source.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    for (index = 0; index < MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE; ++index) {
        source.definition_fingerprint[index] = (uint8_t)index;
    }

    if (music_rig_state_encode(&source, frame, sizeof(frame)) !=
            MUSIC_RIG_RESULT_OK ||
        frame[0] != (uint8_t)'M' || frame[1] != (uint8_t)'R' ||
        frame[2] != (uint8_t)'S' || frame[3] != (uint8_t)'T' ||
        music_rig_state_decode(frame, sizeof(frame), &decoded) !=
            MUSIC_RIG_RESULT_OK ||
        decoded.generation_id != source.generation_id ||
        decoded.output_mode != source.output_mode ||
        memcmp(
            decoded.definition_fingerprint,
            source.definition_fingerprint,
            MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
        ) != 0) {
        fputs("persistent state round trip failed\n", stderr);
        return 1;
    }

    for (index = 0; index < sizeof(frame); ++index) {
        memcpy(corrupted, frame, sizeof(frame));
        corrupted[index] ^= UINT8_C(1);
        if (music_rig_state_decode(corrupted, sizeof(corrupted), &decoded) !=
            MUSIC_RIG_RESULT_INVALID_DATA) {
            fprintf(stderr, "state corruption at byte %zu was accepted\n", index);
            return 1;
        }
    }
    return 0;
}

static int test_invalid_arguments(void)
{
    music_rig_persisted_state state;
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];

    memset(&state, 0, sizeof(state));
    state.generation_id = UINT64_C(1);
    state.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    if (music_rig_state_encode(&state, frame, sizeof(frame)) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_state_encode(NULL, frame, sizeof(frame)) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_state_encode(&state, frame, sizeof(frame) - 1U) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_state_decode(NULL, sizeof(frame), &state) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_state_decode(frame, sizeof(frame) - 1U, &state) !=
            MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("invalid persistent state input was accepted\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    return test_round_trip_and_corruption() != 0 ||
        test_invalid_arguments() != 0;
}
