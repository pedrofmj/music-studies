#include "music_rig/core.h"
#include "music_rig/compiled_tables.h"
#include "music_rig/diagnostics.h"
#include "music_rig/runtime.h"

#if defined(MUSIC_RIG_HAS_FILE_STORAGE)
#include "music_rig/file_storage.h"
#endif

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
#include "music_rig/host_paths.h"
#include "music_rig/journal_diagnostics.h"
#include "music_rig/linux_lifecycle.h"
#endif

#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
#include "music_rig/definition.h"
#include "music_rig/definition_json.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
#include <unistd.h>
#endif

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s --version\n", program);
#if defined(MUSIC_RIG_HAS_LINUX_HOST)
    fprintf(stream, "       %s resolve-paths --check-only\n", program);
    fprintf(
        stream,
        "       %s run-shadow --output-suppressed\n",
        program
    );
#endif
#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
    fprintf(
        stream,
        "       %s validate-definition --definition PATH "
        "--expected-fingerprint SHA256\n",
        program
    );
#endif
}

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
static int resolve_host_paths(music_rig_host_paths *paths)
{
    music_rig_result result = music_rig_linux_host_paths_from_process(paths);

    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "host path resolution failed: result %d\n", (int)result);
    }
    return (int)result;
}

static int report_host_paths(void)
{
    music_rig_host_paths paths;
    int result = resolve_host_paths(&paths);

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    printf("config-directory %s\n", paths.config_directory);
    printf("config-file %s\n", paths.config_file);
    printf("cache-directory %s\n", paths.cache_directory);
    printf("compiled-cache-directory %s\n", paths.compiled_cache_directory);
    printf("state-directory %s\n", paths.state_directory);
    printf("active-state-file %s\n", paths.active_state_file);
    printf("device-state-directory %s\n", paths.device_state_directory);
    printf("runtime-directory %s\n", paths.runtime_directory);
    printf("control-socket %s\n", paths.control_socket);
    puts("output-mode suppressed");
    puts("filesystem-writes 0");
    return MUSIC_RIG_RESULT_OK;
}

static int run_shadow(void)
{
    music_rig_host_paths paths;
    music_rig_journal_diagnostics journal;
    music_rig_diagnostic_sink sink;
    music_rig_result result;

    result = (music_rig_result)resolve_host_paths(&paths);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_journal_diagnostics_init(&journal, STDERR_FILENO, &sink);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_linux_shadow_lifecycle_run(&sink);
    }
    return (int)result;
}
#endif

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
        printf("diagnostic-sink-abi %u\n",
            MUSIC_RIG_DIAGNOSTIC_SINK_ABI_VERSION);
        printf("compiled-tables %u\n", MUSIC_RIG_COMPILED_TABLES_VERSION);
#if defined(MUSIC_RIG_HAS_FILE_STORAGE)
        printf("file-storage-abi %u\n", MUSIC_RIG_FILE_STORAGE_ABI_VERSION);
#endif
#if defined(MUSIC_RIG_HAS_LINUX_HOST)
        printf("host-paths-abi %u\n", MUSIC_RIG_HOST_PATHS_ABI_VERSION);
        printf("linux-lifecycle-abi %u\n",
            MUSIC_RIG_LINUX_LIFECYCLE_ABI_VERSION);
        printf("journal-diagnostics-abi %u\n",
            MUSIC_RIG_JOURNAL_DIAGNOSTICS_ABI_VERSION);
#endif
        printf("output-mode suppressed-only\n");
        return MUSIC_RIG_RESULT_OK;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout, argv[0]);
        return MUSIC_RIG_RESULT_OK;
    }

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
    if (argc == 3 && strcmp(argv[1], "resolve-paths") == 0 &&
        strcmp(argv[2], "--check-only") == 0) {
        return report_host_paths();
    }
    if (argc == 3 && strcmp(argv[1], "run-shadow") == 0 &&
        strcmp(argv[2], "--output-suppressed") == 0) {
        return run_shadow();
    }
#endif

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
