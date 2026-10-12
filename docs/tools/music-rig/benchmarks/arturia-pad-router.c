#define _POSIX_C_SOURCE 200809L

#include <jack/jack.h>
#include <jack/midiport.h>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct router {
    jack_port_t *input;
    jack_port_t *output;
    jack_port_t *genre_outputs[13];
    int trigger_fd;
    atomic_int active_profile;
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
    void *genre_buffers[13];
    uint32_t index;

    jack_midi_clear_buffer(output);
    for (index = 0U; index < 13U; ++index) {
        genre_buffers[index] = jack_port_get_buffer(value->genre_outputs[index], frames);
        jack_midi_clear_buffer(genre_buffers[index]);
    }
    for (index = 0U; index < jack_midi_get_event_count(input); ++index) {
        jack_midi_event_t event;
        int profile;
        int active_profile;
        void *active_output;

        if (jack_midi_event_get(&event, input, index) != 0 || event.size == 0U) {
            continue;
        }
        profile = event.size >= 3U &&
            (event.buffer[0] & 0xf0U) == 0x90U &&
            (event.buffer[0] & 0x0fU) == 0x09U &&
            event.buffer[2] != 0U
            ? profile_for_pad(event.buffer[1]) : 0;
        if (profile != 0) {
            static const unsigned char all_sound_off[] = {0xb0U, 120U, 0U};
            static const unsigned char all_notes_off[] = {0xb0U, 123U, 0U};
            active_profile = atomic_load(&value->active_profile);
            active_output = active_profile >= 4 && active_profile <= 16
                ? genre_buffers[active_profile - 4] : output;

            (void)jack_midi_event_write(
                active_output, event.time, all_sound_off, sizeof(all_sound_off)
            );
            (void)jack_midi_event_write(
                active_output, event.time, all_notes_off, sizeof(all_notes_off)
            );
            unsigned char value_byte = (unsigned char)profile;
            ssize_t trigger_bytes = write(value->trigger_fd, &value_byte, 1U);
            (void)trigger_bytes;
            continue;
        }
        active_profile = atomic_load(&value->active_profile);
        active_output = active_profile >= 4 && active_profile <= 16
            ? genre_buffers[active_profile - 4] : output;
        (void)jack_midi_event_write(
            active_output, event.time, event.buffer, event.size
        );
        if (active_output != output &&
            (event.buffer[0] & 0xf0U) != 0x80U &&
            (event.buffer[0] & 0xf0U) != 0x90U) {
            (void)jack_midi_event_write(output, event.time, event.buffer, event.size);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    jack_status_t status = 0;
    jack_client_t *client;
    router value = {0};
    struct pollfd profile_input = {STDIN_FILENO, POLLIN, 0};
    char port_name[32];
    unsigned char profile_code;
    unsigned int index;

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
    atomic_init(&value.active_profile, 3);
    for (index = 4U; index <= 16U; ++index) {
        (void)snprintf(port_name, sizeof(port_name), "genre-pad-%u", index);
        value.genre_outputs[index - 4U] = jack_port_register(
            client, port_name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0U
        );
    }
    if (value.input == NULL || value.output == NULL ||
        value.genre_outputs[0] == NULL || value.genre_outputs[1] == NULL ||
        value.genre_outputs[2] == NULL || value.genre_outputs[3] == NULL ||
        value.genre_outputs[4] == NULL || value.genre_outputs[5] == NULL ||
        value.genre_outputs[6] == NULL || value.genre_outputs[7] == NULL ||
        value.genre_outputs[8] == NULL || value.genre_outputs[9] == NULL ||
        value.genre_outputs[10] == NULL || value.genre_outputs[11] == NULL ||
        value.genre_outputs[12] == NULL ||
        jack_set_process_callback(client, process, &value) != 0 ||
        jack_activate(client) != 0) {
        jack_client_close(client);
        close(value.trigger_fd);
        return 1;
    }
    signal(SIGINT, stop_router);
    signal(SIGTERM, stop_router);
    while (running) {
        int ready = poll(&profile_input, 1U, 1);
        if (ready > 0 && (profile_input.revents & POLLIN) != 0) {
            ssize_t count = read(STDIN_FILENO, &profile_code, sizeof(profile_code));
            if (count == 1 && profile_code >= 1U && profile_code <= 16U) {
                atomic_store(&value.active_profile, profile_code);
                ssize_t ack_bytes = write(STDOUT_FILENO, &profile_code, sizeof(profile_code));
                (void)ack_bytes;
            }
        }
    }
    jack_deactivate(client);
    jack_client_close(client);
    close(value.trigger_fd);
    return 0;
}
