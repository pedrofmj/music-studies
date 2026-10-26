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

#if defined(MUSIC_RIG_HAS_WIN32_CARLA_RUNTIME)
#include "music_rig/arturia_carla_host.h"
#include "music_rig/win32_carla_runtime.h"
#include "music_rig/winmm_midi_win32.h"
#endif

#if defined(MUSIC_RIG_HAS_JACK_MIDI_SHADOW)
#include "music_rig/device_midi_shadow.h"
#include "music_rig/jack_midi_shadow.h"
#endif

#if defined(MUSIC_RIG_HAS_JACK_MIDI_OUTPUT)
#include "music_rig/jack_midi_output.h"
#endif

#if defined(MUSIC_RIG_HAS_JACK_MIDI_OUTPUT)
#include "music_rig/jack_midi_output.h"
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

#if defined(MUSIC_RIG_HAS_WIN32_CARLA_RUNTIME)
#include <windows.h>
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
#if defined(MUSIC_RIG_HAS_WIN32_CARLA_RUNTIME)
    fprintf(
        stream,
        "       %s run-win32-carla --carla-dll DLL --project CARXP "
        "--driver NAME --client NAME --midi-client LOOPBACK_CLIENT "
        "--midi-port LOOPBACK_PORT --input-interface PHYSICAL_WINMM_PATH "
        "--loopback-output-interface LOOPBACK_WINMM_PATH "
        "--patch-names CSV --patch-indexes 9_CSV --active-layers CSV "
        "[--pad-profile CODE:PATCH_INDEXES:ACTIVE_LAYERS]... "
        "--master-volume 0..127\n",
        program
    );
    fprintf(
        stream,
        "       %s run-win32-carla --help\n",
        program
    );
    fputs(
        "         Windows only; uses WinMM slot arturia-main, with the "
        "physical controller as input and the dedicated Basic Loopback "
        "output. Carla MIDI names must identify the matching loopback input.\n",
        stream
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
    defined(MUSIC_RIG_HAS_JACK_MIDI_OUTPUT)
    fprintf(
        stream,
        "       %s run-midi-output --definition PATH "
        "--expected-fingerprint SHA256 --output-enabled "
        "--acknowledge-output\n",
        program
    );
    fprintf(
        stream,
        "       %s run-output --definition PATH "
        "--expected-fingerprint SHA256 --acknowledge-output\n",
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

#if defined(MUSIC_RIG_HAS_WIN32_CARLA_RUNTIME)
#define WIN32_CARLA_OPTION_COUNT 12U
#define WIN32_CARLA_PATCH_NAME_CAPACITY 256U
#define WIN32_CARLA_VALUE_CAPACITY 32768U
#define WIN32_CARLA_INDEX_CSV_CAPACITY (6U * 9U + 1U)
#define WIN32_CARLA_LAYER_CSV_CAPACITY (2U * 9U + 1U)

typedef enum win32_carla_option {
    WIN32_CARLA_OPTION_DLL = 0,
    WIN32_CARLA_OPTION_PROJECT,
    WIN32_CARLA_OPTION_DRIVER,
    WIN32_CARLA_OPTION_CLIENT,
    WIN32_CARLA_OPTION_MIDI_CLIENT,
    WIN32_CARLA_OPTION_MIDI_PORT,
    WIN32_CARLA_OPTION_INPUT_INTERFACE,
    WIN32_CARLA_OPTION_LOOPBACK_OUTPUT_INTERFACE,
    WIN32_CARLA_OPTION_PATCH_NAMES,
    WIN32_CARLA_OPTION_PATCH_INDEXES,
    WIN32_CARLA_OPTION_ACTIVE_LAYERS,
    WIN32_CARLA_OPTION_MASTER_VOLUME
} win32_carla_option;

static const char *const win32_carla_option_names[WIN32_CARLA_OPTION_COUNT] = {
    "--carla-dll",
    "--project",
    "--driver",
    "--client",
    "--midi-client",
    "--midi-port",
    "--input-interface",
    "--loopback-output-interface",
    "--patch-names",
    "--patch-indexes",
    "--active-layers",
    "--master-volume"
};

typedef struct win32_carla_arguments {
    const char *values[WIN32_CARLA_OPTION_COUNT];
    bool seen[WIN32_CARLA_OPTION_COUNT];
    char patch_name_storage[MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY]
        [WIN32_CARLA_PATCH_NAME_CAPACITY];
    const char *patch_names[MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY];
    uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    uint8_t active_layers[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    const char *pad_profile_values[
        MUSIC_RIG_WIN32_CARLA_RUNTIME_PROFILE_CAPACITY
    ];
    music_rig_win32_carla_profile_map pad_profiles[
        MUSIC_RIG_WIN32_CARLA_RUNTIME_PROFILE_CAPACITY
    ];
    size_t patch_count;
    size_t active_layer_count;
    size_t pad_profile_count;
    uint8_t master_volume;
    music_rig_winmm_midi_endpoint endpoint;
} win32_carla_arguments;

static volatile LONG win32_carla_stop_requested;

static void print_win32_carla_help(FILE *stream, const char *program)
{
    print_usage(stream, program);
    fputs(
        "         --patch-names is a comma-separated catalogue of 1..256 "
        "nonempty names.\n"
        "         --patch-indexes contains exactly 9 zero-based catalogue "
        "indexes.\n"
        "         --active-layers contains 1..9 unique layer numbers "
        "(1..9).\n"
        "         Repeat --pad-profile CODE:PATCH_INDEXES:ACTIVE_LAYERS; "
        "codes 1..16 match Android ArturiaPadModes (Bank A pads 1..8, then "
        "Bank B pads 1..8). Code 2 is unsupported.\n"
        "         Pad-profile indexes use the same catalogue as the initial "
        "profile; each profile has exactly 9 indexes.\n"
        "         Endpoint paths are UTF-8 WinMM interface paths; the Carla "
        "MIDI client/port identify the Basic Loopback input.\n",
        stream
    );
}

static bool win32_carla_option_id(
    const char *name,
    win32_carla_option *option
)
{
    size_t index;

    for (index = 0U; index < WIN32_CARLA_OPTION_COUNT; ++index) {
        if (strcmp(name, win32_carla_option_names[index]) == 0) {
            *option = (win32_carla_option)index;
            return true;
        }
    }
    return false;
}

static bool win32_carla_parse_arguments(
    int argc,
    char **argv,
    win32_carla_arguments *arguments
)
{
    int index;
    size_t option_index;

    memset(arguments, 0, sizeof(*arguments));
    for (index = 2; index < argc; ++index) {
        win32_carla_option option;
        const char *value;

        if (strcmp(argv[index], "--pad-profile") == 0) {
            if (index + 1 >= argc || argv[index + 1][0] == '\0' ||
                argv[index + 1][0] == '-') {
                fprintf(stderr,
                    "missing value for run-win32-carla option: %s\n",
                    argv[index]);
                return false;
            }
            if (arguments->pad_profile_count >=
                    MUSIC_RIG_WIN32_CARLA_RUNTIME_PROFILE_CAPACITY) {
                fprintf(stderr, "too many --pad-profile options\n");
                return false;
            }
            arguments->pad_profile_values[arguments->pad_profile_count++] =
                argv[++index];
            continue;
        }
        if (!win32_carla_option_id(argv[index], &option)) {
            fprintf(stderr, "unknown run-win32-carla option: %s\n", argv[index]);
            return false;
        }
        option_index = (size_t)option;
        if (arguments->seen[option_index]) {
            fprintf(stderr, "duplicate run-win32-carla option: %s\n", argv[index]);
            return false;
        }
        if (index + 1 >= argc || argv[index + 1][0] == '\0' ||
            argv[index + 1][0] == '-') {
            fprintf(stderr, "missing value for run-win32-carla option: %s\n",
                argv[index]);
            return false;
        }
        value = argv[++index];
        arguments->seen[option_index] = true;
        arguments->values[option_index] = value;
    }
    for (option_index = 0U; option_index < WIN32_CARLA_OPTION_COUNT;
         ++option_index) {
        if (!arguments->seen[option_index]) {
            fprintf(stderr, "missing required run-win32-carla option: %s\n",
                win32_carla_option_names[option_index]);
            return false;
        }
    }
    return true;
}

static bool win32_carla_nonempty_text(
    const char *text,
    size_t capacity
)
{
    size_t index;
    bool non_whitespace = false;

    if (text == NULL || capacity == 0U) {
        return false;
    }
    for (index = 0U; index < capacity; ++index) {
        unsigned char character = (unsigned char)text[index];

        if (character == '\0') {
            return non_whitespace;
        }
        if (character != ' ' && character != '\t' && character != '\r' &&
            character != '\n' && character != '\v' && character != '\f') {
            non_whitespace = true;
        }
    }
    return false;
}

static bool win32_carla_parse_decimal(
    const char *text,
    size_t length,
    uint32_t maximum,
    uint32_t *value
)
{
    uint32_t parsed = 0U;
    size_t index;

    if (text == NULL || length == 0U || value == NULL) {
        return false;
    }
    for (index = 0U; index < length; ++index) {
        uint32_t digit;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (uint32_t)(text[index] - '0');
        if (digit > maximum || parsed > (maximum - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
    }
    *value = parsed;
    return true;
}

static bool win32_carla_parse_patch_names(
    const char *csv,
    win32_carla_arguments *arguments
)
{
    const char *start = csv;

    while (true) {
        const char *end = strchr(start, ',');
        size_t length = end == NULL ? strlen(start) : (size_t)(end - start);
        size_t index = arguments->patch_count;

        if (index >= MUSIC_RIG_ARTURIA_CARLA_PATCH_CAPACITY || length == 0U ||
            length >= WIN32_CARLA_PATCH_NAME_CAPACITY) {
            return false;
        }
        memcpy(arguments->patch_name_storage[index], start, length);
        arguments->patch_name_storage[index][length] = '\0';
        if (!win32_carla_nonempty_text(
                arguments->patch_name_storage[index],
                WIN32_CARLA_PATCH_NAME_CAPACITY
            )) {
            return false;
        }
        arguments->patch_names[index] = arguments->patch_name_storage[index];
        arguments->patch_count += 1U;
        if (end == NULL) {
            return true;
        }
        start = end + 1;
        if (*start == '\0') {
            return false;
        }
    }
}

static bool win32_carla_parse_index_csv(
    const char *csv,
    uint16_t patch_count,
    uint16_t indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT]
)
{
    const char *start = csv;
    size_t count = 0U;

    while (true) {
        const char *end = strchr(start, ',');
        size_t length = end == NULL ? strlen(start) : (size_t)(end - start);
        uint32_t parsed;

        if (count >= MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT ||
            !win32_carla_parse_decimal(
                start, length, UINT16_MAX, &parsed
            ) || parsed >= patch_count) {
            return false;
        }
        indexes[count++] = (uint16_t)parsed;
        if (end == NULL) {
            return count == MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT;
        }
        start = end + 1;
        if (*start == '\0') {
            return false;
        }
    }
}

static bool win32_carla_parse_active_layers(
    const char *csv,
    uint8_t active_layers[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    size_t *active_layer_count
)
{
    bool seen[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {false};
    const char *start = csv;

    if (active_layers == NULL || active_layer_count == NULL) {
        return false;
    }
    *active_layer_count = 0U;
    while (true) {
        const char *end = strchr(start, ',');
        size_t length = end == NULL ? strlen(start) : (size_t)(end - start);
        uint32_t parsed;
        size_t layer_index;

        if (*active_layer_count >=
                MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT ||
            !win32_carla_parse_decimal(start, length,
                MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT, &parsed) ||
            parsed == 0U) {
            return false;
        }
        layer_index = (size_t)parsed - 1U;
        if (seen[layer_index]) {
            return false;
        }
        seen[layer_index] = true;
        active_layers[(*active_layer_count)++] =
            (uint8_t)parsed;
        if (end == NULL) {
            return *active_layer_count > 0U;
        }
        start = end + 1;
        if (*start == '\0') {
            return false;
        }
    }
}

static bool win32_carla_parse_pad_profile(
    const char *value,
    uint16_t patch_count,
    music_rig_win32_carla_profile_map *profile
)
{
    const char *first_separator;
    const char *second_separator;
    size_t code_length;
    size_t indexes_length;
    size_t layers_length;
    char indexes_csv[WIN32_CARLA_INDEX_CSV_CAPACITY];
    char layers_csv[WIN32_CARLA_LAYER_CSV_CAPACITY];
    uint32_t code;

    if (value == NULL || profile == NULL) {
        return false;
    }
    first_separator = strchr(value, ':');
    if (first_separator == NULL) {
        return false;
    }
    second_separator = strchr(first_separator + 1, ':');
    if (second_separator == NULL || strchr(second_separator + 1, ':') != NULL) {
        return false;
    }
    code_length = (size_t)(first_separator - value);
    indexes_length = (size_t)(second_separator - first_separator - 1);
    layers_length = strlen(second_separator + 1);
    if (!win32_carla_parse_decimal(value, code_length, 16U, &code) ||
        code == 0U || code == 2U ||
        indexes_length == 0U || indexes_length >= sizeof(indexes_csv) ||
        layers_length == 0U || layers_length >= sizeof(layers_csv)) {
        return false;
    }
    memcpy(indexes_csv, first_separator + 1, indexes_length);
    indexes_csv[indexes_length] = '\0';
    memcpy(layers_csv, second_separator + 1, layers_length + 1U);
    profile->management_pad_code = (uint8_t)code;
    return win32_carla_parse_index_csv(
            indexes_csv, patch_count, profile->patch_indexes
        ) && win32_carla_parse_active_layers(
            layers_csv, profile->active_layers, &profile->active_layer_count
        );
}

static bool win32_carla_utf8_interface_path(
    const char *utf8,
    wchar_t output[MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY]
)
{
    int required;

    if (!win32_carla_nonempty_text(
            utf8, MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY * 4U
        )) {
        return false;
    }
    required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0
    );
    if (required <= 1 ||
        (size_t)required > MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY) {
        return false;
    }
    return MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8,
        -1,
        output,
        (int)MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY
    ) == required;
}

static bool win32_carla_validate_arguments(
    win32_carla_arguments *arguments
)
{
    const char *const *values = arguments->values;
    music_rig_arturia_shared_sf2_plan plan;
    size_t index;
    uint32_t master_volume;

    if (!win32_carla_nonempty_text(values[WIN32_CARLA_OPTION_DLL],
            WIN32_CARLA_VALUE_CAPACITY) ||
        !win32_carla_nonempty_text(values[WIN32_CARLA_OPTION_PROJECT],
            WIN32_CARLA_VALUE_CAPACITY) ||
        !win32_carla_nonempty_text(values[WIN32_CARLA_OPTION_DRIVER],
            WIN32_CARLA_VALUE_CAPACITY) ||
        !win32_carla_nonempty_text(values[WIN32_CARLA_OPTION_CLIENT],
            WIN32_CARLA_VALUE_CAPACITY) ||
        !win32_carla_nonempty_text(values[WIN32_CARLA_OPTION_MIDI_CLIENT],
            WIN32_CARLA_PATCH_NAME_CAPACITY) ||
        !win32_carla_nonempty_text(values[WIN32_CARLA_OPTION_MIDI_PORT],
            WIN32_CARLA_PATCH_NAME_CAPACITY) ||
        !win32_carla_parse_patch_names(
            values[WIN32_CARLA_OPTION_PATCH_NAMES], arguments
        ) ||
        arguments->patch_count > UINT16_MAX ||
        !win32_carla_parse_index_csv(
            values[WIN32_CARLA_OPTION_PATCH_INDEXES],
            (uint16_t)arguments->patch_count,
            arguments->patch_indexes
        ) ||
        !win32_carla_parse_active_layers(
            values[WIN32_CARLA_OPTION_ACTIVE_LAYERS],
            arguments->active_layers, &arguments->active_layer_count
        ) ||
        !win32_carla_parse_decimal(
            values[WIN32_CARLA_OPTION_MASTER_VOLUME],
            strlen(values[WIN32_CARLA_OPTION_MASTER_VOLUME]),
            127U,
            &master_volume
        )) {
        return false;
    }
    if (!win32_carla_utf8_interface_path(
            values[WIN32_CARLA_OPTION_INPUT_INTERFACE],
            arguments->endpoint.input_interface
        ) ||
        !win32_carla_utf8_interface_path(
            values[WIN32_CARLA_OPTION_LOOPBACK_OUTPUT_INTERFACE],
            arguments->endpoint.output_interface
        ) ||
        wcscmp(arguments->endpoint.input_interface,
            arguments->endpoint.output_interface) == 0) {
        return false;
    }
    memcpy(arguments->endpoint.slot, "arturia-main", sizeof("arturia-main"));
    arguments->master_volume = (uint8_t)master_volume;
    if (music_rig_arturia_shared_sf2_plan_build(
            arguments->patch_indexes,
            (uint16_t)arguments->patch_count,
            arguments->active_layers,
            arguments->active_layer_count,
            &plan
        ) != MUSIC_RIG_RESULT_OK) {
        return false;
    }
    for (index = 0U; index < plan.route_count; ++index) {
        if (plan.routes[index].fader_cc > 127U ||
            plan.routes[index].knob_cc > 127U) {
            return false;
        }
    }
    for (index = 0U; index < arguments->pad_profile_count; ++index) {
        size_t previous;

        if (!win32_carla_parse_pad_profile(
                arguments->pad_profile_values[index],
                (uint16_t)arguments->patch_count,
                &arguments->pad_profiles[index]
            )) {
            return false;
        }
        for (previous = 0U; previous < index; ++previous) {
            if (arguments->pad_profiles[previous].management_pad_code ==
                    arguments->pad_profiles[index].management_pad_code) {
                return false;
            }
        }
    }
    return true;
}

static BOOL WINAPI win32_carla_console_control(DWORD control_type)
{
    if (control_type == CTRL_C_EVENT || control_type == CTRL_BREAK_EVENT ||
        control_type == CTRL_CLOSE_EVENT || control_type == CTRL_LOGOFF_EVENT ||
        control_type == CTRL_SHUTDOWN_EVENT) {
        (void)InterlockedExchange(&win32_carla_stop_requested, 1L);
        return TRUE;
    }
    return FALSE;
}

static music_rig_result win32_carla_close_runtime(
    music_rig_win32_carla_runtime **runtime
)
{
    music_rig_result first_failure = MUSIC_RIG_RESULT_OK;

    while (*runtime != NULL) {
        music_rig_result result = music_rig_win32_carla_runtime_close(runtime);

        if (first_failure == MUSIC_RIG_RESULT_OK &&
            result != MUSIC_RIG_RESULT_OK) {
            first_failure = result;
        }
        if (*runtime != NULL) {
            Sleep(100U);
        }
    }
    return first_failure;
}

static int run_win32_carla(int argc, char **argv)
{
    win32_carla_arguments arguments;
    music_rig_winmm_midi_win32 native;
    music_rig_win32_carla_runtime *runtime = NULL;
    music_rig_win32_carla_host_options carla_options;
    music_rig_result result;
    music_rig_result close_result;
    char error_message[512] = {0};
    bool native_initialized = false;
    bool control_handler_installed = false;

    if (argc == 3 && strcmp(argv[2], "--help") == 0) {
        print_win32_carla_help(stdout, argv[0]);
        return MUSIC_RIG_RESULT_OK;
    }
    if (!win32_carla_parse_arguments(argc, argv, &arguments) ||
        !win32_carla_validate_arguments(&arguments)) {
        fprintf(stderr, "invalid run-win32-carla options or values\n");
        print_win32_carla_help(stderr, argv[0]);
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    result = music_rig_winmm_midi_win32_init(&native);
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "WinMM native initialization failed: result %d\n",
            (int)result);
        return (int)result;
    }
    native_initialized = true;

    memset(&carla_options, 0, sizeof(carla_options));
    carla_options.carla_dll_path =
        arguments.values[WIN32_CARLA_OPTION_DLL];
    carla_options.project_path = arguments.values[WIN32_CARLA_OPTION_PROJECT];
    carla_options.driver_name = arguments.values[WIN32_CARLA_OPTION_DRIVER];
    carla_options.client_name = arguments.values[WIN32_CARLA_OPTION_CLIENT];
    carla_options.midi_client_name =
        arguments.values[WIN32_CARLA_OPTION_MIDI_CLIENT];
    carla_options.midi_port_name =
        arguments.values[WIN32_CARLA_OPTION_MIDI_PORT];
    carla_options.patch_names = arguments.patch_names;
    carla_options.patch_count = (uint16_t)arguments.patch_count;
    carla_options.patch_indexes = arguments.patch_indexes;
    carla_options.catalog_patch_count = (uint16_t)arguments.patch_count;
    carla_options.active_layers = arguments.active_layers;
    carla_options.active_layer_count = arguments.active_layer_count;
    carla_options.initial_master_volume = arguments.master_volume;

    result = music_rig_win32_carla_runtime_open(
        &runtime,
        music_rig_winmm_midi_win32_ops(),
        &native,
        &arguments.endpoint,
        1U,
        arguments.pad_profiles,
        arguments.pad_profile_count,
        &carla_options,
        "arturia-main",
        error_message,
        sizeof(error_message)
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "Win32 Carla runtime open failed: result %d%s%s\n",
            (int)result,
            error_message[0] == '\0' ? "" : ": ",
            error_message);
        goto shutdown;
    }

    (void)InterlockedExchange(&win32_carla_stop_requested, 0L);
    if (!SetConsoleCtrlHandler(win32_carla_console_control, TRUE)) {
        fprintf(stderr, "Could not install the Windows console-control handler\n");
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        goto shutdown;
    }
    control_handler_installed = true;
    puts("run-win32-carla ready; press Ctrl+C to stop");
    (void)fflush(stdout);
    while (InterlockedCompareExchange(
            &win32_carla_stop_requested, 0L, 0L
        ) == 0L) {
        result = music_rig_win32_carla_runtime_pump(runtime, NULL);
        if (result != MUSIC_RIG_RESULT_OK) {
            fprintf(stderr, "Win32 Carla runtime pump failed: result %d\n",
                (int)result);
            break;
        }
        Sleep(10U);
    }

shutdown:
    if (control_handler_installed) {
        (void)SetConsoleCtrlHandler(win32_carla_console_control, FALSE);
    }
    close_result = win32_carla_close_runtime(&runtime);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = close_result;
    }
    if (native_initialized) {
        music_rig_result dispose_result =
            music_rig_winmm_midi_win32_dispose(&native);

        if (result == MUSIC_RIG_RESULT_OK) {
            result = dispose_result;
        }
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "Win32 Carla runtime stopped with result %d\n",
            (int)result);
    }
    return (int)result;
}
#endif

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
    music_rig_storage_adapter storage = {0};
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
        if (result != MUSIC_RIG_RESULT_OK) {
            fprintf(stderr, "prepared definition load failed: result %d\n",
                (int)result);
        }
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

#if defined(MUSIC_RIG_HAS_JACK_MIDI_OUTPUT)
static void configure_current_behaviors(
    music_rig_device_midi_shadow_config *config
);

typedef struct music_rig_output_control_context {
    music_rig_linux_control_server *server;
    music_rig_jack_midi_output *host;
    music_rig_output_adoption_adapter *adoption;
    music_rig_runtime *runtime;
} music_rig_output_control_context;

static music_rig_result reconnect_output_backend(
    music_rig_output_control_context *context
)
{
    music_rig_result result;

    if (!atomic_load_explicit(
            &context->host->backend_shutdown, memory_order_acquire
        )) {
        return MUSIC_RIG_RESULT_OK;
    }
    result = music_rig_jack_midi_output_reconnect(context->host);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = context->adoption->prepare(
            context->adoption->context, context->runtime->control_generation
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = context->adoption->confirm(
            context->adoption->context, context->runtime->control_generation
        );
    }
    return result;
}

static music_rig_result output_control_start(void *opaque)
{
    music_rig_output_control_context *context = opaque;

    return music_rig_linux_control_server_start(context->server);
}

static music_rig_control_poll output_control_poll(
    void *opaque,
    music_rig_protocol_request *request
)
{
    music_rig_output_control_context *context = opaque;

    if (reconnect_output_backend(context) != MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_CONTROL_ERROR;
    }
    return music_rig_linux_control_server_poll(context->server, request);
}

static music_rig_result output_control_wait(void *opaque)
{
    music_rig_output_control_context *context = opaque;

    return music_rig_linux_control_server_wait(context->server);
}

static music_rig_result output_control_respond(
    void *opaque,
    const music_rig_protocol_response *response
)
{
    music_rig_output_control_context *context = opaque;

    return music_rig_linux_control_server_respond(context->server, response);
}

static music_rig_result output_control_stop(void *opaque)
{
    music_rig_output_control_context *context = opaque;

    return music_rig_linux_control_server_stop(context->server);
}

static int run_output_runtime(
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
    music_rig_runtime_config runtime_config;
    music_rig_platform_interfaces interfaces;
    music_rig_file_storage storage_file = {0};
    music_rig_storage_adapter storage = {0};
    music_rig_linux_control_server server;
    music_rig_host_paths paths;
    music_rig_jack_midi_output output_host;
    music_rig_device_midi_shadow shadow;
    music_rig_device_midi_shadow_config shadow_config;
    music_rig_device_midi_shadow_observer observer = {0};
    music_rig_output_adoption_adapter output_adapter;
    music_rig_output_control_context control_context;
    music_rig_result result;
    music_rig_result stop_result = MUSIC_RIG_RESULT_OK;
    bool host_started = false;

    memset(&server, 0, sizeof(server));
    server.listener = -1;
    server.client = -1;
    memset(&control_context, 0, sizeof(control_context));
    control_context.server = &server;
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
            &storage_file, definition_path, paths.active_state_file, &storage
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
    interfaces.control.context = &control_context;
    interfaces.control.start = output_control_start;
    interfaces.control.poll = output_control_poll;
    interfaces.control.wait = output_control_wait;
    interfaces.control.respond = output_control_respond;
    interfaces.control.stop = output_control_stop;
    interfaces.storage = storage;
    memset(&runtime_config, 0, sizeof(runtime_config));
    runtime_config.initial_generation = &generation;
    runtime_config.definition_fingerprint = definition.fingerprint;
    runtime_config.definition_fingerprint_size = sizeof(definition.fingerprint);
    runtime_config.active_rig_profile = definition.active_rig_profile;
    runtime_config.prepared_definitions = prepared_path == NULL ? NULL : &prepared;
    runtime_config.prepared_definition_count = prepared_path == NULL ? 0U : 1U;
    runtime_config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    if (result == MUSIC_RIG_RESULT_OK) {
        struct sigaction action;

        memset(&action, 0, sizeof(action));
        action.sa_handler = music_rig_linux_control_server_request_stop;
        sigemptyset(&action.sa_mask);
        (void)sigaction(SIGINT, &action, NULL);
        (void)sigaction(SIGTERM, &action, NULL);
        result = music_rig_runtime_init(&runtime, &runtime_config, &interfaces);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_init(
            &output_host, &runtime.generations,
            runtime.control_generation->mapping
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_observer_init(
            &output_host, &observer
        );
    }
    music_rig_device_midi_shadow_config_init(&shadow_config);
    shadow_config.generations = &runtime.generations;
    shadow_config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    shadow_config.observer = observer;
    if (result == MUSIC_RIG_RESULT_OK) {
        configure_current_behaviors(&shadow_config);
        result = music_rig_device_midi_shadow_init(&shadow, &shadow_config);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_attach_shadow(
            &output_host, &shadow
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_start(&output_host);
        host_started = result == MUSIC_RIG_RESULT_OK;
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        output_adapter = music_rig_jack_midi_output_adapter(&output_host);
        control_context.host = &output_host;
        control_context.adoption = &output_adapter;
        control_context.runtime = &runtime;
        result = music_rig_runtime_enable_output(&runtime, &output_adapter);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        puts("output-runtime ready");
        fflush(stdout);
        result = music_rig_runtime_run(&runtime);
    }
    if (host_started) {
        stop_result = music_rig_jack_midi_output_stop(&output_host);
        host_started = false;
        if (result == MUSIC_RIG_RESULT_OK) {
            result = stop_result;
        }
    }
    if (server.listener >= 0) {
        (void)music_rig_linux_control_server_stop(&server);
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "output runtime failed: result %d\n", (int)result);
        return (int)result;
    }
    printf("output-mode enabled\n");
    printf("output-events %" PRIu64 "\n", output_host.metrics.output_events);
    printf("backend-reconnects %" PRIu64 "\n", output_host.metrics.reconnects);
    return MUSIC_RIG_RESULT_OK;
}
#endif
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

typedef struct music_rig_output_lifecycle_context {
    music_rig_jack_midi_output *host;
    music_rig_output_adoption_adapter *adoption;
    music_rig_generation *generation;
} music_rig_output_lifecycle_context;

static music_rig_result poll_output_backend(void *opaque)
{
    music_rig_output_lifecycle_context *context = opaque;
    music_rig_result result;

    if (!atomic_load_explicit(
            &context->host->backend_shutdown, memory_order_acquire
        )) {
        return MUSIC_RIG_RESULT_OK;
    }
    result = music_rig_jack_midi_output_reconnect(context->host);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = context->adoption->prepare(
            context->adoption->context, context->generation
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = context->adoption->confirm(
            context->adoption->context, context->generation
        );
    }
    return result;
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

static int run_midi_output(const char *path, const char *fingerprint)
{
    music_rig_compiled_definition definition = {0};
    music_rig_generation generation = {0};
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow_config config;
    music_rig_device_midi_shadow shadow;
    music_rig_jack_midi_output host;
    music_rig_device_midi_shadow_observer observer = {0};
    music_rig_output_adoption_adapter adoption;
    music_rig_journal_diagnostics journal;
    music_rig_diagnostic_sink sink;
    music_rig_result result;
    music_rig_result stop_result = MUSIC_RIG_RESULT_OK;
    bool host_started = false;
    music_rig_output_lifecycle_context lifecycle_context;

    result = load_definition(path, fingerprint, &definition, &generation);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_generation_slot_init(&generations, &generation);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_init(
            &host, &generations, &definition_tables
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_observer_init(&host, &observer);
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = &generations;
    config.observer = observer;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    if (result == MUSIC_RIG_RESULT_OK) {
        configure_current_behaviors(&config);
        result = music_rig_device_midi_shadow_init(&shadow, &config);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_attach_shadow(&host, &shadow);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_journal_diagnostics_init(
            &journal, STDERR_FILENO, &sink
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_midi_output_start(&host);
        host_started = result == MUSIC_RIG_RESULT_OK;
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        adoption = music_rig_jack_midi_output_adapter(&host);
        lifecycle_context.host = &host;
        lifecycle_context.adoption = &adoption;
        lifecycle_context.generation = &generation;
        if (atomic_load_explicit(
                &host.backend_shutdown, memory_order_acquire
            )) {
            result = poll_output_backend(&lifecycle_context);
        }
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = adoption.prepare(adoption.context, &generation);
        if (result == MUSIC_RIG_RESULT_OK) {
            result = adoption.confirm(adoption.context, &generation);
        }
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_linux_shadow_lifecycle_run_with_poll(
            &sink, poll_output_backend, &lifecycle_context
        );
        stop_result = music_rig_jack_midi_output_stop(&host);
        host_started = false;
        if (result == MUSIC_RIG_RESULT_OK) {
            result = stop_result;
        }
        if (result == MUSIC_RIG_RESULT_OK &&
            atomic_load_explicit(
                &host.last_process_result, memory_order_acquire
            ) != MUSIC_RIG_RESULT_OK) {
            result = atomic_load_explicit(
                &host.last_process_result, memory_order_relaxed
            );
        }
    }
    if (result != MUSIC_RIG_RESULT_OK && host_started) {
        (void)music_rig_jack_midi_output_stop(&host);
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "MIDI output failed: result %d\n", (int)result);
        return (int)result;
    }
    printf("definition-generation %" PRIu64 "\n", generation.id);
    printf("input-output-port-pairs %zu\n", host.port_count);
    printf("output-events %" PRIu64 "\n", host.metrics.output_events);
    printf("output-reserve-failures %" PRIu64 "\n",
        host.metrics.output_reserve_failures);
    printf("backend-reconnects %" PRIu64 "\n", host.metrics.reconnects);
    puts("output-mode enabled");
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
    bool relay_initialized = false;
    size_t index;

    result = load_definition(path, fingerprint, &definition, &generation);
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_generation_slot_init(&generations, &generation);
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_jack_smc_mixer_relay_init(&host, &generations);
        relay_initialized = result == MUSIC_RIG_RESULT_OK;
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
        if (relay_initialized) {
            metrics = music_rig_smc_mixer_relay_metrics_read(&host.relay);
            fprintf(
                stderr,
                "SMC-Mixer relay diagnostics: result %d stop-result %d "
                "last-process-result %d input-events %" PRIu64
                " emitted-events %" PRIu64 " coalesced-events %" PRIu64
                " adapter-failures %" PRIu64 "\n",
                (int)result,
                (int)stop_result,
                (int)atomic_load_explicit(
                    &host.last_process_result,
                    memory_order_acquire
                ),
                metrics->input_events,
                metrics->emitted_events,
                metrics->coalesced_events,
                metrics->adapter_failures
            );
        }
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
    printf("coalesced-events %" PRIu64 "\n", metrics->coalesced_events);
    printf("unmapped-events %" PRIu64 "\n", metrics->unmapped_events);
    printf("malformed-events %" PRIu64 "\n", metrics->malformed_events);
    printf("adapter-failures %" PRIu64 "\n", metrics->adapter_failures);
    printf("trace-start-epoch %" PRIu64 "\n", host.trace_start_epoch);
    printf("trace-sample-rate %" PRIu32 "\n", host.trace_sample_rate);
    if (host.trace_sample_rate != UINT32_C(0)) {
        const uint64_t trace_capacity =
            (uint64_t)MUSIC_RIG_JACK_SMC_MIXER_RELAY_TRACE_SECOND_CAP;
        const uint64_t first_second = host.trace_last_second >= trace_capacity
            ? host.trace_last_second - trace_capacity + UINT64_C(1)
            : UINT64_C(0);
        uint64_t second;

        for (second = first_second;
             second <= host.trace_last_second;
             ++second) {
            const music_rig_jack_smc_mixer_relay_trace *trace =
                &host.trace[second % trace_capacity];

            if (trace->second != second) {
                continue;
            }
            printf(
                "trace-second %" PRIu64 " input-events %" PRIu64
                " mapped-events %" PRIu64 " emitted-events %" PRIu64
                " coalesced-events %" PRIu64 " unmapped-events %" PRIu64
                " malformed-events %" PRIu64 " adapter-failures %" PRIu64
                "\n",
                trace->second,
                trace->input_events,
                trace->mapped_events,
                trace->emitted_events,
                trace->coalesced_events,
                trace->unmapped_events,
                trace->malformed_events,
                trace->adapter_failures
            );
        }
    }
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

#if defined(MUSIC_RIG_HAS_WIN32_CARLA_RUNTIME)
    if (argc >= 2 && strcmp(argv[1], "run-win32-carla") == 0) {
        return run_win32_carla(argc, argv);
    }
#endif

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
    if (argc == 8 && strcmp(argv[1], "run-midi-output") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0 &&
        strcmp(argv[6], "--output-enabled") == 0 &&
        strcmp(argv[7], "--acknowledge-output") == 0) {
        return run_midi_output(argv[3], argv[5]);
    }
#endif
#if defined(MUSIC_RIG_HAS_JACK_MIDI_OUTPUT)
    if (argc == 7 && strcmp(argv[1], "run-output") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0 &&
        strcmp(argv[6], "--acknowledge-output") == 0) {
        return run_output_runtime(argv[3], argv[5], NULL, NULL);
    }
    if (argc == 11 && strcmp(argv[1], "run-output") == 0 &&
        strcmp(argv[2], "--definition") == 0 &&
        strcmp(argv[4], "--expected-fingerprint") == 0 &&
        strcmp(argv[6], "--prepared-definition") == 0 &&
        strcmp(argv[8], "--prepared-fingerprint") == 0 &&
        strcmp(argv[10], "--acknowledge-output") == 0) {
        return run_output_runtime(argv[3], argv[5], argv[7], argv[9]);
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
