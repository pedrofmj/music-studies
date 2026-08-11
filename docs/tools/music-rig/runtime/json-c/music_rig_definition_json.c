#include "music_rig/definition_json.h"

#include <json-c/json.h>
#include <json-c/json_c_version.h>

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if JSON_C_VERSION_NUM < (17 << 8)
#error "json-c 0.17 or newer is required"
#endif

static bool get_field(
    struct json_object *parent,
    const char *name,
    enum json_type type,
    struct json_object **value
)
{
    return json_object_is_type(parent, json_type_object) &&
        json_object_object_get_ex(parent, name, value) &&
        json_object_is_type(*value, type);
}

static bool string_equals(struct json_object *value, const char *expected)
{
    return json_object_is_type(value, json_type_string) &&
        strcmp(json_object_get_string(value), expected) == 0;
}

static bool copy_string(
    struct json_object *value,
    char *output,
    size_t output_capacity
)
{
    const char *text;
    size_t length;

    if (!json_object_is_type(value, json_type_string)) {
        return false;
    }
    text = json_object_get_string(value);
    length = (size_t)json_object_get_string_len(value);
    if (length == 0 || length >= output_capacity) {
        return false;
    }
    memcpy(output, text, length + 1U);
    return true;
}

static bool copy_string_field(
    struct json_object *parent,
    const char *name,
    char *output,
    size_t output_capacity
)
{
    struct json_object *value;

    return get_field(parent, name, json_type_string, &value) &&
        copy_string(value, output, output_capacity);
}

static bool integer_field(
    struct json_object *parent,
    const char *name,
    int64_t minimum,
    int64_t maximum,
    int64_t *output
)
{
    struct json_object *value;
    int64_t decoded;

    if (!get_field(parent, name, json_type_int, &value)) {
        return false;
    }
    decoded = json_object_get_int64(value);
    if (decoded < minimum || decoded > maximum) {
        return false;
    }
    *output = decoded;
    return true;
}

static bool number_field(
    struct json_object *parent,
    const char *name,
    double *output
)
{
    struct json_object *value;

    if (!json_object_object_get_ex(parent, name, &value) ||
        (!json_object_is_type(value, json_type_int) &&
         !json_object_is_type(value, json_type_double))) {
        return false;
    }
    *output = json_object_get_double(value);
    return true;
}

static bool boolean_field_is(
    struct json_object *parent,
    const char *name,
    bool expected
)
{
    struct json_object *value;

    return get_field(parent, name, json_type_boolean, &value) &&
        (json_object_get_boolean(value) != 0) == expected;
}

static music_rig_readiness readiness_value(struct json_object *value)
{
    if (string_equals(value, "control-only")) {
        return MUSIC_RIG_READINESS_CONTROL_ONLY;
    }
    if (string_equals(value, "prepared")) {
        return MUSIC_RIG_READINESS_PREPARED;
    }
    if (string_equals(value, "cold")) {
        return MUSIC_RIG_READINESS_COLD;
    }
    return MUSIC_RIG_READINESS_INVALID;
}

static music_rig_midi_event_type event_type_value(struct json_object *value)
{
    if (string_equals(value, "cc")) {
        return MUSIC_RIG_MIDI_EVENT_CC;
    }
    if (string_equals(value, "note")) {
        return MUSIC_RIG_MIDI_EVENT_NOTE;
    }
    if (string_equals(value, "program-change")) {
        return MUSIC_RIG_MIDI_EVENT_PROGRAM_CHANGE;
    }
    return MUSIC_RIG_MIDI_EVENT_INVALID;
}

static const char *event_type_text(music_rig_midi_event_type value)
{
    switch (value) {
    case MUSIC_RIG_MIDI_EVENT_CC:
        return "cc";
    case MUSIC_RIG_MIDI_EVENT_NOTE:
        return "note";
    case MUSIC_RIG_MIDI_EVENT_PROGRAM_CHANGE:
        return "program-change";
    default:
        return NULL;
    }
}

static music_rig_midi_edge edge_value(struct json_object *value)
{
    if (string_equals(value, "any")) {
        return MUSIC_RIG_MIDI_EDGE_ANY;
    }
    if (string_equals(value, "change")) {
        return MUSIC_RIG_MIDI_EDGE_CHANGE;
    }
    if (string_equals(value, "press")) {
        return MUSIC_RIG_MIDI_EDGE_PRESS;
    }
    if (string_equals(value, "release")) {
        return MUSIC_RIG_MIDI_EDGE_RELEASE;
    }
    return MUSIC_RIG_MIDI_EDGE_INVALID;
}

static music_rig_control_behavior behavior_value(struct json_object *value)
{
    if (string_equals(value, "absolute")) {
        return MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE;
    }
    if (string_equals(value, "momentary")) {
        return MUSIC_RIG_CONTROL_BEHAVIOR_MOMENTARY;
    }
    if (string_equals(value, "relative")) {
        return MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE;
    }
    if (string_equals(value, "toggle")) {
        return MUSIC_RIG_CONTROL_BEHAVIOR_TOGGLE;
    }
    return MUSIC_RIG_CONTROL_BEHAVIOR_INVALID;
}

static music_rig_transform_type transform_value(struct json_object *value)
{
    if (string_equals(value, "direct")) {
        return MUSIC_RIG_TRANSFORM_DIRECT;
    }
    if (string_equals(value, "scale")) {
        return MUSIC_RIG_TRANSFORM_SCALE;
    }
    if (string_equals(value, "toggle")) {
        return MUSIC_RIG_TRANSFORM_TOGGLE;
    }
    if (string_equals(value, "relative")) {
        return MUSIC_RIG_TRANSFORM_RELATIVE;
    }
    return MUSIC_RIG_TRANSFORM_INVALID;
}

static music_rig_relative_encoding relative_encoding_value(
    struct json_object *value
)
{
    if (string_equals(value, "binary-offset")) {
        return MUSIC_RIG_RELATIVE_ENCODING_BINARY_OFFSET;
    }
    if (string_equals(value, "sign-magnitude")) {
        return MUSIC_RIG_RELATIVE_ENCODING_SIGN_MAGNITUDE;
    }
    if (string_equals(value, "twos-complement")) {
        return MUSIC_RIG_RELATIVE_ENCODING_TWOS_COMPLEMENT;
    }
    return MUSIC_RIG_RELATIVE_ENCODING_NONE;
}

static music_rig_takeover_mode takeover_value(struct json_object *value)
{
    if (json_object_is_type(value, json_type_null)) {
        return MUSIC_RIG_TAKEOVER_NONE;
    }
    if (string_equals(value, "jump")) {
        return MUSIC_RIG_TAKEOVER_JUMP;
    }
    if (string_equals(value, "pickup")) {
        return MUSIC_RIG_TAKEOVER_PICKUP;
    }
    if (string_equals(value, "scaled-pickup")) {
        return MUSIC_RIG_TAKEOVER_SCALED_PICKUP;
    }
    if (string_equals(value, "ignore-until-moved")) {
        return MUSIC_RIG_TAKEOVER_IGNORE_UNTIL_MOVED;
    }
    return MUSIC_RIG_TAKEOVER_NONE;
}

static music_rig_ownership_kind ownership_kind_value(
    struct json_object *value
)
{
    if (string_equals(value, "effect")) {
        return MUSIC_RIG_OWNERSHIP_KIND_EFFECT;
    }
    if (string_equals(value, "engine")) {
        return MUSIC_RIG_OWNERSHIP_KIND_ENGINE;
    }
    if (string_equals(value, "feedback")) {
        return MUSIC_RIG_OWNERSHIP_KIND_FEEDBACK;
    }
    if (string_equals(value, "helper")) {
        return MUSIC_RIG_OWNERSHIP_KIND_HELPER;
    }
    if (string_equals(value, "midi-event")) {
        return MUSIC_RIG_OWNERSHIP_KIND_MIDI_EVENT;
    }
    if (string_equals(value, "parameter")) {
        return MUSIC_RIG_OWNERSHIP_KIND_PARAMETER;
    }
    if (string_equals(value, "port")) {
        return MUSIC_RIG_OWNERSHIP_KIND_PORT;
    }
    if (string_equals(value, "route")) {
        return MUSIC_RIG_OWNERSHIP_KIND_ROUTE;
    }
    if (string_equals(value, "semantic-control")) {
        return MUSIC_RIG_OWNERSHIP_KIND_SEMANTIC_CONTROL;
    }
    if (string_equals(value, "state-key")) {
        return MUSIC_RIG_OWNERSHIP_KIND_STATE_KEY;
    }
    return MUSIC_RIG_OWNERSHIP_KIND_INVALID;
}

static music_rig_ownership_mode ownership_mode_value(
    struct json_object *value
)
{
    if (string_equals(value, "exclusive")) {
        return MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE;
    }
    if (string_equals(value, "shared-event-destination")) {
        return MUSIC_RIG_OWNERSHIP_MODE_SHARED_EVENT_DESTINATION;
    }
    if (string_equals(value, "read-only")) {
        return MUSIC_RIG_OWNERSHIP_MODE_READ_ONLY;
    }
    return MUSIC_RIG_OWNERSHIP_MODE_INVALID;
}

static int profile_index_for(
    const music_rig_compiled_tables *tables,
    const char *slot,
    const char *profile
)
{
    size_t index;

    for (index = 0; index < tables->device_profile_count; ++index) {
        if (strcmp(tables->device_profiles[index].slot, slot) == 0 &&
            strcmp(tables->device_profiles[index].profile, profile) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static int profile_index_for_slot(
    const music_rig_compiled_tables *tables,
    const char *slot
)
{
    size_t index;

    for (index = 0; index < tables->device_profile_count; ++index) {
        if (strcmp(tables->device_profiles[index].slot, slot) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static bool decode_device_profiles(
    struct json_object *profiles,
    music_rig_compiled_tables *tables
)
{
    size_t count = json_object_array_length(profiles);
    size_t index;

    if (count == 0 || count > MUSIC_RIG_DEVICE_PROFILE_CAPACITY) {
        return false;
    }
    tables->device_profile_count = (uint32_t)count;
    for (index = 0; index < count; ++index) {
        struct json_object *source = json_object_array_get_idx(profiles, index);
        struct json_object *readiness;
        music_rig_compiled_device_profile *profile =
            &tables->device_profiles[index];

        if (!copy_string_field(source, "slot", profile->slot,
                sizeof(profile->slot)) ||
            !copy_string_field(source, "profile", profile->profile,
                sizeof(profile->profile)) ||
            !copy_string_field(source, "hardware_preset",
                profile->hardware_preset, sizeof(profile->hardware_preset)) ||
            !get_field(source, "readiness", json_type_string, &readiness)) {
            return false;
        }
        profile->readiness = readiness_value(readiness);
        if (profile->readiness != MUSIC_RIG_READINESS_CONTROL_ONLY) {
            return false;
        }
    }
    return true;
}

static bool decode_endpoints(
    struct json_object *endpoints,
    music_rig_compiled_input_binding *binding
)
{
    struct json_object_iter iterator;
    int count = json_object_object_length(endpoints);
    size_t index = 0;

    if (count <= 0 || (size_t)count > MUSIC_RIG_INPUT_ENDPOINT_CAPACITY) {
        return false;
    }
    binding->endpoint_count = (uint16_t)count;
    json_object_object_foreachC(endpoints, iterator) {
        struct json_object *direction;
        music_rig_compiled_input_endpoint *endpoint =
            &binding->endpoints[index];
        size_t key_length = strlen(iterator.key);

        if (key_length == 0 || key_length >= sizeof(endpoint->purpose) ||
            !get_field(iterator.val, "direction", json_type_string,
                &direction) ||
            !string_equals(direction, "input") ||
            !copy_string_field(iterator.val, "locator", endpoint->locator,
                sizeof(endpoint->locator))) {
            return false;
        }
        memcpy(endpoint->purpose, iterator.key, key_length + 1U);
        ++index;
    }
    return index == (size_t)count;
}

static bool decode_input_bindings(
    struct json_object *inputs,
    music_rig_compiled_tables *tables
)
{
    struct json_object_iter iterator;
    int count = json_object_object_length(inputs);
    size_t decoded = 0;

    if (count <= 0 || (uint32_t)count != tables->device_profile_count) {
        return false;
    }
    tables->input_binding_count = (uint32_t)count;
    json_object_object_foreachC(inputs, iterator) {
        struct json_object *identity;
        struct json_object *endpoints;
        struct json_object *status;
        int profile_index = profile_index_for_slot(tables, iterator.key);
        music_rig_compiled_input_binding *binding;

        if (profile_index < 0) {
            return false;
        }
        binding = &tables->input_bindings[(size_t)profile_index];
        if (binding->slot[0] != '\0' ||
            strlen(iterator.key) >= sizeof(binding->slot) ||
            !copy_string_field(iterator.val, "adapter", binding->adapter,
                sizeof(binding->adapter)) ||
            !get_field(iterator.val, "status", json_type_string, &status) ||
            !string_equals(status, "available") ||
            !get_field(iterator.val, "identity", json_type_object, &identity) ||
            !copy_string_field(identity, "strategy",
                binding->identity_strategy, sizeof(binding->identity_strategy)) ||
            !copy_string_field(identity, "value", binding->identity_value,
                sizeof(binding->identity_value)) ||
            !get_field(iterator.val, "endpoints", json_type_object,
                &endpoints) ||
            !decode_endpoints(endpoints, binding)) {
            return false;
        }
        memcpy(binding->slot, iterator.key, strlen(iterator.key) + 1U);
        binding->status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
        ++decoded;
    }
    return decoded == (size_t)count;
}

static bool dispatch_key_for(
    const music_rig_compiled_tables *tables,
    const music_rig_compiled_mapping *mapping,
    char *output,
    size_t output_capacity
)
{
    const char *event_text = event_type_text(mapping->event_type);
    int length;

    if (event_text == NULL ||
        mapping->profile_index >= tables->device_profile_count) {
        return false;
    }
    length = snprintf(
        output,
        output_capacity,
        "%s|%s|%u|%u",
        tables->device_profiles[mapping->profile_index].slot,
        event_text,
        (unsigned int)mapping->channel,
        (unsigned int)mapping->number
    );
    return length > 0 && (size_t)length < output_capacity;
}

static bool decode_transform(
    struct json_object *source,
    music_rig_compiled_mapping *mapping
)
{
    struct json_object *type;
    struct json_object *encoding;
    music_rig_relative_encoding decoded_encoding;

    if (!get_field(source, "type", json_type_string, &type)) {
        return false;
    }
    mapping->transform = transform_value(type);
    if (mapping->transform == MUSIC_RIG_TRANSFORM_INVALID) {
        return false;
    }
    if (mapping->transform == MUSIC_RIG_TRANSFORM_SCALE) {
        return number_field(source, "input_min", &mapping->input_min) &&
            number_field(source, "input_max", &mapping->input_max) &&
            number_field(source, "output_min", &mapping->output_min) &&
            number_field(source, "output_max", &mapping->output_max) &&
            mapping->input_max > mapping->input_min;
    }
    if (mapping->transform != MUSIC_RIG_TRANSFORM_RELATIVE) {
        return true;
    }
    if (!get_field(source, "relative_encoding", json_type_string, &encoding)) {
        return false;
    }
    decoded_encoding = relative_encoding_value(encoding);
    if (decoded_encoding == MUSIC_RIG_RELATIVE_ENCODING_NONE) {
        return false;
    }
    if (mapping->relative_encoding != MUSIC_RIG_RELATIVE_ENCODING_NONE &&
        mapping->relative_encoding != decoded_encoding) {
        return false;
    }
    mapping->relative_encoding = decoded_encoding;
    return true;
}

static bool decode_switch_values(
    struct json_object *source,
    music_rig_compiled_mapping *mapping
)
{
    struct json_object *off_value;
    struct json_object *on_value;
    bool has_off = json_object_object_get_ex(source, "off_value", &off_value);
    bool has_on = json_object_object_get_ex(source, "on_value", &on_value);
    int64_t off_decoded;
    int64_t on_decoded;

    if (!has_off && !has_on) {
        return true;
    }
    if (!has_off || !has_on ||
        !integer_field(source, "off_value", 0, 127, &off_decoded) ||
        !integer_field(source, "on_value", 0, 127, &on_decoded)) {
        return false;
    }
    mapping->has_switch_values = true;
    mapping->off_value = (uint8_t)off_decoded;
    mapping->on_value = (uint8_t)on_decoded;
    return true;
}

static bool decode_mapping(
    struct json_object *source,
    music_rig_compiled_tables *tables,
    music_rig_compiled_mapping *mapping
)
{
    struct json_object *source_object;
    struct json_object *event;
    struct json_object *event_type;
    struct json_object *edge;
    struct json_object *behavior;
    struct json_object *transform;
    struct json_object *takeover;
    struct json_object *encoding;
    struct json_object *dispatch_key;
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char profile[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char preset[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char expected_dispatch[MUSIC_RIG_IDENTIFIER_CAPACITY + 32U];
    int64_t channel;
    int64_t number;
    int profile_index;

    if (!copy_string_field(source, "slot", slot, sizeof(slot)) ||
        !copy_string_field(source, "profile", profile, sizeof(profile))) {
        return false;
    }
    profile_index = profile_index_for(tables, slot, profile);
    if (profile_index < 0 ||
        !copy_string_field(source, "mapping", mapping->mapping,
            sizeof(mapping->mapping)) ||
        !copy_string_field(source, "target", mapping->target,
            sizeof(mapping->target)) ||
        !get_field(source, "source", json_type_object, &source_object) ||
        !copy_string_field(source_object, "hardware_preset", preset,
            sizeof(preset)) ||
        strcmp(preset, tables->device_profiles[profile_index].hardware_preset)
            != 0 ||
        !copy_string_field(source_object, "control", mapping->control,
            sizeof(mapping->control)) ||
        !get_field(source_object, "behavior", json_type_string, &behavior) ||
        !get_field(source_object, "event", json_type_object, &event) ||
        !get_field(event, "type", json_type_string, &event_type) ||
        !get_field(event, "edge", json_type_string, &edge) ||
        !integer_field(event, "channel", 1, 16, &channel) ||
        !integer_field(event, "number", 0, 127, &number) ||
        !get_field(source, "transform", json_type_object, &transform) ||
        !json_object_object_get_ex(source, "takeover", &takeover)) {
        return false;
    }
    mapping->behavior = behavior_value(behavior);
    mapping->event_type = event_type_value(event_type);
    mapping->edge = edge_value(edge);
    if (mapping->behavior == MUSIC_RIG_CONTROL_BEHAVIOR_INVALID ||
        mapping->event_type == MUSIC_RIG_MIDI_EVENT_INVALID ||
        mapping->edge == MUSIC_RIG_MIDI_EDGE_INVALID) {
        return false;
    }
    mapping->profile_index = (uint16_t)profile_index;
    mapping->channel = (uint8_t)channel;
    mapping->number = (uint8_t)number;
    mapping->takeover = takeover_value(takeover);
    if ((!json_object_is_type(takeover, json_type_null) &&
         mapping->takeover == MUSIC_RIG_TAKEOVER_NONE) ||
        (mapping->behavior == MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE &&
         mapping->takeover == MUSIC_RIG_TAKEOVER_NONE) ||
        (mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE &&
         mapping->takeover != MUSIC_RIG_TAKEOVER_NONE) ||
        !decode_switch_values(source_object, mapping)) {
        return false;
    }

    if (json_object_object_get_ex(source_object, "relative_encoding",
            &encoding)) {
        mapping->relative_encoding = relative_encoding_value(encoding);
        if (mapping->relative_encoding == MUSIC_RIG_RELATIVE_ENCODING_NONE) {
            return false;
        }
    }
    if (!decode_transform(transform, mapping) ||
        (mapping->behavior == MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE &&
         (mapping->relative_encoding == MUSIC_RIG_RELATIVE_ENCODING_NONE ||
          mapping->transform != MUSIC_RIG_TRANSFORM_RELATIVE)) ||
        (mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE &&
         (mapping->transform == MUSIC_RIG_TRANSFORM_RELATIVE ||
          mapping->relative_encoding != MUSIC_RIG_RELATIVE_ENCODING_NONE)) ||
        !get_field(source, "dispatch_key", json_type_string, &dispatch_key) ||
        !dispatch_key_for(tables, mapping, expected_dispatch,
            sizeof(expected_dispatch)) ||
        strcmp(json_object_get_string(dispatch_key), expected_dispatch) != 0) {
        return false;
    }
    return true;
}

static bool decode_mappings(
    struct json_object *mappings,
    music_rig_compiled_tables *tables
)
{
    size_t count = json_object_array_length(mappings);
    size_t index;

    if (count == 0 || count > MUSIC_RIG_MAPPING_CAPACITY) {
        return false;
    }
    tables->mapping_count = (uint32_t)count;
    for (index = 0; index < count; ++index) {
        if (!decode_mapping(
                json_object_array_get_idx(mappings, index),
                tables,
                &tables->mappings[index]
            )) {
            return false;
        }
    }
    return true;
}

static bool validate_mapping_index(
    struct json_object *mapping_index,
    const music_rig_compiled_tables *tables
)
{
    struct json_object_iter iterator;
    bool seen[MUSIC_RIG_MAPPING_CAPACITY] = {false};
    int count = json_object_object_length(mapping_index);
    size_t decoded = 0;

    if (count <= 0 || (uint32_t)count != tables->mapping_count) {
        return false;
    }
    json_object_object_foreachC(mapping_index, iterator) {
        int64_t position;
        char expected[MUSIC_RIG_IDENTIFIER_CAPACITY + 32U];

        if (!json_object_is_type(iterator.val, json_type_int)) {
            return false;
        }
        position = json_object_get_int64(iterator.val);
        if (position < 0 || (uint64_t)position >= tables->mapping_count ||
            seen[(size_t)position] ||
            !dispatch_key_for(
                tables,
                &tables->mappings[(size_t)position],
                expected,
                sizeof(expected)
            ) ||
            strcmp(iterator.key, expected) != 0) {
            return false;
        }
        seen[(size_t)position] = true;
        ++decoded;
    }
    return decoded == tables->mapping_count;
}

static bool decode_target_bindings(
    struct json_object *targets,
    music_rig_compiled_tables *tables
)
{
    struct json_object_iter iterator;
    int count = json_object_object_length(targets);
    size_t index = 0;

    if (count <= 0 || (size_t)count > MUSIC_RIG_TARGET_BINDING_CAPACITY) {
        return false;
    }
    tables->target_binding_count = (uint32_t)count;
    json_object_object_foreachC(targets, iterator) {
        struct json_object *status;
        music_rig_compiled_target_binding *target =
            &tables->target_bindings[index];
        size_t key_length = strlen(iterator.key);

        if (key_length == 0 || key_length >= sizeof(target->target) ||
            !copy_string_field(iterator.val, "adapter", target->adapter,
                sizeof(target->adapter)) ||
            !copy_string_field(iterator.val, "locator", target->locator,
                sizeof(target->locator)) ||
            !get_field(iterator.val, "status", json_type_string, &status) ||
            !string_equals(status, "available")) {
            return false;
        }
        memcpy(target->target, iterator.key, key_length + 1U);
        target->status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
        ++index;
    }
    return index == (size_t)count;
}

static bool decode_owner(
    struct json_object *source,
    const music_rig_compiled_definition *definition,
    const music_rig_compiled_tables *tables,
    music_rig_compiled_owner *owner
)
{
    struct json_object *scope;
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    int profile_index;

    owner->profile_index = MUSIC_RIG_TABLE_INDEX_NONE;
    if (!get_field(source, "scope", json_type_string, &scope) ||
        !copy_string_field(source, "profile", owner->profile,
            sizeof(owner->profile))) {
        return false;
    }
    if (string_equals(scope, "device-profile")) {
        if (!copy_string_field(source, "slot", slot, sizeof(slot)) ||
            (profile_index = profile_index_for(
                tables,
                slot,
                owner->profile
            )) < 0) {
            return false;
        }
        owner->scope = MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE;
        owner->profile_index = (uint16_t)profile_index;
        memcpy(owner->slot, slot, strlen(slot) + 1U);
        return true;
    }
    if (string_equals(scope, "rig-profile") &&
        strcmp(owner->profile, definition->active_rig_profile) == 0 &&
        !json_object_object_get_ex(source, "slot", &scope)) {
        owner->scope = MUSIC_RIG_OWNER_SCOPE_RIG_PROFILE;
        return true;
    }
    return false;
}

static bool decode_ownership(
    struct json_object *ownership_source,
    const music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
)
{
    struct json_object_iter iterator;
    int count = json_object_object_length(ownership_source);
    size_t index = 0;

    if (count <= 0 || (size_t)count > MUSIC_RIG_OWNERSHIP_CAPACITY) {
        return false;
    }
    tables->ownership_count = (uint32_t)count;
    json_object_object_foreachC(ownership_source, iterator) {
        struct json_object *kind;
        struct json_object *mode;
        struct json_object *owners;
        music_rig_compiled_ownership *ownership = &tables->ownership[index];
        size_t owner_count;
        size_t owner_index;
        char expected_key[MUSIC_RIG_SEMANTIC_ID_CAPACITY + 32U];
        int key_length;

        if (!get_field(iterator.val, "kind", json_type_string, &kind) ||
            !get_field(iterator.val, "mode", json_type_string, &mode) ||
            !copy_string_field(iterator.val, "target", ownership->target,
                sizeof(ownership->target)) ||
            !get_field(iterator.val, "owners", json_type_array, &owners)) {
            return false;
        }
        ownership->kind = ownership_kind_value(kind);
        ownership->mode = ownership_mode_value(mode);
        if (ownership->kind == MUSIC_RIG_OWNERSHIP_KIND_INVALID ||
            ownership->mode == MUSIC_RIG_OWNERSHIP_MODE_INVALID) {
            return false;
        }
        key_length = snprintf(
            expected_key,
            sizeof(expected_key),
            "%s|%s",
            json_object_get_string(kind),
            ownership->target
        );
        if (key_length <= 0 || (size_t)key_length >= sizeof(expected_key) ||
            strcmp(iterator.key, expected_key) != 0) {
            return false;
        }
        owner_count = json_object_array_length(owners);
        if (owner_count == 0 ||
            owner_count > MUSIC_RIG_OWNERS_PER_ENTRY_CAPACITY) {
            return false;
        }
        ownership->owner_count = (uint16_t)owner_count;
        for (owner_index = 0; owner_index < owner_count; ++owner_index) {
            if (!decode_owner(
                    json_object_array_get_idx(owners, owner_index),
                    definition,
                    tables,
                    &ownership->owners[owner_index]
                )) {
                return false;
            }
        }
        ++index;
    }
    return index == (size_t)count;
}

static bool decode_definition(
    struct json_object *root,
    music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
)
{
    struct json_object *schema;
    struct json_object *fingerprint;
    struct json_object *generation;
    struct json_object *readiness;
    struct json_object *profiles;
    struct json_object *inputs;
    struct json_object *mappings;
    struct json_object *mapping_index;
    struct json_object *targets;
    struct json_object *ownership;
    struct json_object *graph_delta;
    struct json_object *platform_binding;
    struct json_object *safety;
    struct json_object *activation;
    int64_t generation_id;

    if (!get_field(root, "schema", json_type_string, &schema) ||
        !string_equals(schema, "music-studies/compiled-performance-rig/v1") ||
        !get_field(root, "definition_fingerprint", json_type_string,
            &fingerprint) ||
        music_rig_definition_fingerprint_parse(
            json_object_get_string(fingerprint),
            (size_t)json_object_get_string_len(fingerprint),
            definition->fingerprint,
            sizeof(definition->fingerprint)
        ) != MUSIC_RIG_RESULT_OK ||
        !get_field(root, "generation", json_type_int, &generation)) {
        return false;
    }
    generation_id = json_object_get_int64(generation);
    if (generation_id <= 0 ||
        !copy_string_field(root, "rig", definition->rig_id,
            sizeof(definition->rig_id)) ||
        !copy_string_field(root, "active_rig_profile",
            definition->active_rig_profile,
            sizeof(definition->active_rig_profile)) ||
        !get_field(root, "readiness", json_type_string, &readiness) ||
        !string_equals(readiness, "control-only") ||
        !get_field(root, "device_profiles", json_type_array, &profiles) ||
        !get_field(root, "input_bindings", json_type_object, &inputs) ||
        !get_field(root, "mappings", json_type_array, &mappings) ||
        !get_field(root, "mapping_index", json_type_object, &mapping_index) ||
        !get_field(root, "target_bindings", json_type_object, &targets) ||
        !get_field(root, "ownership", json_type_object, &ownership) ||
        !get_field(root, "graph_delta", json_type_object, &graph_delta) ||
        !boolean_field_is(graph_delta, "empty", true) ||
        !boolean_field_is(graph_delta, "control_only_eligible", true) ||
        !boolean_field_is(graph_delta, "applied", false) ||
        !get_field(root, "platform_binding", json_type_object,
            &platform_binding) ||
        !copy_string_field(platform_binding, "id",
            definition->platform_binding_id,
            sizeof(definition->platform_binding_id)) ||
        !copy_string_field(platform_binding, "platform", definition->platform,
            sizeof(definition->platform)) ||
        !get_field(root, "safety", json_type_object, &safety) ||
        !get_field(safety, "activation", json_type_string, &activation) ||
        !string_equals(activation, "authoring-only") ||
        !boolean_field_is(safety, "materializes_runtime", false) ||
        !boolean_field_is(safety, "applies_graph_delta", false)) {
        return false;
    }

    definition->schema_version = MUSIC_RIG_COMPILED_DEFINITION_VERSION;
    definition->generation_id = (uint64_t)generation_id;
    definition->control_only = true;
    definition->graph_delta_empty = true;
    definition->authoring_only = true;
    if (!decode_device_profiles(profiles, tables) ||
        !decode_input_bindings(inputs, tables) ||
        !decode_mappings(mappings, tables) ||
        !validate_mapping_index(mapping_index, tables) ||
        !decode_target_bindings(targets, tables) ||
        !decode_ownership(ownership, definition, tables)) {
        return false;
    }
    definition->device_profile_count = tables->device_profile_count;
    definition->mapping_count = tables->mapping_count;
    definition->target_binding_count = tables->target_binding_count;
    definition->ownership_count = tables->ownership_count;
    return true;
}

music_rig_result music_rig_definition_json_decode(
    void *context,
    const uint8_t *document,
    size_t document_size,
    music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
)
{
    struct json_tokener *tokener;
    struct json_object *root;
    size_t parse_end;
    bool valid;

    (void)context;
    if (document == NULL || document_size == 0 || definition == NULL ||
        tables == NULL || document_size > (size_t)INT_MAX) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    tokener = json_tokener_new_ex(64);
    if (tokener == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    root = json_tokener_parse_ex(
        tokener,
        (const char *)document,
        (int)document_size
    );
    parse_end = json_tokener_get_parse_end(tokener);
    while (parse_end < document_size &&
        isspace((unsigned char)document[parse_end]) != 0) {
        ++parse_end;
    }
    valid = json_tokener_get_error(tokener) == json_tokener_success &&
        parse_end == document_size && root != NULL &&
        json_object_is_type(root, json_type_object) &&
        decode_definition(root, definition, tables);
    json_object_put(root);
    json_tokener_free(tokener);
    return valid ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_INVALID_DATA;
}
