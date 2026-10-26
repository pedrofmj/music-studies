#include "music_rig/win32_carla_host.h"
#include "music_rig/arturia_session.h"
#include "win32-carla-fake.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, description) do { \
    if (!(condition)) { \
        fprintf(stderr, "Win32 Carla offline test failed at line %d: %s\n", \
            __LINE__, description); \
        return 1; \
    } \
} while (0)

typedef struct fake_raw_midi_output {
    uint8_t messages[
        MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY + 16U
    ][3];
    size_t count;
} fake_raw_midi_output;

static music_rig_result capture_raw_midi(
    void *context, const uint8_t *message, size_t message_size
)
{
    fake_raw_midi_output *output = context;

    if (output == NULL || message == NULL || message_size != 3U ||
        output->count >= sizeof(output->messages) / sizeof(output->messages[0])) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    memcpy(output->messages[output->count], message, message_size);
    output->count += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static bool load_dll_path(const char *argument, wchar_t *wide_path,
    size_t wide_capacity, char *utf8_path, size_t utf8_capacity)
{
    int wide_length;
    int utf8_length;

    if (argument == NULL || wide_path == NULL || utf8_path == NULL ||
        wide_capacity > (size_t)INT_MAX || utf8_capacity > (size_t)INT_MAX) {
        return false;
    }
    wide_length = MultiByteToWideChar(CP_ACP, 0, argument, -1, wide_path,
        (int)wide_capacity);
    if (wide_length <= 0) {
        return false;
    }
    utf8_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_path,
        -1, utf8_path, (int)utf8_capacity, NULL, NULL);
    return utf8_length > 0;
}

static bool query_fake(
    music_rig_win32_carla_fake_query_fn query,
    music_rig_win32_carla_fake_status *status
)
{
    memset(status, 0, sizeof(*status));
    return query(status, sizeof(*status)) != 0 &&
        status->struct_size == sizeof(*status);
}

static bool startup_is_complete(
    const music_rig_win32_carla_fake_status *status
)
{
    const uint32_t expected_lifecycle =
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_HOST_INIT |
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_CALLBACK |
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_OPTION |
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_INIT |
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_PROJECT |
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_EXTERNAL_REFRESH |
        MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_INTERNAL_REFRESH;
    uint32_t index;

    if (status->lifecycle != expected_lifecycle || status->faults != 0U ||
        status->wrong_thread_api_calls != 0U ||
        status->standalone_init_calls != 1U ||
        status->engine_option_calls != 1U || status->engine_init_calls != 1U ||
        status->project_load_calls != 1U ||
        status->external_refresh_calls != 1U ||
        status->internal_refresh_calls != 1U || status->last_error_calls != 0U ||
        status->connection_calls != MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT ||
        status->connection_mask != UINT32_C(0x0f) ||
        status->client_callbacks != MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT + 1U ||
        status->port_callbacks != MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT + 1U ||
        status->deactivated_mask != UINT32_C(0x0f) ||
        status->active_mask != UINT32_C(0x01) || status->activation_calls != 1U ||
        status->deactivation_calls != MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT ||
        status->plugin_info_calls == 0U || status->parameter_info_calls == 0U ||
        status->parameter_range_calls !=
            2U * MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT) {
        return false;
    }
    for (index = 0U; index < MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT; ++index) {
        if (status->option_masks[index] !=
                MUSIC_RIG_WIN32_CARLA_FAKE_REQUIRED_OPTIONS) {
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    static const char *const patch_names[
        MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT
    ] = {"patch-0", "patch-1", "patch-2", "patch-3"};
    static const uint16_t patch_indexes[
        MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT
    ] = {0U, 1U, 2U, 3U, 0U, 1U, 2U, 3U, 0U};
    static const uint8_t initial_layers[] = {1U};
    static const uint8_t next_layers[] = {2U, 4U};
    const uint8_t fader_cc[] = {UINT8_C(0xb0), UINT8_C(75), UINT8_C(99)};
    const uint8_t reverb_cc[] = {UINT8_C(0xb0), UINT8_C(71), UINT8_C(84)};
    const uint8_t controller[] = {UINT8_C(0xb0), UINT8_C(1), UINT8_C(55)};
    const uint8_t sustain_controller[] = {
        UINT8_C(0xb0), UINT8_C(64), UINT8_C(127)
    };
    const uint8_t master_volume[] = {
        UINT8_C(0xb0), MUSIC_RIG_ARTURIA_MASTER_VOLUME_CC, UINT8_C(65)
    };
    const uint8_t master_mute[] = {
        UINT8_C(0xb0), MUSIC_RIG_ARTURIA_MASTER_MUTE_CC, UINT8_C(127)
    };
    const uint8_t pitch_bend[] = {UINT8_C(0xe0), UINT8_C(0), UINT8_C(64)};
    const uint8_t note_on[] = {UINT8_C(0x90), UINT8_C(60), UINT8_C(100)};
    const uint8_t note_off[] = {UINT8_C(0x80), UINT8_C(60), UINT8_C(0)};
    music_rig_win32_carla_host_options options;
    music_rig_win32_carla_host *host = NULL;
    music_rig_win32_carla_fake_status status;
    fake_raw_midi_output raw_output = {0};
    music_rig_win32_carla_fake_query_fn query = NULL;
    HMODULE fake_library;
    FARPROC query_symbol;
    wchar_t wide_dll_path[32768];
    char utf8_dll_path[32768];
    char error_message[512];
    music_rig_result result;

    CHECK(argc == 2, "expected the fake Carla DLL path argument");
    CHECK(load_dll_path(argv[1], wide_dll_path,
            sizeof(wide_dll_path) / sizeof(wide_dll_path[0]), utf8_dll_path,
            sizeof(utf8_dll_path)), "could not convert fake Carla DLL path");
    CHECK(music_rig_win32_carla_host_probe_library(
            wide_dll_path, error_message, sizeof(error_message)
        ) == MUSIC_RIG_RESULT_OK,
        "fake Carla DLL export probe failed");
    fake_library = LoadLibraryW(wide_dll_path);
    CHECK(fake_library != NULL, "could not pin the fake Carla DLL instance");
    query_symbol = GetProcAddress(fake_library,
        "music_rig_win32_carla_fake_get_status");
    CHECK(query_symbol != NULL, "fake Carla status query export is missing");
    _Static_assert(sizeof(query_symbol) == sizeof(query),
        "fake status function pointer must match FARPROC size");
    memcpy(&query, &query_symbol, sizeof(query));

    memset(&options, 0, sizeof(options));
    options.carla_dll_path = utf8_dll_path;
    options.project_path = "offline-project.carxp";
    options.driver_name = "Offline";
    options.client_name = "music-rig-offline";
    options.midi_client_name = "MIDI Services Loopback";
    options.midi_port_name = "MIDI In";
    options.patch_names = patch_names;
    options.patch_count = MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT;
    options.patch_indexes = patch_indexes;
    options.catalog_patch_count = MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT;
    options.active_layers = initial_layers;
    options.active_layer_count = sizeof(initial_layers);
    options.initial_master_volume = 100U;

    CHECK(music_rig_win32_carla_host_open(&host, &options, error_message,
            sizeof(error_message)) == MUSIC_RIG_RESULT_INVALID_ARGUMENT &&
        host == NULL,
        "open accepted missing raw-MIDI loopback output callback");
    options.raw_midi_output = capture_raw_midi;
    options.raw_midi_output_context = &raw_output;
    result = music_rig_win32_carla_host_open(&host, &options, error_message,
        sizeof(error_message));
    if (result != MUSIC_RIG_RESULT_OK) {
        fprintf(stderr, "Win32 Carla host open failed (%d): %s\n", (int)result,
            host == NULL ? error_message :
                music_rig_win32_carla_host_last_error(host));
        return 1;
    }
    CHECK(host != NULL, "adapter open succeeded without returning a host");
    CHECK(query_fake(query, &status), "fake status query failed after open");
    CHECK(startup_is_complete(&status),
        "startup order, initial activation, options, or MIDI routes were wrong");
    CHECK(status.idle_calls >= 1U,
        "adapter did not idle while enumerating the project");

    CHECK(music_rig_win32_carla_host_idle(host) == MUSIC_RIG_RESULT_OK,
        "public idle operation failed");
    CHECK(music_rig_win32_carla_host_select_profile(host, patch_indexes,
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT, next_layers,
            sizeof(next_layers)) == MUSIC_RIG_RESULT_OK,
        "public profile selection failed");
    CHECK(query_fake(query, &status), "fake status query failed after profile selection");
    CHECK(status.faults == 0U && status.active_mask == UINT32_C(0x0a) &&
        status.activation_calls == 3U && status.deactivation_calls == 5U,
        "profile selection did not switch the expected SF2 instances");

    CHECK(music_rig_win32_carla_host_process_midi(host, fader_cc,
            sizeof(fader_cc)) == MUSIC_RIG_RESULT_OK,
        "fader CC operation failed");
    CHECK(music_rig_win32_carla_host_process_midi(host, reverb_cc,
            sizeof(reverb_cc)) == MUSIC_RIG_RESULT_OK,
        "reverb CC operation failed");
    CHECK(music_rig_win32_carla_host_process_midi(host, master_volume,
            sizeof(master_volume)) == MUSIC_RIG_RESULT_OK &&
        music_rig_win32_carla_host_process_midi(host, master_mute,
            sizeof(master_mute)) == MUSIC_RIG_RESULT_OK &&
        raw_output.count == 0U,
        "master controls were not kept on the direct Carla path");
    CHECK(music_rig_win32_carla_host_process_midi(host, controller,
            sizeof(controller)) == MUSIC_RIG_RESULT_OK,
        "raw controller routing operation failed");
    CHECK(music_rig_win32_carla_host_process_midi(host, sustain_controller,
            sizeof(sustain_controller)) == MUSIC_RIG_RESULT_OK,
        "sustain controller routing operation failed");
    CHECK(music_rig_win32_carla_host_process_midi(host, pitch_bend,
            sizeof(pitch_bend)) == MUSIC_RIG_RESULT_OK,
        "raw pitch-bend routing operation failed");
    CHECK(music_rig_win32_carla_host_process_midi(host, note_on,
            sizeof(note_on)) == MUSIC_RIG_RESULT_OK,
        "note-on operation failed");
    CHECK(music_rig_win32_carla_host_process_midi(host, note_off,
            sizeof(note_off)) == MUSIC_RIG_RESULT_OK,
        "note-off operation failed");
    CHECK(query_fake(query, &status), "fake status query failed after MIDI operations");
    CHECK(status.faults == 0U && status.note_calls == 0U &&
        status.note_on_calls == 0U && status.note_off_calls == 0U &&
        status.gain_calls == 20U && status.reverb_calls == 4U &&
        status.active_mask == UINT32_C(0x0a),
        "MIDI operations caused a fake Carla note, gain, or reverb fault");
    CHECK(raw_output.count == 5U &&
        memcmp(raw_output.messages[0], controller, sizeof(controller)) == 0 &&
        memcmp(raw_output.messages[1], sustain_controller,
            sizeof(sustain_controller)) == 0 &&
        memcmp(raw_output.messages[2], pitch_bend, sizeof(pitch_bend)) == 0 &&
        memcmp(raw_output.messages[3], note_on, sizeof(note_on)) == 0 &&
        memcmp(raw_output.messages[4], note_off, sizeof(note_off)) == 0,
        "session-approved MIDI was not FIFO on the loopback");
    CHECK(status.idle_calls >= 2U,
        "public idle operation did not reach the fake Carla engine");

    {
        const uint8_t other_channel_cc[] = {
            UINT8_C(0xb1), UINT8_C(1), UINT8_C(90)
        };
        const uint8_t unsupported_status[] = {
            UINT8_C(0xa0), UINT8_C(60), UINT8_C(90)
        };
        const uint8_t queued_controller[] = {
            UINT8_C(0xb0), UINT8_C(1), UINT8_C(77)
        };
        const uint8_t queued_pitch[] = {
            UINT8_C(0xe0), UINT8_C(12), UINT8_C(34)
        };
        const uint8_t queued_note_on[] = {
            UINT8_C(0x90), UINT8_C(62), UINT8_C(80)
        };
        const uint8_t queued_note_off[] = {
            UINT8_C(0x80), UINT8_C(62), UINT8_C(0)
        };
        size_t drained = 0U;
        uint32_t notes_before = status.note_calls;
        size_t raw_before = raw_output.count;
        size_t index;

        CHECK(music_rig_win32_carla_host_select_profile(host, patch_indexes,
                MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT, initial_layers,
                sizeof(initial_layers)) == MUSIC_RIG_RESULT_OK,
            "could not prepare the queued profile-order check");
        CHECK(music_rig_win32_carla_host_enqueue_midi(
                host, other_channel_cc, sizeof(other_channel_cc)
            ) == MUSIC_RIG_RESULT_OK &&
            music_rig_win32_carla_host_enqueue_midi(
                host, unsupported_status, sizeof(unsupported_status)
            ) == MUSIC_RIG_RESULT_OK,
            "unsupported channel or status traffic was not safely ignored");
        CHECK(music_rig_win32_carla_host_enqueue_midi(
                host, queued_controller, sizeof(queued_controller)
            ) == MUSIC_RIG_RESULT_OK &&
            music_rig_win32_carla_host_enqueue_profile(
                host, patch_indexes, MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT,
                next_layers, sizeof(next_layers)
            ) == MUSIC_RIG_RESULT_OK &&
            music_rig_win32_carla_host_enqueue_midi(
                host, queued_pitch, sizeof(queued_pitch)
            ) == MUSIC_RIG_RESULT_OK &&
            music_rig_win32_carla_host_enqueue_midi(
                host, queued_note_on, sizeof(queued_note_on)
            ) == MUSIC_RIG_RESULT_OK &&
            music_rig_win32_carla_host_enqueue_midi(
                host, queued_note_off, sizeof(queued_note_off)
            ) == MUSIC_RIG_RESULT_OK,
            "supported channel-1 MIDI messages were not enqueued");
        CHECK(music_rig_win32_carla_host_drain_midi(host, &drained) ==
                MUSIC_RIG_RESULT_OK && drained == 5U,
            "Carla owner thread did not drain MIDI and profile events in FIFO order");
        CHECK(raw_output.count == raw_before + 4U &&
            memcmp(raw_output.messages[raw_before], queued_controller,
                sizeof(queued_controller)) == 0 &&
            memcmp(raw_output.messages[raw_before + 1U], queued_pitch,
                sizeof(queued_pitch)) == 0 &&
            memcmp(raw_output.messages[raw_before + 2U], queued_note_on,
                sizeof(queued_note_on)) == 0 &&
            memcmp(raw_output.messages[raw_before + 3U], queued_note_off,
                sizeof(queued_note_off)) == 0,
            "queued raw MIDI was not FIFO or was duplicated across routes");
        CHECK(query_fake(query, &status) &&
            status.note_calls == notes_before &&
            status.active_mask == UINT32_C(0x0a),
            "queued profile event was not applied in the MIDI FIFO drain");

        for (index = 0U;
             index < MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY; ++index) {
            CHECK(music_rig_win32_carla_host_enqueue_midi(
                    host, queued_note_on, sizeof(queued_note_on)
                ) == MUSIC_RIG_RESULT_OK,
                "available Carla MIDI queue slot was rejected");
        }
        CHECK(music_rig_win32_carla_host_enqueue_midi(
                host, queued_note_off, sizeof(queued_note_off)
            ) == MUSIC_RIG_RESULT_BUFFER_TOO_SMALL &&
            music_rig_win32_carla_host_midi_queue_overflows(host) == 1U,
            "full Carla MIDI queue did not report overflow");
        CHECK(music_rig_win32_carla_host_enqueue_profile(
                host, patch_indexes, MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT,
                next_layers, sizeof(next_layers)
            ) == MUSIC_RIG_RESULT_BUFFER_TOO_SMALL &&
            music_rig_win32_carla_host_midi_queue_overflows(host) == 2U,
            "full Carla FIFO did not count a profile-event overflow");
        CHECK(music_rig_win32_carla_host_drain_midi(host, &drained) ==
                MUSIC_RIG_RESULT_OK &&
            drained == MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY,
            "Carla MIDI queue overflow damaged or reordered queued messages");
    }

    CHECK(music_rig_win32_carla_host_close(&host) == MUSIC_RIG_RESULT_OK,
        "public close operation failed");
    CHECK(host == NULL, "public close did not clear the host pointer");
    CHECK(query_fake(query, &status), "fake status query failed after close");
    CHECK(status.faults == 0U && status.engine_close_calls == 1U &&
        status.wrong_thread_api_calls == 0U &&
        status.last_error_calls == 0U &&
        (status.lifecycle & MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_CLOSE) != 0U,
        "fake Carla reported a fault or an unclean engine close");
    CHECK(FreeLibrary(fake_library) != 0,
        "could not release the pinned fake Carla DLL instance");
    puts("Win32 Carla fake-DLL offline integration test: OK");
    return 0;
}
