#ifndef MUSIC_RIG_ARTURIA_CONTROL_STATE_H
#define MUSIC_RIG_ARTURIA_CONTROL_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_ARTURIA_CONTROL_COUNT 18U

typedef struct music_rig_arturia_control_state {
    uint8_t values[MUSIC_RIG_ARTURIA_CONTROL_COUNT];
} music_rig_arturia_control_state;

void music_rig_arturia_control_state_init(
    music_rig_arturia_control_state *state
);

int music_rig_arturia_control_state_index(uint8_t controller);

bool music_rig_arturia_control_state_update(
    music_rig_arturia_control_state *state,
    uint8_t status,
    uint8_t controller,
    uint8_t value
);

bool music_rig_arturia_control_state_message(
    const music_rig_arturia_control_state *state,
    size_t index,
    uint8_t message[3]
);

#endif
