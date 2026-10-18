#include "music_rig_android_portable.h"

#include "music_rig/arturia_control_state.h"
#include "music_rig/arturia_shared_sf2.h"

#include <stdlib.h>

_Static_assert(MUSIC_RIG_ANDROID_LAYER_COUNT ==
        MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT,
    "Android route bridge must match the portable layer count");
_Static_assert(MUSIC_RIG_ARTURIA_CONTROL_COUNT == 18U,
    "Android control bridge must match the portable control count");

void *music_rig_android_control_state_create(void)
{
    music_rig_arturia_control_state *state =
        (music_rig_arturia_control_state *)malloc(sizeof(*state));

    if (state != NULL) {
        music_rig_arturia_control_state_init(state);
    }
    return state;
}

void music_rig_android_control_state_destroy(void *opaque_state)
{
    free(opaque_state);
}

int music_rig_android_control_state_update(
    void *opaque_state,
    uint8_t status,
    uint8_t controller,
    uint8_t value
)
{
    return music_rig_arturia_control_state_update(
        (music_rig_arturia_control_state *)opaque_state,
        status,
        controller,
        value
    ) ? 1 : 0;
}

int music_rig_android_plan_build(
    const uint16_t patch_indexes[MUSIC_RIG_ANDROID_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    music_rig_android_route routes[MUSIC_RIG_ANDROID_LAYER_COUNT],
    size_t *route_count
)
{
    music_rig_arturia_shared_sf2_plan plan;
    music_rig_result result;
    size_t index;

    if (routes == NULL || route_count == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_arturia_shared_sf2_plan_build(
        patch_indexes,
        patch_count,
        active_layers,
        active_layer_count,
        &plan
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        *route_count = 0U;
        return (int)result;
    }
    for (index = 0U; index < plan.route_count; ++index) {
        routes[index].patch_index = plan.routes[index].patch_index;
        routes[index].layer = plan.routes[index].layer;
        routes[index].mixer_slot = plan.routes[index].mixer_slot;
        routes[index].fader_cc = plan.routes[index].fader_cc;
        routes[index].knob_cc = plan.routes[index].knob_cc;
    }
    *route_count = plan.route_count;
    return (int)MUSIC_RIG_RESULT_OK;
}
