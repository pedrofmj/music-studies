#include "music_rig/winmm_midi.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define FAKE_DEVICE_COUNT ((size_t)3)

typedef struct fake_device {
    const wchar_t *interface_path;
    const char *display_name;
} fake_device;

typedef struct fake_winmm {
    fake_device inputs[FAKE_DEVICE_COUNT];
    fake_device outputs[FAKE_DEVICE_COUNT];
    uint32_t input_count;
    uint32_t output_count;
    uint32_t opened_input_index;
    uint32_t opened_output_index;
    music_rig_winmm_midi_input_callback_fn input_callback;
    void *input_callback_argument;
    void *input_handle;
    void *output_handle;
    music_rig_winmm_midi_worker_fn workers[2];
    void *worker_arguments[2];
    unsigned int input_start_count;
    unsigned int input_stop_count;
    unsigned int input_close_count;
    unsigned int output_close_count;
    unsigned int input_requeue_count;
    uint32_t requeued_tokens[16];
    unsigned int worker_join_count[2];
    uint32_t sent_messages[256];
    size_t sent_message_count;
    size_t fail_open_input_at;
    int fail_send;
} fake_winmm;

static music_rig_device_midi_shadow *active_shadow;
static size_t shadow_processed;
static size_t shadow_cycles;
static uint8_t last_shadow_message[256];
static size_t last_shadow_message_size;

#define CHECK(condition, description) do { \
    if (!(condition)) { \
        fprintf(stderr, "WinMM MIDI offline test failed: %s\n", description); \
        return 1; \
    } \
} while (0)

size_t music_rig_device_midi_shadow_slot_count(
    const music_rig_device_midi_shadow *shadow
)
{
    return shadow == active_shadow ? shadow->slot_count : 0U;
}

const char *music_rig_device_midi_shadow_slot_name(
    const music_rig_device_midi_shadow *shadow,
    size_t slot_index
)
{
    return shadow == active_shadow && slot_index < shadow->slot_count
        ? shadow->slots[slot_index].slot : NULL;
}

music_rig_result music_rig_device_midi_shadow_begin_cycle(
    music_rig_device_midi_shadow *shadow
)
{
    if (shadow != active_shadow) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    ++shadow_cycles;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_device_midi_shadow_process(
    music_rig_device_midi_shadow *shadow,
    size_t slot_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    if (shadow != active_shadow || slot_index != 0U || message == NULL ||
        message_size > sizeof(last_shadow_message)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    (void)frame;
    memcpy(last_shadow_message, message, message_size);
    last_shadow_message_size = message_size;
    ++shadow_processed;
    if (message_size == 3U && message[0] == UINT8_C(0xb0) &&
        message[1] == UINT8_C(114)) {
        const uint8_t generated[3] = {
            UINT8_C(0xb0), UINT8_C(119), message[2]
        };

        shadow->observer.output_midi(
            shadow->observer.context, slot_index, frame, generated,
            sizeof(generated)
        );
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_device_count(
    void *context,
    music_rig_winmm_midi_direction direction,
    uint32_t *count
)
{
    fake_winmm *fake = context;

    if (count == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *count = direction == MUSIC_RIG_WINMM_MIDI_INPUT
        ? fake->input_count : fake->output_count;
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
    fake_winmm *fake = context;
    fake_device *devices = direction == MUSIC_RIG_WINMM_MIDI_INPUT
        ? fake->inputs : fake->outputs;
    uint32_t count = direction == MUSIC_RIG_WINMM_MIDI_INPUT
        ? fake->input_count : fake->output_count;

    if (interface_path == NULL || transient_index >= count) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (devices[transient_index].interface_path == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (wcslen(devices[transient_index].interface_path) >= interface_capacity) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    wcscpy(interface_path, devices[transient_index].interface_path);
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

    if (transient_index == fake->fail_open_input_at || callback == NULL ||
        callback_argument == NULL || handle == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    fake->opened_input_index = transient_index;
    fake->input_callback = callback;
    fake->input_callback_argument = callback_argument;
    fake->input_handle = fake;
    *handle = fake->input_handle;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_output_open(
    void *context,
    uint32_t transient_index,
    void **handle
)
{
    fake_winmm *fake = context;

    if (handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    fake->opened_output_index = transient_index;
    fake->output_handle = &fake->opened_output_index;
    *handle = fake->output_handle;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_start(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != fake->input_handle) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    ++fake->input_start_count;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_stop(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != fake->input_handle) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    ++fake->input_stop_count;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_requeue_long(
    void *context,
    void *handle,
    uint32_t token
)
{
    fake_winmm *fake = context;

    if (handle != fake->input_handle || token >= 16U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    fake->requeued_tokens[fake->input_requeue_count++] = token;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_input_close(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != fake->input_handle) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    ++fake->input_close_count;
    fake->input_handle = NULL;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_output_send(
    void *context,
    void *handle,
    uint32_t packed_short_message
)
{
    fake_winmm *fake = context;

    if (handle != fake->output_handle || fake->fail_send != 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    fake->sent_messages[fake->sent_message_count++] = packed_short_message;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_output_close(void *context, void *handle)
{
    fake_winmm *fake = context;

    if (handle != fake->output_handle) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    ++fake->output_close_count;
    fake->output_handle = NULL;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_worker_start(
    void *context,
    music_rig_winmm_midi_worker_kind kind,
    music_rig_winmm_midi_worker_fn worker,
    void *worker_argument,
    void **thread
)
{
    fake_winmm *fake = context;
    size_t index = (size_t)kind;

    if (index >= 2U || worker == NULL || worker_argument == NULL ||
        thread == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    fake->workers[index] = worker;
    fake->worker_arguments[index] = worker_argument;
    *thread = &fake->workers[index];
    return MUSIC_RIG_RESULT_OK;
}

static void fake_worker_signal(
    void *context,
    music_rig_winmm_midi_worker_kind kind
)
{
    (void)context;
    (void)kind;
}

static void fake_worker_wait(
    void *context,
    music_rig_winmm_midi_worker_kind kind
)
{
    (void)context;
    (void)kind;
}

static music_rig_result fake_worker_join(
    void *context,
    music_rig_winmm_midi_worker_kind kind,
    void *thread
)
{
    fake_winmm *fake = context;
    size_t index = (size_t)kind;

    if (index >= 2U || thread != &fake->workers[index]) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    fake->workers[index](fake->worker_arguments[index]);
    ++fake->worker_join_count[index];
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

static void setup_devices(fake_winmm *fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->input_count = 3U;
    fake->output_count = 3U;
    fake->inputs[0] = (fake_device){L"\\\\?\\usb#keylab-other", "KeyLab"};
    fake->inputs[1] = (fake_device){L"\\\\?\\usb#keylab-target-in", "KeyLab"};
    fake->inputs[2] = (fake_device){L"\\\\?\\usb#other", "Other"};
    fake->outputs[0] = (fake_device){L"\\\\?\\usb#keylab-other-out", "KeyLab"};
    fake->outputs[1] = (fake_device){L"\\\\?\\usb#other-out", "Other"};
    fake->outputs[2] = (fake_device){L"\\\\?\\usb#keylab-target-out", "KeyLab"};
    fake->fail_open_input_at = SIZE_MAX;
}

static void setup_host(
    music_rig_winmm_midi *host,
    fake_winmm *fake,
    music_rig_device_midi_shadow *shadow
)
{
    music_rig_winmm_midi_endpoint endpoint;
    music_rig_device_midi_shadow_observer observer;

    memset(&endpoint, 0, sizeof(endpoint));
    strcpy(endpoint.slot, "arturia-main");
    wcscpy(endpoint.input_interface, L"\\\\?\\usb#keylab-target-in");
    wcscpy(endpoint.output_interface, L"\\\\?\\usb#keylab-target-out");
    (void)music_rig_winmm_midi_init(host, &fake_ops, fake, &endpoint, 1U);
    (void)music_rig_winmm_midi_observer_init(host, &observer);
    memset(shadow, 0, sizeof(*shadow));
    shadow->abi_version = MUSIC_RIG_DEVICE_MIDI_SHADOW_ABI_VERSION;
    shadow->observer = observer;
    shadow->slot_count = 1U;
    strcpy(shadow->slots[0].slot, "arturia-main");
    active_shadow = shadow;
    shadow_processed = 0U;
    shadow_cycles = 0U;
    last_shadow_message_size = 0U;
    (void)music_rig_winmm_midi_attach_shadow(host, shadow);
}

static uint32_t pack_message(uint8_t status, uint8_t data1, uint8_t data2)
{
    return (uint32_t)status | ((uint32_t)data1 << 8U) |
        ((uint32_t)data2 << 16U);
}

int main(void)
{
    fake_winmm fake;
    music_rig_winmm_midi host;
    music_rig_device_midi_shadow shadow;
    music_rig_winmm_midi_metrics metrics;
    music_rig_arturia_control_state control_state;
    music_rig_arturia_shared_sf2_plan sf2_plan;
    uint16_t patch_index_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    uint8_t layers[] = {1U, 3U, 8U};
    uint8_t fader_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {0U};
    uint8_t knob_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {0U};
    uint8_t sysex[] = {UINT8_C(0xf0), UINT8_C(0x7d), UINT8_C(0xf7)};
    size_t index;

    setup_devices(&fake);
    setup_host(&host, &fake, &shadow);
    CHECK(music_rig_winmm_midi_start(&host) == MUSIC_RIG_RESULT_OK,
        "start failed");
    CHECK(fake.opened_input_index == 1U && fake.opened_output_index == 2U,
        "endpoint identity did not select exact paths");
    CHECK(host.input_thread != NULL && host.output_thread != NULL,
        "input and output workers were not started");
    CHECK(music_rig_winmm_midi_arturia_state_read(&host, &control_state) ==
            MUSIC_RIG_RESULT_OK && control_state.values[0] == 0U &&
            control_state.values[16] == 64U,
        "Arturia startup control defaults were not initialized");

    fake.input_callback(fake.input_callback_argument,
        MUSIC_RIG_WINMM_MIDI_INPUT_SHORT,
        pack_message(UINT8_C(0xb0), UINT8_C(114), UINT8_C(65)), NULL, 0U,
        0U, UINT64_C(123456));
    CHECK(host.input_rings[0].events[0].qpc_ns == UINT64_C(123456),
        "callback did not preserve the captured QPC timestamp");
    CHECK(music_rig_winmm_midi_drain_input(&host) == 1U,
        "short input was not worker-drained");
    CHECK(shadow_cycles == 1U && shadow_processed == 1U &&
        last_shadow_message_size == 3U &&
        last_shadow_message[1] == UINT8_C(114),
        "short input did not reach the portable shadow");
    CHECK(music_rig_winmm_midi_drain_output(&host) == 1U &&
        fake.sent_message_count == 1U &&
        fake.sent_messages[0] == pack_message(
            UINT8_C(0xb0), UINT8_C(119), UINT8_C(65)),
        "shadow output was not consumed by the output worker");

    fake.input_callback(fake.input_callback_argument,
        MUSIC_RIG_WINMM_MIDI_INPUT_SHORT,
        pack_message(UINT8_C(0xb0), UINT8_C(73), UINT8_C(91)), NULL, 0U,
        0U, UINT64_C(222222));
    CHECK(music_rig_winmm_midi_drain_input(&host) == 1U &&
            music_rig_winmm_midi_arturia_state_read(&host, &control_state) ==
                MUSIC_RIG_RESULT_OK && control_state.values[0] == 91U,
        "Arturia fader CC was not retained by the WinMM state adapter");

    fake.input_callback(fake.input_callback_argument,
        MUSIC_RIG_WINMM_MIDI_INPUT_LONG, 0U, sysex, sizeof(sysex), 2U,
        UINT64_C(456789));
    CHECK(music_rig_winmm_midi_drain_input(&host) == 1U &&
        last_shadow_message_size == sizeof(sysex) &&
        fake.input_requeue_count == 1U && fake.requeued_tokens[0] == 2U,
        "preallocated long input was not drained and requeued");

    for (index = 0U; index < MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY; ++index) {
        fake.input_callback(fake.input_callback_argument,
            MUSIC_RIG_WINMM_MIDI_INPUT_SHORT,
            pack_message(UINT8_C(0x90), (uint8_t)(index % 127U),
                UINT8_C(100)), NULL, 0U, 0U, (uint64_t)index);
    }
    fake.input_callback(fake.input_callback_argument,
        MUSIC_RIG_WINMM_MIDI_INPUT_LONG, 0U, sysex, sizeof(sysex), 1U,
        UINT64_C(999));
    metrics = music_rig_winmm_midi_metrics_read(&host);
    CHECK(metrics.input_overflows == UINT64_C(1),
        "full input ring did not account for overflow");
    CHECK(music_rig_winmm_midi_drain_input(&host) ==
            MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY &&
        fake.input_requeue_count == 2U && fake.requeued_tokens[1] == 1U,
        "overflowed long input buffer was not returned to WinMM");

    CHECK(music_rig_winmm_midi_send_arturia_state(&host, 0U, &control_state) ==
            MUSIC_RIG_RESULT_OK,
        "Arturia state queue failed");
    CHECK(music_rig_winmm_midi_drain_output(&host) ==
            MUSIC_RIG_ARTURIA_CONTROL_COUNT &&
        fake.sent_messages[1] == pack_message(
            UINT8_C(0xb0), UINT8_C(73), UINT8_C(91)),
        "portable Arturia state was not translated to output messages");

    for (index = 0U; index < MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT; ++index) {
        patch_index_values[index] = (uint16_t)(index + 10U);
        fader_values[index] = (uint8_t)(index + 20U);
        knob_values[index] = (uint8_t)(index + 40U);
    }
    CHECK(music_rig_arturia_shared_sf2_plan_build(
            patch_index_values, UINT16_C(32), layers, sizeof(layers), &sf2_plan
        ) == MUSIC_RIG_RESULT_OK,
        "portable shared-SF2 plan setup failed");
    CHECK(music_rig_winmm_midi_send_shared_sf2_plan(
            &host, 0U, &sf2_plan, fader_values, knob_values
        ) == MUSIC_RIG_RESULT_OK,
        "shared-SF2 plan queue failed");
    CHECK(music_rig_winmm_midi_drain_output(&host) == 6U &&
        fake.sent_messages[19] == pack_message(
            UINT8_C(0xb0), UINT8_C(73), UINT8_C(20)) &&
        fake.sent_messages[20] == pack_message(
            UINT8_C(0xb0), UINT8_C(74), UINT8_C(40)) &&
        fake.sent_messages[23] == pack_message(
            UINT8_C(0xb0), UINT8_C(83), UINT8_C(27)),
        "shared-SF2 plan did not map its Arturia CC routes");

    for (index = 0U; index < MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY; ++index) {
        uint8_t message[3] = {UINT8_C(0xb0), UINT8_C(1), UINT8_C(2)};

        CHECK(music_rig_winmm_midi_send(&host, 0U, message, sizeof(message)) ==
                MUSIC_RIG_RESULT_OK,
            "output API rejected an available queue slot");
    }
    CHECK(music_rig_winmm_midi_send_arturia_state(&host, 0U, &control_state) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE,
        "output API partially accepted a batch that does not fit");
    CHECK(music_rig_winmm_midi_drain_output(&host) ==
            MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY,
        "output worker did not drain the bounded API queue");
    metrics = music_rig_winmm_midi_metrics_read(&host);
    CHECK(metrics.output_overflows == UINT64_C(1),
        "output queue overflow was not counted");
    fake.fail_send = 1;
    {
        uint8_t message[3] = {UINT8_C(0xb0), UINT8_C(2), UINT8_C(3)};

        CHECK(music_rig_winmm_midi_send(&host, 0U, message, sizeof(message)) ==
                MUSIC_RIG_RESULT_OK &&
            music_rig_winmm_midi_drain_output(&host) == 1U,
            "output worker did not attempt the fake send");
    }
    metrics = music_rig_winmm_midi_metrics_read(&host);
    CHECK(metrics.output_failures == UINT64_C(1),
        "output send failure was not counted");

    fake.fail_send = 0;
    fake.input_callback(fake.input_callback_argument,
        MUSIC_RIG_WINMM_MIDI_INPUT_SHORT,
        pack_message(UINT8_C(0xb0), UINT8_C(114), UINT8_C(33)), NULL, 0U,
        0U, UINT64_C(1000));
    {
        size_t sent_before_stop = fake.sent_message_count;

        CHECK(music_rig_winmm_midi_stop(&host) == MUSIC_RIG_RESULT_OK,
            "stop failed");
        CHECK(fake.sent_message_count == sent_before_stop + 1U &&
            fake.sent_messages[sent_before_stop] == pack_message(
                UINT8_C(0xb0), UINT8_C(119), UINT8_C(33)),
            "shutdown stopped output before draining the input worker");
    }
    CHECK(fake.input_stop_count == 1U && fake.input_close_count == 1U &&
        fake.output_close_count == 1U && fake.worker_join_count[0] == 1U &&
        fake.worker_join_count[1] == 1U,
        "stop did not close ports and join both workers");

    fake.inputs[0] = fake.inputs[1];
    fake.inputs[1] = (fake_device){L"\\\\?\\usb#keylab-other", "KeyLab"};
    fake.outputs[0] = fake.outputs[2];
    fake.outputs[2] = (fake_device){L"\\\\?\\usb#keylab-other-out", "KeyLab"};
    CHECK(music_rig_winmm_midi_start(&host) == MUSIC_RIG_RESULT_OK &&
        fake.opened_input_index == 0U && fake.opened_output_index == 0U,
        "restart reused stale numeric indices instead of re-resolving paths");
    CHECK(music_rig_winmm_midi_stop(&host) == MUSIC_RIG_RESULT_OK,
        "reordered-device stop failed");

    setup_devices(&fake);
    setup_host(&host, &fake, &shadow);
    fake.inputs[2] = fake.inputs[1];
    CHECK(music_rig_winmm_midi_start(&host) == MUSIC_RIG_RESULT_INVALID_DATA &&
        fake.input_handle == NULL && fake.output_handle == NULL,
        "ambiguous stable interface identity was accepted");

    setup_devices(&fake);
    setup_host(&host, &fake, &shadow);
    fake.fail_open_input_at = 1U;
    CHECK(music_rig_winmm_midi_start(&host) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE &&
        fake.input_handle == NULL && fake.output_handle == NULL,
        "input-open failure leaked a WinMM handle");

    puts("WinMM MIDI fake-seam offline test: OK");
    return 0;
}
