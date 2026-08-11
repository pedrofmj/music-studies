#include "music_rig/definition.h"
#include "music_rig/definition_json.h"

#include <stdio.h>
#include <string.h>

#define DOCUMENT_CAPACITY ((size_t)131072)

typedef struct document_source {
    uint8_t content[DOCUMENT_CAPACITY];
    size_t size;
} document_source;

static int read_file(const char *path, document_source *source)
{
    FILE *stream;
    long length;

#if defined(_MSC_VER)
    if (fopen_s(&stream, path, "rb") != 0) {
        stream = NULL;
    }
#else
    stream = fopen(path, "rb");
#endif
    if (stream == NULL || fseek(stream, 0, SEEK_END) != 0 ||
        (length = ftell(stream)) <= 0 ||
        (unsigned long)length >= (unsigned long)sizeof(source->content) ||
        fseek(stream, 0, SEEK_SET) != 0 ||
        fread(source->content, 1U, (size_t)length, stream) != (size_t)length) {
        if (stream != NULL) {
            fclose(stream);
        }
        return 0;
    }
    fclose(stream);
    source->size = (size_t)length;
    return 1;
}

static music_rig_result source_read(
    void *opaque,
    music_rig_storage_object object,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
)
{
    document_source *source = opaque;

    if (object != MUSIC_RIG_STORAGE_COMPILED_DEFINITION) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (output_capacity < source->size) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, source->content, source->size);
    *output_size = source->size;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result source_replace(
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

int main(int argc, char **argv)
{
    static const char fingerprint_text[] =
        "sha256:9be68993c164802f694f4d6359c208d9"
        "937fbfcf668781ce5a1c3bff5f30cb9e";
    static uint8_t workspace[DOCUMENT_CAPACITY];
    static document_source source;
    music_rig_storage_adapter storage;
    music_rig_definition_decoder decoder;
    music_rig_compiled_definition definition;
    music_rig_generation generation;
    uint8_t expected_fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];

    if (argc != 2 || !read_file(argv[1], &source)) {
        fputs("compiled definition fixture read failed\n", stderr);
        return 1;
    }
    if (music_rig_definition_fingerprint_parse(
            fingerprint_text,
            sizeof(fingerprint_text) - 1U,
            expected_fingerprint,
            sizeof(expected_fingerprint)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("compiled definition expected fingerprint is invalid\n", stderr);
        return 1;
    }

    storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    storage.context = &source;
    storage.read = source_read;
    storage.atomic_replace = source_replace;
    decoder.context = NULL;
    decoder.decode = music_rig_definition_json_decode;
    if (music_rig_definition_load(
            &storage,
            &decoder,
            workspace,
            sizeof(workspace),
            expected_fingerprint,
            sizeof(expected_fingerprint),
            &definition
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_definition_generation_init(&definition, &generation) !=
            MUSIC_RIG_RESULT_OK ||
        generation.id != UINT64_C(1) || generation.mapping != &definition ||
        strcmp(definition.rig_id, "pedro-performance-rig") != 0 ||
        strcmp(definition.active_rig_profile, "full-live-rack") != 0 ||
        strcmp(definition.platform_binding_id, "airstar-current") != 0 ||
        strcmp(definition.platform, "linux") != 0 ||
        definition.device_profile_count != UINT32_C(5) ||
        definition.mapping_count != UINT32_C(72) ||
        definition.target_binding_count != UINT32_C(71) ||
        definition.ownership_count != UINT32_C(57)) {
        fputs("compiled definition load contract failed\n", stderr);
        return 1;
    }

    workspace[source.size] = (uint8_t)'x';
    if (music_rig_definition_json_decode(
            NULL,
            workspace,
            source.size + 1U,
            &definition
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("compiled definition trailing data was accepted\n", stderr);
        return 1;
    }
    return 0;
}
