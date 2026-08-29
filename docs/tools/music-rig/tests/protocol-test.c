#include "music_rig/protocol.h"
#include "protocol-golden.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static int test_request(void)
{
    music_rig_protocol_request expected = {0};
    music_rig_protocol_request decoded;
    uint8_t encoded[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];

    expected.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    expected.operation = (uint32_t)MUSIC_RIG_OPERATION_STATUS;
    expected.request_id = UINT64_C(73);
    expected.expected_generation = UINT64_C(41);
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
            MUSIC_RIG_RESULT_OK ||
        memcmp(
            encoded,
            MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN_PREFIX,
            MUSIC_RIG_PROTOCOL_REQUEST_GOLDEN_PREFIX_SIZE
        ) != 0 ||
        music_rig_protocol_decode_request(encoded, sizeof(encoded), &decoded) !=
            MUSIC_RIG_RESULT_OK ||
        memcmp(&decoded, &expected, sizeof(expected)) != 0) {
        fputs("request protocol round trip failed\n", stderr);
        return 1;
    }

    expected.operation = (uint32_t)MUSIC_RIG_OPERATION_SWITCH_DEVICE;
    expected.flags = MUSIC_RIG_REQUEST_DRY_RUN;
    copy_text(expected.device_slot, "smc-mixer-main");
    copy_text(expected.profile, "eight-band-eq");
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_protocol_decode_request(encoded, sizeof(encoded), &decoded) !=
            MUSIC_RIG_RESULT_OK ||
        memcmp(&decoded, &expected, sizeof(expected)) != 0) {
        fputs("argument request round trip failed\n", stderr);
        return 1;
    }

    encoded[4] = (uint8_t)(MUSIC_RIG_PROTOCOL_VERSION + UINT32_C(1));
    if (music_rig_protocol_decode_request(encoded, sizeof(encoded), &decoded) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("unsupported request version was accepted\n", stderr);
        return 1;
    }
    expected.flags = UINT32_C(0);
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
        MUSIC_RIG_RESULT_OK) {
        fputs("non-dry-run switch framing was rejected\n", stderr);
        return 1;
    }
    expected.profile[0] = '\0';
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("incomplete device switch was accepted\n", stderr);
        return 1;
    }
    memset(&expected, 0, sizeof(expected));
    expected.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    expected.operation = (uint32_t)MUSIC_RIG_OPERATION_STATUS;
    expected.request_id = UINT64_C(1);
    expected.flags = MUSIC_RIG_REQUEST_DRY_RUN;
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("dry-run status request was accepted\n", stderr);
        return 1;
    }
    expected.flags = UINT32_C(0);
    memset(expected.device_slot, 'a', sizeof(expected.device_slot));
    expected.operation = (uint32_t)MUSIC_RIG_OPERATION_LIST_PROFILES;
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("unterminated request identifier was accepted\n", stderr);
        return 1;
    }
    memset(expected.device_slot, 0, sizeof(expected.device_slot));
    if (music_rig_protocol_encode_request(&expected, encoded, sizeof(encoded)) !=
        MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    encoded[175] = UINT8_C(1);
    if (music_rig_protocol_decode_request(encoded, sizeof(encoded), &decoded) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("nonzero request reserved byte was accepted\n", stderr);
        return 1;
    }
    return 0;
}

static int test_response(void)
{
    music_rig_protocol_response expected = {0};
    music_rig_protocol_response decoded;
    uint8_t encoded[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];

    expected.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    expected.operation = (uint32_t)MUSIC_RIG_OPERATION_STATUS;
    expected.result_code = (uint32_t)MUSIC_RIG_RESULT_OK;
    expected.flags = MUSIC_RIG_RESPONSE_OUTPUT_SUPPRESSED;
    expected.request_id = UINT64_C(73);
    expected.previous_generation = UINT64_C(41);
    expected.resulting_generation = UINT64_C(41);
    expected.rollback_status = (uint32_t)MUSIC_RIG_ROLLBACK_NOT_REQUIRED;
    expected.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    copy_text(expected.active_rig_profile, "full-live-rack");
    expected.readiness = UINT32_C(1);
    expected.profile_count = UINT32_C(1);
    copy_text(expected.profiles[0].device_slot, "smc-mixer-main");
    copy_text(expected.profiles[0].profile, "eight-band-eq");
    expected.profiles[0].readiness = UINT32_C(1);
    expected.profiles[0].flags = MUSIC_RIG_PROFILE_ACTIVE;

    if (music_rig_protocol_encode_response(
            &expected,
            encoded,
            sizeof(encoded)
        ) != MUSIC_RIG_RESULT_OK ||
        memcmp(
            encoded,
            MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN_PREFIX,
            MUSIC_RIG_PROTOCOL_RESPONSE_GOLDEN_PREFIX_SIZE
        ) != 0 ||
        music_rig_protocol_decode_response(
            encoded,
            sizeof(encoded),
            &decoded
        ) != MUSIC_RIG_RESULT_OK ||
        memcmp(&decoded, &expected, sizeof(expected)) != 0) {
        fputs("response protocol round trip failed\n", stderr);
        return 1;
    }
    encoded[76] = UINT8_C(2);
    if (music_rig_protocol_decode_response(
            encoded, sizeof(encoded), &decoded
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid response output mode was accepted\n", stderr);
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
    if (music_rig_protocol_encode_response(
            &expected,
            encoded,
            sizeof(encoded)
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    encoded[288 + MUSIC_RIG_PROTOCOL_PROFILE_SIZE] = UINT8_C(1);
    if (music_rig_protocol_decode_response(
            encoded,
            sizeof(encoded),
            &decoded
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("nonzero unused response row was accepted\n", stderr);
        return 1;
    }
    expected.flags |= UINT32_C(0x80000000);
    if (music_rig_protocol_encode_response(
            &expected,
            encoded,
            sizeof(encoded)
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("unknown response flag was accepted\n", stderr);
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
