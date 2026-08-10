#include "music_rig/core.h"

static const music_rig_build_info BUILD_INFO = {
    MUSIC_RIG_CORE_VERSION,
    MUSIC_RIG_PROTOCOL_VERSION,
    MUSIC_RIG_PROFILE_SCHEMA_VERSION
};

const music_rig_build_info *music_rig_get_build_info(void)
{
    return &BUILD_INFO;
}
