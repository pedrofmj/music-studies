#ifndef MUSIC_RIG_CONTROL_H
#define MUSIC_RIG_CONTROL_H

#include "music_rig/compiled_tables.h"
#include "music_rig/protocol.h"
#include "music_rig/state.h"

#include <stdint.h>

typedef struct music_rig_control_snapshot {
    uint64_t generation_id;
    const char *active_rig_profile;
    const music_rig_compiled_tables *tables;
    music_rig_output_mode output_mode;
} music_rig_control_snapshot;

/*
 * Produces a response without publishing a generation or persisting state.
 * The function returns OK when a protocol response was produced; operation
 * failures are carried in response.result_code.
 */
music_rig_result music_rig_control_dispatch(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
);

#endif
