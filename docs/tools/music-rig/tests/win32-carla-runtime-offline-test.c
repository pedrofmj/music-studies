#include "music_rig/win32_carla_runtime.h"
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
        fprintf(stderr, "Win32 Carla runtime test failed at line %d: %s\n", \
            __LINE__, description); \
        return 1; \
    } \
} while (0)

typedef enum fake_shutdown_event {
    FAKE_SHUTDOWN_INPUT_STOP = 1,
    FAKE_SHUTDOWN_INPUT_JOIN = 2,
    FAKE_SHUTDOWN_OUTPUT_JOIN = 3,
    FAKE_SHUTDOWN_INPUT_CLOSE = 4,
    FAKE_SHUTDOWN_OUTPUT_CLOSE = 5
} fake_shutdown_event;

typedef struct fake_worker {
    music_rig_winmm_midi_worker_fn entry;
    void *argument;
    HANDLE thread;
    HANDLE wake_event;
    HANDLE idle_event;
} fake_worker;

typedef struct fake_winmm {
    music_rig_win32_carla_fake_query_fn carla_query;
    music_rig_winmm_midi_input_callback_fn input_callback;
    void *input_callback_argument;
    fake_worker workers[2];
    HANDLE output_event;
    CRITICAL_SECTION lock;
    uint32_t sent_messages[16];
    size_t sent_message_count;
    fake_shutdown_event shutdown_events[8];
    size_t shutdown_event_count;
    bool defer_input_wake;
    bool fail_input_start;
    bool carla_was_open_at_output_close;
} fake_winmm;

typedef struct wrong_thread_call {
    music_rig_win32_carla_runtime *runtime;
    music_rig_result result;
} wrong_thread_call;

static void record_shutdown(fake_winmm *fake, fake_shutdown_event event)
{
    EnterCriticalSection(&fake->lock);
    if (fake->shutdown_event_count <
            sizeof(fake->shutdown_events) / sizeof(fake->shutdown_events[0])) {
        fake->shutdown_events[fake->shutdown_event_count++] = event;
    }
    LeaveCriticalSection(&fake->lock);
}

static DWORD WINAPI run_fake_worker(void *argument)
{
    fake_worker *worker = argument;

    worker->entry(worker->argument);
    return 0U;
}

static DWORD WINAPI call_runtime_from_wrong_thread(void *argument)
{
    wrong_thread_call *call = argument;

    call->result = music_rig_win32_carla_runtime_idle(call->runtime);
    return 0U;
}

static music_rig_result fake_device_count(
    void *context,
    music_rig_winmm_midi_direction direction,
    uint32_t *count
)
{
    (void)context;
    (void)direction;
    if (count == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *count = 1U;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_device_interface(
    void *context,
    music_rig_winmm_midi_direction direction,
    uint32_t transient_index,
    wchar_t *interface_path,
    size_t interface_capacity
)
{
    const wchar_t *path = direction == MUSIC_RIG_WINMM_MIDI_INPUT
        ? L"fake://physical-arturia-input"
        : L"fake://basic-loopback-output";
    size_t length = wcslen(path);

    (void)context;
    if (transient_index != 0U) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (interface_path == NULL || length >= interface_capacity) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    wmemcpy(interface_path, path, length + 1U);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_open(
    void *context,
    uint32_t transient_index,
    music_rig_winmm_midi_input_callback_fn callback,
    void *callback_argument,
    void **handle
)
{
    fake_winmm *fake = context;

    if (transient_index != 0U || callback == NULL ||
        callback_argument == NULL || handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    fake->input_callback = callback;
    fake->input_callback_argument = callback_argument;
    *handle = &fake->input_callback;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_output_open(
    void *context, uint32_t transient_index, void **handle
)
{
    fake_winmm *fake = context;

    if (transient_index != 0U || handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *handle = &fake->output_event;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_start(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != &fake->input_callback) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return fake->fail_input_start
        ? MUSIC_RIG_RESULT_ADAPTER_FAILURE : MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_stop(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != &fake->input_callback) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    record_shutdown(fake, FAKE_SHUTDOWN_INPUT_STOP);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_requeue_long(
    void *context, void *handle, uint32_t token
)
{
    (void)context;
    (void)token;
    return handle == NULL
        ? MUSIC_RIG_RESULT_INVALID_ARGUMENT : MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_close(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != &fake->input_callback) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    record_shutdown(fake, FAKE_SHUTDOWN_INPUT_CLOSE);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_output_send(
    void *context, void *handle, uint32_t packed_short_message
)
{
    fake_winmm *fake = context;

    if (handle != &fake->output_event) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&fake->lock);
    if (fake->sent_message_count >=
            sizeof(fake->sent_messages) / sizeof(fake->sent_messages[0])) {
        LeaveCriticalSection(&fake->lock);
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    fake->sent_messages[fake->sent_message_count++] = packed_short_message;
    LeaveCriticalSection(&fake->lock);
    (void)SetEvent(fake->output_event);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_output_close(void *context, void *handle)
{
    fake_winmm *fake = context;
    music_rig_win32_carla_fake_status status;

    if (handle != &fake->output_event || fake->carla_query == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(&status, 0, sizeof(status));
    if (!fake->carla_query(&status, sizeof(status))) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    fake->carla_was_open_at_output_close = status.engine_close_calls == 0U;
    record_shutdown(fake, FAKE_SHUTDOWN_OUTPUT_CLOSE);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_worker_start(
    void *context,
    music_rig_winmm_midi_worker_kind kind,
    music_rig_winmm_midi_worker_fn entry,
    void *worker_argument,
    void **thread_output
)
{
    fake_winmm *fake = context;
    fake_worker *worker;

    if ((size_t)kind >= 2U || entry == NULL || worker_argument == NULL ||
        thread_output == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    worker = &fake->workers[(size_t)kind];
    worker->entry = entry;
    worker->argument = worker_argument;
    worker->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    worker->idle_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (worker->wake_event == NULL || worker->idle_event == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    worker->thread = CreateThread(NULL, 0U, run_fake_worker, worker, 0U, NULL);
    if (worker->thread == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *thread_output = worker;
    return MUSIC_RIG_RESULT_OK;
}

static void fake_worker_signal(
    void *context, music_rig_winmm_midi_worker_kind kind
)
{
    fake_winmm *fake = context;
    fake_worker *worker = &fake->workers[(size_t)kind];

    if (kind == MUSIC_RIG_WINMM_MIDI_INPUT_WORKER &&
        fake->defer_input_wake) {
        return;
    }
    (void)SetEvent(worker->wake_event);
}

static void fake_worker_wait(
    void *context, music_rig_winmm_midi_worker_kind kind
)
{
    fake_winmm *fake = context;
    fake_worker *worker = &fake->workers[(size_t)kind];

    (void)SetEvent(worker->idle_event);
    (void)WaitForSingleObject(worker->wake_event, INFINITE);
}

static music_rig_result fake_worker_join(
    void *context,
    music_rig_winmm_midi_worker_kind kind,
    void *thread
)
{
    fake_winmm *fake = context;
    fake_worker *worker;
    DWORD wait_result;

    if ((size_t)kind >= 2U || thread != &fake->workers[(size_t)kind]) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    worker = thread;
    (void)SetEvent(worker->wake_event);
    wait_result = WaitForSingleObject(worker->thread, 5000U);
    if (wait_result != WAIT_OBJECT_0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    record_shutdown(fake, kind == MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
        ? FAKE_SHUTDOWN_INPUT_JOIN : FAKE_SHUTDOWN_OUTPUT_JOIN);
    (void)CloseHandle(worker->thread);
    (void)CloseHandle(worker->wake_event);
    (void)CloseHandle(worker->idle_event);
    memset(worker, 0, sizeof(*worker));
    return MUSIC_RIG_RESULT_OK;
}

static const music_rig_winmm_midi_ops fake_ops = {
    fake_device_count,
    fake_device_interface,
    fake_input_open,
    fake_output_open,
    fake_input_start,
    fake_input_stop,
    fake_input_requeue_long,
    fake_input_close,
    fake_output_send,
    fake_output_close,
    fake_worker_start,
    fake_worker_signal,
    fake_worker_wait,
    fake_worker_join
};

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

static uint32_t pack_message(const uint8_t message[3])
{
    return (uint32_t)message[0] | ((uint32_t)message[1] << 8U) |
        ((uint32_t)message[2] << 16U);
}

static bool deliver_input_and_wait(
    fake_winmm *fake,
    const uint8_t message[3],
    uint64_t timestamp
)
{
    fake_worker *worker = &fake->workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER];

    if (fake->input_callback == NULL || !ResetEvent(worker->idle_event)) {
        return false;
    }
    fake->input_callback(fake->input_callback_argument,
        MUSIC_RIG_WINMM_MIDI_INPUT_SHORT, pack_message(message), NULL, 0U, 0U,
        timestamp);
    return WaitForSingleObject(worker->idle_event, 5000U) == WAIT_OBJECT_0;
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
    static const uint8_t management_release[] = {
        UINT8_C(0x89), UINT8_C(42), UINT8_C(64)
    };
    static const uint8_t unsupported_pad_two[] = {
        UINT8_C(0x99), UINT8_C(41), UINT8_C(100)
    };
    static const uint8_t non_management_channel_press[] = {
        UINT8_C(0x91), UINT8_C(42), UINT8_C(100)
    };
    static const uint8_t management_press[] = {
        UINT8_C(0x99), UINT8_C(42), UINT8_C(100)
    };
    const uint8_t cc[] = {UINT8_C(0xb0), UINT8_C(1), UINT8_C(55)};
    const uint8_t pitch[] = {UINT8_C(0xe0), UINT8_C(12), UINT8_C(34)};
    const uint8_t note_on[] = {UINT8_C(0x90), UINT8_C(62), UINT8_C(80)};
    const uint8_t note_off[] = {UINT8_C(0x80), UINT8_C(62), UINT8_C(0)};
    const uint8_t other_channel[] = {UINT8_C(0xb1), UINT8_C(1), UINT8_C(90)};
    const uint8_t unsupported[] = {UINT8_C(0xa0), UINT8_C(62), UINT8_C(90)};
    const uint8_t *const input_messages[] = {
        cc, other_channel, pitch, note_on, note_off, unsupported
    };
    music_rig_win32_carla_runtime *runtime = NULL;
    music_rig_win32_carla_host_options carla_options;
    music_rig_win32_carla_profile_map pad_profiles[1];
    music_rig_winmm_midi_endpoint endpoint;
    music_rig_win32_carla_fake_status status;
    fake_winmm fake;
    music_rig_win32_carla_fake_query_fn query = NULL;
    HMODULE fake_library;
    FARPROC query_symbol;
    wchar_t wide_dll_path[32768];
    char utf8_dll_path[32768];
    char error_message[512];
    size_t index;
    size_t drained = 0U;
    uint32_t idle_before_input;

    CHECK(argc == 2, "expected the fake Carla DLL path argument");
    CHECK(load_dll_path(argv[1], wide_dll_path,
            sizeof(wide_dll_path) / sizeof(wide_dll_path[0]), utf8_dll_path,
            sizeof(utf8_dll_path)), "could not convert fake Carla DLL path");
    fake_library = LoadLibraryW(wide_dll_path);
    CHECK(fake_library != NULL, "could not load the fake Carla DLL");
    query_symbol = GetProcAddress(fake_library,
        "music_rig_win32_carla_fake_get_status");
    CHECK(query_symbol != NULL, "fake Carla status query export is missing");
    _Static_assert(sizeof(query_symbol) == sizeof(query),
        "fake status function pointer must match FARPROC size");
    memcpy(&query, &query_symbol, sizeof(query));

    memset(&fake, 0, sizeof(fake));
    fake.carla_query = query;
    fake.output_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    CHECK(fake.output_event != NULL, "could not create fake output event");
    InitializeCriticalSection(&fake.lock);

    memset(&endpoint, 0, sizeof(endpoint));
    memcpy(endpoint.slot, "arturia-main", sizeof("arturia-main"));
    memcpy(endpoint.input_interface, L"fake://physical-arturia-input",
        sizeof(L"fake://physical-arturia-input"));
    memcpy(endpoint.output_interface, L"fake://basic-loopback-output",
        sizeof(L"fake://basic-loopback-output"));
    memset(&carla_options, 0, sizeof(carla_options));
    carla_options.carla_dll_path = utf8_dll_path;
    carla_options.project_path = "offline-project.carxp";
    carla_options.driver_name = "Offline";
    carla_options.client_name = "music-rig-offline";
    carla_options.midi_client_name = "MIDI Services Loopback";
    carla_options.midi_port_name = "MIDI In";
    carla_options.patch_names = patch_names;
    carla_options.patch_count = MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT;
    carla_options.patch_indexes = patch_indexes;
    carla_options.catalog_patch_count = MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT;
    carla_options.active_layers = initial_layers;
    carla_options.active_layer_count = sizeof(initial_layers);
    carla_options.initial_master_volume = 100U;

    memset(pad_profiles, 0, sizeof(pad_profiles));
    pad_profiles[0].management_pad_code = 3U;
    memcpy(pad_profiles[0].patch_indexes, patch_indexes,
        sizeof(pad_profiles[0].patch_indexes));
    memcpy(pad_profiles[0].active_layers, next_layers, sizeof(next_layers));
    pad_profiles[0].active_layer_count = sizeof(next_layers);

    pad_profiles[0].management_pad_code = 2U;
    CHECK(music_rig_win32_carla_runtime_open(
            &runtime, &fake_ops, &fake, &endpoint, 1U, pad_profiles, 1U,
            &carla_options, "arturia-main", error_message,
            sizeof(error_message)
        ) == MUSIC_RIG_RESULT_INVALID_DATA && runtime == NULL &&
        fake.input_callback == NULL,
        "runtime accepted unsupported code 2 or initialized WinMM before validation");
    pad_profiles[0].management_pad_code = 3U;

    CHECK(music_rig_win32_carla_runtime_open(
            &runtime, &fake_ops, &fake, &endpoint, 1U, pad_profiles, 1U,
            &carla_options, "arturia-main", error_message,
            sizeof(error_message)
        ) == MUSIC_RIG_RESULT_OK,
        error_message[0] == '\0' ? "runtime open failed" : error_message);
    CHECK(runtime != NULL && fake.input_callback != NULL,
        "runtime did not start the configured WinMM input");
    CHECK(query_fake(query, &status) && status.idle_calls >= 1U &&
        status.engine_close_calls == 0U,
        "Carla did not open and enumerate before WinMM input");
    idle_before_input = status.idle_calls;
    CHECK(music_rig_win32_carla_runtime_select_profile(runtime, patch_indexes,
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT, next_layers,
            sizeof(next_layers)) == MUSIC_RIG_RESULT_OK,
        "runtime did not forward owner-thread profile selection");

    {
        wrong_thread_call call;
        HANDLE thread;

        call.runtime = runtime;
        call.result = MUSIC_RIG_RESULT_OK;
        thread = CreateThread(NULL, 0U, call_runtime_from_wrong_thread,
            &call, 0U, NULL);
        CHECK(thread != NULL, "could not create owner-thread constraint probe");
        CHECK(WaitForSingleObject(thread, 5000U) == WAIT_OBJECT_0,
            "owner-thread constraint probe timed out");
        (void)CloseHandle(thread);
        CHECK(call.result == MUSIC_RIG_RESULT_INVALID_STATE,
            "runtime pump accepted a non-owner thread");
    }

    (void)ResetEvent(fake.workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER].idle_event);
    fake.defer_input_wake = true;
    for (index = 0U; index < sizeof(input_messages) / sizeof(input_messages[0]);
         ++index) {
        uint32_t packed = pack_message(input_messages[index]);

        fake.input_callback(fake.input_callback_argument,
            MUSIC_RIG_WINMM_MIDI_INPUT_SHORT, packed, NULL, 0U, 0U,
            (uint64_t)index + 1U);
    }
    fake.defer_input_wake = false;
    (void)SetEvent(fake.workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER].wake_event);
    CHECK(WaitForSingleObject(
            fake.workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER].idle_event,
            5000U
        ) == WAIT_OBJECT_0,
        "WinMM input worker did not drain the callback ring");
    CHECK(query_fake(query, &status) &&
        status.idle_calls == idle_before_input &&
        status.note_calls == 0U && status.wrong_thread_api_calls == 0U,
        "WinMM callback path called Carla instead of only enqueuing MIDI");

    CHECK(music_rig_win32_carla_runtime_pump(runtime, &drained) ==
            MUSIC_RIG_RESULT_OK && drained == 4U,
        "owner-thread pump did not drain the filtered Carla MIDI FIFO");
    CHECK(WaitForSingleObject(fake.output_event, 5000U) == WAIT_OBJECT_0,
        "fake WinMM output worker did not emit loopback MIDI");
    for (;;) {
        bool complete;

        EnterCriticalSection(&fake.lock);
        complete = fake.sent_message_count == 4U;
        LeaveCriticalSection(&fake.lock);
        if (complete) {
            break;
        }
        CHECK(WaitForSingleObject(fake.output_event, 5000U) == WAIT_OBJECT_0,
            "timed out waiting for all filtered loopback messages");
    }
    CHECK(query_fake(query, &status) && status.faults == 0U &&
        status.note_calls == 0U && status.idle_calls >= 2U,
        "Carla fake reported a callback, note, or pump fault");
    EnterCriticalSection(&fake.lock);
    CHECK(fake.sent_message_count == 4U &&
        fake.sent_messages[0] == pack_message(cc) &&
        fake.sent_messages[1] == pack_message(pitch) &&
        fake.sent_messages[2] == pack_message(note_on) &&
        fake.sent_messages[3] == pack_message(note_off),
        "loopback output was not filtered and FIFO ordered");
    LeaveCriticalSection(&fake.lock);

    CHECK(music_rig_win32_carla_runtime_select_profile(runtime, patch_indexes,
            MUSIC_RIG_WIN32_CARLA_FAKE_PATCH_COUNT, initial_layers,
            sizeof(initial_layers)) == MUSIC_RIG_RESULT_OK &&
        query_fake(query, &status) && status.active_mask == UINT32_C(0x01),
        "could not restore the initial profile before pad-input checks");
    pad_profiles[0].management_pad_code = 4U;
    {
        static const uint8_t *const ignored_pad_messages[] = {
            management_release, unsupported_pad_two,
            non_management_channel_press
        };
        uint32_t activation_calls_before = status.activation_calls;
        uint32_t deactivation_calls_before = status.deactivation_calls;
        uint32_t idle_calls_before = status.idle_calls;
        size_t sent_count;

        (void)ResetEvent(
            fake.workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER].idle_event
        );
        fake.defer_input_wake = true;
        for (index = 0U;
             index < sizeof(ignored_pad_messages) /
                    sizeof(ignored_pad_messages[0]); ++index) {
            fake.input_callback(fake.input_callback_argument,
                MUSIC_RIG_WINMM_MIDI_INPUT_SHORT,
                pack_message(ignored_pad_messages[index]), NULL, 0U, 0U,
                (uint64_t)index + 20U);
        }
        fake.defer_input_wake = false;
        (void)SetEvent(
            fake.workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER].wake_event
        );
        CHECK(WaitForSingleObject(
                fake.workers[MUSIC_RIG_WINMM_MIDI_INPUT_WORKER].idle_event,
                5000U
            ) == WAIT_OBJECT_0,
            "WinMM worker did not consume release/ignored pad messages");
        CHECK(query_fake(query, &status) &&
            status.activation_calls == activation_calls_before &&
            status.deactivation_calls == deactivation_calls_before &&
            status.idle_calls == idle_calls_before &&
            status.wrong_thread_api_calls == 0U &&
            status.active_mask == UINT32_C(0x01),
            "WinMM worker called Carla or an ignored pad changed the profile");
        CHECK(music_rig_win32_carla_runtime_pump(runtime, &drained) ==
                MUSIC_RIG_RESULT_OK && drained == 0U &&
            query_fake(query, &status) && status.active_mask == UINT32_C(0x01),
            "release, unsupported Pad 2, or non-management press switched profile");
        EnterCriticalSection(&fake.lock);
        sent_count = fake.sent_message_count;
        LeaveCriticalSection(&fake.lock);
        CHECK(sent_count == 4U,
            "ignored management messages reached the Basic Loopback output");
    }
    {
        uint32_t activation_calls_before = status.activation_calls;
        uint32_t deactivation_calls_before = status.deactivation_calls;
        uint32_t idle_calls_before = status.idle_calls;
        size_t sent_count;

        CHECK(deliver_input_and_wait(&fake, management_press, 30U),
            "WinMM worker did not consume the mapped management-pad press");
        CHECK(query_fake(query, &status) &&
            status.activation_calls == activation_calls_before &&
            status.deactivation_calls == deactivation_calls_before &&
            status.idle_calls == idle_calls_before &&
            status.wrong_thread_api_calls == 0U &&
            status.active_mask == UINT32_C(0x01),
            "mapped management-pad press called Carla from the WinMM worker");
        CHECK(music_rig_win32_carla_runtime_pump(runtime, &drained) ==
                MUSIC_RIG_RESULT_OK && drained == 1U &&
            query_fake(query, &status) && status.active_mask == UINT32_C(0x0a),
            "owner-thread pump did not apply the copied pad profile");
        EnterCriticalSection(&fake.lock);
        sent_count = fake.sent_message_count;
        LeaveCriticalSection(&fake.lock);
        CHECK(sent_count == 4U,
            "management-pad press was also routed as normal MIDI");
    }

    CHECK(music_rig_win32_carla_runtime_close(&runtime) ==
            MUSIC_RIG_RESULT_OK && runtime == NULL,
        "runtime close failed");
    CHECK(fake.carla_was_open_at_output_close,
        "Carla closed before WinMM output teardown");
    CHECK(fake.shutdown_event_count == 5U &&
        fake.shutdown_events[0] == FAKE_SHUTDOWN_INPUT_STOP &&
        fake.shutdown_events[1] == FAKE_SHUTDOWN_INPUT_JOIN &&
        fake.shutdown_events[2] == FAKE_SHUTDOWN_OUTPUT_JOIN &&
        fake.shutdown_events[3] == FAKE_SHUTDOWN_INPUT_CLOSE &&
        fake.shutdown_events[4] == FAKE_SHUTDOWN_OUTPUT_CLOSE,
        "WinMM stop/join/close order was not preserved");
    CHECK(query_fake(query, &status) && status.engine_close_calls == 1U &&
        (status.lifecycle & MUSIC_RIG_WIN32_CARLA_FAKE_LIFECYCLE_ENGINE_CLOSE) != 0U &&
        status.faults == 0U,
        "Carla did not close cleanly after WinMM shutdown");

    memset(fake.shutdown_events, 0, sizeof(fake.shutdown_events));
    fake.shutdown_event_count = 0U;
    fake.carla_was_open_at_output_close = false;
    fake.input_callback = NULL;
    fake.fail_input_start = true;
    CHECK(music_rig_win32_carla_runtime_open(
            &runtime, &fake_ops, &fake, &endpoint, 1U, pad_profiles, 1U,
            &carla_options, "arturia-main", error_message,
            sizeof(error_message)
        ) == MUSIC_RIG_RESULT_ADAPTER_FAILURE && runtime == NULL,
        "WinMM start failure did not fail and unwind runtime open");
    CHECK(fake.carla_was_open_at_output_close &&
        fake.shutdown_event_count == 4U &&
        fake.shutdown_events[0] == FAKE_SHUTDOWN_INPUT_JOIN &&
        fake.shutdown_events[1] == FAKE_SHUTDOWN_OUTPUT_JOIN &&
        fake.shutdown_events[2] == FAKE_SHUTDOWN_INPUT_CLOSE &&
        fake.shutdown_events[3] == FAKE_SHUTDOWN_OUTPUT_CLOSE,
        "failed startup did not release WinMM before closing Carla");
    CHECK(query_fake(query, &status) && status.engine_close_calls == 1U &&
        status.faults == 0U,
        "Carla resources remained after failed runtime startup");

    DeleteCriticalSection(&fake.lock);
    (void)CloseHandle(fake.output_event);
    CHECK(FreeLibrary(fake_library) != 0,
        "could not unload the fake Carla DLL");
    puts("Win32 Carla runtime fake integration test: OK");
    return 0;
}
