#ifndef MUSIC_RIG_CLI_H
#define MUSIC_RIG_CLI_H

#include "music_rig/protocol.h"

#include <stddef.h>
#include <stdint.h>

typedef enum music_rig_cli_format {
    MUSIC_RIG_CLI_FORMAT_HUMAN = 0,
    MUSIC_RIG_CLI_FORMAT_JSON = 1
} music_rig_cli_format;

typedef struct music_rig_cli_command {
    music_rig_protocol_request request;
    music_rig_cli_format format;
} music_rig_cli_command;

typedef struct music_rig_client_transport {
    void *context;
    music_rig_result (*exchange)(
        void *context,
        const music_rig_protocol_request *request,
        music_rig_protocol_response *response
    );
} music_rig_client_transport;

music_rig_result music_rig_cli_parse(
    int argc,
    char *const argv[],
    uint64_t request_id,
    music_rig_cli_command *command
);

music_rig_result music_rig_cli_render(
    const music_rig_protocol_response *response,
    music_rig_cli_format format,
    char *output,
    size_t output_capacity,
    size_t *output_size
);

music_rig_result music_rig_cli_execute(
    const music_rig_cli_command *command,
    const music_rig_client_transport *transport,
    char *output,
    size_t output_capacity,
    size_t *output_size
);

#endif
