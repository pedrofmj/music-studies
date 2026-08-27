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
#include "music_rig/linux_control_socket.h"
#include "music_rig/linux_lifecycle.h"
#endif

#if defined(MUSIC_RIG_HAS_JACK_MIDI_SHADOW)
#include "music_rig/device_midi_shadow.h"
#include "music_rig/jack_midi_shadow.h"
#endif

#if defined(MUSIC_RIG_HAS_JACK_SMC_MIXER_RELAY)
#include "music_rig/jack_smc_mixer_relay.h"
#endif

#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
#include "music_rig/definition.h"
#include "music_rig/definition_json.h"
#endif

#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
#include <unistd.h>
#endif

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s --version\n", program);
#if defined(MUSIC_RIG_HAS_LINUX_HOST)
    fprintf(stream,
        "       %s run --definition PATH --expected-fingerprint SHA256 "
        "[--prepared-definition PATH --prepared-fingerprint SHA256]\n",
        program
    );
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
#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION) && \
    defined(MUSIC_RIG_HAS_JACK_MIDI_SHADOW)
    fprintf(
        stream,
        "       %s run-midi-shadow --definition PATH "
        "--expected-fingerprint SHA256 --output-suppressed\n",
        program
    );
#endif
#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION) && \
    defined(MUSIC_RIG_HAS_JACK_SMC_MIXER_RELAY)
    fprintf(
        stream,
        "       %s run-smc-mixer-relay --definition PATH "
        "--expected-fingerprint SHA256 --output-enabled "
        "--acknowledge-smc-mixer-cutover\n",
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

static music_rig_result load_definition_into(
    const char *path,
    const char *fingerprint,
    uint8_t *document,
    size_t document_capacity,
    music_rig_compiled_tables *tables,
    music_rig_compiled_definition *definition,
    music_rig_generation *generation
)
{
    music_rig_file_storage file_storage = {0};
    music_rig_storage_adapter storage = {0};
    music_rig_definition_decoder decoder;
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
            document,
            document_capacity,
            expected,
            sizeof(expected),
            definition,
            tables
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_definition_generation_init(
            definition,
            tables,
            generation
        );
    }
    return result;
}

static music_rig_result load_definition(
    const char *path,
    const char *fingerprint,
    music_rig_compiled_definition *definition,
    music_rig_generation *generation
)
{
    return load_definition_into(
        path,
        fingerprint,
        definition_document,
        sizeof(definition_document),
        &definition_tables,
        definition,
        generation
    );
}

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
static uint64_t daemon_clock(void *context)
{
    struct timespec now;
    (void)context;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return UINT64_C(0);
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
        (uint64_t)now.tv_nsec;
}

#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
static int run_runtime(
    const char *definition_path,
    const char *fingerprint,
    const char *prepared_path,
    const char *prepared_fingerprint
)
{
    static uint8_t prepared_document[DEFINITION_DOCUMENT_CAPACITY];
    static music_rig_compiled_tables prepared_tables;
    static music_rig_compiled_definition prepared_definition;
    music_rig_compiled_definition definition = {0};
    music_rig_generation generation = {0};
    music_rig_generation prepared_generation = {0};
    music_rig_prepared_definition prepared = {0};
    music_rig_runtime runtime;
    music_rig_runtime_config config;
    music_rig_platform_interfaces interfaces;
    music_rig_file_storage storage_file = {0};
    music_rig_storage_adapter storage;
    music_rig_linux_control_server server;
    music_rig_host_paths paths;
    music_rig_result result;

    memset(&server, 0, sizeof(server));
    server.listener = -1;
    server.client = -1;
    result = music_rig_linux_host_paths_from_process(&paths);
    if (result == MUSIC_RIG_RESULT_OK &&
        mkdir(paths.runtime_directory, 0700) != 0 && errno != EEXIST) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK &&
        mkdir(paths.state_directory, 0700) != 0 && errno != EEXIST) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = load_definition(definition_path, fingerprint,
            &definition, &generation);
    }
    if (result == MUSIC_RIG_RESULT_OK && prepared_path != NULL) {
        result = load_definition_into(
            prepared_path,
            prepared_fingerprint,
            prepared_document,
            sizeof(prepared_document),
            &prepared_tables,
            &prepared_definition,
            &prepared_generation
        );
        prepared.definition = &prepared_definition;
        prepared.tables = &prepared_tables;
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_file_storage_init(
            &storage_file,
            definition_path,
            paths.active_state_file,
            &storage
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_linux_control_server_open(
            &server, paths.control_socket
        );
    }
    memset(&interfaces, 0, sizeof(interfaces));
    interfaces.abi_version = MUSIC_RIG_RUNTIME_ABI_VERSION;
    interfaces.clock.now_ns = daemon_clock;
    interfaces.control.context = &server;
    interfaces.control.start = music_rig_linux_control_server_start;
    interfaces.control.poll = music_rig_linux_control_server_poll;
    interfaces.control.wait = music_rig_linux_control_server_wait;
    interfaces.control.respond = music_rig_linux_control_server_respond;
    interfaces.control.stop = music_rig_linux_control_server_stop;
    interfaces.storage = storage;
    memset(&config, 0, sizeof(config));
    config.initial_generation = &generation;
    config.definition_fingerprint = definition.fingerprint;
    config.definition_fingerprint_size = sizeof(definition.fingerprint);
    config.active_rig_profile = definition.active_rig_profile;
    config.prepared_definitions = prepared_path == NULL ? NULL : &prepared;
    config.prepared_definition_count = prepared_path == NULL ? 0U : 1U;
    config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    if (result == MUSIC_RIG_RESULT_OK) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = music_rig_linux_control_server_request_stop;
        sigemptyset(&action.sa_mask);
        (void)sigaction(SIGINT, &action, NULL);
        (void)sigaction(SIGTERM, &action, NULL);
        result = music_rig_runtime_init(&runtime, &config, &interfaces);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_runtime_run(&runtime);
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "runtime failed: result %d\n", (int)result);
    }
    if (server.listener >= 0) {
        (void)music_rig_linux_control_server_stop(&server);
    }
    return (int)result;
}
#endif
#endif

static int validate_definition(const char *path, const char *fingerprint)
{
    music_rig_compiled_definition definition = {0};
    music_rig_generation generation = {0};
    music_rig_result result = load_definition(
        path,
        fingerprint,
        &definition,
        &generation
    );

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

#if defined(MUSIC_RIG_HAS_JACK_MIDI_SHADOW)
static void configure_current_behaviors(
    music_rig_device_midi_shadow_config *config
)
{
    size_t index;

    for (index = 0U; index < definition_tables.device_profile_count; ++index) {
        const char *preset =
            definition_tables.device_profiles[index].hardware_preset;

        if (strcmp(preset, "arturia-current-rack") == 0) {
            config->behaviors[index] =
                MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA;
        } else if (strcmp(preset, "smk25-current-pad-layers") == 0) {
            config->behaviors[index] =
                MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25;
        }
    }
}

static int run_midi_shadow(const char *path, const char *fingerprint)
{
    music_rig_compiled_definition definition = {0};
    music_rig_generation generation = {0};
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow_config config;
    music_rig_device_midi_shadow shadow;
    music_rig_jack_midi_shadow host;
    music_rig_journal_diagnostics journal;
    music_rig_diagnostic_sink sink;
    const music_rig_device_midi_shadow_metrics *metrics;
    music_rig_result result;
    music_rig_result stop_result = MUSIC_RIG_RESULT_OK;
    size_t index;

    result = load_definition(path, fingerprint, &definition, &generation);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_generation_slot_init(&generations, &generation);
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = &generations;
    if (result == MUSIC_RIG_RESULT_OK) {
        configure_current_behaviors(&config);
        result = music_rig_device_midi_shadow_init(&shadow, &config);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_shadow_init(&host, &shadow);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_journal_diagnostics_init(
            &journal,
            STDERR_FILENO,
            &sink
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_shadow_start(&host);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_linux_shadow_lifecycle_run(&sink);
        stop_result = music_rig_jack_midi_shadow_stop(&host);
        if (result == MUSIC_RIG_RESULT_OK) {
            result = stop_result;
        }
        if (result == MUSIC_RIG_RESULT_OK &&
            atomic_load_explicit(
                &host.last_process_result,
                memory_order_acquire
            ) != MUSIC_RIG_RESULT_OK) {
            result = atomic_load_explicit(
                &host.last_process_result,
                memory_order_relaxed
            );
        }
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "MIDI shadow failed: result %d\n", (int)result);
        return (int)result;
    }
    metrics = music_rig_device_midi_shadow_metrics_read(&shadow);
    printf("definition-generation %" PRIu64 "\n", generation.id);
    printf("input-ports %zu\n", host.port_count);
    printf("cycles %" PRIu64 "\n", metrics->cycles);
    printf("input-events %" PRIu64 "\n", metrics->input_events);
    printf("mapping-decisions %" PRIu64 "\n", metrics->mapping_decisions);
    printf("suppressed-midi-events %" PRIu64 "\n",
        metrics->suppressed_midi_events);
    for (index = 0U; index < host.port_count; ++index) {
        printf(
            "slot %s input-events %" PRIu64 " mapping-decisions %" PRIu64
            " suppressed-midi-events %" PRIu64 "\n",
            music_rig_device_midi_shadow_slot_name(&shadow, index),
            metrics->slots[index].input_events,
            metrics->slots[index].mapping_decisions,
            metrics->slots[index].suppressed_midi_events
        );
    }
    puts("output-mode suppressed");
    return MUSIC_RIG_RESULT_OK;
}
#endif

#if defined(MUSIC_RIG_HAS_JACK_SMC_MIXER_RELAY)
static int run_smc_mixer_relay(const char *path, const char *fingerprint)
{
    music_rig_compiled_definition definition = {0};
    music_rig_generation generation = {0};
    music_rig_generation_slot generations;
    music_rig_jack_smc_mixer_relay host;
    music_rig_journal_diagnostics journal;
    music_rig_diagnostic_sink sink;
    const music_rig_smc_mixer_relay_metrics *metrics;
    music_rig_result result;
    music_rig_result stop_result = MUSIC_RIG_RESULT_OK;
    size_t index;

    result = load_definition(path, fingerprint, &definition, &generation);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_generation_slot_init(&generations, &generation);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_smc_mixer_relay_init(&host, &generations);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_journal_diagnostics_init(
            &journal,
            STDERR_FILENO,
            &sink
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_smc_mixer_relay_start(&host);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_linux_shadow_lifecycle_run(&sink);
        stop_result = music_rig_jack_smc_mixer_relay_stop(&host);
        if (result == MUSIC_RIG_RESULT_OK) {
            result = stop_result;
        }
        if (result == MUSIC_RIG_RESULT_OK &&
            atomic_load_explicit(
                &host.last_process_result,
                memory_order_acquire
            ) != MUSIC_RIG_RESULT_OK) {
            result = atomic_load_explicit(
                &host.last_process_result,
                memory_order_relaxed
            );
        }
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "SMC-Mixer relay failed: result %d\n", (int)result);
        return (int)result;
    }
    metrics = music_rig_smc_mixer_relay_metrics_read(&host.relay);
    printf("definition-generation %" PRIu64 "\n", generation.id);
    puts("slot smc-mixer-main");
    puts("input-ports 1");
    puts("output-ports 1");
    printf("cycles %" PRIu64 "\n", metrics->cycles);
    printf("input-events %" PRIu64 "\n", metrics->input_events);
    printf("mapped-events %" PRIu64 "\n", metrics->mapped_events);
    for (index = 0U; index < 8U; ++index) {
        printf(
            "control-cc-%zu-mapped-events %" PRIu64 "\n",
            index + 40U,
            metrics->control_mapped_events[index]
        );
    }
    printf("emitted-events %" PRIu64 "\n", metrics->emitted_events);
    printf("unmapped-events %" PRIu64 "\n", metrics->unmapped_events);
    printf("malformed-events %" PRIu64 "\n", metrics->malformed_events);
    printf("adapter-failures %" PRIu64 "\n", metrics->adapter_failures);
    puts("output-mode enabled-smc-mixer-only");
    return MUSIC_RIG_RESULT_OK;
}
#endif
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
#if defined(MUSIC_RIG_HAS_JACK_MIDI_SHADOW)
        printf("jack-midi-shadow-abi %u\n",
            MUSIC_RIG_JACK_MIDI_SHADOW_ABI_VERSION);
#endif
#if defined(MUSIC_RIG_HAS_JACK_SMC_MIXER_RELAY)
        printf("jack-smc-mixer-relay-abi %u\n",
            MUSIC_RIG_JACK_SMC_MIXER_RELAY_ABI_VERSION);
        printf("output-mode suppressed-default-explicit-smc-mixer\n");
#else
        printf("output-mode suppressed-only\n");
#endif
        return MUSIC_RIG_RESULT_OK;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout, argv[0]);
        return MUSIC_RIG_RESULT_OK;
    }

#if defined(MUSIC_RIG_HAS_LINUX_HOST)
    if (argc == 6 && strcmp(argv[1], "run") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0) {
#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
        return run_runtime(argv[3], argv[5], NULL, NULL);
#else
        return MUSIC_RIG_RESULT_UNSUPPORTED;
#endif
    }
    if (argc == 10 && strcmp(argv[1], "run") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0 &&
        strcmp(argv[6], "--prepared-definition") == 0 &&
        strcmp(argv[8], "--prepared-fingerprint") == 0) {
#if defined(MUSIC_RIG_ENABLE_JSON_DEFINITION)
        return run_runtime(argv[3], argv[5], argv[7], argv[9]);
#else
        return MUSIC_RIG_RESULT_UNSUPPORTED;
#endif
    }
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
#if defined(MUSIC_RIG_HAS_JACK_MIDI_SHADOW)
    if (argc == 7 && strcmp(argv[1], "run-midi-shadow") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0 &&
        strcmp(argv[6], "--output-suppressed") == 0) {
        return run_midi_shadow(argv[3], argv[5]);
    }
#endif
#if defined(MUSIC_RIG_HAS_JACK_SMC_MIXER_RELAY)
    if (argc == 8 && strcmp(argv[1], "run-smc-mixer-relay") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0 &&
        strcmp(argv[6], "--output-enabled") == 0 &&
        strcmp(argv[7], "--acknowledge-smc-mixer-cutover") == 0) {
        return run_smc_mixer_relay(argv[3], argv[5]);
    }
#endif
#endif

    print_usage(stderr, argv[0]);
    return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}
