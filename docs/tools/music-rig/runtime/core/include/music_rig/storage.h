#ifndef MUSIC_RIG_STORAGE_H
#define MUSIC_RIG_STORAGE_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_STORAGE_ABI_VERSION UINT32_C(1)

typedef enum music_rig_storage_object {
    MUSIC_RIG_STORAGE_COMPILED_DEFINITION = 1,
    MUSIC_RIG_STORAGE_RUNTIME_STATE = 2
} music_rig_storage_object;

/*
 * The adapter resolves platform paths and performs I/O. Runtime-state replace
 * must be atomic; the portable core never observes a temporary state file.
 */
typedef struct music_rig_storage_adapter {
    uint32_t abi_version;
    void *context;
    music_rig_result (*read)(
        void *context,
        music_rig_storage_object object,
        uint8_t *output,
        size_t output_capacity,
        size_t *output_size
    );
    music_rig_result (*atomic_replace)(
        void *context,
        music_rig_storage_object object,
        const uint8_t *input,
        size_t input_size
    );
} music_rig_storage_adapter;

#endif
