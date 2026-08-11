#ifndef MUSIC_RIG_PROTOCOL_H
#define MUSIC_RIG_PROTOCOL_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_PROTOCOL_MAGIC UINT32_C(0x4749524d)
#define MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY ((size_t)65)
#define MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY ((size_t)16)
#define MUSIC_RIG_PROTOCOL_REQUEST_SIZE ((size_t)176)
#define MUSIC_RIG_PROTOCOL_PROFILE_SIZE ((size_t)144)
#define MUSIC_RIG_PROTOCOL_RESPONSE_SIZE ((size_t)2592)

#define MUSIC_RIG_REQUEST_DRY_RUN UINT32_C(0x00000001)

#define MUSIC_RIG_RESPONSE_OUTPUT_SUPPRESSED UINT32_C(0x00000001)
#define MUSIC_RIG_RESPONSE_DRY_RUN UINT32_C(0x00000002)
#define MUSIC_RIG_RESPONSE_VALID UINT32_C(0x00000004)
#define MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY UINT32_C(0x00000008)

#define MUSIC_RIG_PROFILE_ACTIVE UINT32_C(0x00000001)
#define MUSIC_RIG_PROFILE_OVERRIDE UINT32_C(0x00000002)

#define MUSIC_RIG_WARNING_COLD_REQUIRED UINT32_C(0x00000001)
#define MUSIC_RIG_WARNING_UNAVAILABLE_BINDING UINT32_C(0x00000002)

typedef enum music_rig_operation {
    MUSIC_RIG_OPERATION_STATUS = 1,
    MUSIC_RIG_OPERATION_LIST_PROFILES = 2,
    MUSIC_RIG_OPERATION_PREPARE_GLOBAL = 3,
    MUSIC_RIG_OPERATION_PREPARE_DEVICE = 4,
    MUSIC_RIG_OPERATION_SWITCH_GLOBAL = 5,
    MUSIC_RIG_OPERATION_SWITCH_DEVICE = 6,
    MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE = 7,
    MUSIC_RIG_OPERATION_RELOAD_COMPILED_DEFINITION = 8,
    MUSIC_RIG_OPERATION_VALIDATE_ACTIVE = 9
} music_rig_operation;

typedef enum music_rig_rollback_status {
    MUSIC_RIG_ROLLBACK_NOT_REQUIRED = 0,
    MUSIC_RIG_ROLLBACK_SUCCEEDED = 1,
    MUSIC_RIG_ROLLBACK_FAILED = 2
} music_rig_rollback_status;

typedef struct music_rig_protocol_request {
    uint32_t protocol_version;
    uint32_t operation;
    uint32_t flags;
    uint64_t request_id;
    uint64_t expected_generation;
    char device_slot[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    char profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
} music_rig_protocol_request;

typedef struct music_rig_protocol_profile {
    char device_slot[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    char profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    uint32_t readiness;
    uint32_t flags;
} music_rig_protocol_profile;

typedef struct music_rig_protocol_response {
    uint32_t protocol_version;
    uint32_t operation;
    uint32_t result_code;
    uint32_t flags;
    uint32_t readiness;
    uint64_t request_id;
    uint64_t previous_generation;
    uint64_t resulting_generation;
    uint64_t control_duration_ns;
    uint64_t adopted_at_ns;
    uint32_t rollback_status;
    uint32_t warning_flags;
    char active_rig_profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    char selected_device_slot[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    char selected_profile[MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY];
    uint32_t profile_count;
    music_rig_protocol_profile profiles[MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY];
} music_rig_protocol_response;

music_rig_result music_rig_protocol_encode_request(
    const music_rig_protocol_request *request,
    uint8_t *output,
    size_t output_size
);

music_rig_result music_rig_protocol_decode_request(
    const uint8_t *input,
    size_t input_size,
    music_rig_protocol_request *request
);

music_rig_result music_rig_protocol_encode_response(
    const music_rig_protocol_response *response,
    uint8_t *output,
    size_t output_size
);

music_rig_result music_rig_protocol_decode_response(
    const uint8_t *input,
    size_t input_size,
    music_rig_protocol_response *response
);

#endif
