#include "music_rig/definition.h"

#include <stdio.h>
#include <string.h>

typedef struct mock_definition_source {
    const uint8_t *document;
    size_t document_size;
    music_rig_result read_result;
    music_rig_result decode_result;
    music_rig_compiled_definition decoded;
    music_rig_compiled_tables decoded_tables;
    unsigned int read_calls;
    unsigned int decode_calls;
} mock_definition_source;

static music_rig_result mock_read(
    void *opaque,
    music_rig_storage_object object,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
)
{
    mock_definition_source *source = opaque;

    source->read_calls += 1U;
    if (object != MUSIC_RIG_STORAGE_COMPILED_DEFINITION) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (source->read_result != MUSIC_RIG_RESULT_OK) {
        return source->read_result;
    }
    if (output_capacity < source->document_size) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, source->document, source->document_size);
    *output_size = source->document_size;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result mock_replace(
    void *opaque,
    music_rig_storage_object object,
    const uint8_t *input,
    size_t input_size
)
{
    (void)opaque;
    (void)object;
    (void)input;
    (void)input_size;
    return MUSIC_RIG_RESULT_UNSUPPORTED;
}

static music_rig_result mock_decode(
    void *opaque,
    const uint8_t *document,
    size_t document_size,
    music_rig_compiled_definition *definition,
    music_rig_compiled_tables *tables
)
{
    mock_definition_source *source = opaque;

    source->decode_calls += 1U;
    if (document == NULL || document_size != source->document_size) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (source->decode_result != MUSIC_RIG_RESULT_OK) {
        return source->decode_result;
    }
    *definition = source->decoded;
    *tables = source->decoded_tables;
    return MUSIC_RIG_RESULT_OK;
}

static void init_source(mock_definition_source *source)
{
    static const uint8_t document[] = {0x7b, 0x7d};
    size_t index;

    memset(source, 0, sizeof(*source));
    source->document = document;
    source->document_size = sizeof(document);
    source->read_result = MUSIC_RIG_RESULT_OK;
    source->decode_result = MUSIC_RIG_RESULT_OK;
    source->decoded.schema_version = MUSIC_RIG_COMPILED_DEFINITION_VERSION;
    source->decoded.generation_id = UINT64_C(1);
    memcpy(source->decoded.rig_id, "pedro-performance-rig",
        sizeof("pedro-performance-rig"));
    memcpy(source->decoded.active_rig_profile, "full-live-rack",
        sizeof("full-live-rack"));
    memcpy(source->decoded.platform_binding_id, "airstar-current",
        sizeof("airstar-current"));
    memcpy(source->decoded.platform, "linux", sizeof("linux"));
    source->decoded.device_profile_count = UINT32_C(1);
    source->decoded.mapping_count = UINT32_C(1);
    source->decoded.target_binding_count = UINT32_C(1);
    source->decoded.ownership_count = UINT32_C(1);
    source->decoded.control_only = true;
    source->decoded.graph_delta_empty = true;
    source->decoded.authoring_only = true;
    for (index = 0; index < MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE; ++index) {
        source->decoded.fingerprint[index] = (uint8_t)index;
    }

    source->decoded_tables.device_profile_count = UINT32_C(1);
    source->decoded_tables.input_binding_count = UINT32_C(1);
    source->decoded_tables.mapping_count = UINT32_C(1);
    source->decoded_tables.target_binding_count = UINT32_C(1);
    source->decoded_tables.ownership_count = UINT32_C(1);
    memcpy(source->decoded_tables.device_profiles[0].slot, "arturia-main",
        sizeof("arturia-main"));
    memcpy(source->decoded_tables.device_profiles[0].profile,
        "multi-instrument-rack", sizeof("multi-instrument-rack"));
    memcpy(source->decoded_tables.device_profiles[0].hardware_preset,
        "arturia-current-rack", sizeof("arturia-current-rack"));
    source->decoded_tables.device_profiles[0].readiness =
        MUSIC_RIG_READINESS_CONTROL_ONLY;
    memcpy(source->decoded_tables.input_bindings[0].slot, "arturia-main",
        sizeof("arturia-main"));
    memcpy(source->decoded_tables.input_bindings[0].adapter, "mock-midi",
        sizeof("mock-midi"));
    memcpy(source->decoded_tables.input_bindings[0].identity_strategy,
        "mock-identity", sizeof("mock-identity"));
    memcpy(source->decoded_tables.input_bindings[0].identity_value,
        "arturia", sizeof("arturia"));
    source->decoded_tables.input_bindings[0].status =
        MUSIC_RIG_BINDING_STATUS_AVAILABLE;
    source->decoded_tables.input_bindings[0].endpoint_count = UINT16_C(1);
    memcpy(source->decoded_tables.input_bindings[0].endpoints[0].purpose,
        "midi.performance-input", sizeof("midi.performance-input"));
    memcpy(source->decoded_tables.input_bindings[0].endpoints[0].locator,
        "mock:arturia", sizeof("mock:arturia"));

    memcpy(source->decoded_tables.mappings[0].mapping, "master-volume",
        sizeof("master-volume"));
    memcpy(source->decoded_tables.mappings[0].control, "central-encoder",
        sizeof("central-encoder"));
    memcpy(source->decoded_tables.mappings[0].target, "master.volume",
        sizeof("master.volume"));
    source->decoded_tables.mappings[0].profile_index = UINT16_C(0);
    source->decoded_tables.mappings[0].event_type = MUSIC_RIG_MIDI_EVENT_CC;
    source->decoded_tables.mappings[0].edge = MUSIC_RIG_MIDI_EDGE_CHANGE;
    source->decoded_tables.mappings[0].channel = UINT8_C(1);
    source->decoded_tables.mappings[0].number = UINT8_C(114);
    source->decoded_tables.mappings[0].behavior =
        MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE;
    source->decoded_tables.mappings[0].transform =
        MUSIC_RIG_TRANSFORM_DIRECT;
    source->decoded_tables.mappings[0].takeover = MUSIC_RIG_TAKEOVER_PICKUP;

    memcpy(source->decoded_tables.target_bindings[0].target,
        "parameter.master-volume", sizeof("parameter.master-volume"));
    memcpy(source->decoded_tables.target_bindings[0].adapter, "mock-control",
        sizeof("mock-control"));
    memcpy(source->decoded_tables.target_bindings[0].locator, "mock:volume",
        sizeof("mock:volume"));
    source->decoded_tables.target_bindings[0].status =
        MUSIC_RIG_BINDING_STATUS_AVAILABLE;

    source->decoded_tables.ownership[0].kind =
        MUSIC_RIG_OWNERSHIP_KIND_PARAMETER;
    source->decoded_tables.ownership[0].mode =
        MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE;
    memcpy(source->decoded_tables.ownership[0].target, "master.volume",
        sizeof("master.volume"));
    source->decoded_tables.ownership[0].owner_count = UINT16_C(1);
    source->decoded_tables.ownership[0].owners[0].scope =
        MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE;
    source->decoded_tables.ownership[0].owners[0].profile_index = UINT16_C(0);
    memcpy(source->decoded_tables.ownership[0].owners[0].slot,
        "arturia-main", sizeof("arturia-main"));
    memcpy(source->decoded_tables.ownership[0].owners[0].profile,
        "multi-instrument-rack", sizeof("multi-instrument-rack"));
}

static music_rig_storage_adapter storage_for(mock_definition_source *source)
{
    music_rig_storage_adapter storage;

    storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    storage.context = source;
    storage.read = mock_read;
    storage.atomic_replace = mock_replace;
    return storage;
}

static music_rig_definition_decoder decoder_for(mock_definition_source *source)
{
    music_rig_definition_decoder decoder;

    decoder.context = source;
    decoder.decode = mock_decode;
    return decoder;
}

static int test_load_and_failures(void)
{
    static mock_definition_source source;
    static music_rig_compiled_tables tables;
    music_rig_storage_adapter storage;
    music_rig_definition_decoder decoder;
    music_rig_compiled_definition definition;
    music_rig_generation generation;
    uint8_t buffer[16];
    uint8_t unexpected[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];

    init_source(&source);
    storage = storage_for(&source);
    decoder = decoder_for(&source);
    if (music_rig_definition_load(
            &storage,
            &decoder,
            buffer,
            sizeof(buffer),
            source.decoded.fingerprint,
            sizeof(source.decoded.fingerprint),
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_definition_generation_init(
            &definition,
            &tables,
            &generation
        ) !=
            MUSIC_RIG_RESULT_OK ||
        generation.id != UINT64_C(1) || generation.mapping != &tables ||
        source.read_calls != 1U || source.decode_calls != 1U ||
        definition.mapping_count != UINT32_C(1) ||
        music_rig_compiled_mapping_lookup(
            &tables,
            UINT16_C(0),
            MUSIC_RIG_MIDI_EVENT_CC,
            UINT8_C(1),
            UINT8_C(114)
        ) != &tables.mappings[0] ||
        music_rig_compiled_target_lookup(
            &tables,
            "parameter.master-volume"
        ) != &tables.target_bindings[0] ||
        music_rig_compiled_ownership_lookup(
            &tables,
            MUSIC_RIG_OWNERSHIP_KIND_PARAMETER,
            "master.volume"
        ) != &tables.ownership[0]) {
        fputs("compiled definition load failed\n", stderr);
        return 1;
    }

    memset(unexpected, 0xff, sizeof(unexpected));
    if (music_rig_definition_load(
            &storage,
            &decoder,
            buffer,
            sizeof(buffer),
            unexpected,
            sizeof(unexpected),
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("unexpected definition fingerprint was accepted\n", stderr);
        return 1;
    }

    source.read_result = MUSIC_RIG_RESULT_NOT_FOUND;
    if (music_rig_definition_load(
            &storage,
            &decoder,
            buffer,
            sizeof(buffer),
            source.decoded.fingerprint,
            sizeof(source.decoded.fingerprint),
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_NOT_FOUND) {
        fputs("missing compiled definition was accepted\n", stderr);
        return 1;
    }

    source.read_result = MUSIC_RIG_RESULT_OK;
    source.document_size = sizeof(buffer) + 1U;
    if (music_rig_definition_load(
            &storage,
            &decoder,
            buffer,
            sizeof(buffer),
            source.decoded.fingerprint,
            sizeof(source.decoded.fingerprint),
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_BUFFER_TOO_SMALL) {
        fputs("oversize compiled definition was accepted\n", stderr);
        return 1;
    }

    source.document_size = 2U;
    source.decoded.graph_delta_empty = false;
    if (music_rig_definition_load(
            &storage,
            &decoder,
            buffer,
            sizeof(buffer),
            source.decoded.fingerprint,
            sizeof(source.decoded.fingerprint),
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("unsafe compiled definition was accepted\n", stderr);
        return 1;
    }
    return 0;
}

static int test_duplicate_dispatch_rejected(void)
{
    static mock_definition_source source;
    static music_rig_compiled_tables tables;

    init_source(&source);
    tables = source.decoded_tables;
    tables.mapping_count = UINT32_C(2);
    tables.mappings[1] = tables.mappings[0];
    if (music_rig_compiled_tables_prepare(
            &tables,
            UINT32_C(1),
            UINT32_C(2),
            UINT32_C(1),
            UINT32_C(1)
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("duplicate mapping dispatch was accepted\n", stderr);
        return 1;
    }
    return 0;
}

static int test_fingerprint_parser(void)
{
    static const char valid[] =
        "sha256:000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f";
    static const char uppercase[] =
        "sha256:000102030405060708090A0B0C0D0E0F"
        "101112131415161718191A1B1C1D1E1F";
    uint8_t output[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    size_t index;

    if (music_rig_definition_fingerprint_parse(
            valid,
            sizeof(valid) - 1U,
            output,
            sizeof(output)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("valid definition fingerprint was rejected\n", stderr);
        return 1;
    }
    for (index = 0; index < sizeof(output); ++index) {
        if (output[index] != (uint8_t)index) {
            fputs("definition fingerprint decoded incorrectly\n", stderr);
            return 1;
        }
    }
    if (music_rig_definition_fingerprint_parse(
            uppercase,
            sizeof(uppercase) - 1U,
            output,
            sizeof(output)
        ) != MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_definition_fingerprint_parse(
            valid,
            sizeof(valid) - 2U,
            output,
            sizeof(output)
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid definition fingerprint was accepted\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    return test_load_and_failures() != 0 ||
        test_duplicate_dispatch_rejected() != 0 ||
        test_fingerprint_parser() != 0;
}
