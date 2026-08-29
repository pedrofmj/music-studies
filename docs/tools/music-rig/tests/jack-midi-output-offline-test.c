#include "music_rig/jack_midi_output.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define FAKE_PORT_CAPACITY ((size_t)8)
#define FAKE_EVENT_CAPACITY ((size_t)8)

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

typedef struct fake_event {
    jack_nframes_t time;
    size_t size;
    uint8_t data[16];
} fake_event;

typedef struct fake_buffer {
    fake_event events[FAKE_EVENT_CAPACITY];
    uint32_t count;
} fake_buffer;

struct _jack_client { int unused; };
struct _jack_port { size_t index; };

static struct _jack_client fake_client;
static struct _jack_port fake_ports[FAKE_PORT_CAPACITY];
static fake_buffer fake_buffers[FAKE_PORT_CAPACITY];
static char registered_names[FAKE_PORT_CAPACITY][MUSIC_RIG_DEVICE_PORT_ID_CAPACITY];
static unsigned long registered_flags[FAKE_PORT_CAPACITY];
static int (*registered_process)(jack_nframes_t, void *);
static void *registered_process_context;
static void (*registered_shutdown)(void *);
static void *registered_shutdown_context;
static size_t registered_count;
static size_t fail_registration_at;
static int fail_activation;
static int close_count;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Offline JACK output test failed: %s\n", message); \
        return 1; \
    } \
} while (0)

static void reset_fake(void)
{
    memset(fake_buffers, 0, sizeof(fake_buffers));
    memset(registered_names, 0, sizeof(registered_names));
    memset(registered_flags, 0, sizeof(registered_flags));
    registered_process = NULL;
    registered_process_context = NULL;
    registered_shutdown = NULL;
    registered_shutdown_context = NULL;
    registered_count = 0U;
    fail_registration_at = SIZE_MAX;
    fail_activation = 0;
    close_count = 0;
}

static music_rig_result init_tables(music_rig_compiled_tables *tables)
{
    music_rig_result result = init_compiled_tables_fixture(tables);

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    fixture_copy(tables->device_profiles[0].hardware_preset,
        "arturia-current-rack");
    tables->mappings[0].number = UINT8_C(114);
    tables->mappings[0].behavior = MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE;
    tables->mappings[0].transform = MUSIC_RIG_TRANSFORM_RELATIVE;
    tables->mappings[0].relative_encoding =
        MUSIC_RIG_RELATIVE_ENCODING_BINARY_OFFSET;
    tables->mappings[0].takeover = MUSIC_RIG_TAKEOVER_NONE;
    return music_rig_compiled_tables_prepare(
        tables, UINT32_C(2), UINT32_C(2), UINT32_C(2), UINT32_C(2)
    );
}

int main(void)
{
    music_rig_compiled_tables tables;
    music_rig_generation generation = {UINT64_C(1), &tables};
    music_rig_generation next_generation = {UINT64_C(2), &tables};
    music_rig_generation_slot generations;
    music_rig_jack_midi_output host;
    music_rig_device_midi_shadow shadow;
    music_rig_device_midi_shadow_config shadow_config;
    music_rig_device_midi_shadow_observer observer;
    music_rig_output_adoption_adapter adapter;
    uint8_t message[3] = {UINT8_C(0xb0), UINT8_C(114), UINT8_C(65)};
    music_rig_result result;

    reset_fake();
    CHECK(init_tables(&tables) == MUSIC_RIG_RESULT_OK, "table fixture failed");
    CHECK(music_rig_generation_slot_init(&generations, &generation) ==
            MUSIC_RIG_RESULT_OK, "generation fixture failed");
    CHECK(music_rig_jack_midi_output_init(&host, &generations, &tables) ==
            MUSIC_RIG_RESULT_OK, "output host initialization failed");
    CHECK(music_rig_jack_midi_output_observer_init(&host, &observer) ==
            MUSIC_RIG_RESULT_OK, "output observer initialization failed");
    music_rig_device_midi_shadow_config_init(&shadow_config);
    shadow_config.generations = &generations;
    shadow_config.observer = observer;
    shadow_config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    shadow_config.behaviors[0] =
        MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA;
    shadow_config.arturia_initial_volume = 64;
    CHECK(music_rig_device_midi_shadow_init(&shadow, &shadow_config) ==
            MUSIC_RIG_RESULT_OK, "shadow initialization failed");
    CHECK(music_rig_jack_midi_output_attach_shadow(&host, &shadow) ==
            MUSIC_RIG_RESULT_OK, "shadow attachment failed");
    CHECK(music_rig_jack_midi_output_start(&host) == MUSIC_RIG_RESULT_OK,
        "output host start failed");
    CHECK(registered_count == 4U && registered_flags[0] == 1UL &&
        registered_flags[1] == 2UL && registered_flags[2] == 1UL &&
        registered_flags[3] == 2UL, "paired output ports were not registered");
    CHECK(strcmp(registered_names[1], "device.arturia-main.midi-output") == 0,
        "stable output port name was not registered");
    fake_buffers[0].count = 1U;
    fake_buffers[0].events[0].time = UINT32_C(7);
    fake_buffers[0].events[0].size = sizeof(message);
    memcpy(fake_buffers[0].events[0].data, message, sizeof(message));
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0,
        "output process callback failed");
    CHECK(fake_buffers[1].count == 1U && fake_buffers[1].events[0].time ==
            UINT32_C(7) && fake_buffers[1].events[0].size == 3U &&
        fake_buffers[1].events[0].data[1] == UINT8_C(119) &&
        fake_buffers[1].events[0].data[2] == UINT8_C(65),
        "generated MIDI was not emitted");
    CHECK(music_rig_jack_midi_output_metrics_read(&host)->output_events ==
            UINT64_C(1), "output metrics were not recorded");
    adapter = music_rig_jack_midi_output_adapter(&host);
    CHECK(adapter.prepare(adapter.context, &generation) == MUSIC_RIG_RESULT_OK &&
        adapter.confirm(adapter.context, &generation) == MUSIC_RIG_RESULT_OK,
        "initial output adoption failed");
    CHECK(adapter.prepare(adapter.context, &next_generation) ==
            MUSIC_RIG_RESULT_OK && adapter.confirm(adapter.context,
                &next_generation) == MUSIC_RIG_RESULT_OK &&
        adapter.rollback(adapter.context, &generation) == MUSIC_RIG_RESULT_OK,
        "output adoption rollback contract failed");
    registered_shutdown(registered_shutdown_context);
    result = music_rig_jack_midi_output_reconnect(&host);
    CHECK(result == MUSIC_RIG_RESULT_OK && host.active &&
        !host.backend_shutdown && registered_count == 4U && close_count == 1,
        "output host reconnect failed");
    registered_shutdown(registered_shutdown_context);
    fail_activation = 1;
    CHECK(music_rig_jack_midi_output_reconnect(&host) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE && host.client == NULL &&
        !host.active && close_count == 3,
        "output host reconnect failure did not clean up");

    reset_fake();
    CHECK(music_rig_jack_midi_output_init(&host, &generations, &tables) ==
            MUSIC_RIG_RESULT_OK && music_rig_jack_midi_output_observer_init(
                &host, &observer) == MUSIC_RIG_RESULT_OK &&
        music_rig_device_midi_shadow_init(&shadow, &shadow_config) ==
            MUSIC_RIG_RESULT_OK && music_rig_jack_midi_output_attach_shadow(
                &host, &shadow) == MUSIC_RIG_RESULT_OK,
        "failure fixture initialization failed");
    fail_registration_at = 1U;
    result = music_rig_jack_midi_output_start(&host);
    CHECK(result == MUSIC_RIG_RESULT_ADAPTER_FAILURE && close_count == 1 &&
        host.client == NULL, "registration failure did not clean up");
    puts("Offline JACK MIDI output test: OK");
    return 0;
}

jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
)
{
    if (strcmp(name, "music-rigd-device-output") != 0 ||
        options != UINT32_C(1)) {
        return NULL;
    }
    *status = UINT32_C(0);
    registered_count = 0U;
    return &fake_client;
}

int jack_client_close(jack_client_t *client)
{
    CHECK(client == &fake_client, "wrong client closed");
    close_count += 1;
    return 0;
}

int jack_activate(jack_client_t *client)
{
    CHECK(client == &fake_client, "wrong client activated");
    return fail_activation;
}

int jack_deactivate(jack_client_t *client)
{
    CHECK(client == &fake_client, "wrong client deactivated");
    return 0;
}

int jack_set_process_callback(
    jack_client_t *client,
    int (*callback)(jack_nframes_t, void *),
    void *argument
)
{
    CHECK(client == &fake_client, "wrong callback client");
    registered_process = callback;
    registered_process_context = argument;
    return 0;
}

void jack_on_shutdown(
    jack_client_t *client,
    void (*callback)(void *),
    void *argument
)
{
    if (client == &fake_client) {
        registered_shutdown = callback;
        registered_shutdown_context = argument;
    }
}

jack_port_t *jack_port_register(
    jack_client_t *client,
    const char *name,
    const char *type,
    unsigned long flags,
    unsigned long buffer_size
)
{
    size_t index = registered_count;

    if (client != &fake_client || strcmp(type, "8 bit raw midi") != 0 ||
        buffer_size != 0UL || index == fail_registration_at ||
        index >= FAKE_PORT_CAPACITY) {
        return NULL;
    }
    fake_ports[index].index = index;
    fixture_copy(registered_names[index], name);
    registered_flags[index] = flags;
    registered_count += 1U;
    return &fake_ports[index];
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t frame_count)
{
    (void)frame_count;
    return port == NULL || port->index >= FAKE_PORT_CAPACITY
        ? NULL : &fake_buffers[port->index];
}

void jack_midi_clear_buffer(void *port_buffer)
{
    fake_buffer *buffer = port_buffer;
    buffer->count = 0U;
}

uint32_t jack_midi_get_event_count(void *port_buffer)
{
    return ((const fake_buffer *)port_buffer)->count;
}

int jack_midi_event_get(
    jack_midi_event_t *event,
    void *port_buffer,
    uint32_t index
)
{
    fake_buffer *buffer = port_buffer;

    if (event == NULL || index >= buffer->count) {
        return 1;
    }
    event->time = buffer->events[index].time;
    event->size = buffer->events[index].size;
    event->buffer = buffer->events[index].data;
    return 0;
}

jack_midi_data_t *jack_midi_event_reserve(
    void *port_buffer,
    jack_nframes_t time,
    size_t data_size
)
{
    fake_buffer *buffer = port_buffer;
    fake_event *event;

    if (buffer == NULL || data_size > sizeof(buffer->events[0].data) ||
        buffer->count >= FAKE_EVENT_CAPACITY) {
        return NULL;
    }
    event = &buffer->events[buffer->count++];
    event->time = time;
    event->size = data_size;
    return event->data;
}
