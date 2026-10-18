#ifndef MUSIC_RIG_WIN32_SOUNDFONT_PATHS_H
#define MUSIC_RIG_WIN32_SOUNDFONT_PATHS_H

#include "music_rig/core.h"

#include <stddef.h>
#include <wchar.h>

#define MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY ((size_t)4096)

/* Resolves the first standard Insync SoundFont library below the user profile. */
music_rig_result music_rig_win32_soundfont_root_resolve(
    const wchar_t *profile_root,
    wchar_t *root,
    size_t root_capacity
);

/* Joins a relative catalog path under a previously resolved library root. */
music_rig_result music_rig_win32_soundfont_path_join(
    const wchar_t *root,
    const wchar_t *relative_path,
    wchar_t *output,
    size_t output_capacity
);

#endif
