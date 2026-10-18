#include "music_rig/arturia_control_state.h"

#include <stddef.h>
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    static const uint8_t expected_ccs[MUSIC_RIG_ARTURIA_CONTROL_COUNT] = {
        73U, 75U, 79U, 72U, 80U, 81U, 82U, 83U, 85U,
        74U, 71U, 76U, 77U, 93U, 18U, 19U, 16U, 17U,
    };
    music_rig_arturia_control_state state;
    uint8_t message[3];
    size_t index;

    music_rig_arturia_control_state_init(&state);
    for (index = 0U; index < MUSIC_RIG_ARTURIA_CONTROL_COUNT; ++index) {
        CHECK(music_rig_arturia_control_state_index(expected_ccs[index]) ==
              (int)index);
        CHECK(music_rig_arturia_control_state_message(
            &state, index, message
        ));
        CHECK(message[0] == UINT8_C(0xb0));
        CHECK(message[1] == expected_ccs[index]);
        CHECK(message[2] == (index == 16U ? UINT8_C(64) : UINT8_C(0)));
    }

    for (index = 0U; index < MUSIC_RIG_ARTURIA_CONTROL_COUNT; ++index) {
        uint8_t next_value = (uint8_t)(index * 7U);

        CHECK(music_rig_arturia_control_state_update(
            &state, UINT8_C(0xb0), expected_ccs[index], next_value
        ));
        CHECK(music_rig_arturia_control_state_message(
            &state, index, message
        ));
        CHECK(message[1] == expected_ccs[index]);
        CHECK(message[2] == next_value);
    }

    CHECK(!music_rig_arturia_control_state_update(
        &state, UINT8_C(0xb1), expected_ccs[0], UINT8_C(42)
    ));
    CHECK(!music_rig_arturia_control_state_update(
        &state, UINT8_C(0xb0), UINT8_C(1), UINT8_C(42)
    ));
    CHECK(!music_rig_arturia_control_state_update(
        &state, UINT8_C(0xb0), expected_ccs[0], UINT8_C(128)
    ));
    CHECK(!music_rig_arturia_control_state_update(
        NULL, UINT8_C(0xb0), expected_ccs[0], UINT8_C(42)
    ));
    CHECK(!music_rig_arturia_control_state_message(
        &state, MUSIC_RIG_ARTURIA_CONTROL_COUNT, message
    ));
    CHECK(!music_rig_arturia_control_state_message(
        NULL, 0U, message
    ));
    CHECK(!music_rig_arturia_control_state_message(
        &state, 0U, NULL
    ));
    CHECK(music_rig_arturia_control_state_index(UINT8_C(1)) == -1);

    return 0;
}
