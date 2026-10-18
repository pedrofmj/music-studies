#include "music_rig/arturia_shared_sf2.h"

#include <stdbool.h>
#include <string.h>

static const uint8_t fader_ccs[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {
    73U, 75U, 79U, 72U, 80U, 81U, 82U, 83U, 85U,
};

static const uint8_t knob_ccs[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {
    74U, 71U, 76U, 77U, 93U, 18U, 19U, 16U, 17U,
};

music_rig_result music_rig_arturia_shared_sf2_plan_build(
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    music_rig_arturia_shared_sf2_plan *plan
)
{
    bool active[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {false};
    size_t index;

    if (patch_indexes == NULL || active_layers == NULL || plan == NULL ||
        patch_count == 0U || active_layer_count == 0U ||
        active_layer_count > MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(plan, 0, sizeof(*plan));
    for (index = 0U; index < MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT; ++index) {
        if (patch_indexes[index] >= patch_count) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    for (index = 0U; index < active_layer_count; ++index) {
        uint8_t layer = active_layers[index];
        size_t layer_index;
        music_rig_arturia_shared_sf2_route *route;

        if (layer == 0U || layer > MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        layer_index = (size_t)layer - 1U;
        if (active[layer_index]) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        active[layer_index] = true;
        route = &plan->routes[index];
        route->patch_index = patch_indexes[layer_index];
        route->layer = layer;
        route->mixer_slot = (uint8_t)(index + 1U);
        route->fader_cc = fader_ccs[layer_index];
        route->knob_cc = knob_ccs[layer_index];
    }
    plan->route_count = active_layer_count;
    return MUSIC_RIG_RESULT_OK;
}
