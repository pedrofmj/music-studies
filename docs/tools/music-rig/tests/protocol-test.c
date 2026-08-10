#include "music_rig/protocol.h"
#include "protocol-golden.h"

#include <stdio.h>
#include <string.h>

static int test_request(void)
{
    const music_rig_protocol_request expected = {
        MUSIC_RIG_PROTOCOL_VERSION,
        (uint32_t)MUSIC_RIG_OPERATION_STATUS,
        UINT64_C(73),
        UINT64_C(41)
    };
    music_rig_protocol_request decoded;
    uint8_t encoded[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];

    if (music_rig_protocol_encode_request(
            &expected,
            encoded,
            sizeof(encoded)
        ) != MUSIC_RIG_RESULT_OK ||
        memcmp(
            encoded,
            MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN,
            sizeof(encoded)
        ) != 0 ||
        music_rig_protocol_decode_request(
            encoded,
            sizeof(encoded),
            &decoded
        ) != MUSIC_RIG_RESULT_OK ||
        decoded.protocol_version != expected.protocol_version ||
        decoded.operation != expected.operation ||
        decoded.request_id != expected.request_id ||
        decoded.expected_generation != expected.expected_generation) {
        fputs("request protocol round trip failed\n", stderr);
        return 1;
    }

    encoded[4] = (uint8_t)(MUSIC_RIG_PROTOCOL_VERSION + UINT32_C(1));
    if (music_rig_protocol_decode_request(
            encoded,
            sizeof(encoded),
            &decoded
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("unsupported request version was accepted\n", stderr);
        return 1;
    }

    return 0;
}

static int test_response(void)
{
    const music_rig_protocol_response expected = {
        MUSIC_RIG_PROTOCOL_VERSION,
        (uint32_t)MUSIC_RIG_RESULT_OK,
        UINT64_C(73),
        UINT64_C(41),
        UINT64_C(41)
    };
    music_rig_protocol_response decoded;
    uint8_t encoded[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];

    if (music_rig_protocol_encode_response(
            &expected,
            encoded,
            sizeof(encoded)
        ) != MUSIC_RIG_RESULT_OK ||
        memcmp(
            encoded,
            MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN,
            sizeof(encoded)
        ) != 0 ||
        music_rig_protocol_decode_response(
            encoded,
            sizeof(encoded),
            &decoded
        ) != MUSIC_RIG_RESULT_OK ||
        decoded.protocol_version != expected.protocol_version ||
        decoded.result_code != expected.result_code ||
        decoded.request_id != expected.request_id ||
        decoded.previous_generation != expected.previous_generation ||
        decoded.resulting_generation != expected.resulting_generation) {
        fputs("response protocol round trip failed\n", stderr);
        return 1;
    }

    encoded[0] ^= UINT8_C(1);
    if (music_rig_protocol_decode_response(
            encoded,
            sizeof(encoded),
            &decoded
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid response magic was accepted\n", stderr);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_request() != 0) {
        return 1;
    }
    return test_response();
}
