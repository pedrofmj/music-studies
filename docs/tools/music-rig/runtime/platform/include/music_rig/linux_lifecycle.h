#ifndef MUSIC_RIG_LINUX_LIFECYCLE_H
#define MUSIC_RIG_LINUX_LIFECYCLE_H

#include "music_rig/diagnostics.h"

#define MUSIC_RIG_LINUX_LIFECYCLE_ABI_VERSION UINT32_C(1)

typedef music_rig_result (*music_rig_linux_lifecycle_poll_fn)(
    void *context
);

music_rig_result music_rig_linux_shadow_lifecycle_run(
    const music_rig_diagnostic_sink *sink
);

music_rig_result music_rig_linux_shadow_lifecycle_run_with_poll(
    const music_rig_diagnostic_sink *sink,
    music_rig_linux_lifecycle_poll_fn poll_fn,
    void *context
);

#endif
