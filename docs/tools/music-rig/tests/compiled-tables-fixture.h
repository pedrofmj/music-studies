#ifndef MUSIC_RIG_TEST_COMPILED_TABLES_FIXTURE_H
#define MUSIC_RIG_TEST_COMPILED_TABLES_FIXTURE_H

#include "music_rig/compiled_tables.h"
#include "music_rig/definition.h"

#include <string.h>

static void fixture_copy(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static music_rig_result init_compiled_tables_fixture(
    music_rig_compiled_tables *tables
)
{
    size_t index;

    memset(tables, 0, sizeof(*tables));
    tables->device_profile_count = UINT32_C(2);
    tables->input_binding_count = UINT32_C(2);
    tables->mapping_count = UINT32_C(2);
    tables->target_binding_count = UINT32_C(2);
    tables->ownership_count = UINT32_C(2);

    fixture_copy(tables->device_profiles[0].slot, "arturia-main");
    fixture_copy(
        tables->device_profiles[0].profile,
        "multi-instrument-rack"
    );
    fixture_copy(tables->device_profiles[0].hardware_preset, "default");
    tables->device_profiles[0].readiness =
        MUSIC_RIG_READINESS_CONTROL_ONLY;
    fixture_copy(tables->device_profiles[1].slot, "smc-mixer-main");
    fixture_copy(tables->device_profiles[1].profile, "eight-band-eq");
    fixture_copy(tables->device_profiles[1].hardware_preset, "default");
    tables->device_profiles[1].readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;

    for (index = 0; index < tables->input_binding_count; ++index) {
        music_rig_compiled_input_binding *input =
            &tables->input_bindings[index];

        fixture_copy(input->slot, tables->device_profiles[index].slot);
        fixture_copy(input->adapter, "mock-midi");
        fixture_copy(input->identity_strategy, "stable-id");
        fixture_copy(input->identity_value, tables->device_profiles[index].slot);
        input->status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
        input->endpoint_count = UINT16_C(1);
        fixture_copy(input->endpoints[0].purpose, "standard-midi");
        fixture_copy(input->endpoints[0].locator, "mock:input");
    }

    for (index = 0; index < tables->mapping_count; ++index) {
        music_rig_compiled_mapping *mapping = &tables->mappings[index];

        fixture_copy(mapping->mapping,
            index == 0U ? "main-volume" : "band-1");
        fixture_copy(mapping->control,
            index == 0U ? "encoder-1" : "fader-1");
        fixture_copy(mapping->target,
            index == 0U ? "master.volume" : "eq.band-1");
        mapping->profile_index = (uint16_t)index;
        mapping->event_type = MUSIC_RIG_MIDI_EVENT_CC;
        mapping->edge = MUSIC_RIG_MIDI_EDGE_CHANGE;
        mapping->channel = UINT8_C(1);
        mapping->number = index == 0U ? UINT8_C(1) : UINT8_C(2);
        mapping->behavior = MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE;
        mapping->transform = MUSIC_RIG_TRANSFORM_DIRECT;
        mapping->relative_encoding = MUSIC_RIG_RELATIVE_ENCODING_NONE;
        mapping->takeover = MUSIC_RIG_TAKEOVER_PICKUP;
    }

    fixture_copy(tables->target_bindings[0].target, "eq.band-1");
    fixture_copy(tables->target_bindings[0].adapter, "mock-control");
    fixture_copy(tables->target_bindings[0].locator, "mock:eq-band-1");
    tables->target_bindings[0].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
    fixture_copy(tables->target_bindings[1].target, "master.volume");
    fixture_copy(tables->target_bindings[1].adapter, "mock-control");
    fixture_copy(tables->target_bindings[1].locator, "mock:master-volume");
    tables->target_bindings[1].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;

    for (index = 0; index < tables->ownership_count; ++index) {
        music_rig_compiled_ownership *ownership = &tables->ownership[index];

        ownership->kind = MUSIC_RIG_OWNERSHIP_KIND_PARAMETER;
        ownership->mode = MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE;
        fixture_copy(ownership->target,
            index == 0U ? "eq.band-1" : "master.volume");
        ownership->owner_count = UINT16_C(1);
        ownership->owners[0].scope =
            MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE;
        ownership->owners[0].profile_index = (uint16_t)index;
        fixture_copy(
            ownership->owners[0].slot,
            tables->device_profiles[index].slot
        );
        fixture_copy(
            ownership->owners[0].profile,
            tables->device_profiles[index].profile
        );
    }

    return music_rig_compiled_tables_prepare(
        tables,
        UINT32_C(2),
        UINT32_C(2),
        UINT32_C(2),
        UINT32_C(2)
    );
}

static inline music_rig_result init_alternate_prepared_definition_fixture(
    music_rig_compiled_tables *tables,
    music_rig_compiled_definition *definition,
    music_rig_prepared_definition *prepared
)
{
    music_rig_result result;

    if (tables == NULL || definition == NULL || prepared == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = init_compiled_tables_fixture(tables);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    fixture_copy(
        tables->device_profiles[1].profile,
        "multilevel-volume"
    );
    fixture_copy(tables->mappings[1].mapping, "group-1-volume");
    fixture_copy(tables->mappings[1].target, "master.volume");
    fixture_copy(
        tables->ownership[1].owners[0].profile,
        "multilevel-volume"
    );
    result = music_rig_compiled_tables_prepare(
        tables,
        tables->device_profile_count,
        tables->mapping_count,
        tables->target_binding_count,
        tables->ownership_count
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    memset(definition, 0, sizeof(*definition));
    definition->schema_version = MUSIC_RIG_COMPILED_DEFINITION_VERSION;
    definition->generation_id = UINT64_C(42);
    definition->fingerprint[0] = UINT8_C(0x42);
    fixture_copy(definition->rig_id, "pedro-performance-rig");
    fixture_copy(
        definition->active_rig_profile,
        "multilevel-volume-mixed-pads"
    );
    fixture_copy(definition->platform_binding_id, "airstar-current");
    fixture_copy(definition->platform, "linux");
    definition->device_profile_count = tables->device_profile_count;
    definition->mapping_count = tables->mapping_count;
    definition->target_binding_count = tables->target_binding_count;
    definition->ownership_count = tables->ownership_count;
    definition->control_only = true;
    definition->graph_delta_empty = true;
    definition->authoring_only = true;
    prepared->definition = definition;
    prepared->tables = tables;
    return MUSIC_RIG_RESULT_OK;
}

#endif
