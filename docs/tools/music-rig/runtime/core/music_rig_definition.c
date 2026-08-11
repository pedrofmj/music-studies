#include "music_rig/definition.h"

#include <string.h>

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

static bool valid_id(const char *value)
{
    size_t index;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (index = 1; index < MUSIC_RIG_DEFINITION_ID_CAPACITY; ++index) {
        if (value[index] == '\0') {
            return true;
        }
    }
    return false;
}

static bool storage_is_valid(const music_rig_storage_adapter *storage)
{
    return storage != NULL &&
        storage->abi_version == MUSIC_RIG_STORAGE_ABI_VERSION &&
        storage->read != NULL && storage->atomic_replace != NULL;
}

music_rig_result music_rig_definition_fingerprint_parse(
    const char *fingerprint,
    size_t fingerprint_size,
    uint8_t *output,
    size_t output_size
)
{
    static const char prefix[] = "sha256:";
    size_t index;

    if (fingerprint == NULL || output == NULL ||
        fingerprint_size != sizeof(prefix) - 1U + 64U ||
        output_size != MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE ||
        memcmp(fingerprint, prefix, sizeof(prefix) - 1U) != 0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    for (index = 0; index < MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE; ++index) {
        int high = hex_value(fingerprint[sizeof(prefix) - 1U + index * 2U]);
        int low = hex_value(
            fingerprint[sizeof(prefix) - 1U + index * 2U + 1U]
        );

        if (high < 0 || low < 0) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        output[index] = (uint8_t)((unsigned int)high << 4U |
            (unsigned int)low);
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_definition_validate(
    const music_rig_compiled_definition *definition
)
{
    if (definition == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (definition->schema_version !=
            MUSIC_RIG_COMPILED_DEFINITION_VERSION ||
        definition->generation_id == UINT64_C(0) ||
        !valid_id(definition->rig_id) ||
        !valid_id(definition->active_rig_profile) ||
        !valid_id(definition->platform_binding_id) ||
        !valid_id(definition->platform) ||
        definition->device_profile_count == UINT32_C(0) ||
        definition->device_profile_count >
            MUSIC_RIG_DEVICE_PROFILE_CAPACITY ||
        definition->mapping_count == UINT32_C(0) ||
        definition->mapping_count > MUSIC_RIG_MAPPING_CAPACITY ||
        definition->target_binding_count == UINT32_C(0) ||
        definition->target_binding_count >
            MUSIC_RIG_TARGET_BINDING_CAPACITY ||
        definition->ownership_count == UINT32_C(0) ||
        definition->ownership_count > MUSIC_RIG_OWNERSHIP_CAPACITY ||
        !definition->control_only || !definition->graph_delta_empty ||
        !definition->authoring_only) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_definition_generation_init(
    const music_rig_compiled_definition *definition,
    const music_rig_compiled_tables *tables,
    music_rig_generation *generation
)
{
    music_rig_result result;

    if (generation == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_definition_validate(definition);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = music_rig_compiled_tables_validate(
        tables,
        definition->device_profile_count,
        definition->mapping_count,
        definition->target_binding_count,
        definition->ownership_count
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    generation->id = definition->generation_id;
    generation->mapping = tables;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_definition_load(
    const music_rig_storage_adapter *storage,
    const music_rig_definition_decoder *decoder,
    uint8_t *document_buffer,
    size_t document_capacity,
    const uint8_t *expected_fingerprint,
    size_t expected_fingerprint_size,
    music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
)
{
    music_rig_result result;
    size_t document_size = 0;

    if (!storage_is_valid(storage) || decoder == NULL ||
        decoder->decode == NULL || document_buffer == NULL ||
        document_capacity == 0 || expected_fingerprint == NULL ||
        expected_fingerprint_size != MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE ||
        definition == NULL || tables == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    result = storage->read(
        storage->context,
        MUSIC_RIG_STORAGE_COMPILED_DEFINITION,
        document_buffer,
        document_capacity,
        &document_size
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    if (document_size == 0 || document_size > document_capacity) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }

    memset(definition, 0, sizeof(*definition));
    memset(tables, 0, sizeof(*tables));
    result = decoder->decode(
        decoder->context,
        document_buffer,
        document_size,
        definition,
        tables
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = music_rig_definition_validate(definition);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    if (memcmp(
            definition->fingerprint,
            expected_fingerprint,
            MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE
        ) != 0) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return music_rig_compiled_tables_prepare(
        tables,
        definition->device_profile_count,
        definition->mapping_count,
        definition->target_binding_count,
        definition->ownership_count
    );
}
