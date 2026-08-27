#include "music_rig/compiled_tables.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static int initialize_fixture(music_rig_compiled_tables *tables,
    const char *profile, const char *target)
{
    memset(tables, 0, sizeof(*tables));
    tables->device_profile_count = 2U;
    tables->input_binding_count = 2U;
    tables->mapping_count = 2U;
    tables->target_binding_count = 2U;
    tables->ownership_count = 2U;
    copy_text(tables->device_profiles[0].slot, "arturia-main");
    copy_text(tables->device_profiles[0].profile, "organ");
    copy_text(tables->device_profiles[0].hardware_preset, "default");
    tables->device_profiles[0].readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;
    copy_text(tables->device_profiles[1].slot, "smc-mixer-main");
    copy_text(tables->device_profiles[1].profile, profile);
    copy_text(tables->device_profiles[1].hardware_preset, "default");
    tables->device_profiles[1].readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;
    for (size_t index = 0U; index < 2U; ++index) {
        copy_text(tables->input_bindings[index].slot,
            tables->device_profiles[index].slot);
        copy_text(tables->input_bindings[index].adapter, "mock-midi");
        copy_text(tables->input_bindings[index].identity_strategy, "stable-id");
        copy_text(tables->input_bindings[index].identity_value, "same-device");
        tables->input_bindings[index].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
        tables->input_bindings[index].endpoint_count = 1U;
        copy_text(tables->input_bindings[index].endpoints[0].purpose, "midi");
        copy_text(tables->input_bindings[index].endpoints[0].locator, "mock:midi");
    }
    for (size_t index = 0U; index < 2U; ++index) {
        music_rig_compiled_mapping *mapping = &tables->mappings[index];
        copy_text(mapping->mapping, "mapping");
        copy_text(mapping->control, "control");
        copy_text(mapping->target, index == 0U ? "organ.volume" : target);
        mapping->profile_index = (uint16_t)index;
        mapping->event_type = MUSIC_RIG_MIDI_EVENT_CC;
        mapping->edge = MUSIC_RIG_MIDI_EDGE_CHANGE;
        mapping->channel = 1U;
        mapping->number = (uint8_t)(index + 1U);
        mapping->behavior = MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE;
        mapping->transform = MUSIC_RIG_TRANSFORM_DIRECT;
        mapping->relative_encoding = MUSIC_RIG_RELATIVE_ENCODING_NONE;
        mapping->takeover = MUSIC_RIG_TAKEOVER_PICKUP;
    }
    copy_text(tables->target_bindings[0].target, "organ.volume");
    copy_text(tables->target_bindings[0].adapter, "mock");
    copy_text(tables->target_bindings[0].locator, "mock:organ");
    tables->target_bindings[0].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
    copy_text(tables->target_bindings[1].target, target);
    copy_text(tables->target_bindings[1].adapter, "mock");
    copy_text(tables->target_bindings[1].locator, "mock:mixer");
    tables->target_bindings[1].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
    for (size_t index = 0U; index < 2U; ++index) {
        music_rig_compiled_ownership *ownership = &tables->ownership[index];
        ownership->kind = MUSIC_RIG_OWNERSHIP_KIND_PARAMETER;
        ownership->mode = MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE;
        copy_text(ownership->target, index == 0U ? "organ.volume" : target);
        ownership->owner_count = 1U;
        ownership->owners[0].scope = MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE;
        ownership->owners[0].profile_index = (uint16_t)index;
        copy_text(ownership->owners[0].slot, tables->device_profiles[index].slot);
        copy_text(ownership->owners[0].profile, tables->device_profiles[index].profile);
    }
    return music_rig_compiled_tables_prepare(tables, 2U, 2U, 2U, 2U) ==
        MUSIC_RIG_RESULT_OK;
}

int main(void)
{
    music_rig_compiled_tables base;
    music_rig_compiled_tables source;
    music_rig_compiled_tables output;

    if (!initialize_fixture(&base, "eq", "volume.level") ||
        !initialize_fixture(&source, "volume", "volume.level") ||
        (copy_text(source.target_bindings[2].target, "volume.zz-extra"),
         copy_text(source.target_bindings[2].adapter, "mock"),
         copy_text(source.target_bindings[2].locator, "mock:extra"),
         source.target_bindings[2].status =
             MUSIC_RIG_BINDING_STATUS_AVAILABLE,
         source.target_binding_count = 3U,
        copy_text(source.ownership[2].target, "volume.zz-extra"),
         source.ownership[2].kind = MUSIC_RIG_OWNERSHIP_KIND_PARAMETER,
         source.ownership[2].mode = MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE,
         source.ownership[2].owner_count = 1U,
         source.ownership[2].owners[0].scope =
             MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE,
         source.ownership[2].owners[0].profile_index = 1U,
         copy_text(source.ownership[2].owners[0].slot, "smc-mixer-main"),
         copy_text(source.ownership[2].owners[0].profile, "volume"),
         source.ownership_count = 3U,
         music_rig_compiled_tables_prepare(&source, 2U, 2U, 3U, 3U) !=
             MUSIC_RIG_RESULT_OK) ||
        music_rig_compiled_tables_compose_device(
            &base, &source, "smc-mixer-main", &output
        ) != MUSIC_RIG_RESULT_OK ||
        strcmp(output.device_profiles[1].profile, "volume") != 0 ||
        strcmp(output.mappings[1].target, "volume.level") != 0 ||
        output.target_binding_count != 3U || output.ownership_count != 3U ||
        music_rig_compiled_tables_validate(&output, 2U, 2U, 3U, 3U) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("compiled table composition failed\n", stderr);
        return 1;
    }
    return 0;
}
