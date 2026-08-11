#ifndef MUSIC_RIG_FILE_STORAGE_H
#define MUSIC_RIG_FILE_STORAGE_H

#include "music_rig/storage.h"

#include <stdint.h>

#define MUSIC_RIG_FILE_STORAGE_ABI_VERSION UINT32_C(1)

/*
 * Paths are explicit UTF-8 host inputs. The adapter does not select production
 * configuration or state locations and does not create parent directories.
 */
typedef struct music_rig_file_storage {
    uint32_t abi_version;
    const char *definition_path;
    const char *state_path;
} music_rig_file_storage;

music_rig_result music_rig_file_storage_init(
    music_rig_file_storage *storage,
    const char *definition_path,
    const char *state_path,
    music_rig_storage_adapter *adapter
);

#endif
