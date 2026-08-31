#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t jack_nframes_t;
typedef uint32_t jack_options_t;
typedef uint32_t jack_status_t;
typedef struct _jack_client jack_client_t;
typedef struct _jack_port jack_port_t;
typedef unsigned char jack_midi_data_t;

typedef struct jack_midi_event {
    jack_nframes_t time;
    size_t size;
    jack_midi_data_t *buffer;
} jack_midi_event_t;

struct _jack_client {
    int unused;
};

struct _jack_port {
    size_t index;
};

static struct _jack_client client;
static struct _jack_port ports[9];
static uint8_t buffers[9];
static size_t registered_count;
static const char *const expected_ports[] = {
    "device.smc-mixer-main.midi-input",
    "device.smc-mixer-main.fader-1-midi-output",
    "device.smc-mixer-main.fader-2-midi-output",
    "device.smc-mixer-main.fader-3-midi-output",
    "device.smc-mixer-main.fader-4-midi-output",
    "device.smc-mixer-main.fader-5-midi-output",
    "device.smc-mixer-main.fader-6-midi-output",
    "device.smc-mixer-main.fader-7-midi-output",
    "device.smc-mixer-main.fader-8-midi-output"
};
static const unsigned long expected_flags[] = {
    1UL, 2UL, 2UL, 2UL, 2UL, 2UL, 2UL, 2UL, 2UL
};

jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
)
{
    if (strcmp(name, "music-rigd-smc-mixer") != 0 ||
        options != UINT32_C(1) || status == NULL) {
        return NULL;
    }
    *status = UINT32_C(0);
    registered_count = 0U;
    return &client;
}

int jack_client_close(jack_client_t *value)
{
    return value == &client ? 0 : -1;
}

int jack_activate(jack_client_t *value)
{
    return value == &client && registered_count == 9U ? 0 : -1;
}

int jack_deactivate(jack_client_t *value)
{
    return value == &client ? 0 : -1;
}

int jack_set_process_callback(
    jack_client_t *value,
    int (*callback)(jack_nframes_t, void *),
    void *argument
)
{
    return value == &client && callback != NULL && argument != NULL ? 0 : -1;
}

void jack_on_shutdown(
    jack_client_t *value,
    void (*callback)(void *),
    void *argument
)
{
    (void)value;
    (void)callback;
    (void)argument;
}

jack_port_t *jack_port_register(
    jack_client_t *value,
    const char *name,
    const char *type,
    unsigned long flags,
    unsigned long buffer_size
)
{
    size_t index = registered_count;

    if (value != &client || index >= 9U ||
        strcmp(name, expected_ports[index]) != 0 ||
        strcmp(type, "8 bit raw midi") != 0 ||
        flags != expected_flags[index] || buffer_size != 0UL) {
        return NULL;
    }
    ports[index].index = index;
    registered_count += 1U;
    return &ports[index];
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t frame_count)
{
    (void)frame_count;
    return port != NULL && port->index < 9U ? &buffers[port->index] : NULL;
}

uint32_t jack_midi_get_event_count(void *port_buffer)
{
    (void)port_buffer;
    return UINT32_C(0);
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
    return -1;
}

void jack_midi_clear_buffer(void *port_buffer)
{
    if (port_buffer != NULL) {
        *(uint8_t *)port_buffer = UINT8_C(0);
    }
}

int jack_midi_event_write(
    void *port_buffer,
    jack_nframes_t time,
    const jack_midi_data_t *data,
    size_t data_size
)
{
    (void)port_buffer;
    (void)time;
    (void)data;
    (void)data_size;
    return -1;
}
