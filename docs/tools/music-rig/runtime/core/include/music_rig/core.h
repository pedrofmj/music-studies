#ifndef MUSIC_RIG_CORE_H
#define MUSIC_RIG_CORE_H

#include <stdint.h>

#define MUSIC_RIG_CORE_VERSION "0.1.0"
#define MUSIC_RIG_PROTOCOL_VERSION UINT32_C(1)
#define MUSIC_RIG_PROFILE_SCHEMA_VERSION UINT32_C(1)

typedef enum music_rig_result {
    MUSIC_RIG_RESULT_OK = 0,
    MUSIC_RIG_RESULT_INVALID_ARGUMENT = 2
} music_rig_result;

typedef struct music_rig_build_info {
    const char *core_version;
    uint32_t protocol_version;
    uint32_t profile_schema_version;
} music_rig_build_info;

const music_rig_build_info *music_rig_get_build_info(void);

#endif
