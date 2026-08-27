#include "music_rig/compiled_tables.h"

#include <string.h>

_Static_assert(
    (MUSIC_RIG_MAPPING_DISPATCH_CAPACITY &
        (MUSIC_RIG_MAPPING_DISPATCH_CAPACITY - 1U)) == 0,
    "mapping dispatch capacity must be a power of two"
);
_Static_assert(
    MUSIC_RIG_MAPPING_DISPATCH_CAPACITY > MUSIC_RIG_MAPPING_CAPACITY,
    "mapping dispatch table must have spare capacity"
);

static bool bounded_text(const char *value, size_t capacity, bool allow_empty)
{
    size_t index;

    if (value == NULL || (!allow_empty && value[0] == '\0')) {
        return false;
    }
    for (index = 0; index < capacity; ++index) {
        if (value[index] == '\0') {
            return allow_empty || index != 0;
        }
    }
    return false;
}

static bool valid_event_type(music_rig_midi_event_type value)
{
    return value >= MUSIC_RIG_MIDI_EVENT_CC &&
        value <= MUSIC_RIG_MIDI_EVENT_PROGRAM_CHANGE;
}

static bool valid_edge(music_rig_midi_edge value)
{
    return value >= MUSIC_RIG_MIDI_EDGE_ANY &&
        value <= MUSIC_RIG_MIDI_EDGE_RELEASE;
}

static bool valid_behavior(music_rig_control_behavior value)
{
    return value >= MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE &&
        value <= MUSIC_RIG_CONTROL_BEHAVIOR_TOGGLE;
}

static bool valid_transform(music_rig_transform_type value)
{
    return value >= MUSIC_RIG_TRANSFORM_DIRECT &&
        value <= MUSIC_RIG_TRANSFORM_RELATIVE;
}

static bool valid_encoding(music_rig_relative_encoding value)
{
    return value >= MUSIC_RIG_RELATIVE_ENCODING_NONE &&
        value <= MUSIC_RIG_RELATIVE_ENCODING_TWOS_COMPLEMENT;
}

static bool valid_takeover(music_rig_takeover_mode value)
{
    return value >= MUSIC_RIG_TAKEOVER_NONE &&
        value <= MUSIC_RIG_TAKEOVER_IGNORE_UNTIL_MOVED;
}

static bool valid_mapping(const music_rig_compiled_mapping *mapping)
{
    if (!bounded_text(mapping->mapping, sizeof(mapping->mapping), false) ||
        !bounded_text(mapping->control, sizeof(mapping->control), false) ||
        !bounded_text(mapping->target, sizeof(mapping->target), false) ||
        !valid_event_type(mapping->event_type) || !valid_edge(mapping->edge) ||
        mapping->channel < UINT8_C(1) || mapping->channel > UINT8_C(16) ||
        mapping->number > UINT8_C(127) ||
        !valid_behavior(mapping->behavior) ||
        !valid_transform(mapping->transform) ||
        !valid_encoding(mapping->relative_encoding) ||
        !valid_takeover(mapping->takeover)) {
        return false;
    }
    if (mapping->behavior == MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE &&
        (mapping->relative_encoding == MUSIC_RIG_RELATIVE_ENCODING_NONE ||
         mapping->transform != MUSIC_RIG_TRANSFORM_RELATIVE)) {
        return false;
    }
    if (mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE &&
        mapping->relative_encoding != MUSIC_RIG_RELATIVE_ENCODING_NONE) {
        return false;
    }
    if (mapping->transform == MUSIC_RIG_TRANSFORM_RELATIVE &&
        (mapping->relative_encoding == MUSIC_RIG_RELATIVE_ENCODING_NONE ||
         mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE)) {
        return false;
    }
    if ((mapping->behavior == MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE &&
         mapping->takeover == MUSIC_RIG_TAKEOVER_NONE) ||
        (mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE &&
         mapping->takeover != MUSIC_RIG_TAKEOVER_NONE)) {
        return false;
    }
    if (mapping->transform == MUSIC_RIG_TRANSFORM_SCALE &&
        mapping->input_max <= mapping->input_min) {
        return false;
    }
    return true;
}

static uint32_t mapping_hash(
    uint16_t profile_index,
    music_rig_midi_event_type event_type,
    uint8_t channel,
    uint8_t number
)
{
    uint32_t value = (uint32_t)profile_index;

    value = value * UINT32_C(16777619) ^ (uint32_t)event_type;
    value = value * UINT32_C(16777619) ^ (uint32_t)channel;
    value = value * UINT32_C(16777619) ^ (uint32_t)number;
    return value;
}

static bool mapping_matches(
    const music_rig_compiled_mapping *mapping,
    uint16_t profile_index,
    music_rig_midi_event_type event_type,
    uint8_t channel,
    uint8_t number
)
{
    return mapping->profile_index == profile_index &&
        mapping->event_type == event_type && mapping->channel == channel &&
        mapping->number == number;
}

static bool ownership_has_profile(
    const music_rig_compiled_ownership *ownership,
    uint16_t profile_index
)
{
    size_t index;

    for (index = 0U; index < ownership->owner_count; ++index) {
        if (ownership->owners[index].scope ==
                MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE &&
            ownership->owners[index].profile_index == profile_index) {
            return true;
        }
    }
    return false;
}

static const music_rig_compiled_ownership *find_ownership(
    const music_rig_compiled_tables *tables,
    music_rig_ownership_kind kind,
    const char *target
)
{
    size_t index;

    for (index = 0U; index < tables->ownership_count; ++index) {
        if (tables->ownership[index].kind == kind && strcmp(
                tables->ownership[index].target, target
            ) == 0) {
            return &tables->ownership[index];
        }
    }
    return NULL;
}

static music_rig_result remap_ownership(
    const music_rig_compiled_ownership *source,
    const music_rig_compiled_tables *current,
    uint16_t source_index,
    uint16_t base_index,
    music_rig_compiled_ownership *output
)
{
    size_t index;

    *output = *source;
    for (index = 0U; index < output->owner_count; ++index) {
        music_rig_compiled_owner *owner = &output->owners[index];
        uint16_t base_owner_index;

        if (owner->scope != MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE) {
            continue;
        }
        if (owner->profile_index == source_index) {
            base_owner_index = base_index;
        } else if (music_rig_compiled_profile_index(
                current, owner->slot, &base_owner_index
            ) != MUSIC_RIG_RESULT_OK) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        owner->profile_index = base_owner_index;
        memcpy(owner->slot, current->device_profiles[base_owner_index].slot,
            sizeof(owner->slot));
        memcpy(owner->profile,
            current->device_profiles[base_owner_index].profile,
            sizeof(owner->profile));
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result validate_structure(
    const music_rig_compiled_tables *tables,
    uint32_t expected_device_profiles,
    uint32_t expected_mappings,
    uint32_t expected_target_bindings,
    uint32_t expected_ownership
)
{
    size_t index;

    if (tables == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (tables->device_profile_count != expected_device_profiles ||
        tables->input_binding_count != expected_device_profiles ||
        tables->mapping_count != expected_mappings ||
        tables->target_binding_count != expected_target_bindings ||
        tables->ownership_count != expected_ownership ||
        tables->device_profile_count == 0 ||
        tables->device_profile_count > MUSIC_RIG_DEVICE_PROFILE_CAPACITY ||
        tables->mapping_count == 0 ||
        tables->mapping_count > MUSIC_RIG_MAPPING_CAPACITY ||
        tables->target_binding_count == 0 ||
        tables->target_binding_count > MUSIC_RIG_TARGET_BINDING_CAPACITY ||
        tables->ownership_count == 0 ||
        tables->ownership_count > MUSIC_RIG_OWNERSHIP_CAPACITY) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }

    for (index = 0; index < tables->device_profile_count; ++index) {
        const music_rig_compiled_device_profile *profile =
            &tables->device_profiles[index];
        const music_rig_compiled_input_binding *input =
            &tables->input_bindings[index];
        size_t endpoint_index;

        if (!bounded_text(profile->slot, sizeof(profile->slot), false) ||
            !bounded_text(profile->profile, sizeof(profile->profile), false) ||
            !bounded_text(profile->hardware_preset,
                sizeof(profile->hardware_preset), false) ||
            profile->readiness < MUSIC_RIG_READINESS_CONTROL_ONLY ||
            profile->readiness > MUSIC_RIG_READINESS_COLD ||
            (index != 0 && strcmp(
                tables->device_profiles[index - 1U].slot,
                profile->slot
            ) >= 0) ||
            !bounded_text(input->slot, sizeof(input->slot), false) ||
            strcmp(input->slot, profile->slot) != 0 ||
            !bounded_text(input->adapter, sizeof(input->adapter), false) ||
            !bounded_text(input->identity_strategy,
                sizeof(input->identity_strategy), false) ||
            !bounded_text(input->identity_value,
                sizeof(input->identity_value), false) ||
            input->status != MUSIC_RIG_BINDING_STATUS_AVAILABLE ||
            input->endpoint_count == 0 ||
            input->endpoint_count > MUSIC_RIG_INPUT_ENDPOINT_CAPACITY) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        for (endpoint_index = 0;
             endpoint_index < input->endpoint_count;
             ++endpoint_index) {
            const music_rig_compiled_input_endpoint *endpoint =
                &input->endpoints[endpoint_index];

            if (!bounded_text(endpoint->purpose,
                    sizeof(endpoint->purpose), false) ||
                !bounded_text(endpoint->locator,
                    sizeof(endpoint->locator), false) ||
                (endpoint_index != 0 && strcmp(
                    input->endpoints[endpoint_index - 1U].purpose,
                    endpoint->purpose
                ) >= 0)) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
    }

    for (index = 0; index < tables->mapping_count; ++index) {
        const music_rig_compiled_mapping *mapping = &tables->mappings[index];

        if (!valid_mapping(mapping) ||
            mapping->profile_index >= tables->device_profile_count) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }

    for (index = 0; index < tables->target_binding_count; ++index) {
        const music_rig_compiled_target_binding *target =
            &tables->target_bindings[index];

        if (!bounded_text(target->target, sizeof(target->target), false) ||
            !bounded_text(target->adapter, sizeof(target->adapter), false) ||
            !bounded_text(target->locator, sizeof(target->locator), false) ||
            target->status != MUSIC_RIG_BINDING_STATUS_AVAILABLE ||
            (index != 0 && strcmp(
                tables->target_bindings[index - 1U].target,
                target->target
            ) >= 0)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }

    for (index = 0; index < tables->ownership_count; ++index) {
        const music_rig_compiled_ownership *ownership =
            &tables->ownership[index];
        size_t owner_index;
        size_t previous;

        if (ownership->kind < MUSIC_RIG_OWNERSHIP_KIND_EFFECT ||
            ownership->kind > MUSIC_RIG_OWNERSHIP_KIND_STATE_KEY ||
            ownership->mode < MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE ||
            ownership->mode > MUSIC_RIG_OWNERSHIP_MODE_READ_ONLY ||
            !bounded_text(ownership->target,
                sizeof(ownership->target), false) ||
            ownership->owner_count == 0 ||
            ownership->owner_count > MUSIC_RIG_OWNERS_PER_ENTRY_CAPACITY) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        for (previous = 0; previous < index; ++previous) {
            if (tables->ownership[previous].kind == ownership->kind &&
                strcmp(tables->ownership[previous].target,
                    ownership->target) == 0) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
        for (owner_index = 0;
             owner_index < ownership->owner_count;
             ++owner_index) {
            const music_rig_compiled_owner *owner =
                &ownership->owners[owner_index];

            if (!bounded_text(owner->profile,
                    sizeof(owner->profile), false)) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
            if (owner->scope == MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE) {
                if (owner->profile_index >= tables->device_profile_count ||
                    !bounded_text(owner->slot, sizeof(owner->slot), false) ||
                    strcmp(owner->slot, tables->device_profiles[
                        owner->profile_index
                    ].slot) != 0 ||
                    strcmp(owner->profile, tables->device_profiles[
                        owner->profile_index
                    ].profile) != 0) {
                    return MUSIC_RIG_RESULT_INVALID_DATA;
                }
            } else if (owner->scope == MUSIC_RIG_OWNER_SCOPE_RIG_PROFILE) {
                if (owner->profile_index != MUSIC_RIG_TABLE_INDEX_NONE ||
                    !bounded_text(owner->slot, sizeof(owner->slot), true) ||
                    owner->slot[0] != '\0') {
                    return MUSIC_RIG_RESULT_INVALID_DATA;
                }
            } else {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_compiled_tables_prepare(
    music_rig_compiled_tables *tables,
    uint32_t expected_device_profiles,
    uint32_t expected_mappings,
    uint32_t expected_target_bindings,
    uint32_t expected_ownership
)
{
    music_rig_result result;
    size_t index;

    if (tables != NULL) {
        tables->prepared_version = UINT32_C(0);
    }
    result = validate_structure(
        tables,
        expected_device_profiles,
        expected_mappings,
        expected_target_bindings,
        expected_ownership
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    for (index = 0; index < MUSIC_RIG_MAPPING_DISPATCH_CAPACITY; ++index) {
        tables->mapping_dispatch[index] = MUSIC_RIG_TABLE_INDEX_NONE;
    }
    for (index = 0; index < tables->mapping_count; ++index) {
        const music_rig_compiled_mapping *mapping = &tables->mappings[index];
        size_t probe;
        size_t position = (size_t)(mapping_hash(
            mapping->profile_index,
            mapping->event_type,
            mapping->channel,
            mapping->number
        ) & (MUSIC_RIG_MAPPING_DISPATCH_CAPACITY - 1U));

        for (probe = 0; probe < MUSIC_RIG_MAPPING_DISPATCH_CAPACITY; ++probe) {
            uint16_t existing = tables->mapping_dispatch[position];

            if (existing == MUSIC_RIG_TABLE_INDEX_NONE) {
                tables->mapping_dispatch[position] = (uint16_t)index;
                break;
            }
            if (mapping_matches(
                    &tables->mappings[existing],
                    mapping->profile_index,
                    mapping->event_type,
                    mapping->channel,
                    mapping->number
                )) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
            position = (position + 1U) &
                (MUSIC_RIG_MAPPING_DISPATCH_CAPACITY - 1U);
        }
        if (probe == MUSIC_RIG_MAPPING_DISPATCH_CAPACITY) {
            return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
        }
    }
    tables->prepared_version = MUSIC_RIG_COMPILED_TABLES_VERSION;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_compiled_tables_validate(
    const music_rig_compiled_tables *tables,
    uint32_t expected_device_profiles,
    uint32_t expected_mappings,
    uint32_t expected_target_bindings,
    uint32_t expected_ownership
)
{
    music_rig_result result;
    size_t index;

    result = validate_structure(
        tables,
        expected_device_profiles,
        expected_mappings,
        expected_target_bindings,
        expected_ownership
    );
    if (result != MUSIC_RIG_RESULT_OK ||
        tables->prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION) {
        return result != MUSIC_RIG_RESULT_OK
            ? result
            : MUSIC_RIG_RESULT_INVALID_DATA;
    }
    for (index = 0; index < tables->mapping_count; ++index) {
        const music_rig_compiled_mapping *mapping = &tables->mappings[index];

        if (music_rig_compiled_mapping_lookup(
                tables,
                mapping->profile_index,
                mapping->event_type,
                mapping->channel,
                mapping->number
            ) != mapping) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_compiled_tables_compose_device(
    const music_rig_compiled_tables *base,
    const music_rig_compiled_tables *source,
    const char *device_slot,
    music_rig_compiled_tables *output
)
{
    uint16_t base_index;
    uint16_t source_index;
    size_t index;

    if (base == NULL || source == NULL || device_slot == NULL ||
        output == NULL || music_rig_compiled_profile_index(
            base, device_slot, &base_index
        ) != MUSIC_RIG_RESULT_OK || music_rig_compiled_profile_index(
            source, device_slot, &source_index
        ) != MUSIC_RIG_RESULT_OK || base->device_profile_count !=
            source->device_profile_count || base->input_binding_count !=
            source->input_binding_count) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < base->input_binding_count; ++index) {
        if (memcmp(&base->input_bindings[index],
                &source->input_bindings[index],
                sizeof(base->input_bindings[index])) != 0) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    *output = *base;
    output->device_profiles[base_index] = source->device_profiles[source_index];
    output->mapping_count = 0U;
    for (index = 0U; index < base->mapping_count; ++index) {
        if (base->mappings[index].profile_index != base_index) {
            if (output->mapping_count >= MUSIC_RIG_MAPPING_CAPACITY) {
                return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
            }
            output->mappings[output->mapping_count++] = base->mappings[index];
        }
    }
    for (index = 0U; index < source->mapping_count; ++index) {
        if (source->mappings[index].profile_index == source_index) {
            music_rig_compiled_mapping mapping = source->mappings[index];
            mapping.profile_index = base_index;
            if (output->mapping_count >= MUSIC_RIG_MAPPING_CAPACITY) {
                return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
            }
            output->mappings[output->mapping_count++] = mapping;
        }
    }
    {
        music_rig_compiled_target_binding merged[
            MUSIC_RIG_TARGET_BINDING_CAPACITY
        ];
        size_t base_target = 0U;
        size_t source_target = 0U;
        size_t merged_count = 0U;

        while (base_target < base->target_binding_count ||
               source_target < source->target_binding_count) {
            if (merged_count >= MUSIC_RIG_TARGET_BINDING_CAPACITY) {
                return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
            }
            if (source_target == source->target_binding_count ||
                (base_target < base->target_binding_count && strcmp(
                    base->target_bindings[base_target].target,
                    source->target_bindings[source_target].target
                ) < 0)) {
                merged[merged_count++] = base->target_bindings[base_target++];
            } else if (base_target == base->target_binding_count ||
                strcmp(
                    source->target_bindings[source_target].target,
                    base->target_bindings[base_target].target
                ) < 0) {
                merged[merged_count++] = source->target_bindings[source_target++];
            } else {
                if (memcmp(&base->target_bindings[base_target],
                        &source->target_bindings[source_target],
                        sizeof(base->target_bindings[base_target])) != 0) {
                    return MUSIC_RIG_RESULT_INVALID_DATA;
                }
                merged[merged_count++] = base->target_bindings[base_target++];
                ++source_target;
            }
        }
        memset(output->target_bindings, 0, sizeof(output->target_bindings));
        memcpy(output->target_bindings, merged,
            merged_count * sizeof(merged[0]));
        output->target_binding_count = (uint32_t)merged_count;
    }
    {
        music_rig_compiled_ownership merged[
            MUSIC_RIG_OWNERSHIP_CAPACITY
        ];
        size_t merged_count = 0U;

        for (index = 0U; index < base->ownership_count; ++index) {
            const music_rig_compiled_ownership *base_ownership =
                &base->ownership[index];
            const music_rig_compiled_ownership *candidate;

            if (!ownership_has_profile(base_ownership, base_index)) {
                candidate = base_ownership;
            } else {
                candidate = find_ownership(
                    source, base_ownership->kind, base_ownership->target
                );
                if (candidate == NULL) {
                    continue;
                }
            }
            if (merged_count >= MUSIC_RIG_OWNERSHIP_CAPACITY) {
                return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
            }
            if (candidate == base_ownership) {
                merged[merged_count++] = *candidate;
            } else if (remap_ownership(
                    candidate, output, source_index, base_index,
                    &merged[merged_count]
                ) != MUSIC_RIG_RESULT_OK) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            } else {
                ++merged_count;
            }
        }
        for (index = 0U; index < source->ownership_count; ++index) {
            const music_rig_compiled_ownership *candidate =
                &source->ownership[index];
            const music_rig_compiled_ownership *base_candidate;

            if (!ownership_has_profile(candidate, source_index)) {
                continue;
            }
            base_candidate = find_ownership(
                base, candidate->kind, candidate->target
            );
            if (base_candidate != NULL) {
                if (!ownership_has_profile(base_candidate, base_index)) {
                    music_rig_compiled_ownership *merged_candidate =
                        (music_rig_compiled_ownership *)find_ownership(
                            output, candidate->kind, candidate->target
                        );

                    if (merged_candidate == NULL || remap_ownership(
                            candidate, output, source_index, base_index,
                            merged_candidate
                        ) != MUSIC_RIG_RESULT_OK) {
                        return MUSIC_RIG_RESULT_INVALID_DATA;
                    }
                }
                continue;
            }
            if (merged_count >= MUSIC_RIG_OWNERSHIP_CAPACITY ||
                remap_ownership(candidate, output, source_index, base_index,
                    &merged[merged_count]) != MUSIC_RIG_RESULT_OK) {
                return merged_count >= MUSIC_RIG_OWNERSHIP_CAPACITY
                    ? MUSIC_RIG_RESULT_BUFFER_TOO_SMALL
                    : MUSIC_RIG_RESULT_INVALID_DATA;
            }
            ++merged_count;
        }
        memset(output->ownership, 0, sizeof(output->ownership));
        memcpy(output->ownership, merged, merged_count * sizeof(merged[0]));
        output->ownership_count = (uint32_t)merged_count;
    }
    {
        music_rig_result result = music_rig_compiled_tables_prepare(
        output,
        output->device_profile_count,
        output->mapping_count,
        output->target_binding_count,
        output->ownership_count
        );
        return result;
    }
}

music_rig_result music_rig_compiled_profile_index(
    const music_rig_compiled_tables *tables,
    const char *slot,
    uint16_t *profile_index
)
{
    size_t lower = 0;
    size_t upper;

    if (tables == NULL || slot == NULL || profile_index == NULL ||
        tables->prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    upper = tables->device_profile_count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2U;
        int order = strcmp(slot, tables->device_profiles[middle].slot);

        if (order == 0) {
            *profile_index = (uint16_t)middle;
            return MUSIC_RIG_RESULT_OK;
        }
        if (order < 0) {
            upper = middle;
        } else {
            lower = middle + 1U;
        }
    }
    return MUSIC_RIG_RESULT_NOT_FOUND;
}

const music_rig_compiled_mapping *music_rig_compiled_mapping_lookup(
    const music_rig_compiled_tables *tables,
    uint16_t profile_index,
    music_rig_midi_event_type event_type,
    uint8_t channel,
    uint8_t number
)
{
    size_t probe;
    size_t position;

    if (tables == NULL ||
        tables->prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION ||
        profile_index >= tables->device_profile_count ||
        !valid_event_type(event_type) || channel < UINT8_C(1) ||
        channel > UINT8_C(16) || number > UINT8_C(127)) {
        return NULL;
    }
    position = (size_t)(mapping_hash(
        profile_index,
        event_type,
        channel,
        number
    ) & (MUSIC_RIG_MAPPING_DISPATCH_CAPACITY - 1U));
    for (probe = 0; probe < MUSIC_RIG_MAPPING_DISPATCH_CAPACITY; ++probe) {
        uint16_t mapping_index = tables->mapping_dispatch[position];

        if (mapping_index == MUSIC_RIG_TABLE_INDEX_NONE) {
            return NULL;
        }
        if (mapping_index >= tables->mapping_count) {
            return NULL;
        }
        if (mapping_matches(
                &tables->mappings[mapping_index],
                profile_index,
                event_type,
                channel,
                number
            )) {
            return &tables->mappings[mapping_index];
        }
        position = (position + 1U) &
            (MUSIC_RIG_MAPPING_DISPATCH_CAPACITY - 1U);
    }
    return NULL;
}

const music_rig_compiled_target_binding *music_rig_compiled_target_lookup(
    const music_rig_compiled_tables *tables,
    const char *target
)
{
    size_t lower = 0;
    size_t upper;

    if (tables == NULL || target == NULL ||
        tables->prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION) {
        return NULL;
    }
    upper = tables->target_binding_count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2U;
        int order = strcmp(target, tables->target_bindings[middle].target);

        if (order == 0) {
            return &tables->target_bindings[middle];
        }
        if (order < 0) {
            upper = middle;
        } else {
            lower = middle + 1U;
        }
    }
    return NULL;
}

const music_rig_compiled_ownership *music_rig_compiled_ownership_lookup(
    const music_rig_compiled_tables *tables,
    music_rig_ownership_kind kind,
    const char *target
)
{
    size_t index;

    if (tables == NULL || target == NULL ||
        tables->prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION) {
        return NULL;
    }
    for (index = 0; index < tables->ownership_count; ++index) {
        if (tables->ownership[index].kind == kind &&
            strcmp(tables->ownership[index].target, target) == 0) {
            return &tables->ownership[index];
        }
    }
    return NULL;
}
