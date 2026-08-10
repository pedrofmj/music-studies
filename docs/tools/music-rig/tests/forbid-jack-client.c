#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

_Noreturn static void forbidden(const char *function_name)
{
    fprintf(
        stderr,
        "Offline helper test attempted forbidden JACK call: %s\n",
        function_name
    );
    fflush(stderr);
    _Exit(99);
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
    forbidden("jack_client_open");
}

int jack_client_close(jack_client_t *client)
{
    (void)client;
    forbidden("jack_client_close");
}

int jack_activate(jack_client_t *client)
{
    (void)client;
    forbidden("jack_activate");
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
    forbidden("jack_set_process_callback");
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
    forbidden("jack_on_shutdown");
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
    forbidden("jack_port_register");
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t frame_count)
{
    (void)port;
    (void)frame_count;
    forbidden("jack_port_get_buffer");
}

int jack_port_connected(const jack_port_t *port)
{
    (void)port;
    forbidden("jack_port_connected");
}

uint32_t jack_midi_get_event_count(void *port_buffer)
{
    (void)port_buffer;
    forbidden("jack_midi_get_event_count");
}

int jack_midi_event_get(
    jack_midi_event_t *event,
    void *port_buffer,
    uint32_t index
)
{
    (void)event;
    (void)port_buffer;
    (void)index;
    forbidden("jack_midi_event_get");
}

void jack_midi_clear_buffer(void *port_buffer)
{
    (void)port_buffer;
    forbidden("jack_midi_clear_buffer");
}

int jack_midi_event_write(
    void *port_buffer,
    jack_nframes_t time_value,
    const jack_midi_data_t *data,
    size_t size
)
{
    (void)port_buffer;
    (void)time_value;
    (void)data;
    (void)size;
    forbidden("jack_midi_event_write");
}
