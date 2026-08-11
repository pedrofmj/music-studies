#include "music_rig/core.h"
#include "music_rig/compiled_tables.h"
#include "music_rig/runtime.h"

#if defined(MUSIC_RIG_HAS_FILE_STORAGE)
#include "music_rig/file_storage.h"
#endif

#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
#include "music_rig/definition.h"
#include "music_rig/definition_json.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s --version\n", program);
#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
    fprintf(
        stream,
        "       %s validate-definition --definition PATH "
        "--expected-fingerprint SHA256\n",
        program
    );
#endif
}

#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
#define DEFINITION_DOCUMENT_CAPACITY ((size_t)131072)

static uint8_t definition_document[DEFINITION_DOCUMENT_CAPACITY];
static music_rig_compiled_tables definition_tables;

static int validate_definition(const char *path, const char *fingerprint)
{
    music_rig_file_storage file_storage = {0};
    music_rig_storage_adapter storage = {0};
    music_rig_definition_decoder decoder;
    music_rig_compiled_definition definition = {0};
    music_rig_generation generation = {0};
    uint8_t expected[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];
    music_rig_result result;

    result = music_rig_definition_fingerprint_parse(
        fingerprint,
        strlen(fingerprint),
        expected,
        sizeof(expected)
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_file_storage_init(
            &file_storage,
            path,
            NULL,
            &storage
        );
    }
    decoder.context = NULL;
    decoder.decode = music_rig_definition_json_decode;
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_definition_load(
            &storage,
            &decoder,
            definition_document,
            sizeof(definition_document),
            expected,
            sizeof(expected),
            &definition,
            &definition_tables
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_definition_generation_init(
            &definition,
            &definition_tables,
            &generation
        );
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "definition validation failed: result %d\n", (int)result);
        return (int)result;
    }

    puts("definition valid");
    printf("generation %" PRIu64 "\n", generation.id);
    printf("rig %s\n", definition.rig_id);
    printf("rig-profile %s\n", definition.active_rig_profile);
    printf("platform-binding %s\n", definition.platform_binding_id);
    printf("platform %s\n", definition.platform);
    printf("device-profiles %" PRIu32 "\n", definition.device_profile_count);
    printf("input-bindings %" PRIu32 "\n",
        definition_tables.input_binding_count);
    printf("mappings %" PRIu32 "\n", definition.mapping_count);
    printf("dispatch-entries %" PRIu32 "\n",
        definition_tables.mapping_count);
    printf("target-bindings %" PRIu32 "\n", definition.target_binding_count);
    printf("ownership %" PRIu32 "\n", definition.ownership_count);
    printf("table-storage-bytes %zu\n", sizeof(definition_tables));
    puts("output-mode suppressed");
    return MUSIC_RIG_RESULT_OK;
}
#endif

int main(int argc, char **argv)
{
    const music_rig_build_info *build_info;

    if (argc == 2 &&
        (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0)) {
        build_info = music_rig_get_build_info();
        printf("music-rigd %s\n", build_info->core_version);
        printf("protocol %u\n", build_info->protocol_version);
        printf("runtime-abi %u\n", MUSIC_RIG_RUNTIME_ABI_VERSION);
        printf("runtime-state %u\n", MUSIC_RIG_RUNTIME_STATE_VERSION);
        printf("storage-abi %u\n", MUSIC_RIG_STORAGE_ABI_VERSION);
        printf("compiled-tables %u\n", MUSIC_RIG_COMPILED_TABLES_VERSION);
#if defined(MUSIC_RIG_HAS_FILE_STORAGE)
        printf("file-storage-abi %u\n", MUSIC_RIG_FILE_STORAGE_ABI_VERSION);
#endif
        printf("output-mode suppressed-only\n");
        return MUSIC_RIG_RESULT_OK;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout, argv[0]);
        return MUSIC_RIG_RESULT_OK;
    }

#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
    if (argc == 6 && strcmp(argv[1], "validate-definition") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0) {
        return validate_definition(argv[3], argv[5]);
    }
#endif

    print_usage(stderr, argv[0]);
    return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}
