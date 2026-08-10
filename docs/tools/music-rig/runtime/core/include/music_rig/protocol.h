#ifndef MUSIC_RIG_PROTOCOL_H
#define MUSIC_RIG_PROTOCOL_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_PROTOCOL_MAGIC UINT32_C(0x4749524d)
#define MUSIC_RIG_PROTOCOL_REQUEST_SIZE ((size_t)32)
#define MUSIC_RIG_PROTOCOL_RESPONSE_SIZE ((size_t)40)

typedef enum music_rig_operation {
    MUSIC_RIG_OPERATION_STATUS = 1
} music_rig_operation;

typedef struct music_rig_protocol_request {
    uint32_t protocol_version;
    uint32_t operation;
    uint64_t request_id;
    uint64_t expected_generation;
} music_rig_protocol_request;

typedef struct music_rig_protocol_response {
    uint32_t protocol_version;
    uint32_t result_code;
    uint64_t request_id;
    uint64_t previous_generation;
    uint64_t resulting_generation;
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
