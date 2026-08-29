#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

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

struct _jack_client { int unused; };
struct _jack_port { size_t index; };

static struct _jack_client client;
static struct _jack_port ports[10];
static uint8_t buffers[10][256];
static size_t registered_count;
static int activation_count;
static void (*shutdown_callback)(void *);
static void *shutdown_context;
static const char *const expected_ports[] = {
    "device.arturia-main.midi-input",
    "device.arturia-main.midi-output",
    "device.smc-mixer-main.midi-input",
    "device.smc-mixer-main.midi-output",
    "device.smc-pad-main.midi-input",
    "device.smc-pad-main.midi-output",
    "device.smc-pad-pocket.midi-input",
    "device.smc-pad-pocket.midi-output",
    "device.smk25-main.midi-input",
    "device.smk25-main.midi-output"
};

jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
)
{
    if (strcmp(name, "music-rigd-device-output") != 0 ||
        options != UINT32_C(1) || status == NULL) {
        return NULL;
    }
    *status = UINT32_C(0);
    registered_count = 0U;
    return &client;
}

static void *shutdown_worker(void *context)
{
    struct timespec delay = {0, 50000000L};

    (void)context;
    (void)nanosleep(&delay, NULL);
    if (shutdown_callback != NULL) {
        shutdown_callback(shutdown_context);
    }
    return NULL;
}

int jack_client_close(jack_client_t *value)
{
    return value == &client ? 0 : -1;
}

int jack_activate(jack_client_t *value)
{
    pthread_t worker;

    if (value != &client || registered_count != 10U) {
        return -1;
    }
    activation_count += 1;
    if (activation_count == 1 && pthread_create(
            &worker, NULL, shutdown_worker, NULL
        ) != 0) {
        return -1;
    }
    if (activation_count == 1) {
        (void)pthread_detach(worker);
    }
    return 0;
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
    shutdown_callback = callback;
    shutdown_context = argument;
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
    unsigned long expected_flags = index % 2U == 0U ? 1UL : 2UL;

    if (value != &client || index >= 10U ||
        strcmp(name, expected_ports[index]) != 0 ||
        strcmp(type, "8 bit raw midi") != 0 || flags != expected_flags ||
        buffer_size != 0UL) {
        return NULL;
    }
    ports[index].index = index;
    registered_count += 1U;
    return &ports[index];
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t frame_count)
{
    (void)frame_count;
    return port != NULL && port->index < 10U ? buffers[port->index] : NULL;
}

void jack_midi_clear_buffer(void *port_buffer)
{
    memset(port_buffer, 0, 256U);
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

jack_midi_data_t *jack_midi_event_reserve(
    void *port_buffer,
    jack_nframes_t time,
    size_t data_size
)
{
    (void)port_buffer;
    (void)time;
    (void)data_size;
    return NULL;
}
