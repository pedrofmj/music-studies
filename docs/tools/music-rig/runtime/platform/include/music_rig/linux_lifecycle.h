#ifndef MUSIC_RIG_LINUX_LIFECYCLE_H
#define MUSIC_RIG_LINUX_LIFECYCLE_H

#include "music_rig/diagnostics.h"

#define MUSIC_RIG_LINUX_LIFECYCLE_ABI_VERSION UINT32_C(1)

music_rig_result music_rig_linux_shadow_lifecycle_run(
    const music_rig_diagnostic_sink *sink
);

#endif
