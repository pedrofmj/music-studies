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
    strcpy(source.active_rig_profile, "full-live-rack");
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
        strcmp(decoded.active_rig_profile, source.active_rig_profile) != 0 ||
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

static int test_legacy_state_compatibility(void)
{
    static const uint8_t legacy_frame[64] = {
        0x4d, 0x52, 0x53, 0x54, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xa9, 0x6d, 0xcd, 0x44, 0x66, 0x6e, 0x3d, 0x48
    };
    music_rig_persisted_state decoded;
    size_t index;

    if (music_rig_state_decode(
            legacy_frame,
            sizeof(legacy_frame),
            &decoded
        ) != MUSIC_RIG_RESULT_OK ||
        decoded.generation_id != UINT64_C(42) ||
        decoded.output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED ||
        decoded.active_rig_profile[0] != 0) {
        fputs("legacy state compatibility failed", stderr);
        return 1;
    }
    for (index = 0U; index < MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE; ++index) {
        if (decoded.definition_fingerprint[index] != (uint8_t)index) {
            fputs("legacy state fingerprint failed", stderr);
            return 1;
        }
    }
    return 0;
}

static uint64_t test_checksum(const uint8_t *input, size_t input_size)
{
    uint64_t value = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0U; index < input_size; ++index) {
        value ^= (uint64_t)input[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static void test_write_u64(uint8_t *output, uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        output[index] = (uint8_t)(value >> (56U - index * 8U));
    }
}

static int test_v2_state_compatibility(void)
{
    uint8_t frame[128] = {0};
    music_rig_persisted_state decoded;

    frame[0] = (uint8_t)'M';
    frame[1] = (uint8_t)'R';
    frame[2] = (uint8_t)'S';
    frame[3] = (uint8_t)'T';
    frame[7] = UINT8_C(2);
    frame[15] = UINT8_C(42);
    frame[52] = (uint8_t)'f';
    frame[53] = (uint8_t)'u';
    frame[54] = (uint8_t)'l';
    frame[55] = (uint8_t)'l';
    frame[56] = (uint8_t)'-';
    frame[57] = (uint8_t)'l';
    frame[58] = (uint8_t)'i';
    frame[59] = (uint8_t)'v';
    frame[60] = (uint8_t)'e';
    frame[61] = (uint8_t)'-';
    frame[62] = (uint8_t)'r';
    frame[63] = (uint8_t)'a';
    frame[64] = (uint8_t)'c';
    frame[65] = (uint8_t)'k';
    test_write_u64(frame + 120U, test_checksum(frame, 120U));
    if (music_rig_state_decode(frame, sizeof(frame), &decoded) !=
            MUSIC_RIG_RESULT_OK || decoded.generation_id != UINT64_C(42) ||
        strcmp(decoded.active_rig_profile, "full-live-rack") != 0 ||
        decoded.device_override_count != 0U) {
        fputs("v2 state compatibility failed\n", stderr);
        return 1;
    }
    return 0;
}

static int test_device_override_state(void)
{
    music_rig_persisted_state source;
    music_rig_persisted_state decoded;
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];

    memset(&source, 0, sizeof(source));
    source.generation_id = UINT64_C(43);
    source.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    strcpy(source.active_rig_profile, "full-live-rack");
    source.device_override_count = 1U;
    strcpy(source.device_overrides[0].device_slot, "smc-mixer-main");
    strcpy(source.device_overrides[0].profile, "multilevel-volume");
    if (music_rig_state_encode(&source, frame, sizeof(frame)) !=
            MUSIC_RIG_RESULT_OK || music_rig_state_decode(
                frame, sizeof(frame), &decoded
            ) != MUSIC_RIG_RESULT_OK || decoded.device_override_count != 1U ||
        strcmp(decoded.device_overrides[0].device_slot, "smc-mixer-main") != 0 ||
        strcmp(decoded.device_overrides[0].profile, "multilevel-volume") != 0) {
        fputs("device override state round trip failed\n", stderr);
        return 1;
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
        test_legacy_state_compatibility() != 0 ||
        test_v2_state_compatibility() != 0 ||
        test_device_override_state() != 0 ||
        test_invalid_arguments() != 0;
}
