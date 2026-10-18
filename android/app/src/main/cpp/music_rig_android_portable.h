#ifndef MUSIC_RIG_ANDROID_PORTABLE_H
#define MUSIC_RIG_ANDROID_PORTABLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_RIG_ANDROID_LAYER_COUNT 9U

typedef struct music_rig_android_route {
    uint16_t patch_index;
    uint8_t layer;
    uint8_t mixer_slot;
    uint8_t fader_cc;
    uint8_t knob_cc;
} music_rig_android_route;

void *music_rig_android_control_state_create(void);
void music_rig_android_control_state_destroy(void *state);
int music_rig_android_control_state_update(
    void *state,
    uint8_t status,
    uint8_t controller,
    uint8_t value
);
int music_rig_android_plan_build(
    const uint16_t patch_indexes[MUSIC_RIG_ANDROID_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    music_rig_android_route routes[MUSIC_RIG_ANDROID_LAYER_COUNT],
    size_t *route_count
);

#ifdef __cplusplus
}
#endif

#endif
