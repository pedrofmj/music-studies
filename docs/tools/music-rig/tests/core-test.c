#include "music_rig/core.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const music_rig_build_info *build_info = music_rig_get_build_info();

    if (build_info == NULL) {
        fputs("build info is null\n", stderr);
        return 1;
    }
    if (strcmp(build_info->core_version, MUSIC_RIG_CORE_VERSION) != 0) {
        fputs("core version mismatch\n", stderr);
        return 1;
    }
    if (build_info->protocol_version != MUSIC_RIG_PROTOCOL_VERSION) {
        fputs("protocol version mismatch\n", stderr);
        return 1;
    }
    if (build_info->profile_schema_version !=
        MUSIC_RIG_PROFILE_SCHEMA_VERSION) {
        fputs("profile schema version mismatch\n", stderr);
        return 1;
    }

    return 0;
}
