#ifndef MUSIC_RIG_HOST_PATHS_H
#define MUSIC_RIG_HOST_PATHS_H

#include "music_rig/core.h"

#include <stddef.h>

#define MUSIC_RIG_HOST_PATHS_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_HOST_PATH_CAPACITY ((size_t)4096)

typedef struct music_rig_linux_path_environment {
    const char *home;
    const char *xdg_config_home;
    const char *xdg_cache_home;
    const char *xdg_state_home;
    const char *xdg_runtime_dir;
} music_rig_linux_path_environment;

typedef struct music_rig_host_paths {
    char config_directory[MUSIC_RIG_HOST_PATH_CAPACITY];
    char config_file[MUSIC_RIG_HOST_PATH_CAPACITY];
    char cache_directory[MUSIC_RIG_HOST_PATH_CAPACITY];
    char compiled_cache_directory[MUSIC_RIG_HOST_PATH_CAPACITY];
    char state_directory[MUSIC_RIG_HOST_PATH_CAPACITY];
    char active_state_file[MUSIC_RIG_HOST_PATH_CAPACITY];
    char device_state_directory[MUSIC_RIG_HOST_PATH_CAPACITY];
    char runtime_directory[MUSIC_RIG_HOST_PATH_CAPACITY];
    char control_socket[MUSIC_RIG_HOST_PATH_CAPACITY];
} music_rig_host_paths;

music_rig_result music_rig_linux_host_paths_resolve(
    const music_rig_linux_path_environment *environment,
    music_rig_host_paths *paths
);

music_rig_result music_rig_linux_host_paths_from_process(
    music_rig_host_paths *paths
);

#endif
