#include "music_rig/protocol.h"

#include <stdbool.h>
#include <string.h>

#define RESPONSE_HEADER_SIZE ((size_t)288)

_Static_assert(
    (size_t)32 + 2U * MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY + 14U ==
        MUSIC_RIG_PROTOCOL_REQUEST_SIZE,
    "protocol request offsets must fill the fixed frame"
);
_Static_assert(
    RESPONSE_HEADER_SIZE + MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY *
        MUSIC_RIG_PROTOCOL_PROFILE_SIZE == MUSIC_RIG_PROTOCOL_RESPONSE_SIZE,
    "protocol response rows must fill the fixed frame"
);

static void put_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value & UINT32_C(0xff));
    output[1] = (uint8_t)((value >> 8) & UINT32_C(0xff));
    output[2] = (uint8_t)((value >> 16) & UINT32_C(0xff));
    output[3] = (uint8_t)((value >> 24) & UINT32_C(0xff));
}

static uint32_t get_u32_le(const uint8_t *input)
{
    return (uint32_t)input[0] |
        ((uint32_t)input[1] << 8) |
        ((uint32_t)input[2] << 16) |
        ((uint32_t)input[3] << 24);
}

static void put_u64_le(uint8_t *output, uint64_t value)
{
    put_u32_le(output, (uint32_t)(value & UINT64_C(0xffffffff)));
    put_u32_le(output + 4, (uint32_t)(value >> 32));
}

static uint64_t get_u64_le(const uint8_t *input)
{
    return (uint64_t)get_u32_le(input) |
        ((uint64_t)get_u32_le(input + 4) << 32);
}

static bool bytes_are_zero(const uint8_t *input, size_t size)
{
    size_t index;

    for (index = 0; index < size; ++index) {
        if (input[index] != UINT8_C(0)) {
            return false;
        }
    }
    return true;
}

static bool bounded_text(const char *value, bool required)
{
    size_t index;

    if (value == NULL) {
        return false;
    }
    for (index = 0; index < MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY; ++index) {
        unsigned char character = (unsigned char)value[index];

        if (character == (unsigned char)'\0') {
            return !required || index != 0;
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

static bool operation_is_supported(uint32_t operation)
{
    return operation >= (uint32_t)MUSIC_RIG_OPERATION_STATUS &&
        operation <= (uint32_t)MUSIC_RIG_OPERATION_VALIDATE_ACTIVE;
}

static bool request_shape_is_valid(const music_rig_protocol_request *request)
{
    bool has_slot;
    bool has_profile;

    if (request == NULL ||
        request->protocol_version != MUSIC_RIG_PROTOCOL_VERSION ||
        request->request_id == UINT64_C(0) ||
        !operation_is_supported(request->operation) ||
        (request->flags & ~MUSIC_RIG_REQUEST_DRY_RUN) != UINT32_C(0) ||
        !bounded_text(request->device_slot, false) ||
        !bounded_text(request->profile, false)) {
        return false;
    }

    has_slot = request->device_slot[0] != '\0';
    has_profile = request->profile[0] != '\0';
    switch ((music_rig_operation)request->operation) {
    case MUSIC_RIG_OPERATION_STATUS:
    case MUSIC_RIG_OPERATION_VALIDATE_ACTIVE:
        return request->flags == UINT32_C(0) &&
            !has_slot && !has_profile;
    case MUSIC_RIG_OPERATION_LIST_PROFILES:
        return request->flags == UINT32_C(0) && !has_profile;
    case MUSIC_RIG_OPERATION_RELOAD_COMPILED_DEFINITION:
        return !has_slot && !has_profile;
    case MUSIC_RIG_OPERATION_PREPARE_GLOBAL:
    case MUSIC_RIG_OPERATION_SWITCH_GLOBAL:
        return !has_slot && has_profile;
    case MUSIC_RIG_OPERATION_PREPARE_DEVICE:
    case MUSIC_RIG_OPERATION_SWITCH_DEVICE:
        return has_slot && has_profile;
    case MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE:
        return has_slot && !has_profile;
    default:
        return false;
    }
}

static void put_text(uint8_t *output, const char *value)
{
    size_t size = strlen(value);

    memcpy(output, value, size);
}

static bool profile_is_valid(const music_rig_protocol_profile *profile)
{
    return bounded_text(profile->device_slot, true) &&
        bounded_text(profile->profile, true) &&
        profile->readiness >= UINT32_C(1) &&
        profile->readiness <= UINT32_C(3) &&
        (profile->flags & ~(MUSIC_RIG_PROFILE_ACTIVE |
            MUSIC_RIG_PROFILE_OVERRIDE)) == UINT32_C(0);
}

static bool response_is_valid(const music_rig_protocol_response *response)
{
    size_t index;

    if (response == NULL ||
        response->protocol_version != MUSIC_RIG_PROTOCOL_VERSION ||
        !operation_is_supported(response->operation) ||
        response->result_code > (uint32_t)MUSIC_RIG_RESULT_BUFFER_TOO_SMALL ||
        (response->flags & ~(MUSIC_RIG_RESPONSE_OUTPUT_SUPPRESSED |
            MUSIC_RIG_RESPONSE_DRY_RUN | MUSIC_RIG_RESPONSE_VALID |
            MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY)) != UINT32_C(0) ||
        response->readiness > UINT32_C(3) ||
        response->request_id == UINT64_C(0) ||
        response->rollback_status > (uint32_t)MUSIC_RIG_ROLLBACK_FAILED ||
        response->output_mode > MUSIC_RIG_OUTPUT_ENABLED ||
        (response->warning_flags & ~(MUSIC_RIG_WARNING_COLD_REQUIRED |
            MUSIC_RIG_WARNING_UNAVAILABLE_BINDING)) != UINT32_C(0) ||
        !bounded_text(response->active_rig_profile, false) ||
        !bounded_text(response->selected_device_slot, false) ||
        !bounded_text(response->selected_profile, false) ||
        response->profile_count > MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY) {
        return false;
    }
    for (index = 0; index < response->profile_count; ++index) {
        if (!profile_is_valid(&response->profiles[index])) {
            return false;
        }
    }
    return true;
}

music_rig_result music_rig_protocol_encode_request(
    const music_rig_protocol_request *request,
    uint8_t *output,
    size_t output_size
)
{
    if (!request_shape_is_valid(request) || output == NULL ||
        output_size != MUSIC_RIG_PROTOCOL_REQUEST_SIZE) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(output, 0, output_size);
    put_u32_le(output, MUSIC_RIG_PROTOCOL_MAGIC);
    put_u32_le(output + 4, request->protocol_version);
    put_u32_le(output + 8, request->operation);
    put_u32_le(output + 12, request->flags);
    put_u64_le(output + 16, request->request_id);
    put_u64_le(output + 24, request->expected_generation);
    put_text(output + 32, request->device_slot);
    put_text(output + 97, request->profile);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_protocol_decode_request(
    const uint8_t *input,
    size_t input_size,
    music_rig_protocol_request *request
)
{
    if (input == NULL || request == NULL ||
        input_size != MUSIC_RIG_PROTOCOL_REQUEST_SIZE ||
        get_u32_le(input) != MUSIC_RIG_PROTOCOL_MAGIC ||
        !bytes_are_zero(input + 162, 14)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(request, 0, sizeof(*request));
    request->protocol_version = get_u32_le(input + 4);
    request->operation = get_u32_le(input + 8);
    request->flags = get_u32_le(input + 12);
    request->request_id = get_u64_le(input + 16);
    request->expected_generation = get_u64_le(input + 24);
    memcpy(request->device_slot, input + 32, sizeof(request->device_slot));
    memcpy(request->profile, input + 97, sizeof(request->profile));
    return request_shape_is_valid(request)
        ? MUSIC_RIG_RESULT_OK
        : MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}

music_rig_result music_rig_protocol_encode_response(
    const music_rig_protocol_response *response,
    uint8_t *output,
    size_t output_size
)
{
    size_t index;

    if (!response_is_valid(response) || output == NULL ||
        output_size != MUSIC_RIG_PROTOCOL_RESPONSE_SIZE) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(output, 0, output_size);
    put_u32_le(output, MUSIC_RIG_PROTOCOL_MAGIC);
    put_u32_le(output + 4, response->protocol_version);
    put_u32_le(output + 8, response->operation);
    put_u32_le(output + 12, response->result_code);
    put_u32_le(output + 16, response->flags);
    put_u32_le(output + 20, response->readiness);
    put_u64_le(output + 24, response->request_id);
    put_u64_le(output + 32, response->previous_generation);
    put_u64_le(output + 40, response->resulting_generation);
    put_u64_le(output + 48, response->control_duration_ns);
    put_u64_le(output + 56, response->adopted_at_ns);
    put_u32_le(output + 64, response->rollback_status);
    put_u32_le(output + 68, response->warning_flags);
    put_u32_le(output + 76, (uint32_t)response->output_mode);
    put_u32_le(output + 72, response->profile_count);
    put_text(output + 80, response->active_rig_profile);
    put_text(output + 145, response->selected_device_slot);
    put_text(output + 210, response->selected_profile);

    for (index = 0; index < response->profile_count; ++index) {
        const music_rig_protocol_profile *profile = &response->profiles[index];
        uint8_t *entry = output + RESPONSE_HEADER_SIZE +
            index * MUSIC_RIG_PROTOCOL_PROFILE_SIZE;

        put_text(entry, profile->device_slot);
        put_text(entry + 65, profile->profile);
        put_u32_le(entry + 132, profile->readiness);
        put_u32_le(entry + 136, profile->flags);
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_protocol_decode_response(
    const uint8_t *input,
    size_t input_size,
    music_rig_protocol_response *response
)
{
    size_t index;
    uint32_t profile_count;

    if (input == NULL || response == NULL ||
        input_size != MUSIC_RIG_PROTOCOL_RESPONSE_SIZE ||
        get_u32_le(input) != MUSIC_RIG_PROTOCOL_MAGIC ||
        !bytes_are_zero(input + 275, 13)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    profile_count = get_u32_le(input + 72);
    if (profile_count > MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(response, 0, sizeof(*response));
    response->protocol_version = get_u32_le(input + 4);
    response->operation = get_u32_le(input + 8);
    response->result_code = get_u32_le(input + 12);
    response->flags = get_u32_le(input + 16);
    response->readiness = get_u32_le(input + 20);
    response->request_id = get_u64_le(input + 24);
    response->previous_generation = get_u64_le(input + 32);
    response->resulting_generation = get_u64_le(input + 40);
    response->control_duration_ns = get_u64_le(input + 48);
    response->adopted_at_ns = get_u64_le(input + 56);
    response->rollback_status = get_u32_le(input + 64);
    response->warning_flags = get_u32_le(input + 68);
    response->output_mode = (music_rig_output_mode)get_u32_le(input + 76);
    if (response->output_mode > MUSIC_RIG_OUTPUT_ENABLED) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    response->profile_count = profile_count;
    memcpy(response->active_rig_profile, input + 80,
        sizeof(response->active_rig_profile));
    memcpy(response->selected_device_slot, input + 145,
        sizeof(response->selected_device_slot));
    memcpy(response->selected_profile, input + 210,
        sizeof(response->selected_profile));

    for (index = 0; index < response->profile_count; ++index) {
        music_rig_protocol_profile *profile = &response->profiles[index];
        const uint8_t *entry = input + RESPONSE_HEADER_SIZE +
            index * MUSIC_RIG_PROTOCOL_PROFILE_SIZE;

        if (!bytes_are_zero(entry + 130, 2) ||
            !bytes_are_zero(entry + 140, 4)) {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
        memcpy(profile->device_slot, entry, sizeof(profile->device_slot));
        memcpy(profile->profile, entry + 65, sizeof(profile->profile));
        profile->readiness = get_u32_le(entry + 132);
        profile->flags = get_u32_le(entry + 136);
    }
    for (; index < MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY; ++index) {
        const uint8_t *entry = input + RESPONSE_HEADER_SIZE +
            index * MUSIC_RIG_PROTOCOL_PROFILE_SIZE;

        if (!bytes_are_zero(entry, MUSIC_RIG_PROTOCOL_PROFILE_SIZE)) {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
    }
    return response_is_valid(response)
        ? MUSIC_RIG_RESULT_OK
        : MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}
