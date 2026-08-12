#ifndef MUSIC_RIG_JOURNAL_DIAGNOSTICS_H
#define MUSIC_RIG_JOURNAL_DIAGNOSTICS_H

#include "music_rig/diagnostics.h"

#include <stdint.h>

#define MUSIC_RIG_JOURNAL_DIAGNOSTICS_ABI_VERSION UINT32_C(1)

typedef struct music_rig_journal_diagnostics {
    uint32_t abi_version;
    int descriptor;
} music_rig_journal_diagnostics;

/*
 * The supplied descriptor is normally STDERR_FILENO. The systemd unit routes
 * standard error to the per-user journal. Tests inject an anonymous pipe.
 */
music_rig_result music_rig_journal_diagnostics_init(
    music_rig_journal_diagnostics *journal,
    int descriptor,
    music_rig_diagnostic_sink *sink
);

#endif
