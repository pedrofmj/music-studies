#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Keep this utility buildable on hosts that have libjack but not JACK headers. */
typedef uint32_t jack_nframes_t;
typedef uint32_t jack_options_t;
typedef uint32_t jack_status_t;
typedef struct _jack_client jack_client_t;
typedef struct _jack_port jack_port_t;
typedef unsigned char jack_midi_data_t;
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
extern int jack_set_process_callback(
    jack_client_t *, int (*)(jack_nframes_t, void *), void *
);
extern void jack_on_shutdown(jack_client_t *, void (*)(void *), void *);
extern jack_port_t *jack_port_register(
    jack_client_t *, const char *, const char *, unsigned long, unsigned long
);
extern void *jack_port_get_buffer(jack_port_t *, jack_nframes_t);
extern int jack_port_connected(const jack_port_t *);
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
    LAYER_COUNT = 8,
    MIDI_NOTE_COUNT = 128,
    MAX_CHORD_NOTES = 16,
    MAX_TEST_EVENTS = 1024,
};

static const char *const JACK_MIDI_TYPE = "8 bit raw midi";

typedef enum {
    CONTROL_NONE,
    CONTROL_CC,
    CONTROL_NOTE,
} control_type_t;

typedef enum {
    PAD_VALUE,
    PAD_TOGGLE,
} pad_behavior_t;

typedef struct {
    control_type_t type;
    int number;
} control_spec_t;

typedef struct {
    int channel;
    control_type_t pad_type;
    pad_behavior_t pad_behavior;
    int pad_on_minimum;
    int pads[LAYER_COUNT];
    int pad_channels[LAYER_COUNT];
    int knobs[LAYER_COUNT];
    int knob_channels[LAYER_COUNT];
    control_spec_t play;
    control_spec_t stop;
    int default_chord[MAX_CHORD_NOTES];
    int default_chord_count;
} configuration_t;

typedef struct {
    atomic_bool enabled;
    atomic_bool paused;
    atomic_uchar last_velocity[MIDI_NOTE_COUNT];
    unsigned char active_velocity[MIDI_NOTE_COUNT];
    bool pad_down;
} layer_state_t;

typedef void (*emit_function_t)(
    void *, int, jack_nframes_t, const unsigned char *, size_t
);

typedef struct {
    void *buffers[LAYER_COUNT];
} jack_output_context_t;

typedef struct {
    int layer;
    size_t size;
    unsigned char data[8];
} test_event_t;

typedef struct {
    test_event_t events[MAX_TEST_EVENTS];
    size_t count;
} test_output_context_t;

static configuration_t configuration;
static layer_state_t layers[LAYER_COUNT];
static bool physical_notes[MIDI_NOTE_COUNT];
static int physical_note_count;
static jack_port_t *input_port;
static jack_port_t *output_ports[LAYER_COUNT];
static atomic_uchar knob_values[LAYER_COUNT];
static int output_connection_counts[LAYER_COUNT];
static atomic_uint state_generation;
static atomic_bool panic_requested;
static atomic_bool panic_complete;
static volatile sig_atomic_t running = 1;

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        ++text;
    if (*text == '\0')
        return text;
    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end))
        --end;
    end[1] = '\0';
    return text;
}

static int parse_number(const char *text, int minimum, int maximum, int *value)
{
    char *end;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *trim(end) != '\0'
        || parsed < minimum || parsed > maximum)
        return -1;
    *value = (int)parsed;
    return 0;
}

static int parse_csv(
    const char *text, int *values, int expected, int minimum, int maximum
)
{
    char buffer[512];
    char *save = NULL;
    char *token;
    int count = 0;

    if (strlen(text) >= sizeof(buffer))
        return -1;
    strcpy(buffer, text);
    token = strtok_r(buffer, ",", &save);
    while (token != NULL) {
        if (count >= expected
            || parse_number(trim(token), minimum, maximum, &values[count]) != 0)
            return -1;
        ++count;
        token = strtok_r(NULL, ",", &save);
    }
    return count == expected ? 0 : -1;
}

static int parse_control(const char *text, control_spec_t *control)
{
    const char *number;

    if (strcmp(text, "none") == 0) {
        control->type = CONTROL_NONE;
        control->number = -1;
        return 0;
    }
    if (strncmp(text, "cc:", 3) == 0) {
        control->type = CONTROL_CC;
        number = text + 3;
    } else if (strncmp(text, "note:", 5) == 0) {
        control->type = CONTROL_NOTE;
        number = text + 5;
    } else {
        return -1;
    }
    return parse_number(number, 0, 127, &control->number);
}

static void initialize_configuration(void)
{
    int index;

    memset(&configuration, 0, sizeof(configuration));
    configuration.channel = 0;
    configuration.pad_type = CONTROL_NONE;
    configuration.pad_behavior = PAD_VALUE;
    configuration.pad_on_minimum = 64;
    configuration.play.type = CONTROL_NONE;
    configuration.play.number = -1;
    configuration.stop.type = CONTROL_NONE;
    configuration.stop.number = -1;
    for (index = 0; index < LAYER_COUNT; ++index) {
        configuration.pads[index] = -1;
        configuration.pad_channels[index] = -1;
        configuration.knobs[index] = -1;
        configuration.knob_channels[index] = -1;
    }
    configuration.default_chord[0] = 48;
    configuration.default_chord[1] = 52;
    configuration.default_chord[2] = 55;
    configuration.default_chord_count = 3;
}

static int validate_unique(const int *values, int count)
{
    int left;
    int right;

    for (left = 0; left < count; ++left)
        for (right = left + 1; right < count; ++right)
            if (values[left] == values[right])
                return -1;
    return 0;
}

static int validate_configuration(void)
{
    int pad;
    int knob;

    for (pad = 0; pad < LAYER_COUNT; ++pad) {
        if (configuration.pad_channels[pad] < 0)
            configuration.pad_channels[pad] = configuration.channel;
        if (configuration.knob_channels[pad] < 0)
            configuration.knob_channels[pad] = configuration.channel;
    }

    if (configuration.pad_type == CONTROL_NONE
        || validate_unique(configuration.pads, LAYER_COUNT) != 0
        || validate_unique(configuration.knobs, LAYER_COUNT) != 0) {
        fprintf(stderr, "Pad and knob mappings must contain eight unique values\n");
        return -1;
    }
    if (configuration.pad_type == CONTROL_CC) {
        for (pad = 0; pad < LAYER_COUNT; ++pad)
            for (knob = 0; knob < LAYER_COUNT; ++knob)
                if (configuration.pads[pad] == configuration.knobs[knob]
                    && configuration.pad_channels[pad]
                        == configuration.knob_channels[knob]) {
                    fprintf(stderr, "Pad and knob CC mappings overlap\n");
                    return -1;
                }
    }
    return 0;
}

static int load_configuration(const char *path)
{
    FILE *stream;
    char line[1024];
    int line_number = 0;

    initialize_configuration();
    stream = fopen(path, "r");
    if (stream == NULL) {
        perror("Could not open SMK-25 mapping configuration");
        return -1;
    }
    while (fgets(line, sizeof(line), stream) != NULL) {
        char *key;
        char *value;
        char *equals;

        ++line_number;
        key = trim(line);
        if (*key == '\0' || *key == '#')
            continue;
        equals = strchr(key, '=');
        if (equals == NULL) {
            fprintf(stderr, "%s:%d: expected KEY=VALUE\n", path, line_number);
            fclose(stream);
            return -1;
        }
        *equals = '\0';
        value = trim(equals + 1);
        key = trim(key);
        if (strcmp(key, "midi_channel") == 0) {
            if (parse_number(value, 1, 16, &configuration.channel) != 0)
                goto invalid;
            --configuration.channel;
        } else if (strcmp(key, "pad_type") == 0) {
            if (strcmp(value, "cc") == 0)
                configuration.pad_type = CONTROL_CC;
            else if (strcmp(value, "note") == 0)
                configuration.pad_type = CONTROL_NOTE;
            else
                goto invalid;
        } else if (strcmp(key, "pad_behavior") == 0) {
            if (strcmp(value, "value") == 0)
                configuration.pad_behavior = PAD_VALUE;
            else if (strcmp(value, "toggle") == 0)
                configuration.pad_behavior = PAD_TOGGLE;
            else
                goto invalid;
        } else if (strcmp(key, "pad_on_minimum") == 0) {
            if (parse_number(value, 1, 127, &configuration.pad_on_minimum) != 0)
                goto invalid;
        } else if (strcmp(key, "pads") == 0) {
            if (parse_csv(value, configuration.pads, LAYER_COUNT, 0, 127) != 0)
                goto invalid;
        } else if (strcmp(key, "pad_channels") == 0) {
            int index;
            if (parse_csv(
                    value, configuration.pad_channels, LAYER_COUNT, 1, 16
                ) != 0)
                goto invalid;
            for (index = 0; index < LAYER_COUNT; ++index)
                --configuration.pad_channels[index];
        } else if (strcmp(key, "knobs") == 0) {
            if (parse_csv(value, configuration.knobs, LAYER_COUNT, 0, 127) != 0)
                goto invalid;
        } else if (strcmp(key, "knob_channels") == 0) {
            int index;
            if (parse_csv(
                    value, configuration.knob_channels, LAYER_COUNT, 1, 16
                ) != 0)
                goto invalid;
            for (index = 0; index < LAYER_COUNT; ++index)
                --configuration.knob_channels[index];
        } else if (strcmp(key, "play") == 0) {
            if (parse_control(value, &configuration.play) != 0)
                goto invalid;
        } else if (strcmp(key, "stop") == 0) {
            if (parse_control(value, &configuration.stop) != 0)
                goto invalid;
        } else if (strcmp(key, "default_chord") == 0) {
            int chord[MAX_CHORD_NOTES];
            int count;
            char buffer[512];
            char *save = NULL;
            char *token;

            if (strlen(value) >= sizeof(buffer))
                goto invalid;
            strcpy(buffer, value);
            count = 0;
            token = strtok_r(buffer, ",", &save);
            while (token != NULL) {
                if (count >= MAX_CHORD_NOTES
                    || parse_number(trim(token), 0, 127, &chord[count]) != 0)
                    goto invalid;
                ++count;
                token = strtok_r(NULL, ",", &save);
            }
            if (count == 0)
                goto invalid;
            memcpy(configuration.default_chord, chord, sizeof(int) * count);
            configuration.default_chord_count = count;
        } else {
            fprintf(stderr, "%s:%d: unknown key '%s'\n", path, line_number, key);
            fclose(stream);
            return -1;
        }
        continue;
invalid:
        fprintf(stderr, "%s:%d: invalid value for %s\n", path, line_number, key);
        fclose(stream);
        return -1;
    }
    if (ferror(stream)) {
        perror("Could not read SMK-25 mapping configuration");
        fclose(stream);
        return -1;
    }
    fclose(stream);
    return validate_configuration();
}

static void initialize_state(void)
{
    int layer;
    int note;

    memset(physical_notes, 0, sizeof(physical_notes));
    physical_note_count = 0;
    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        atomic_init(&layers[layer].enabled, false);
        atomic_init(&layers[layer].paused, false);
        atomic_init(&knob_values[layer], 0);
        output_connection_counts[layer] = -1;
        layers[layer].pad_down = false;
        memset(layers[layer].active_velocity, 0, MIDI_NOTE_COUNT);
        for (note = 0; note < MIDI_NOTE_COUNT; ++note)
            atomic_init(&layers[layer].last_velocity[note], 0);
    }
    atomic_init(&state_generation, 0);
    atomic_init(&panic_requested, false);
    atomic_init(&panic_complete, false);
}

static void clear_last_chord(int layer)
{
    int note;

    for (note = 0; note < MIDI_NOTE_COUNT; ++note)
        atomic_store(&layers[layer].last_velocity[note], 0);
}

static bool has_last_chord(int layer)
{
    int note;

    for (note = 0; note < MIDI_NOTE_COUNT; ++note)
        if (atomic_load(&layers[layer].last_velocity[note]) != 0)
            return true;
    return false;
}

static void emit_message(
    emit_function_t emit, void *context, int layer, jack_nframes_t frame,
    unsigned char status, unsigned char data1, unsigned char data2
)
{
    const unsigned char message[3] = { status, data1, data2 };

    emit(context, layer, frame, message, sizeof(message));
}

static void release_active(
    int layer, jack_nframes_t frame, emit_function_t emit, void *context
)
{
    int note;

    for (note = 0; note < MIDI_NOTE_COUNT; ++note) {
        if (layers[layer].active_velocity[note] == 0)
            continue;
        emit_message(
            emit, context, layer, frame,
            (unsigned char)(0x80 | configuration.channel),
            (unsigned char)note, 0
        );
        layers[layer].active_velocity[note] = 0;
    }
}

static void emit_all_notes_off(
    int layer, jack_nframes_t frame, emit_function_t emit, void *context
)
{
    emit_message(
        emit, context, layer, frame,
        (unsigned char)(0xb0 | configuration.channel), 123, 0
    );
}

static void start_last_chord(
    int layer, jack_nframes_t frame, emit_function_t emit, void *context
)
{
    int note;

    if (!has_last_chord(layer)) {
        int index;
        for (index = 0; index < configuration.default_chord_count; ++index)
            atomic_store(
                &layers[layer].last_velocity[configuration.default_chord[index]],
                96
            );
    }
    for (note = 0; note < MIDI_NOTE_COUNT; ++note) {
        const unsigned char velocity =
            atomic_load(&layers[layer].last_velocity[note]);
        if (velocity == 0)
            continue;
        emit_message(
            emit, context, layer, frame,
            (unsigned char)(0x90 | configuration.channel),
            (unsigned char)note, velocity
        );
        layers[layer].active_velocity[note] = velocity;
    }
    atomic_store(&layers[layer].paused, false);
}

static void set_layer_enabled(
    int layer, bool enabled, jack_nframes_t frame,
    emit_function_t emit, void *context
)
{
    const bool previous = atomic_load(&layers[layer].enabled);

    if (previous == enabled)
        return;
    release_active(layer, frame, emit, context);
    clear_last_chord(layer);
    atomic_store(&layers[layer].enabled, enabled);
    atomic_store(&layers[layer].paused, false);
    atomic_fetch_add(&state_generation, 1);
}

static void stop_layers(
    jack_nframes_t frame, emit_function_t emit, void *context
)
{
    int layer;

    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        release_active(layer, frame, emit, context);
        emit_all_notes_off(layer, frame, emit, context);
        if (atomic_load(&layers[layer].enabled))
            atomic_store(&layers[layer].paused, true);
    }
    atomic_fetch_add(&state_generation, 1);
}

static void play_layers(
    jack_nframes_t frame, emit_function_t emit, void *context
)
{
    int layer;

    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        if (!atomic_load(&layers[layer].enabled))
            continue;
        release_active(layer, frame, emit, context);
        start_last_chord(layer, frame, emit, context);
    }
    atomic_fetch_add(&state_generation, 1);
}

static bool is_control_message(
    const control_spec_t *control, const unsigned char *message, size_t size
)
{
    const int type = size > 0 ? message[0] & 0xf0 : -1;
    const int channel = size > 0 ? message[0] & 0x0f : -1;

    if (control->type == CONTROL_NONE || size != 3
        || channel != configuration.channel
        || message[1] != control->number)
        return false;
    if (control->type == CONTROL_CC)
        return type == 0xb0;
    return type == 0x80 || type == 0x90;
}

static bool matches_control(
    const control_spec_t *control, const unsigned char *message, size_t size
)
{
    const int type = size > 0 ? message[0] & 0xf0 : -1;

    if (!is_control_message(control, message, size))
        return false;
    if (control->type == CONTROL_CC)
        return message[2] >= 64;
    return type == 0x90 && message[2] > 0;
}

static bool handle_standard_transport(
    const unsigned char *message, size_t size, jack_nframes_t frame,
    emit_function_t emit, void *context
)
{
    if (size == 1 && message[0] == 0xfc) {
        stop_layers(frame, emit, context);
        return true;
    }
    if (size == 1 && (message[0] == 0xfa || message[0] == 0xfb)) {
        play_layers(frame, emit, context);
        return true;
    }
    if (size >= 6 && message[0] == 0xf0 && message[1] == 0x7f
        && message[3] == 0x06 && message[size - 1] == 0xf7) {
        if (message[4] == 0x01) {
            stop_layers(frame, emit, context);
            return true;
        }
        if (message[4] == 0x02 || message[4] == 0x03) {
            play_layers(frame, emit, context);
            return true;
        }
    }
    return false;
}

static int matching_pad(const unsigned char *message, size_t size)
{
    const int type = size > 0 ? message[0] & 0xf0 : -1;
    const int channel = size > 0 ? message[0] & 0x0f : -1;
    int index;

    if (size != 3)
        return -1;
    if (configuration.pad_type == CONTROL_CC && type != 0xb0)
        return -1;
    if (configuration.pad_type == CONTROL_NOTE
        && type != 0x80 && type != 0x90)
        return -1;
    for (index = 0; index < LAYER_COUNT; ++index)
        if (message[1] == configuration.pads[index]
            && channel == configuration.pad_channels[index])
            return index;
    return -1;
}

static bool handle_pad(
    const unsigned char *message, size_t size, jack_nframes_t frame,
    emit_function_t emit, void *context
)
{
    const int layer = matching_pad(message, size);
    bool pressed;

    if (layer < 0)
        return false;
    pressed = (message[0] & 0xf0) == 0x90
        ? message[2] > 0
        : message[2] >= configuration.pad_on_minimum;
    if (configuration.pad_behavior == PAD_VALUE) {
        set_layer_enabled(layer, pressed, frame, emit, context);
    } else {
        const bool was_down = layers[layer].pad_down;
        layers[layer].pad_down = pressed;
        if (pressed && !was_down)
            set_layer_enabled(
                layer, !atomic_load(&layers[layer].enabled),
                frame, emit, context
            );
    }
    return true;
}

static int matching_knob(const unsigned char *message, size_t size)
{
    int index;

    if (size != 3 || (message[0] & 0xf0) != 0xb0)
        return -1;
    for (index = 0; index < LAYER_COUNT; ++index)
        if (message[1] == configuration.knobs[index]
            && (message[0] & 0x0f) == configuration.knob_channels[index])
            return index;
    return -1;
}

static void handle_note(
    const unsigned char *message, jack_nframes_t frame,
    emit_function_t emit, void *context
)
{
    const int type = message[0] & 0xf0;
    const int note = message[1];
    const int velocity = message[2];
    const bool note_on = type == 0x90 && velocity > 0;
    int layer;

    if (note_on) {
        const bool new_gesture = physical_note_count == 0;
        if (!physical_notes[note]) {
            physical_notes[note] = true;
            ++physical_note_count;
        }
        if (new_gesture) {
            for (layer = 0; layer < LAYER_COUNT; ++layer) {
                if (!atomic_load(&layers[layer].enabled))
                    continue;
                release_active(layer, frame, emit, context);
                clear_last_chord(layer);
                atomic_store(&layers[layer].paused, false);
            }
        }
        for (layer = 0; layer < LAYER_COUNT; ++layer) {
            emit(context, layer, frame, message, 3);
            layers[layer].active_velocity[note] = (unsigned char)velocity;
            if (atomic_load(&layers[layer].enabled))
                atomic_store(
                    &layers[layer].last_velocity[note],
                    (unsigned char)velocity
                );
        }
        atomic_fetch_add(&state_generation, 1);
        return;
    }

    if (physical_notes[note]) {
        physical_notes[note] = false;
        if (physical_note_count > 0)
            --physical_note_count;
    }
    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        if (atomic_load(&layers[layer].enabled))
            continue;
        emit(context, layer, frame, message, 3);
        layers[layer].active_velocity[note] = 0;
    }
}

static void process_event(
    const unsigned char *message, size_t size, jack_nframes_t frame,
    emit_function_t emit, void *context
)
{
    const int type = size > 0 ? message[0] & 0xf0 : -1;
    const int channel = size > 0 ? message[0] & 0x0f : -1;
    int knob;
    int layer;

    if (handle_standard_transport(message, size, frame, emit, context))
        return;
    if (is_control_message(&configuration.stop, message, size)) {
        if (matches_control(&configuration.stop, message, size))
            stop_layers(frame, emit, context);
        return;
    }
    if (is_control_message(&configuration.play, message, size)) {
        if (matches_control(&configuration.play, message, size))
            play_layers(frame, emit, context);
        return;
    }
    if (handle_pad(message, size, frame, emit, context))
        return;
    knob = matching_knob(message, size);
    if (knob >= 0) {
        const unsigned char volume[] = {
            (unsigned char)(0xb0 | configuration.knob_channels[knob]),
            (unsigned char)configuration.knobs[knob],
            message[2],
        };
        atomic_store(&knob_values[knob], message[2]);
        atomic_fetch_add(&state_generation, 1);
        emit(context, knob, frame, volume, sizeof(volume));
        return;
    }
    if (size == 3 && channel == configuration.channel
        && (type == 0x80 || type == 0x90)) {
        handle_note(message, frame, emit, context);
        return;
    }
    for (layer = 0; layer < LAYER_COUNT; ++layer)
        emit(context, layer, frame, message, size);
}

static void jack_emit(
    void *opaque, int layer, jack_nframes_t frame,
    const unsigned char *message, size_t size
)
{
    jack_output_context_t *context = opaque;

    (void)jack_midi_event_write(context->buffers[layer], frame, message, size);
}

static int process_midi(jack_nframes_t frame_count, void *unused)
{
    jack_output_context_t context;
    void *input_buffer;
    uint32_t event_count;
    uint32_t index;
    int layer;

    (void)unused;
    input_buffer = jack_port_get_buffer(input_port, frame_count);
    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        context.buffers[layer] =
            jack_port_get_buffer(output_ports[layer], frame_count);
        jack_midi_clear_buffer(context.buffers[layer]);
    }
    if (atomic_exchange(&panic_requested, false)) {
        stop_layers(0, jack_emit, &context);
        atomic_store(&panic_complete, true);
        return 0;
    }
    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        const int connections = jack_port_connected(output_ports[layer]);

        if (connections > 0 && connections > output_connection_counts[layer]) {
            const unsigned char volume[] = {
                (unsigned char)(0xb0 | configuration.knob_channels[layer]),
                (unsigned char)configuration.knobs[layer],
                atomic_load(&knob_values[layer]),
            };
            (void)jack_midi_event_write(
                context.buffers[layer], 0, volume, sizeof(volume)
            );
        }
        output_connection_counts[layer] = connections;
    }
    event_count = jack_midi_get_event_count(input_buffer);
    for (index = 0; index < event_count; ++index) {
        jack_midi_event_t event;

        if (jack_midi_event_get(&event, input_buffer, index) != 0)
            continue;
        process_event(event.buffer, event.size, event.time, jack_emit, &context);
    }
    return 0;
}

static void stop_running(int signal_number)
{
    (void)signal_number;
    atomic_store(&panic_requested, true);
    running = 0;
}

static void jack_shutdown(void *unused)
{
    (void)unused;
    running = 0;
}

static int save_state(const char *path)
{
    char temporary[PATH_MAX];
    FILE *stream;
    int layer;

    if (snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid())
        >= (int)sizeof(temporary)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    stream = fopen(temporary, "w");
    if (stream == NULL)
        return -1;
    if (fprintf(stream, "SMK25_PAD_LAYERS 1\n") < 0)
        goto failed;
    if (fprintf(stream, "KNOBS") < 0)
        goto failed;
    for (layer = 0; layer < LAYER_COUNT; ++layer)
        if (fprintf(stream, " %u", atomic_load(&knob_values[layer])) < 0)
            goto failed;
    if (fputc('\n', stream) == EOF)
        goto failed;
    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        int note;
        if (fprintf(
                stream, "LAYER %d %d %d", layer + 1,
                atomic_load(&layers[layer].enabled) ? 1 : 0,
                atomic_load(&layers[layer].paused) ? 1 : 0
            ) < 0)
            goto failed;
        for (note = 0; note < MIDI_NOTE_COUNT; ++note) {
            const unsigned int velocity =
                atomic_load(&layers[layer].last_velocity[note]);
            if (velocity != 0
                && fprintf(stream, " %d:%u", note, velocity) < 0)
                goto failed;
        }
        if (fputc('\n', stream) == EOF)
            goto failed;
    }
    if (fflush(stream) != 0 || fsync(fileno(stream)) != 0)
        goto failed;
    if (fclose(stream) != 0)
        goto close_failed;
    if (rename(temporary, path) != 0)
        goto close_failed;
    return 0;
failed:
    {
        const int saved_errno = errno;
        (void)fclose(stream);
        (void)unlink(temporary);
        errno = saved_errno;
    }
    return -1;
close_failed:
    {
        const int saved_errno = errno;
        (void)unlink(temporary);
        errno = saved_errno;
    }
    return -1;
}

static void load_state(const char *path)
{
    FILE *stream;
    char line[2048];

    stream = fopen(path, "r");
    if (stream == NULL)
        return;
    while (fgets(line, sizeof(line), stream) != NULL) {
        int layer_number;
        int enabled;
        int paused;
        int consumed;
        char *cursor;

        if (strncmp(line, "KNOBS ", 6) == 0) {
            int values[LAYER_COUNT];
            if (sscanf(
                    line, "KNOBS %d %d %d %d %d %d %d %d",
                    &values[0], &values[1], &values[2], &values[3],
                    &values[4], &values[5], &values[6], &values[7]
                ) == LAYER_COUNT) {
                int index;
                for (index = 0; index < LAYER_COUNT; ++index)
                    if (values[index] >= 0 && values[index] <= 127)
                        atomic_store(&knob_values[index], values[index]);
            }
            continue;
        }

        if (sscanf(
                line, "LAYER %d %d %d%n",
                &layer_number, &enabled, &paused, &consumed
            ) != 3
            || layer_number < 1 || layer_number > LAYER_COUNT)
            continue;
        atomic_store(&layers[layer_number - 1].enabled, enabled != 0);
        atomic_store(
            &layers[layer_number - 1].paused,
            enabled != 0 ? true : paused != 0
        );
        cursor = line + consumed;
        while (*cursor != '\0') {
            int note;
            int velocity;
            while (isspace((unsigned char)*cursor))
                ++cursor;
            if (sscanf(cursor, "%d:%d%n", &note, &velocity, &consumed) != 2)
                break;
            if (note >= 0 && note < MIDI_NOTE_COUNT
                && velocity > 0 && velocity <= 127)
                atomic_store(
                    &layers[layer_number - 1].last_velocity[note],
                    (unsigned char)velocity
                );
            cursor += consumed;
        }
    }
    fclose(stream);
}

static void test_emit(
    void *opaque, int layer, jack_nframes_t frame,
    const unsigned char *message, size_t size
)
{
    test_output_context_t *context = opaque;
    test_event_t *event;

    (void)frame;
    if (context->count >= MAX_TEST_EVENTS || size > sizeof(event->data))
        return;
    event = &context->events[context->count++];
    event->layer = layer;
    event->size = size;
    memcpy(event->data, message, size);
}

static int self_test(void)
{
    test_output_context_t output = { 0 };
    unsigned char event[3];
    int index;

    initialize_configuration();
    configuration.pad_type = CONTROL_CC;
    configuration.pad_behavior = PAD_VALUE;
    configuration.play.type = CONTROL_NOTE;
    configuration.play.number = 94;
    configuration.stop.type = CONTROL_NOTE;
    configuration.stop.number = 93;
    for (index = 0; index < LAYER_COUNT; ++index) {
        configuration.pads[index] = index < 4 ? 40 + index : 32 + index;
        configuration.pad_channels[index] = index;
        configuration.knobs[index] = 20 + index;
        configuration.knob_channels[index] = 0;
    }
    initialize_state();

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Self-test failed: %s\n", message); \
        return 1; \
    } \
} while (0)

    event[0] = 0xb0;
    event[1] = 23;
    event[2] = 0;
    process_event(event, 3, 0, test_emit, &output);
    CHECK(output.count == 1, "Knob did not produce one volume event");
    CHECK(output.events[0].layer == 3, "Knob 4 reached the wrong layer");
    CHECK(atomic_load(&knob_values[3]) == 0, "Knob state was not stored");
    CHECK(
        output.events[0].data[1] == 23 && output.events[0].data[2] == 0,
        "Knob CC and zero value were not preserved"
    );
    output.count = 0;

    event[0] = 0xb0;
    event[1] = 40;
    event[2] = 127;
    process_event(event, 3, 0, test_emit, &output);
    CHECK(atomic_load(&layers[0].enabled), "Pad 1 did not enable layer 1");

    event[0] = 0x90;
    event[1] = 60;
    event[2] = 100;
    process_event(event, 3, 1, test_emit, &output);
    CHECK(layers[0].active_velocity[60] == 100, "Latched note did not start");
    CHECK(
        atomic_load(&layers[0].last_velocity[60]) == 100,
        "Latched note was not remembered"
    );

    event[0] = 0x80;
    event[2] = 0;
    process_event(event, 3, 2, test_emit, &output);
    CHECK(layers[0].active_velocity[60] == 100, "Note release broke latch");

    event[0] = 0x90;
    event[1] = 64;
    event[2] = 90;
    process_event(event, 3, 3, test_emit, &output);
    CHECK(layers[0].active_velocity[60] == 0, "New gesture did not clear chord");
    CHECK(layers[0].active_velocity[64] == 90, "New gesture did not latch");
    event[0] = 0x80;
    event[2] = 0;
    process_event(event, 3, 4, test_emit, &output);

    event[0] = 0x90;
    event[1] = 93;
    event[2] = 127;
    process_event(event, 3, 5, test_emit, &output);
    CHECK(layers[0].active_velocity[64] == 0, "Stop did not release chord");
    CHECK(atomic_load(&layers[0].paused), "Stop did not pause enabled layer");

    {
        const size_t before_release = output.count;
        event[0] = 0x80;
        event[2] = 64;
        process_event(event, 3, 6, test_emit, &output);
        CHECK(
            output.count == before_release,
            "Transport Note Off leaked into instrument layers"
        );
    }

    event[0] = 0x90;
    event[1] = 94;
    process_event(event, 3, 6, test_emit, &output);
    CHECK(layers[0].active_velocity[64] == 90, "Play did not resume chord");

    event[0] = 0xb0;
    event[1] = 40;
    event[2] = 0;
    process_event(event, 3, 7, test_emit, &output);
    CHECK(!atomic_load(&layers[0].enabled), "Pad off did not disable layer");
    CHECK(!has_last_chord(0), "Pad off did not clear remembered chord");

    event[0] = 0xb0;
    event[1] = 41;
    event[2] = 127;
    process_event(event, 3, 8, test_emit, &output);
    CHECK(!atomic_load(&layers[1].enabled), "Pad 2 matched the wrong channel");
    event[0] = 0xb1;
    process_event(event, 3, 9, test_emit, &output);
    CHECK(atomic_load(&layers[1].enabled), "Pad 2 did not match channel 2");
    event[0] = 0x90;
    event[1] = 94;
    process_event(event, 3, 10, test_emit, &output);
    CHECK(layers[1].active_velocity[48] == 96, "Play did not seed default C");
    CHECK(layers[1].active_velocity[52] == 96, "Default C lacks E");
    CHECK(layers[1].active_velocity[55] == 96, "Default C lacks G");
    CHECK(output.count > 0, "No MIDI output was produced");

#undef CHECK
    puts("Self-test: OK");
    return 0;
}

int main(int argc, char **argv)
{
    const char *configuration_path;
    const char *state_path;
    jack_client_t *client;
    jack_status_t status = 0;
    unsigned int saved_generation = 0;
    const struct timespec delay = { .tv_sec = 0, .tv_nsec = 250000000L };
    int layer;

    if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
        return self_test();
    if (argc == 3 && strcmp(argv[1], "--check-config") == 0) {
        if (load_configuration(argv[2]) != 0)
            return 2;
        puts("Configuration: OK");
        return 0;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: %s CONFIG_FILE STATE_FILE\n", argv[0]);
        fprintf(stderr, "       %s --self-test\n", argv[0]);
        fprintf(stderr, "       %s --check-config CONFIG_FILE\n", argv[0]);
        return 2;
    }
    configuration_path = argv[1];
    state_path = argv[2];
    if (load_configuration(configuration_path) != 0)
        return 2;
    initialize_state();
    load_state(state_path);

    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);

    client = jack_client_open(
        "SMK25 Pad Layers", JACK_NO_START_SERVER, &status
    );
    if (client == NULL) {
        fprintf(stderr, "Could not connect to JACK/PipeWire (status 0x%x)\n", status);
        return 1;
    }
    input_port = jack_port_register(
        client, "midi-in", JACK_MIDI_TYPE, JACK_PORT_IS_INPUT, 0
    );
    for (layer = 0; layer < LAYER_COUNT; ++layer) {
        char name[32];
        snprintf(name, sizeof(name), "layer-%d", layer + 1);
        output_ports[layer] = jack_port_register(
            client, name, JACK_MIDI_TYPE, JACK_PORT_IS_OUTPUT, 0
        );
    }
    if (input_port == NULL) {
        fprintf(stderr, "Could not register MIDI input\n");
        jack_client_close(client);
        return 1;
    }
    for (layer = 0; layer < LAYER_COUNT; ++layer)
        if (output_ports[layer] == NULL) {
            fprintf(stderr, "Could not register MIDI layer output\n");
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
        "SMK-25 layers ready: channel=%d pad_type=%s pad_behavior=%s; "
        "standard MIDI and MMC Play/Stop enabled\n",
        configuration.channel + 1,
        configuration.pad_type == CONTROL_CC ? "cc" : "note",
        configuration.pad_behavior == PAD_VALUE ? "value" : "toggle"
    );
    fflush(stdout);

    while (running) {
        const unsigned int current_generation = atomic_load(&state_generation);
        (void)nanosleep(&delay, NULL);
        if (current_generation == saved_generation)
            continue;
        if (save_state(state_path) != 0)
            perror("Could not persist SMK-25 layer state");
        else
            saved_generation = current_generation;
    }

    atomic_store(&panic_requested, true);
    for (layer = 0; layer < 10 && !atomic_load(&panic_complete); ++layer)
        (void)nanosleep(&delay, NULL);
    (void)save_state(state_path);
    jack_client_close(client);
    return 0;
}
