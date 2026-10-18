#include "music_rig/arturia_shared_sf2.h"

#include <stddef.h>
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    static const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {
        10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U,
    };
    static const uint8_t worship_layers[] = {1U, 2U, 3U, 6U, 8U, 9U};
    static const uint8_t ambient_layers[] = {3U, 7U, 8U, 9U};
    music_rig_arturia_shared_sf2_plan plan;
    size_t index;

    CHECK(music_rig_arturia_shared_sf2_plan_build(
        patch_indexes,
        UINT16_C(24),
        worship_layers,
        sizeof(worship_layers),
        &plan
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(plan.route_count == sizeof(worship_layers));
    for (index = 0U; index < plan.route_count; ++index) {
        const music_rig_arturia_shared_sf2_route *route = &plan.routes[index];

        CHECK(route->layer == worship_layers[index]);
        CHECK((size_t)route->mixer_slot == index + 1U);
        CHECK(route->patch_index == patch_indexes[route->layer - 1U]);
    }
    CHECK(plan.routes[2].layer == 3U);
    CHECK(plan.routes[2].fader_cc == 79U);
    CHECK(plan.routes[2].knob_cc == 76U);

    CHECK(music_rig_arturia_shared_sf2_plan_build(
        patch_indexes,
        UINT16_C(24),
        ambient_layers,
        sizeof(ambient_layers),
        &plan
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(plan.route_count == sizeof(ambient_layers));
    CHECK(plan.routes[0].layer == 3U);
    CHECK(plan.routes[0].fader_cc == 79U);
    CHECK(plan.routes[0].knob_cc == 76U);

    CHECK(music_rig_arturia_shared_sf2_plan_build(
        patch_indexes,
        UINT16_C(24),
        NULL,
        1U,
        &plan
    ) == MUSIC_RIG_RESULT_INVALID_ARGUMENT);
    {
        static const uint8_t duplicate_layers[] = {3U, 3U};

        CHECK(music_rig_arturia_shared_sf2_plan_build(
            patch_indexes,
            UINT16_C(24),
            duplicate_layers,
            sizeof(duplicate_layers),
            &plan
        ) == MUSIC_RIG_RESULT_INVALID_DATA);
    }
    {
        static const uint8_t invalid_layer[] = {0U};

        CHECK(music_rig_arturia_shared_sf2_plan_build(
            patch_indexes,
            UINT16_C(24),
            invalid_layer,
            sizeof(invalid_layer),
            &plan
        ) == MUSIC_RIG_RESULT_INVALID_DATA);
    }
    {
        uint16_t invalid_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {
            0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 24U,
        };

        CHECK(music_rig_arturia_shared_sf2_plan_build(
            invalid_indexes,
            UINT16_C(24),
            worship_layers,
            sizeof(worship_layers),
            &plan
        ) == MUSIC_RIG_RESULT_INVALID_DATA);
    }
    return 0;
}
