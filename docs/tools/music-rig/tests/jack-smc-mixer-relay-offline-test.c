#include "music_rig/jack_smc_mixer_relay.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

static const char *const MAPPINGS[] = {
    "band-63-hz-gain", "band-125-hz-gain", "band-250-hz-gain",
    "band-500-hz-gain", "band-1000-hz-gain", "band-2000-hz-gain",
    "band-4000-hz-gain", "band-8000-hz-gain"
};
static const char *const TARGETS[] = {
    "equalizer.band-63-hz.gain", "equalizer.band-125-hz.gain",
    "equalizer.band-250-hz.gain", "equalizer.band-500-hz.gain",
    "equalizer.band-1000-hz.gain", "equalizer.band-2000-hz.gain",
    "equalizer.band-4000-hz.gain", "equalizer.band-8000-hz.gain"
};
static const char *const SORTED_TARGETS[] = {
    "equalizer.band-1000-hz.gain", "equalizer.band-125-hz.gain",
    "equalizer.band-2000-hz.gain", "equalizer.band-250-hz.gain",
    "equalizer.band-4000-hz.gain", "equalizer.band-500-hz.gain",
    "equalizer.band-63-hz.gain", "equalizer.band-8000-hz.gain"
};

static struct _jack_client fake_client;
static struct _jack_port fake_ports[2];
static fake_buffer fake_buffers[2];
static char registered_names[2][MUSIC_RIG_DEVICE_PORT_ID_CAPACITY];
static unsigned long registered_flags[2];
static int (*registered_process)(jack_nframes_t, void *);
static void *registered_process_context;
static void (*registered_shutdown)(void *);
static void *registered_shutdown_context;
static size_t registered_count;
static size_t fail_registration_at;
static int fail_activation;
static int fail_write;
static int open_count;
static int activate_count;
static int deactivate_count;
static int close_count;
static int clear_count;
static int fake_contract_failed;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Offline JACK SMC-Mixer relay failed: %s\n", message); \
        return 1; \
    } \
} while (0)

static void copy_text(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static music_rig_result init_tables(music_rig_compiled_tables *tables)
{
    size_t index;

    memset(tables, 0, sizeof(*tables));
    tables->device_profile_count = UINT32_C(1);
    tables->input_binding_count = UINT32_C(1);
    tables->mapping_count = UINT32_C(8);
    tables->target_binding_count = UINT32_C(8);
    tables->ownership_count = UINT32_C(8);
    copy_text(tables->device_profiles[0].slot, "smc-mixer-main");
    copy_text(tables->device_profiles[0].profile, "eight-band-eq");
    copy_text(tables->device_profiles[0].hardware_preset,
        "smc-mixer-current-cc");
    tables->device_profiles[0].readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;
    copy_text(tables->input_bindings[0].slot, "smc-mixer-main");
    copy_text(tables->input_bindings[0].adapter, "mock-midi");
    copy_text(tables->input_bindings[0].identity_strategy, "stable-id");
    copy_text(tables->input_bindings[0].identity_value, "smc-mixer-main");
    tables->input_bindings[0].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
    tables->input_bindings[0].endpoint_count = UINT16_C(1);
    copy_text(tables->input_bindings[0].endpoints[0].purpose,
        "midi.control-input");
    copy_text(tables->input_bindings[0].endpoints[0].locator,
        "mock:smc-mixer");
    for (index = 0U; index < 8U; ++index) {
        music_rig_compiled_mapping *mapping = &tables->mappings[index];
        music_rig_compiled_target_binding *target =
            &tables->target_bindings[index];
        music_rig_compiled_ownership *ownership = &tables->ownership[index];

        copy_text(mapping->mapping, MAPPINGS[index]);
        (void)snprintf(mapping->control, sizeof(mapping->control),
            "fader-%zu", index + 1U);
        copy_text(mapping->target, TARGETS[index]);
        mapping->profile_index = UINT16_C(0);
        mapping->event_type = MUSIC_RIG_MIDI_EVENT_CC;
        mapping->edge = MUSIC_RIG_MIDI_EDGE_CHANGE;
        mapping->channel = UINT8_C(1);
        mapping->number = (uint8_t)(UINT8_C(40) + (uint8_t)index);
        mapping->behavior = MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE;
        mapping->transform = MUSIC_RIG_TRANSFORM_DIRECT;
        mapping->takeover = MUSIC_RIG_TAKEOVER_PICKUP;

        copy_text(target->target, SORTED_TARGETS[index]);
        copy_text(target->adapter, "mock-control");
        copy_text(target->locator, "mock:equalizer");
        target->status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;

        ownership->kind = MUSIC_RIG_OWNERSHIP_KIND_PARAMETER;
        ownership->mode = MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE;
        copy_text(ownership->target, TARGETS[index]);
        ownership->owner_count = UINT16_C(1);
        ownership->owners[0].scope = MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE;
        ownership->owners[0].profile_index = UINT16_C(0);
        copy_text(ownership->owners[0].slot, "smc-mixer-main");
        copy_text(ownership->owners[0].profile, "eight-band-eq");
    }
    return music_rig_compiled_tables_prepare(
        tables, UINT32_C(1), UINT32_C(8), UINT32_C(8), UINT32_C(8)
    );
}

static int init_host(
    music_rig_compiled_tables *tables,
    music_rig_generation *generation,
    music_rig_generation_slot *generations,
    music_rig_jack_smc_mixer_relay *host
)
{
    if (init_tables(tables) != MUSIC_RIG_RESULT_OK) {
        return 0;
    }
    generation->id = UINT64_C(1);
    generation->mapping = tables;
    return music_rig_generation_slot_init(generations, generation) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_jack_smc_mixer_relay_init(host, generations) ==
            MUSIC_RIG_RESULT_OK;
}

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
    fail_write = 0;
    open_count = 0;
    activate_count = 0;
    deactivate_count = 0;
    close_count = 0;
    clear_count = 0;
    fake_contract_failed = 0;
}

static void queue_input(
    size_t index,
    jack_nframes_t frame,
    uint8_t status,
    uint8_t number,
    uint8_t value
)
{
    fake_event *event = &fake_buffers[0].events[index];

    event->time = frame;
    event->size = sizeof(event->data);
    event->data[0] = status;
    event->data[1] = number;
    event->data[2] = value;
    fake_buffers[0].count = (uint32_t)(index + 1U);
}

static int test_relay_lifecycle(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation;
    music_rig_generation_slot generations;
    music_rig_jack_smc_mixer_relay host;
    const music_rig_smc_mixer_relay_metrics *metrics;

    reset_fake_jack();
    CHECK(init_host(&tables, &generation, &generations, &host),
        "host initialization");
    CHECK(music_rig_jack_smc_mixer_relay_start(&host) ==
            MUSIC_RIG_RESULT_OK,
        "host start");
    CHECK(open_count == 1 && activate_count == 1 && registered_count == 2U,
        "lifecycle counts");
    CHECK(strcmp(registered_names[0],
            "device.smc-mixer-main.midi-input") == 0 &&
        strcmp(registered_names[1],
            "device.smc-mixer-main.midi-output") == 0 &&
        registered_flags[0] == 1UL && registered_flags[1] == 2UL,
        "fixed input/output registration");
    queue_input(0U, UINT32_C(7), UINT8_C(0xb0), UINT8_C(40), UINT8_C(91));
    queue_input(1U, UINT32_C(8), UINT8_C(0xb0), UINT8_C(40), UINT8_C(92));
    queue_input(2U, UINT32_C(9), UINT8_C(0xb0), UINT8_C(39), UINT8_C(22));
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0,
        "process callback");
    CHECK(fake_buffers[1].count == UINT32_C(0) && clear_count == 1 &&
        host.last_process_result == MUSIC_RIG_RESULT_OK,
        "first cycle defers coalesced output");
    fake_buffers[0].count = UINT32_C(0);
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0,
        "deferred process callback");
    CHECK(fake_buffers[1].count == UINT32_C(1) && clear_count == 2 &&
        fake_buffers[1].events[0].time == UINT32_C(8) &&
        memcmp(fake_buffers[1].events[0].data,
            (uint8_t[]){UINT8_C(0xb0), UINT8_C(40), UINT8_C(92)}, 3U) == 0 &&
        host.last_process_result == MUSIC_RIG_RESULT_OK,
        "byte-for-byte JACK relay");
    metrics = music_rig_smc_mixer_relay_metrics_read(&host.relay);
    CHECK(metrics != NULL && metrics->cycles == UINT64_C(2) &&
        metrics->input_events == UINT64_C(3) &&
        metrics->mapped_events == UINT64_C(2) &&
        metrics->control_mapped_events[0] == UINT64_C(2) &&
        metrics->emitted_events == UINT64_C(1) &&
        metrics->coalesced_events == UINT64_C(1) &&
        metrics->unmapped_events == UINT64_C(1),
        "relay metrics");
    CHECK(music_rig_jack_smc_mixer_relay_stop(&host) == MUSIC_RIG_RESULT_OK &&
        deactivate_count == 1 && close_count == 1 && host.client == NULL,
        "host stop cleanup");
    CHECK(!fake_contract_failed, "fake JACK contract");
    return 0;
}

static int test_fail_closed_cleanup(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation;
    music_rig_generation_slot generations;
    music_rig_jack_smc_mixer_relay host;

    reset_fake_jack();
    CHECK(init_host(&tables, &generation, &generations, &host),
        "failure host initialization");
    fail_registration_at = 1U;
    CHECK(music_rig_jack_smc_mixer_relay_start(&host) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE &&
        close_count == 1 && host.client == NULL && !host.active,
        "registration failure cleanup");

    reset_fake_jack();
    CHECK(music_rig_jack_smc_mixer_relay_init(&host, &generations) ==
            MUSIC_RIG_RESULT_OK,
        "activation host initialization");
    fail_activation = 1;
    CHECK(music_rig_jack_smc_mixer_relay_start(&host) ==
            MUSIC_RIG_RESULT_ADAPTER_FAILURE &&
        close_count == 1 && host.client == NULL && !host.active,
        "activation failure cleanup");

    reset_fake_jack();
    CHECK(music_rig_jack_smc_mixer_relay_init(&host, &generations) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_jack_smc_mixer_relay_start(&host) == MUSIC_RIG_RESULT_OK,
        "write failure start");
    queue_input(0U, UINT32_C(9), UINT8_C(0xb0), UINT8_C(40), UINT8_C(12));
    fail_write = 1;
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0 &&
        host.last_process_result == MUSIC_RIG_RESULT_OK &&
        fake_buffers[1].count == UINT32_C(0),
        "write failure deferral");
    fake_buffers[0].count = UINT32_C(0);
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0 &&
        host.last_process_result == MUSIC_RIG_RESULT_ADAPTER_FAILURE &&
        fake_buffers[1].count == UINT32_C(0),
        "write failure propagation");
    fail_write = 0;
    fake_buffers[0].count = UINT32_C(0);
    CHECK(registered_process(UINT32_C(128), registered_process_context) == 0 &&
        host.last_process_result == MUSIC_RIG_RESULT_ADAPTER_FAILURE,
        "write failure latch");
    registered_shutdown(registered_shutdown_context);
    CHECK(host.backend_shutdown && !host.active,
        "backend shutdown state");
    CHECK(music_rig_jack_smc_mixer_relay_stop(&host) == MUSIC_RIG_RESULT_OK &&
        deactivate_count == 0 && close_count == 1,
        "backend shutdown cleanup");
    CHECK(music_rig_jack_smc_mixer_relay_init(NULL, &generations) ==
            MUSIC_RIG_RESULT_INVALID_ARGUMENT,
        "null host boundary");
    return 0;
}

int main(void)
{
    if (test_relay_lifecycle() != 0 || test_fail_closed_cleanup() != 0) {
        return 1;
    }
    puts("Offline JACK SMC-Mixer relay test: OK");
    return 0;
}

jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
)
{
    if (strcmp(name, "music-rigd-smc-mixer") != 0 ||
        options != UINT32_C(1) || status == NULL) {
        fake_contract_failed = 1;
        return NULL;
    }
    *status = UINT32_C(0);
    open_count += 1;
    return &fake_client;
}

int jack_client_close(jack_client_t *client)
{
    if (client != &fake_client) {
        fake_contract_failed = 1;
        return -1;
    }
    close_count += 1;
    return 0;
}

int jack_activate(jack_client_t *client)
{
    activate_count += 1;
    return client == &fake_client ? fail_activation : -1;
}

int jack_deactivate(jack_client_t *client)
{
    deactivate_count += 1;
    return client == &fake_client ? 0 : -1;
}

int jack_set_process_callback(
    jack_client_t *client,
    int (*callback)(jack_nframes_t, void *),
    void *argument
)
{
    if (client != &fake_client || callback == NULL || argument == NULL) {
        return -1;
    }
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

    if (client != &fake_client || index >= 2U ||
        index == fail_registration_at || name == NULL ||
        strcmp(type, "8 bit raw midi") != 0 || buffer_size != 0UL) {
        return NULL;
    }
    copy_text(registered_names[index], name);
    registered_flags[index] = flags;
    fake_ports[index].index = index;
    registered_count += 1U;
    return &fake_ports[index];
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t frame_count)
{
    (void)frame_count;
    return port != NULL && port->index < 2U ? &fake_buffers[port->index] : NULL;
}

uint32_t jack_midi_get_event_count(void *port_buffer)
{
    return ((fake_buffer *)port_buffer)->count;
}

int jack_midi_event_get(
    jack_midi_event_t *event,
    void *port_buffer,
    uint32_t index
)
{
    fake_buffer *buffer = port_buffer;

    if (event == NULL || buffer == NULL || index >= buffer->count) {
        return -1;
    }
    event->time = buffer->events[index].time;
    event->size = buffer->events[index].size;
    event->buffer = buffer->events[index].data;
    return 0;
}

void jack_midi_clear_buffer(void *port_buffer)
{
    fake_buffer *buffer = port_buffer;

    buffer->count = UINT32_C(0);
    clear_count += 1;
}

int jack_midi_event_write(
    void *port_buffer,
    jack_nframes_t time,
    const jack_midi_data_t *data,
    size_t data_size
)
{
    fake_buffer *buffer = port_buffer;
    fake_event *event;

    if (fail_write || buffer == NULL || data == NULL || data_size != 3U ||
        buffer->count >= FAKE_EVENT_CAPACITY) {
        return -1;
    }
    event = &buffer->events[buffer->count];
    event->time = time;
    event->size = data_size;
    memcpy(event->data, data, data_size);
    buffer->count += UINT32_C(1);
    return 0;
}
