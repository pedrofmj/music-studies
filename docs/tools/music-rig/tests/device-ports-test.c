#include "music_rig/device_ports.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <string.h>

static int test_port_catalogue(void)
{
    static music_rig_compiled_tables tables;
    static music_rig_compiled_tables changed_profile;
    static music_rig_compiled_tables changed_slot;
    music_rig_device_port_catalogue catalogue;
    music_rig_device_port_catalogue matching;
    music_rig_device_port_catalogue different;
    const music_rig_device_port *port;

    if (init_compiled_tables_fixture(&tables) != MUSIC_RIG_RESULT_OK) {
        fputs("device-port fixture failed\n", stderr);
        return 1;
    }
    if (music_rig_device_port_catalogue_build(&tables, &catalogue) !=
            MUSIC_RIG_RESULT_OK ||
        catalogue.count != 4U) {
        fputs("device-port catalogue build failed\n", stderr);
        return 1;
    }
    port = music_rig_device_port_lookup(
        &catalogue,
        "arturia-main",
        MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT
    );
    if (port == NULL ||
        strcmp(port->id, "device.arturia-main.midi-input") != 0) {
        fputs("stable input port identity is incorrect\n", stderr);
        return 1;
    }
    port = music_rig_device_port_lookup(
        &catalogue,
        "smc-mixer-main",
        MUSIC_RIG_DEVICE_PORT_DIRECTION_OUTPUT
    );
    if (port == NULL ||
        strcmp(port->id, "device.smc-mixer-main.midi-output") != 0 ||
        music_rig_device_port_lookup(
            &catalogue,
            "missing",
            MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT
        ) != NULL) {
        fputs("stable output port lookup is incorrect\n", stderr);
        return 1;
    }

    changed_profile = tables;
    fixture_copy(changed_profile.device_profiles[0].profile, "organ");
    fixture_copy(changed_profile.ownership[0].owners[0].profile, "organ");
    if (music_rig_compiled_tables_prepare(
            &changed_profile,
            UINT32_C(2), UINT32_C(2), UINT32_C(2), UINT32_C(2)
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_port_catalogue_build(
            &changed_profile,
            &matching
        ) != MUSIC_RIG_RESULT_OK ||
        !music_rig_device_port_catalogues_match(&catalogue, &matching)) {
        fputs("profile-only change altered stable ports\n", stderr);
        return 1;
    }

    changed_slot = tables;
    fixture_copy(changed_slot.device_profiles[0].slot, "arturia-secondary");
    fixture_copy(changed_slot.input_bindings[0].slot, "arturia-secondary");
    fixture_copy(changed_slot.ownership[0].owners[0].slot,
        "arturia-secondary");
    if (music_rig_compiled_tables_prepare(
            &changed_slot,
            UINT32_C(2), UINT32_C(2), UINT32_C(2), UINT32_C(2)
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_port_catalogue_build(&changed_slot, &different) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_device_port_catalogues_match(&catalogue, &different)) {
        fputs("changed slot catalogue was treated as stable\n", stderr);
        return 1;
    }

    changed_slot.device_profiles[0].slot[0] = 'A';
    changed_slot.input_bindings[0].slot[0] = 'A';
    if (music_rig_device_port_catalogue_build(&changed_slot, &different) !=
            MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_device_port_catalogue_build(NULL, &different) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_device_port_catalogue_build(&tables, NULL) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid device-port input was accepted\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    return test_port_catalogue();
}
