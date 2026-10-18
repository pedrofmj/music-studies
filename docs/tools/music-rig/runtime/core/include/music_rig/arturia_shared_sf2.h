#ifndef MUSIC_RIG_ARTURIA_SHARED_SF2_H
#define MUSIC_RIG_ARTURIA_SHARED_SF2_H

#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT ((size_t)9)

typedef struct music_rig_arturia_shared_sf2_route {
    uint16_t patch_index;
    uint8_t layer;
    uint8_t mixer_slot;
    uint8_t fader_cc;
    uint8_t knob_cc;
} music_rig_arturia_shared_sf2_route;

typedef struct music_rig_arturia_shared_sf2_plan {
    size_t route_count;
    music_rig_arturia_shared_sf2_route routes[
        MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT
    ];
} music_rig_arturia_shared_sf2_plan;

music_rig_result music_rig_arturia_shared_sf2_plan_build(
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    music_rig_arturia_shared_sf2_plan *plan
);

#endif
