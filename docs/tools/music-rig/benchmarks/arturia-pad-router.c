#define _POSIX_C_SOURCE 200809L

#include <jack/jack.h>
#include <jack/midiport.h>

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct router {
    jack_port_t *input;
    jack_port_t *output;
    int trigger_fd;
} router;

static volatile sig_atomic_t running = 1;

static void stop_router(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static int profile_for_pad(unsigned char note)
{
    static const unsigned char notes[] = {
        40, 41, 42, 43, 36, 37, 38, 39,
        48, 49, 50, 51, 44, 45, 46, 47
    };
    size_t index;

    for (index = 0U; index < sizeof(notes); ++index) {
        if (notes[index] == note) {
            return (int)index + 1;
        }
    }
    return 0;
}

static int process(jack_nframes_t frames, void *opaque)
{
    router *value = opaque;
    void *input = jack_port_get_buffer(value->input, frames);
    void *output = jack_port_get_buffer(value->output, frames);
    uint32_t index;

    jack_midi_clear_buffer(output);
    for (index = 0U; index < jack_midi_get_event_count(input); ++index) {
        jack_midi_event_t event;
        int profile;

        if (jack_midi_event_get(&event, input, index) != 0 || event.size == 0U) {
            continue;
        }
        profile = event.size >= 3U &&
            (event.buffer[0] & 0xf0U) == 0x90U &&
            (event.buffer[0] & 0x0fU) == 0x09U &&
            event.buffer[2] != 0U
            ? profile_for_pad(event.buffer[1]) : 0;
        if (profile != 0) {
            unsigned char value_byte = (unsigned char)profile;
            (void)write(value->trigger_fd, &value_byte, 1U);
            continue;
        }
        (void)jack_midi_event_write(
            output, event.time, event.buffer, event.size
        );
    }
    return 0;
}

int main(int argc, char **argv)
{
    jack_status_t status = 0;
    jack_client_t *client;
    router value = {0};
    struct timespec delay = {0, 100000000L};

    if (argc != 3) {
        fprintf(stderr, "usage: %s CLIENT TRIGGER_FIFO\n", argv[0]);
        return 2;
    }
    value.trigger_fd = open(argv[2], O_WRONLY | O_NONBLOCK);
    if (value.trigger_fd < 0) {
        perror("trigger fifo");
        return 1;
    }
    client = jack_client_open(argv[1], JackNoStartServer, &status, NULL);
    if (client == NULL) {
        fprintf(stderr, "jack_client_open failed: 0x%x\n", status);
        close(value.trigger_fd);
        return 1;
    }
    value.input = jack_port_register(
        client, "in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0U
    );
    value.output = jack_port_register(
        client, "out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0U
    );
    if (value.input == NULL || value.output == NULL ||
        jack_set_process_callback(client, process, &value) != 0 ||
        jack_activate(client) != 0) {
        jack_client_close(client);
        close(value.trigger_fd);
        return 1;
    }
    signal(SIGINT, stop_router);
    signal(SIGTERM, stop_router);
    while (running) {
        nanosleep(&delay, NULL);
    }
    jack_deactivate(client);
    jack_client_close(client);
    close(value.trigger_fd);
    return 0;
}
