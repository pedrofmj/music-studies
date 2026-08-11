#ifndef MUSIC_RIG_DEFINITION_H
#define MUSIC_RIG_DEFINITION_H

#include "music_rig/compiled_tables.h"
#include "music_rig/core.h"
#include "music_rig/state.h"
#include "music_rig/storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_COMPILED_DEFINITION_VERSION UINT32_C(1)
#define MUSIC_RIG_DEFINITION_ID_CAPACITY MUSIC_RIG_IDENTIFIER_CAPACITY

typedef struct music_rig_compiled_definition {
    uint32_t schema_version;
    uint64_t generation_id;
    uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    char rig_id[MUSIC_RIG_DEFINITION_ID_CAPACITY];
    char active_rig_profile[MUSIC_RIG_DEFINITION_ID_CAPACITY];
    char platform_binding_id[MUSIC_RIG_DEFINITION_ID_CAPACITY];
    char platform[MUSIC_RIG_DEFINITION_ID_CAPACITY];
    uint32_t device_profile_count;
    uint32_t mapping_count;
    uint32_t target_binding_count;
    uint32_t ownership_count;
    bool control_only;
    bool graph_delta_empty;
    bool authoring_only;
} music_rig_compiled_definition;

typedef struct music_rig_definition_decoder {
    void *context;
    music_rig_result (*decode)(
        void *context,
        const uint8_t *document,
        size_t document_size,
        music_rig_compiled_definition *definition,
        music_rig_compiled_tables *tables
    );
} music_rig_definition_decoder;

music_rig_result music_rig_definition_fingerprint_parse(
    const char *fingerprint,
    size_t fingerprint_size,
    uint8_t *output,
    size_t output_size
);

music_rig_result music_rig_definition_validate(
    const music_rig_compiled_definition *definition
);

music_rig_result music_rig_definition_generation_init(
    const music_rig_compiled_definition *definition,
    const music_rig_compiled_tables *tables,
    music_rig_generation *generation
);

music_rig_result music_rig_definition_load(
    const music_rig_storage_adapter *storage,
    const music_rig_definition_decoder *decoder,
    uint8_t *document_buffer,
    size_t document_capacity,
    const uint8_t *expected_fingerprint,
    size_t expected_fingerprint_size,
    music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
);

#endif
