#define _POSIX_C_SOURCE 200809L

#include <jack/jack.h>
#include <jack/midiport.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct stimulus {
    jack_port_t *output;
    jack_port_t *input;
    unsigned long input_events;
    unsigned long output_events;
    unsigned long input_note_on;
    unsigned long input_note_off;
    unsigned long input_control;
} stimulus;

static int process(jack_nframes_t frames, void *opaque)
{
    stimulus *value = opaque;
    void *output = jack_port_get_buffer(value->output, frames);
    void *input = jack_port_get_buffer(value->input, frames);
    jack_midi_event_t event;
    jack_nframes_t frame;
    unsigned char note_on[] = {0x90, 60, 48};
    unsigned char control_a[] = {0xb0, 20, 0};
    unsigned char control_b[] = {0xb0, 20, 127};
    unsigned char control_c[] = {0xb0, 74, 64};
    unsigned char note_off[] = {0x80, 60, 0};
    uint32_t index;

    jack_midi_clear_buffer(output);
    if (frames == 0U) {
        return 0;
    }
    frame = frames > 4U ? frames - 1U : 0U;
    (void)jack_midi_event_write(output, 0U, note_on, sizeof(note_on));
    (void)jack_midi_event_write(output, 1U, control_a, sizeof(control_a));
    (void)jack_midi_event_write(output, 2U, control_b, sizeof(control_b));
    (void)jack_midi_event_write(output, 3U, control_c, sizeof(control_c));
    (void)jack_midi_event_write(output, frame, note_off, sizeof(note_off));
    value->output_events += 5UL;

    for (index = 0U; index < jack_midi_get_event_count(input); ++index) {
        if (jack_midi_event_get(&event, input, index) == 0) {
            value->input_events += 1UL;
            if (event.size > 0U) {
                switch (event.buffer[0] & 0xf0U) {
                case 0x80U:
                    value->input_note_off += 1UL;
                    break;
                case 0x90U:
                    value->input_note_on += 1UL;
                    break;
                case 0xb0U:
                    value->input_control += 1UL;
                    break;
                default:
                    break;
                }
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    jack_status_t status = 0;
    jack_client_t *client;
    stimulus value = {0};
    struct timespec delay;
    const char *server_name = getenv("JACK_SERVER_NAME");
    const char *source = NULL;
    char feedback_port[256];
    long duration_ms;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s CLIENT TARGET DURATION_MS [SOURCE]\n", argv[0]);
        return 2;
    }
    duration_ms = strtol(argv[3], NULL, 10);
    if (duration_ms <= 0) {
        return 2;
    }
    if (argc == 5) {
        source = argv[4];
    }
    if (server_name != NULL && server_name[0] != '\0') {
        client = jack_client_open(
            argv[1], JackNoStartServer | JackServerName, &status, server_name
        );
    } else {
        client = jack_client_open(argv[1], JackNoStartServer, &status);
    }
    if (client == NULL) {
        fprintf(stderr, "jack_client_open failed: 0x%x\n", status);
        return 1;
    }
    value.output = jack_port_register(
        client, "out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0U
    );
    value.input = jack_port_register(
        client, "feedback", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0U
    );
    (void)snprintf(
        feedback_port, sizeof(feedback_port), "%s:feedback",
        jack_get_client_name(client)
    );
    if (value.output == NULL || value.input == NULL) {
        fputs("jack_port_register failed\n", stderr);
        jack_client_close(client);
        return 1;
    }
    if (jack_set_process_callback(client, process, &value) != 0 ||
        jack_activate(client) != 0) {
        fputs("jack callback/activate failed\n", stderr);
        jack_client_close(client);
        return 1;
    }
    if (strcmp(argv[2], "-") != 0) {
        int connection_result = jack_connect(client, jack_port_name(value.output), argv[2]);
        if (connection_result != 0) {
            fprintf(stderr, "could not connect %s to %s: %d\n",
                jack_port_name(value.output), argv[2], connection_result);
            jack_client_close(client);
            return 1;
        }
    }
    if (source != NULL && jack_connect(client, source, feedback_port) != 0) {
        fprintf(stderr, "could not connect feedback from %s\n", source);
        jack_client_close(client);
        return 1;
    }

    delay.tv_sec = duration_ms / 1000L;
    delay.tv_nsec = (duration_ms % 1000L) * 1000000L;
    (void)nanosleep(&delay, NULL);
    (void)jack_deactivate(client);
    (void)jack_client_close(client);
    printf("output_events=%lu input_events=%lu\n",
        value.output_events, value.input_events);
    printf("input_note_on=%lu input_note_off=%lu input_control=%lu\n",
        value.input_note_on, value.input_note_off, value.input_control);
    return 0;
}
