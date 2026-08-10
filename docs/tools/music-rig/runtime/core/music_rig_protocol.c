#include "music_rig/protocol.h"

#include <string.h>

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

static int operation_is_supported(uint32_t operation)
{
    return operation == (uint32_t)MUSIC_RIG_OPERATION_STATUS;
}

music_rig_result music_rig_protocol_encode_request(
    const music_rig_protocol_request *request,
    uint8_t *output,
    size_t output_size
)
{
    if (request == NULL || output == NULL ||
        output_size != MUSIC_RIG_PROTOCOL_REQUEST_SIZE ||
        request->protocol_version != MUSIC_RIG_PROTOCOL_VERSION ||
        request->request_id == UINT64_C(0) ||
        !operation_is_supported(request->operation)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(output, 0, output_size);
    put_u32_le(output, MUSIC_RIG_PROTOCOL_MAGIC);
    put_u32_le(output + 4, request->protocol_version);
    put_u32_le(output + 8, request->operation);
    put_u64_le(output + 16, request->request_id);
    put_u64_le(output + 24, request->expected_generation);
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
        get_u32_le(input + 4) != MUSIC_RIG_PROTOCOL_VERSION ||
        get_u32_le(input + 12) != UINT32_C(0) ||
        get_u64_le(input + 16) == UINT64_C(0) ||
        !operation_is_supported(get_u32_le(input + 8))) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    request->protocol_version = get_u32_le(input + 4);
    request->operation = get_u32_le(input + 8);
    request->request_id = get_u64_le(input + 16);
    request->expected_generation = get_u64_le(input + 24);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_protocol_encode_response(
    const music_rig_protocol_response *response,
    uint8_t *output,
    size_t output_size
)
{
    if (response == NULL || output == NULL ||
        output_size != MUSIC_RIG_PROTOCOL_RESPONSE_SIZE ||
        response->protocol_version != MUSIC_RIG_PROTOCOL_VERSION ||
        response->request_id == UINT64_C(0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(output, 0, output_size);
    put_u32_le(output, MUSIC_RIG_PROTOCOL_MAGIC);
    put_u32_le(output + 4, response->protocol_version);
    put_u32_le(output + 8, response->result_code);
    put_u64_le(output + 16, response->request_id);
    put_u64_le(output + 24, response->previous_generation);
    put_u64_le(output + 32, response->resulting_generation);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_protocol_decode_response(
    const uint8_t *input,
    size_t input_size,
    music_rig_protocol_response *response
)
{
    if (input == NULL || response == NULL ||
        input_size != MUSIC_RIG_PROTOCOL_RESPONSE_SIZE ||
        get_u32_le(input) != MUSIC_RIG_PROTOCOL_MAGIC ||
        get_u32_le(input + 4) != MUSIC_RIG_PROTOCOL_VERSION ||
        get_u32_le(input + 12) != UINT32_C(0) ||
        get_u64_le(input + 16) == UINT64_C(0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    response->protocol_version = get_u32_le(input + 4);
    response->result_code = get_u32_le(input + 8);
    response->request_id = get_u64_le(input + 16);
    response->previous_generation = get_u64_le(input + 24);
    response->resulting_generation = get_u64_le(input + 32);
    return MUSIC_RIG_RESULT_OK;
}
