#define main arturia_protected_main
#include "../../arturia-main-volume-encoder/arturia-main-volume-encoder.c"
#undef main

#include <string.h>

#define TEST_EVENT_CAPACITY 16
#define TEST_AUDIO_FRAMES 16

enum {
    PORT_MIDI_INPUT,
    PORT_MIDI_OUTPUT,
    PORT_AUDIO_INPUT_LEFT,
    PORT_AUDIO_INPUT_RIGHT,
    PORT_AUDIO_OUTPUT_LEFT,
    PORT_AUDIO_OUTPUT_RIGHT,
};

struct _jack_client {
    int unused;
};

struct _jack_port {
    int kind;
};

typedef struct {
    jack_nframes_t time;
    size_t size;
    unsigned char data[3];
} fake_midi_event_t;

typedef struct {
    fake_midi_event_t events[TEST_EVENT_CAPACITY];
    uint32_t count;
} fake_midi_buffer_t;

static struct _jack_port fake_midi_input_port = { PORT_MIDI_INPUT };
static struct _jack_port fake_midi_output_port = { PORT_MIDI_OUTPUT };
static struct _jack_port fake_audio_input_left_port = { PORT_AUDIO_INPUT_LEFT };
static struct _jack_port fake_audio_input_right_port = { PORT_AUDIO_INPUT_RIGHT };
static struct _jack_port fake_audio_output_left_port = { PORT_AUDIO_OUTPUT_LEFT };
static struct _jack_port fake_audio_output_right_port = {
    PORT_AUDIO_OUTPUT_RIGHT
};
static fake_midi_buffer_t fake_midi_input;
static fake_midi_buffer_t fake_midi_output;
static float fake_audio_input_left[TEST_AUDIO_FRAMES];
static float fake_audio_input_right[TEST_AUDIO_FRAMES];
static float fake_audio_output_left[TEST_AUDIO_FRAMES];
static float fake_audio_output_right[TEST_AUDIO_FRAMES];
static int fake_output_connections;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Offline Arturia test failed: %s\n", message); \
        return 1; \
    } \
} while (0)

static void reset_runtime(void)
{
    size_t index;

    input_port = &fake_midi_input_port;
    output_port = &fake_midi_output_port;
    audio_input_left_port = &fake_audio_input_left_port;
    audio_input_right_port = &fake_audio_input_right_port;
    audio_output_left_port = &fake_audio_output_left_port;
    audio_output_right_port = &fake_audio_output_right_port;
    memset(&fake_midi_input, 0, sizeof(fake_midi_input));
    memset(&fake_midi_output, 0, sizeof(fake_midi_output));
    atomic_store(&volume_value, 64);
    atomic_store(&mute_value, 0);
    atomic_store(&button_down, 0);
    atomic_store(&generation, 0);
    output_connection_count = 0;
    fake_output_connections = 0;
    audio_gain = 1.0f;
    audio_ramp_step = 0.25f;

    for (index = 0; index < TEST_AUDIO_FRAMES; ++index) {
        fake_audio_input_left[index] = 1.0f;
        fake_audio_input_right[index] = 0.5f;
        fake_audio_output_left[index] = 0.0f;
        fake_audio_output_right[index] = 0.0f;
    }
}

static void set_input(
    jack_nframes_t time_value,
    unsigned char status,
    unsigned char controller,
    unsigned char value
)
{
    fake_midi_event_t *event = &fake_midi_input.events[0];

    fake_midi_input.count = 1;
    event->time = time_value;
    event->size = 3;
    event->data[0] = status;
    event->data[1] = controller;
    event->data[2] = value;
}

static int output_matches(
    uint32_t index,
    jack_nframes_t time_value,
    unsigned char controller,
    unsigned char value
)
{
    const fake_midi_event_t *event;

    if (index >= fake_midi_output.count)
        return 0;
    event = &fake_midi_output.events[index];
    return event->time == time_value &&
        event->size == 3 &&
        event->data[0] == 0xb0 &&
        event->data[1] == controller &&
        event->data[2] == value;
}

static int test_clamping_and_state(void)
{
    char path[] = "/tmp/music-rig-arturia-state.XXXXXX";
    int descriptor;
    int loaded_volume;
    int loaded_mute;

    CHECK(clamp_midi_value(-1) == 0, "negative value did not clamp");
    CHECK(clamp_midi_value(42) == 42, "valid value changed");
    CHECK(clamp_midi_value(128) == 127, "high value did not clamp");

    descriptor = mkstemp(path);
    CHECK(descriptor >= 0, "could not reserve temporary state path");
    CHECK(close(descriptor) == 0, "could not close temporary state file");
    CHECK(unlink(path) == 0, "could not prepare missing-state test");

    load_state(path, 42, &loaded_volume, &loaded_mute);
    CHECK(loaded_volume == 42, "missing state did not use fallback volume");
    CHECK(loaded_mute == 0, "missing state did not default to unmuted");

    CHECK(save_state(path, 91, 127) == 0, "atomic state save failed");
    load_state(path, 0, &loaded_volume, &loaded_mute);
    CHECK(loaded_volume == 91, "saved volume did not round trip");
    CHECK(loaded_mute == 127, "saved mute did not round trip");
    CHECK(unlink(path) == 0, "could not remove temporary state file");
    return 0;
}

static int test_connection_replay(void)
{
    reset_runtime();
    fake_output_connections = 1;

    CHECK(process_midi(4, NULL) == 0, "connection replay process failed");
    CHECK(fake_midi_output.count == 1, "new connection did not replay volume");
    CHECK(
        output_matches(0, 0, VOLUME_OUTPUT_CC, 64),
        "connection replay emitted the wrong value"
    );

    CHECK(process_midi(4, NULL) == 0, "steady connection process failed");
    CHECK(fake_midi_output.count == 0, "steady connection replayed twice");
    return 0;
}

static int test_relative_volume(void)
{
    reset_runtime();
    set_input(3, 0xb0, VOLUME_INPUT_CC, 65);

    CHECK(process_midi(4, NULL) == 0, "relative volume process failed");
    CHECK(atomic_load(&volume_value) == 65, "relative volume was not applied");
    CHECK(atomic_load(&generation) == 1, "volume change was not published");
    CHECK(fake_midi_output.count == 1, "volume change emitted wrong count");
    CHECK(
        output_matches(0, 3, VOLUME_OUTPUT_CC, 65),
        "volume change emitted wrong message"
    );

    reset_runtime();
    set_input(1, 0xb0, VOLUME_INPUT_CC, 64);
    CHECK(process_midi(4, NULL) == 0, "neutral encoder process failed");
    CHECK(fake_midi_output.count == 0, "neutral encoder emitted output");
    CHECK(atomic_load(&generation) == 0, "neutral encoder changed generation");

    reset_runtime();
    set_input(1, 0xb1, VOLUME_INPUT_CC, 65);
    CHECK(process_midi(4, NULL) == 0, "wrong-channel process failed");
    CHECK(fake_midi_output.count == 0, "wrong MIDI channel was consumed");
    return 0;
}

static int test_mute_edge_and_audio_ramp(void)
{
    reset_runtime();
    set_input(2, 0xb0, BUTTON_INPUT_CC, 127);

    CHECK(process_midi(4, NULL) == 0, "mute press process failed");
    CHECK(atomic_load(&mute_value) == 127, "mute press did not toggle on");
    CHECK(output_matches(0, 2, MUTE_OUTPUT_CC, 127), "wrong mute-on output");

    CHECK(process_midi(4, NULL) == 0, "held mute process failed");
    CHECK(fake_midi_output.count == 0, "held button toggled more than once");

    set_input(2, 0xb0, BUTTON_INPUT_CC, 0);
    CHECK(process_midi(4, NULL) == 0, "mute release process failed");
    CHECK(fake_midi_output.count == 0, "button release emitted output");

    set_input(2, 0xb0, BUTTON_INPUT_CC, 127);
    CHECK(process_midi(4, NULL) == 0, "second mute press process failed");
    CHECK(atomic_load(&mute_value) == 0, "second press did not toggle off");
    CHECK(output_matches(0, 2, MUTE_OUTPUT_CC, 0), "wrong mute-off output");
    CHECK(atomic_load(&generation) == 2, "mute edges published wrong count");

    reset_runtime();
    atomic_store(&mute_value, 127);
    CHECK(process_midi(4, NULL) == 0, "mute ramp process failed");
    CHECK(
        fake_audio_output_left[0] == 0.75f &&
        fake_audio_output_left[1] == 0.50f &&
        fake_audio_output_left[2] == 0.25f &&
        fake_audio_output_left[3] == 0.00f,
        "mute ramp was not click-free and bounded"
    );
    CHECK(
        fake_audio_output_right[0] == 0.375f &&
        fake_audio_output_right[3] == 0.0f,
        "stereo mute ramp diverged"
    );

    atomic_store(&mute_value, 0);
    CHECK(process_midi(4, NULL) == 0, "unmute ramp process failed");
    CHECK(
        fake_audio_output_left[0] == 0.25f &&
        fake_audio_output_left[3] == 1.0f,
        "unmute ramp did not restore unity"
    );
    return 0;
}

int main(void)
{
    if (test_clamping_and_state() != 0 ||
        test_connection_replay() != 0 ||
        test_relative_volume() != 0 ||
        test_mute_edge_and_audio_ramp() != 0)
        return 1;

    puts("Offline Arturia helper test: OK");
    return 0;
}

jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
)
{
    (void)name;
    (void)options;
    (void)status;
    return NULL;
}

int jack_client_close(jack_client_t *client)
{
    (void)client;
    return 0;
}

int jack_activate(jack_client_t *client)
{
    (void)client;
    return 0;
}

jack_nframes_t jack_get_sample_rate(jack_client_t *client)
{
    (void)client;
    return 48000;
}

int jack_set_process_callback(
    jack_client_t *client,
    int (*callback)(jack_nframes_t, void *),
    void *argument
)
{
    (void)client;
    (void)callback;
    (void)argument;
    return 0;
}

void jack_on_shutdown(
    jack_client_t *client,
    void (*callback)(void *),
    void *argument
)
{
    (void)client;
    (void)callback;
    (void)argument;
}

jack_port_t *jack_port_register(
    jack_client_t *client,
    const char *name,
    const char *type,
    unsigned long flags,
    unsigned long buffer_size
)
{
    (void)client;
    (void)name;
    (void)type;
    (void)flags;
    (void)buffer_size;
    return NULL;
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t frame_count)
{
    (void)frame_count;
    switch (port->kind) {
    case PORT_MIDI_INPUT:
        return &fake_midi_input;
    case PORT_MIDI_OUTPUT:
        return &fake_midi_output;
    case PORT_AUDIO_INPUT_LEFT:
        return fake_audio_input_left;
    case PORT_AUDIO_INPUT_RIGHT:
        return fake_audio_input_right;
    case PORT_AUDIO_OUTPUT_LEFT:
        return fake_audio_output_left;
    case PORT_AUDIO_OUTPUT_RIGHT:
        return fake_audio_output_right;
    default:
        return NULL;
    }
}

uint32_t jack_midi_get_event_count(void *port_buffer)
{
    const fake_midi_buffer_t *buffer = port_buffer;

    return buffer == &fake_midi_input ? buffer->count : 0;
}

int jack_midi_event_get(
    jack_midi_event_t *event,
    void *port_buffer,
    uint32_t index
)
{
    fake_midi_buffer_t *buffer = port_buffer;

    if (buffer != &fake_midi_input || index >= buffer->count)
        return -1;
    event->time = buffer->events[index].time;
    event->size = buffer->events[index].size;
    event->buffer = buffer->events[index].data;
    return 0;
}

void jack_midi_clear_buffer(void *port_buffer)
{
    fake_midi_buffer_t *buffer = port_buffer;

    if (buffer == &fake_midi_output)
        buffer->count = 0;
}

int jack_port_connected(const jack_port_t *port)
{
    return port == &fake_midi_output_port ? fake_output_connections : 0;
}

int jack_midi_event_write(
    void *port_buffer,
    jack_nframes_t time_value,
    const jack_midi_data_t *data,
    size_t size
)
{
    fake_midi_buffer_t *buffer = port_buffer;
    fake_midi_event_t *event;

    if (buffer != &fake_midi_output ||
        buffer->count >= TEST_EVENT_CAPACITY ||
        size > sizeof(event->data))
        return -1;

    event = &buffer->events[buffer->count++];
    event->time = time_value;
    event->size = size;
    memcpy(event->data, data, size);
    return 0;
}
