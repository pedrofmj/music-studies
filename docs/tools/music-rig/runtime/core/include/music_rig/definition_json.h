#ifndef MUSIC_RIG_DEFINITION_JSON_H
#define MUSIC_RIG_DEFINITION_JSON_H

#include "music_rig/definition.h"

music_rig_result music_rig_definition_json_decode(
    void *context,
    const uint8_t *document,
    size_t document_size,
    music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
);

#endif
