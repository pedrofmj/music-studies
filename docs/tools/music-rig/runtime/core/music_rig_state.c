#include "music_rig/state.h"

#include <string.h>

#define STATE_MAGIC UINT32_C(0x4d525354)
#define LEGACY_STATE_VERSION UINT32_C(1)
#define LEGACY_STATE_FRAME_SIZE ((size_t)64)
#define LEGACY_CHECKSUM_OFFSET ((size_t)56)
#define STATE_PROFILE_OFFSET ((size_t)52)
#define STATE_CHECKSUM_OFFSET ((size_t)120)
#define FNV1A_OFFSET_BASIS UINT64_C(14695981039346656037)
#define FNV1A_PRIME UINT64_C(1099511628211)

static void write_u32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

static void write_u64(uint8_t *output, uint64_t value)
{
    size_t index;

    for (index = 0; index < 8; ++index) {
        output[index] = (uint8_t)(value >> (56U - (unsigned int)index * 8U));
    }
}

static uint32_t read_u32(const uint8_t *input)
{
    return ((uint32_t)input[0] << 24) |
        ((uint32_t)input[1] << 16) |
        ((uint32_t)input[2] << 8) |
        (uint32_t)input[3];
}

static uint64_t read_u64(const uint8_t *input)
{
    uint64_t value = UINT64_C(0);
    size_t index;

    for (index = 0; index < 8; ++index) {
        value = (value << 8) | (uint64_t)input[index];
    }
    return value;
}

static uint64_t checksum(const uint8_t *input, size_t input_size)
{
    uint64_t value = FNV1A_OFFSET_BASIS;
    size_t index;

    for (index = 0; index < input_size; ++index) {
        value ^= (uint64_t)input[index];
        value *= FNV1A_PRIME;
    }
    return value;
}

static bool bytes_are_zero(const uint8_t *input, size_t input_size)
{
    size_t index;

    for (index = 0U; index < input_size; ++index) {
        if (input[index] != UINT8_C(0)) {
            return false;
        }
    }
    return true;
}

static bool profile_is_valid(const char *profile)
{
    size_t index;

    for (index = 0U; index < MUSIC_RIG_PERSISTED_PROFILE_CAPACITY; ++index) {
        unsigned char character = (unsigned char)profile[index];

        if (character == (unsigned char)'\0') {
            return index != 0U;
        }
        if (!((character >= (unsigned char)'a' &&
                character <= (unsigned char)'z') ||
            (character >= (unsigned char)'A' &&
                character <= (unsigned char)'Z') ||
            (character >= (unsigned char)'0' &&
                character <= (unsigned char)'9') ||
            character == (unsigned char)'-' ||
            character == (unsigned char)'_' ||
            character == (unsigned char)'.')) {
            return false;
        }
    }
    return false;
}

music_rig_result music_rig_state_encode(
    const music_rig_persisted_state *state,
    uint8_t *output,
    size_t output_size
)
{
    if (state == NULL || output == NULL ||
        output_size != MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE ||
        state->generation_id == UINT64_C(0) ||
        state->output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED ||
        !profile_is_valid(state->active_rig_profile)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(output, 0, output_size);
    write_u32(output, STATE_MAGIC);
    write_u32(output + 4, MUSIC_RIG_RUNTIME_STATE_VERSION);
    write_u64(output + 8, state->generation_id);
    memcpy(
        output + 16,
        state->definition_fingerprint,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    write_u32(output + 48, (uint32_t)state->output_mode);
    memcpy(
        output + STATE_PROFILE_OFFSET,
        state->active_rig_profile,
        strlen(state->active_rig_profile)
    );
    write_u64(
        output + STATE_CHECKSUM_OFFSET,
        checksum(output, STATE_CHECKSUM_OFFSET)
    );
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_state_decode(
    const uint8_t *input,
    size_t input_size,
    music_rig_persisted_state *state
)
{
    uint64_t generation_id;
    uint32_t output_mode;
    bool legacy;

    if (input == NULL || state == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    legacy = input_size == LEGACY_STATE_FRAME_SIZE;
    if ((!legacy && input_size != MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE) ||
        read_u32(input) != STATE_MAGIC ||
        read_u32(input + 4) != (legacy
            ? LEGACY_STATE_VERSION
            : MUSIC_RIG_RUNTIME_STATE_VERSION) ||
        (legacy
            ? (read_u32(input + 52) != UINT32_C(0) ||
                read_u64(input + LEGACY_CHECKSUM_OFFSET) !=
                    checksum(input, LEGACY_CHECKSUM_OFFSET))
            : (!bytes_are_zero(input + 117, 3U) ||
                read_u64(input + STATE_CHECKSUM_OFFSET) !=
                    checksum(input, STATE_CHECKSUM_OFFSET)))) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }

    generation_id = read_u64(input + 8);
    output_mode = read_u32(input + 48);
    if (generation_id == UINT64_C(0) ||
        output_mode != (uint32_t)MUSIC_RIG_OUTPUT_SUPPRESSED) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }

    memset(state, 0, sizeof(*state));
    state->generation_id = generation_id;
    memcpy(
        state->definition_fingerprint,
        input + 16,
        MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
    );
    state->output_mode = (music_rig_output_mode)output_mode;
    if (!legacy) {
        memcpy(
            state->active_rig_profile,
            input + STATE_PROFILE_OFFSET,
            MUSIC_RIG_PERSISTED_PROFILE_CAPACITY
        );
        if (!profile_is_valid(state->active_rig_profile)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}
