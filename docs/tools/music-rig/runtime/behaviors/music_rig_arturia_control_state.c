#include "music_rig/arturia_control_state.h"

#include <string.h>

static const uint8_t control_ccs[MUSIC_RIG_ARTURIA_CONTROL_COUNT] = {
    73U, 75U, 79U, 72U, 80U, 81U, 82U, 83U, 85U,
    74U, 71U, 76U, 77U, 93U, 18U, 19U, 16U, 17U,
};

void music_rig_arturia_control_state_init(
    music_rig_arturia_control_state *state
)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->values[16] = UINT8_C(64);
}

int music_rig_arturia_control_state_index(uint8_t controller)
{
    size_t index;

    for (index = 0U; index < MUSIC_RIG_ARTURIA_CONTROL_COUNT; ++index) {
        if (control_ccs[index] == controller) {
            return (int)index;
        }
    }
    return -1;
}

bool music_rig_arturia_control_state_update(
    music_rig_arturia_control_state *state,
    uint8_t status,
    uint8_t controller,
    uint8_t value
)
{
    int index;

    if (state == NULL || status != UINT8_C(0xb0) || value > UINT8_C(127)) {
        return false;
    }
    index = music_rig_arturia_control_state_index(controller);
    if (index >= 0) {
        state->values[(size_t)index] = value;
        return true;
    }
    return false;
}

bool music_rig_arturia_control_state_message(
    const music_rig_arturia_control_state *state,
    size_t index,
    uint8_t message[3]
)
{
    if (state == NULL || message == NULL ||
        index >= MUSIC_RIG_ARTURIA_CONTROL_COUNT) {
        return false;
    }
    message[0] = UINT8_C(0xb0);
    message[1] = control_ccs[index];
    message[2] = state->values[index];
    return true;
}
