#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Keep this utility buildable on hosts that have libjack but not JACK headers. */
typedef uint32_t jack_nframes_t;
typedef uint32_t jack_options_t;
typedef uint32_t jack_status_t;
typedef struct _jack_client jack_client_t;
typedef struct _jack_port jack_port_t;
typedef unsigned char jack_midi_data_t;
typedef float jack_default_audio_sample_t;
typedef struct {
    jack_nframes_t time;
    size_t size;
    jack_midi_data_t *buffer;
} jack_midi_event_t;

extern jack_client_t *jack_client_open(
    const char *, jack_options_t, jack_status_t *, ...
);
extern int jack_client_close(jack_client_t *);
extern int jack_activate(jack_client_t *);
extern jack_nframes_t jack_get_sample_rate(jack_client_t *);
extern int jack_set_process_callback(
    jack_client_t *, int (*)(jack_nframes_t, void *), void *
);
extern void jack_on_shutdown(jack_client_t *, void (*)(void *), void *);
extern jack_port_t *jack_port_register(
    jack_client_t *, const char *, const char *, unsigned long, unsigned long
);
extern void *jack_port_get_buffer(jack_port_t *, jack_nframes_t);
extern uint32_t jack_midi_get_event_count(void *);
extern int jack_midi_event_get(jack_midi_event_t *, void *, uint32_t);
extern void jack_midi_clear_buffer(void *);
extern int jack_midi_event_write(
    void *, jack_nframes_t, const jack_midi_data_t *, size_t
);

enum {
    JACK_NO_START_SERVER = 0x01,
    JACK_PORT_IS_INPUT = 0x01,
    JACK_PORT_IS_OUTPUT = 0x02,
    VOLUME_INPUT_CC = 114,
    BUTTON_INPUT_CC = 115,
    MUTE_OUTPUT_CC = 118,
    VOLUME_OUTPUT_CC = 119,
    MIDI_CHANNEL = 0,
};

static const char *const JACK_MIDI_TYPE = "8 bit raw midi";
static const char *const JACK_AUDIO_TYPE = "32 bit float mono audio";
static jack_port_t *input_port;
static jack_port_t *output_port;
static jack_port_t *audio_input_left_port;
static jack_port_t *audio_input_right_port;
static jack_port_t *audio_output_left_port;
static jack_port_t *audio_output_right_port;
static atomic_int volume_value;
static atomic_int mute_value;
static atomic_int button_down;
static atomic_uint generation;
static float audio_gain;
static float audio_ramp_step;
static volatile sig_atomic_t running = 1;

static int clamp_midi_value(int value)
{
    if (value < 0)
        return 0;
    if (value > 127)
        return 127;
    return value;
}

static int process_midi(jack_nframes_t frame_count, void *unused)
{
    void *input_buffer;
    void *output_buffer;
    uint32_t event_count;
    uint32_t index;

    (void)unused;
    input_buffer = jack_port_get_buffer(input_port, frame_count);
    output_buffer = jack_port_get_buffer(output_port, frame_count);
    jack_midi_clear_buffer(output_buffer);
    event_count = jack_midi_get_event_count(input_buffer);

    for (index = 0; index < event_count; ++index) {
        jack_midi_event_t event;
        jack_midi_data_t message[3];
        int controller;

        if (jack_midi_event_get(&event, input_buffer, index) != 0)
            continue;
        if (event.size != 3)
            continue;
        if ((event.buffer[0] & 0xf0) != 0xb0)
            continue;
        if ((event.buffer[0] & 0x0f) != MIDI_CHANNEL)
            continue;
        controller = event.buffer[1];
        if (controller == VOLUME_INPUT_CC) {
            int delta = (int)event.buffer[2] - 64;
            int next_value;

            if (delta == 0)
                continue;
            next_value = clamp_midi_value(atomic_load(&volume_value) + delta);
            atomic_store(&volume_value, next_value);
            atomic_fetch_add(&generation, 1);

            message[0] = 0xb0 | MIDI_CHANNEL;
            message[1] = VOLUME_OUTPUT_CC;
            message[2] = (jack_midi_data_t)next_value;
            (void)jack_midi_event_write(output_buffer, event.time, message, 3);
        } else if (controller == BUTTON_INPUT_CC) {
            int pressed = event.buffer[2] >= 64;
            int was_down = atomic_exchange(&button_down, pressed);
            int next_value;

            if (!pressed || was_down)
                continue;
            next_value = atomic_load(&mute_value) == 0 ? 127 : 0;
            atomic_store(&mute_value, next_value);
            atomic_fetch_add(&generation, 1);

            message[0] = 0xb0 | MIDI_CHANNEL;
            message[1] = MUTE_OUTPUT_CC;
            message[2] = (jack_midi_data_t)next_value;
            (void)jack_midi_event_write(output_buffer, event.time, message, 3);
        }
    }

    {
        jack_default_audio_sample_t *input_left;
        jack_default_audio_sample_t *input_right;
        jack_default_audio_sample_t *output_left;
        jack_default_audio_sample_t *output_right;
        const float target_gain = atomic_load(&mute_value) == 0 ? 1.0f : 0.0f;
        jack_nframes_t frame;

        input_left = jack_port_get_buffer(audio_input_left_port, frame_count);
        input_right = jack_port_get_buffer(audio_input_right_port, frame_count);
        output_left = jack_port_get_buffer(audio_output_left_port, frame_count);
        output_right = jack_port_get_buffer(audio_output_right_port, frame_count);

        for (frame = 0; frame < frame_count; ++frame) {
            if (audio_gain < target_gain) {
                audio_gain += audio_ramp_step;
                if (audio_gain > target_gain)
                    audio_gain = target_gain;
            } else if (audio_gain > target_gain) {
                audio_gain -= audio_ramp_step;
                if (audio_gain < target_gain)
                    audio_gain = target_gain;
            }
            output_left[frame] = input_left[frame] * audio_gain;
            output_right[frame] = input_right[frame] * audio_gain;
        }
    }

    return 0;
}

static void stop_running(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void jack_shutdown(void *unused)
{
    (void)unused;
    running = 0;
}

static void load_state(const char *path, int fallback, int *volume, int *mute)
{
    FILE *stream;
    int parsed_volume;
    int parsed_mute = 0;

    stream = fopen(path, "r");
    if (stream == NULL) {
        *volume = fallback;
        *mute = 0;
        return;
    }
    if (fscanf(stream, "%d %d", &parsed_volume, &parsed_mute) < 1)
        parsed_volume = fallback;
    fclose(stream);
    *volume = clamp_midi_value(parsed_volume);
    *mute = parsed_mute >= 64 ? 127 : 0;
}

static int save_state(const char *path, int volume, int mute)
{
    char temporary[PATH_MAX];
    FILE *stream;

    if (snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid())
        >= (int)sizeof(temporary)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    stream = fopen(temporary, "w");
    if (stream == NULL)
        return -1;
    if (fprintf(stream, "%d %d\n", volume, mute) < 0 || fflush(stream) != 0
        || fsync(fileno(stream)) != 0) {
        int saved_errno = errno;
        (void)fclose(stream);
        (void)unlink(temporary);
        errno = saved_errno;
        return -1;
    }
    if (fclose(stream) != 0) {
        int saved_errno = errno;
        (void)unlink(temporary);
        errno = saved_errno;
        return -1;
    }
    if (rename(temporary, path) != 0) {
        int saved_errno = errno;
        (void)unlink(temporary);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *state_path;
    jack_client_t *client;
    jack_status_t status = 0;
    unsigned int saved_generation = 0;
    int initial_value;
    int initial_mute;
    const struct timespec delay = { .tv_sec = 0, .tv_nsec = 250000000L };

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s STATE_FILE [INITIAL_VALUE]\n", argv[0]);
        return 2;
    }
    state_path = argv[1];
    initial_value = argc == 3 ? atoi(argv[2]) : 3;
    load_state(
        state_path, clamp_midi_value(initial_value), &initial_value, &initial_mute
    );
    atomic_init(&volume_value, initial_value);
    atomic_init(&mute_value, initial_mute);
    atomic_init(&button_down, 0);
    atomic_init(&generation, 0);

    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);

    client = jack_client_open(
        "Arturia Main Volume Encoder", JACK_NO_START_SERVER, &status
    );
    if (client == NULL) {
        fprintf(stderr, "Could not connect to JACK/PipeWire (status 0x%x)\n", status);
        return 1;
    }
    {
        const jack_nframes_t sample_rate = jack_get_sample_rate(client);
        audio_gain = initial_mute == 0 ? 1.0f : 0.0f;
        audio_ramp_step = sample_rate > 0 ? 1.0f / (0.01f * sample_rate) : 1.0f;
    }
    input_port = jack_port_register(
        client, "relative-in", JACK_MIDI_TYPE, JACK_PORT_IS_INPUT, 0
    );
    output_port = jack_port_register(
        client, "absolute-out", JACK_MIDI_TYPE, JACK_PORT_IS_OUTPUT, 0
    );
    audio_input_left_port = jack_port_register(
        client, "audio-in-l", JACK_AUDIO_TYPE, JACK_PORT_IS_INPUT, 0
    );
    audio_input_right_port = jack_port_register(
        client, "audio-in-r", JACK_AUDIO_TYPE, JACK_PORT_IS_INPUT, 0
    );
    audio_output_left_port = jack_port_register(
        client, "audio-out-l", JACK_AUDIO_TYPE, JACK_PORT_IS_OUTPUT, 0
    );
    audio_output_right_port = jack_port_register(
        client, "audio-out-r", JACK_AUDIO_TYPE, JACK_PORT_IS_OUTPUT, 0
    );
    if (input_port == NULL || output_port == NULL
        || audio_input_left_port == NULL || audio_input_right_port == NULL
        || audio_output_left_port == NULL || audio_output_right_port == NULL) {
        fprintf(stderr, "Could not register MIDI/audio ports\n");
        jack_client_close(client);
        return 1;
    }
    if (jack_set_process_callback(client, process_midi, NULL) != 0) {
        fprintf(stderr, "Could not register JACK process callback\n");
        jack_client_close(client);
        return 1;
    }
    jack_on_shutdown(client, jack_shutdown, NULL);
    if (jack_activate(client) != 0) {
        fprintf(stderr, "Could not activate JACK client\n");
        jack_client_close(client);
        return 1;
    }

    printf(
        "Converting channel 1 CC%d to volume CC%d and button CC%d to mute CC%d; "
        "initial volume=%d mute=%d; stereo output gate enabled\n",
        VOLUME_INPUT_CC, VOLUME_OUTPUT_CC, BUTTON_INPUT_CC, MUTE_OUTPUT_CC,
        atomic_load(&volume_value), atomic_load(&mute_value)
    );
    fflush(stdout);

    while (running) {
        unsigned int current_generation;

        (void)nanosleep(&delay, NULL);
        current_generation = atomic_load(&generation);
        if (current_generation == saved_generation)
            continue;
        if (save_state(
                state_path, atomic_load(&volume_value), atomic_load(&mute_value)
            ) != 0)
            perror("Could not persist controller state");
        else
            saved_generation = current_generation;
    }

    (void)save_state(
        state_path, atomic_load(&volume_value), atomic_load(&mute_value)
    );
    jack_client_close(client);
    return 0;
}
