#include "music_rig/definition_json.h"

#include <json-c/json.h>
#include <json-c/json_c_version.h>

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
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

static bool copy_string_field(
    struct json_object *parent,
    const char *name,
    char *output,
    size_t output_capacity
)
{
    struct json_object *value;
    const char *text;
    size_t length;

    if (!get_field(parent, name, json_type_string, &value)) {
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

static bool device_profiles_are_valid(struct json_object *profiles)
{
    size_t count = json_object_array_length(profiles);
    size_t index;

    if (count == 0 || count > UINT32_MAX) {
        return false;
    }
    for (index = 0; index < count; ++index) {
        struct json_object *profile = json_object_array_get_idx(profiles, index);
        struct json_object *slot;
        struct json_object *profile_id;
        struct json_object *readiness;

        if (!get_field(profile, "slot", json_type_string, &slot) ||
            json_object_get_string_len(slot) == 0 ||
            !get_field(profile, "profile", json_type_string, &profile_id) ||
            json_object_get_string_len(profile_id) == 0 ||
            !get_field(profile, "readiness", json_type_string, &readiness) ||
            !string_equals(readiness, "control-only")) {
            return false;
        }
    }
    return true;
}

static bool decode_definition(
    struct json_object *root,
    music_rig_compiled_definition *definition
)
{
    struct json_object *schema;
    struct json_object *fingerprint;
    struct json_object *generation;
    struct json_object *readiness;
    struct json_object *profiles;
    struct json_object *inputs;
    struct json_object *mappings;
    struct json_object *targets;
    struct json_object *ownership;
    struct json_object *graph_delta;
    struct json_object *platform_binding;
    struct json_object *safety;
    struct json_object *activation;
    int64_t generation_id;
    size_t count;
    int input_count;
    int target_count;
    int ownership_count;

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
        !device_profiles_are_valid(profiles) ||
        !get_field(root, "input_bindings", json_type_object, &inputs) ||
        !get_field(root, "mappings", json_type_array, &mappings) ||
        !get_field(root, "target_bindings", json_type_object, &targets) ||
        !get_field(root, "ownership", json_type_object, &ownership)) {
        return false;
    }

    input_count = json_object_object_length(inputs);
    target_count = json_object_object_length(targets);
    ownership_count = json_object_object_length(ownership);
    count = json_object_array_length(mappings);
    if (input_count < 0 || (size_t)input_count !=
            json_object_array_length(profiles) ||
        target_count < 0 || ownership_count < 0 || count > UINT32_MAX) {
        return false;
    }

    if (!get_field(root, "graph_delta", json_type_object, &graph_delta) ||
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
    definition->device_profile_count =
        (uint32_t)json_object_array_length(profiles);
    definition->mapping_count = (uint32_t)count;
    definition->target_binding_count = (uint32_t)target_count;
    definition->ownership_count = (uint32_t)ownership_count;
    definition->control_only = true;
    definition->graph_delta_empty = true;
    definition->authoring_only = true;
    return true;
}

music_rig_result music_rig_definition_json_decode(
    void *context,
    const uint8_t *document,
    size_t document_size,
    music_rig_compiled_definition *definition
)
{
    struct json_tokener *tokener;
    struct json_object *root;
    size_t parse_end;
    bool valid;

    (void)context;
    if (document == NULL || document_size == 0 || definition == NULL ||
        document_size > (size_t)INT_MAX) {
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
        decode_definition(root, definition);
    json_object_put(root);
    json_tokener_free(tokener);
    return valid ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_INVALID_DATA;
}
