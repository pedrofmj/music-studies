#ifndef MUSIC_RIG_SYNTHV1_PROCESS_H
#define MUSIC_RIG_SYNTHV1_PROCESS_H

#include "music_rig/core.h"

#include <stdbool.h>
#include <sys/types.h>

typedef struct music_rig_synthv1_process {
    const char *binary_path;
    const char *client_name;
    const char *jack_server;
    pid_t pid;
} music_rig_synthv1_process;

music_rig_result music_rig_synthv1_process_init(
    music_rig_synthv1_process *process,
    const char *binary_path,
    const char *client_name,
    const char *jack_server
);

music_rig_result music_rig_synthv1_process_start(
    void *context,
    const char *preset_path
);

music_rig_result music_rig_synthv1_process_stop(void *context);

bool music_rig_synthv1_process_is_healthy(
    music_rig_synthv1_process *process
);

#endif
