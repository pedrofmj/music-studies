#include "music_rig/jack_midi_shadow.h"
#include "compiled-tables-fixture.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FAKE_PORT_CAPACITY MUSIC_RIG_DEVICE_PROFILE_CAPACITY
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
    uint8_t data[3];
} fake_event;

typedef struct fake_buffer {
    fake_event events[FAKE_EVENT_CAPACITY];
    uint32_t count;
} fake_buffer;

struct _jack_client {
    int unused;
};

struct _jack_port {
    size_t index;
};

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
static int open_count;
static int activate_count;
static int deactivate_count;
static int close_count;
static int fake_contract_failed;
static uint8_t fake_output_data[16];

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Offline JACK shadow test failed: %s\n", message); \
        return 1; \
    } \
} while (0)

static void reset_fake_jack(void)
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
    open_count = 0;
    activate_count = 0;
    deactivate_count = 0;
    close_count = 0;
    fake_contract_failed = 0;
}

static music_rig_result init_shadow_tables(music_rig_compiled_tables *tables)
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
    fixture_copy(tables->device_profiles[1].slot, "smk25-main");
    fixture_copy(tables->device_profiles[1].profile, "ambient-pad-layers");
    fixture_copy(tables->device_profiles[1].hardware_preset,
        "smk25-current-pad-layers");
    fixture_copy(tables->input_bindings[1].slot, "smk25-main");
    fixture_copy(tables->input_bindings[1].identity_value, "smk25-main");
    fixture_copy(tables->ownership[1].owners[0].slot, "smk25-main");
    fixture_copy(tables->ownership[1].owners[0].profile,
        "ambient-pad-layers");
    tables->mappings[1].number = UINT8_C(20);
    return music_rig_compiled_tables_prepare(
        tables,
        UINT32_C(2),
        UINT32_C(2),
        UINT32_C(2),
        UINT32_C(2)
    );
}

static int init_shadow(
    music_rig_compiled_tables *tables,
    music_rig_generation *generation,
    music_rig_generation_slot *generations,
    music_rig_device_midi_shadow *shadow
)
{
    music_rig_device_midi_shadow_config config;

    if (init_shadow_tables(tables) != MUSIC_RIG_RESULT_OK) {
        return 0;
    }
    generation->id = UINT64_C(1);
    generation->mapping = tables;
    if (music_rig_generation_slot_init(generations, generation) !=
            MUSIC_RIG_RESULT_OK) {
        return 0;
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = generations;
    if (music_rig_device_midi_shadow_configure_behavior(
            &config,
            tables,
            "arturia-main",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_configure_behavior(
            &config,
            tables,
            "smk25-main",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25
        ) != MUSIC_RIG_RESULT_OK) {
        return 0;
    }
    return music_rig_device_midi_shadow_init(shadow, &config) ==
        MUSIC_RIG_RESULT_OK;
}

static void queue_event(
    size_t port_index,
    jack_nframes_t frame,
    uint8_t status,
    uint8_t number,
    uint8_t value
)
{
    fake_event *event = &fake_buffers[port_index].events[0];

    fake_buffers[port_index].count = UINT32_C(1);
    event->time = frame;
    event->size = sizeof(event->data);
    event->data[0] = status;
    event->data[1] = number;
    event->data[2] = value;
}

static int test_input_only_lifecycle(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation;
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow shadow;
    music_rig_jack_midi_shadow host;
    const music_rig_device_midi_shadow_metrics *metrics;

    reset_fake_jack();
    CHECK(init_shadow(&tables, &generation, &generations, &shadow),
        "shadow initialization failed");
    CHECK(music_rig_jack_midi_shadow_init(&host, &shadow) ==
            MUSIC_RIG_RESULT_OK,
        "host initialization failed");
    CHECK(music_rig_jack_midi_shadow_start(&host) == MUSIC_RIG_RESULT_OK,
        "host start failed");
    CHECK(open_count == 1 && activate_count == 1 && registered_count == 2U,
        "host lifecycle counts are wrong");
    CHECK(strcmp(registered_names[0],
            "device.arturia-main.midi-input") == 0 &&
        strcmp(registered_names[1],
            "device.smk25-main.midi-input") == 0,
        "stable input names were not registered");
    CHECK(registered_flags[0] == 1UL && registered_flags[1] == 1UL,
        "a non-input JACK port was registered");
    CHECK(registered_process != NULL && registered_shutdown != NULL,
        "JACK callbacks were not registered");

    queue_event(0U, UINT32_C(7), UINT8_C(0xb0), UINT8_C(114), UINT8_C(65));
    queue_event(1U, UINT32_C(8), UINT8_C(0xb0), UINT8_C(20), UINT8_C(91));
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0,
        "process callback failed");
    metrics = music_rig_device_midi_shadow_metrics_read(&shadow);
    CHECK(metrics != NULL && metrics->cycles == UINT64_C(1) &&
        metrics->input_events == UINT64_C(2) &&
        metrics->mapping_decisions == UINT64_C(2) &&
        metrics->suppressed_midi_events == UINT64_C(2) &&
        host.last_process_result == MUSIC_RIG_RESULT_OK,
        "input shadow decisions were not processed");
    CHECK(music_rig_jack_midi_shadow_stop(&host) == MUSIC_RIG_RESULT_OK &&
        deactivate_count == 1 && close_count == 1 && host.client == NULL,
        "host stop did not clean up");
    CHECK(!fake_contract_failed, "fake JACK ABI contract failed");
    return 0;
}

static int test_fail_closed_cleanup(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation;
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow shadow;
    music_rig_jack_midi_shadow host;

    reset_fake_jack();
    CHECK(init_shadow(&tables, &generation, &generations, &shadow),
        "failure-test shadow initialization failed");
    CHECK(music_rig_jack_midi_shadow_init(&host, &shadow) ==
            MUSIC_RIG_RESULT_OK,
        "failure-test host initialization failed");
    fail_registration_at = 1U;
    CHECK(music_rig_jack_midi_shadow_start(&host) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE &&
        close_count == 1 && host.client == NULL && !host.active,
        "registration failure did not close the client");

    reset_fake_jack();
    CHECK(music_rig_jack_midi_shadow_init(&host, &shadow) ==
            MUSIC_RIG_RESULT_OK,
        "activation-test host initialization failed");
    fail_activation = 1;
    CHECK(music_rig_jack_midi_shadow_start(&host) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE &&
        close_count == 1 && host.client == NULL && !host.active,
        "activation failure did not close the client");

    reset_fake_jack();
    CHECK(music_rig_jack_midi_shadow_init(&host, &shadow) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_jack_midi_shadow_start(&host) == MUSIC_RIG_RESULT_OK,
        "shutdown-test start failed");
    registered_shutdown(registered_shutdown_context);
    CHECK(host.backend_shutdown && !host.active,
        "backend shutdown was not recorded");
    CHECK(music_rig_jack_midi_shadow_stop(&host) == MUSIC_RIG_RESULT_OK &&
        deactivate_count == 0 && close_count == 1,
        "backend shutdown cleanup failed");
    CHECK(music_rig_jack_midi_shadow_init(NULL, &shadow) ==
            MUSIC_RIG_RESULT_INVALID_ARGUMENT,
        "null host was accepted");
    return 0;
}

static int test_output_host_contract(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation;
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow shadow;
    music_rig_jack_midi_output host;

    reset_fake_jack();
    CHECK(init_shadow(&tables, &generation, &generations, &shadow),
        "output shadow initialization failed");
    CHECK(music_rig_jack_midi_output_init(&host, &shadow) ==
            MUSIC_RIG_RESULT_OK, "output host initialization failed");
    CHECK(music_rig_jack_midi_output_start(&host) == MUSIC_RIG_RESULT_OK,
        "output host start failed");
    CHECK(registered_count == 4U && registered_flags[0] == 1UL &&
        registered_flags[1] == 2UL && registered_flags[2] == 1UL &&
        registered_flags[3] == 2UL, "paired port contract failed");
    CHECK(music_rig_jack_midi_output_stop(&host) == MUSIC_RIG_RESULT_OK &&
        close_count == 1, "output host cleanup failed");
    return 0;
}

int main(void)
{
    if (test_input_only_lifecycle() != 0 ||
        test_output_host_contract() != 0 ||
        test_fail_closed_cleanup() != 0) {
        return 1;
    }
    puts("Offline input-only JACK MIDI shadow test: OK");
    return 0;
}

jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
)
{
    if ((strcmp(name, "music-rigd-shadow") != 0 &&
         strcmp(name, "music-rigd-output") != 0) ||
        options != UINT32_C(1)) {
        fake_contract_failed = 1;
        return NULL;
    }
    *status = UINT32_C(0);
    open_count += 1;
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
    activate_count += 1;
    return fail_activation;
}

jack_midi_data_t *jack_midi_event_reserve(
    void *port_buffer,
    jack_nframes_t time,
    size_t data_size
)
{
    (void)port_buffer;
    (void)time;
    return data_size <= sizeof(fake_output_data) ? fake_output_data : NULL;
}

int jack_deactivate(jack_client_t *client)
{
    CHECK(client == &fake_client, "wrong client deactivated");
    deactivate_count += 1;
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
        buffer_size != 0UL) {
        fake_contract_failed = 1;
        return NULL;
    }
    if (index == fail_registration_at) {
        return NULL;
    }
    if (index >= FAKE_PORT_CAPACITY) {
        fake_contract_failed = 1;
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
    return &fake_buffers[port->index];
}

uint32_t jack_midi_get_event_count(void *port_buffer)
{
    const fake_buffer *buffer = port_buffer;

    return buffer->count;
}

int jack_midi_event_get(
    jack_midi_event_t *event,
    void *port_buffer,
    uint32_t index
)
{
    fake_buffer *buffer = port_buffer;

    if (index >= buffer->count) {
        return -1;
    }
    event->time = buffer->events[index].time;
    event->size = buffer->events[index].size;
    event->buffer = buffer->events[index].data;
    return 0;
}
