#include "music_rig/definition.h"

#include <stdio.h>
#include <string.h>

typedef struct mock_definition_source {
    const uint8_t *document;
    size_t document_size;
    music_rig_result read_result;
    music_rig_result decode_result;
    music_rig_compiled_definition decoded;
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
    music_rig_compiled_definition *definition
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
    source->decoded.device_profile_count = UINT32_C(5);
    source->decoded.mapping_count = UINT32_C(72);
    source->decoded.target_binding_count = UINT32_C(71);
    source->decoded.ownership_count = UINT32_C(57);
    source->decoded.control_only = true;
    source->decoded.graph_delta_empty = true;
    source->decoded.authoring_only = true;
    for (index = 0; index < MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE; ++index) {
        source->decoded.fingerprint[index] = (uint8_t)index;
    }
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
    mock_definition_source source;
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
            &definition
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_definition_generation_init(&definition, &generation) !=
            MUSIC_RIG_RESULT_OK ||
        generation.id != UINT64_C(1) || generation.mapping != &definition ||
        source.read_calls != 1U || source.decode_calls != 1U ||
        definition.mapping_count != UINT32_C(72)) {
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
            &definition
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
            &definition
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
            &definition
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
            &definition
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("unsafe compiled definition was accepted\n", stderr);
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
    return test_load_and_failures() != 0 || test_fingerprint_parser() != 0;
}
