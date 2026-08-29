#include "music_rig/jack_midi_output.h"

#include <stddef.h>
#include <stdatomic.h>
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

#define JACK_NO_START_SERVER UINT32_C(1)
#define JACK_PORT_IS_INPUT 1UL
#define JACK_PORT_IS_OUTPUT 2UL

static const char jack_default_midi_type[] = "8 bit raw midi";

extern jack_client_t *jack_client_open(
    const char *name,
    jack_options_t options,
    jack_status_t *status,
    ...
);
extern int jack_client_close(jack_client_t *client);
extern int jack_activate(jack_client_t *client);
extern int jack_deactivate(jack_client_t *client);
extern uint64_t jack_get_time(void);
extern int jack_set_process_callback(
    jack_client_t *client,
    int (*callback)(jack_nframes_t, void *),
    void *argument
);
extern void jack_on_shutdown(
    jack_client_t *client,
    void (*callback)(void *),
    void *argument
);
extern jack_port_t *jack_port_register(
    jack_client_t *client,
    const char *name,
    const char *type,
    unsigned long flags,
    unsigned long buffer_size
);
extern void *jack_port_get_buffer(
    jack_port_t *port,
    jack_nframes_t frame_count
);
extern void jack_midi_clear_buffer(void *port_buffer);
extern uint32_t jack_midi_get_event_count(void *port_buffer);
extern int jack_midi_event_get(
    jack_midi_event_t *event,
    void *port_buffer,
    uint32_t index
);
extern jack_midi_data_t *jack_midi_event_reserve(
    void *port_buffer,
    jack_nframes_t time,
    size_t data_size
);

static bool valid_host(const music_rig_jack_midi_output *host)
{
    return host != NULL &&
        host->abi_version == MUSIC_RIG_JACK_MIDI_OUTPUT_ABI_VERSION &&
        host->port_count > 0U &&
        host->port_count <= MUSIC_RIG_DEVICE_PROFILE_CAPACITY;
}

static bool valid_attached_host(const music_rig_jack_midi_output *host)
{
    return valid_host(host) && host->shadow != NULL &&
        music_rig_device_midi_shadow_slot_count(host->shadow) ==
            host->port_count;
}

static bool generation_ports_match(
    const music_rig_jack_midi_output *host,
    const music_rig_generation *generation
)
{
    music_rig_device_port_catalogue ports;

    return valid_host(host) && generation != NULL &&
        generation->mapping != NULL &&
        music_rig_device_port_catalogue_build(generation->mapping, &ports) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_device_port_catalogues_match(&host->ports, &ports);
}

static void output_midi(
    void *context,
    size_t slot_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_jack_midi_output *host = context;
    jack_midi_data_t *target;

    if (!valid_attached_host(host) || slot_index >= host->port_count ||
        host->output_buffers[slot_index] == NULL || message == NULL ||
        message_size == 0U) {
        if (host != NULL) {
            host->output_failed = true;
            if (host->metrics.output_reserve_failures != UINT64_MAX) {
                host->metrics.output_reserve_failures += UINT64_C(1);
            }
        }
        return;
    }
    target = jack_midi_event_reserve(
        host->output_buffers[slot_index], frame, message_size
    );
    if (target == NULL) {
        host->output_failed = true;
        if (host->metrics.output_reserve_failures != UINT64_MAX) {
            host->metrics.output_reserve_failures += UINT64_C(1);
        }
        return;
    }
    memcpy(target, message, message_size);
    if (host->metrics.output_events != UINT64_MAX) {
        host->metrics.output_events += UINT64_C(1);
    }
}

static int process_cycle(jack_nframes_t frame_count, void *opaque)
{
    music_rig_jack_midi_output *host = opaque;
    music_rig_result result;
    size_t slot_index;

    if (!valid_attached_host(host)) {
        return 0;
    }
    host->output_failed = false;
    result = music_rig_device_midi_shadow_begin_cycle(host->shadow);
    if (result == MUSIC_RIG_RESULT_OK) {
        const music_rig_generation *adopted =
            music_rig_generation_slot_adopted(host->generations);

        if (adopted != NULL) {
            atomic_store_explicit(
                &host->adopted_generation_id, adopted->id, memory_order_release
            );
            atomic_store_explicit(
                &host->adopted_at_ns, jack_get_time() * UINT64_C(1000),
                memory_order_release
            );
        }
    }
    for (slot_index = 0U;
         result == MUSIC_RIG_RESULT_OK && slot_index < host->port_count;
         ++slot_index) {
        host->output_buffers[slot_index] = jack_port_get_buffer(
            host->output_ports[slot_index], frame_count
        );
        if (host->output_buffers[slot_index] == NULL) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        } else {
            jack_midi_clear_buffer(host->output_buffers[slot_index]);
        }
    }
    for (slot_index = 0U;
         result == MUSIC_RIG_RESULT_OK && slot_index < host->port_count;
         ++slot_index) {
        void *buffer = jack_port_get_buffer(
            host->input_ports[slot_index], frame_count
        );
        uint32_t event_count;
        uint32_t event_index;

        if (buffer == NULL) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            break;
        }
        event_count = jack_midi_get_event_count(buffer);
        for (event_index = 0U; event_index < event_count; ++event_index) {
            jack_midi_event_t event;

            if (jack_midi_event_get(&event, buffer, event_index) != 0 ||
                event.buffer == NULL) {
                result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
                break;
            }
            result = music_rig_device_midi_shadow_process(
                host->shadow,
                slot_index,
                event.time,
                event.buffer,
                event.size
            );
            if (result == MUSIC_RIG_RESULT_INVALID_DATA) {
                result = MUSIC_RIG_RESULT_OK;
            }
        }
    }
    if (result == MUSIC_RIG_RESULT_OK && host->output_failed) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    atomic_store_explicit(
        &host->last_process_result, result, memory_order_relaxed
    );
    return 0;
}

static void backend_shutdown(void *opaque)
{
    music_rig_jack_midi_output *host = opaque;

    if (valid_host(host)) {
        atomic_store_explicit(
            &host->backend_shutdown, true, memory_order_release
        );
        atomic_store_explicit(&host->active, false, memory_order_release);
    }
}

static music_rig_result output_prepare(
    void *context,
    const music_rig_generation *generation
)
{
    music_rig_jack_midi_output *host = context;

    if (!valid_attached_host(host) ||
        !atomic_load_explicit(&host->active, memory_order_acquire) ||
        !generation_ports_match(host, generation)) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    host->prepared_generation = generation;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_confirm(
    void *context,
    const music_rig_generation *generation
)
{
    music_rig_jack_midi_output *host = context;

    return valid_attached_host(host) &&
        atomic_load_explicit(&host->active, memory_order_acquire) &&
        host->prepared_generation == generation
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static music_rig_result output_rollback(
    void *context,
    const music_rig_generation *generation
)
{
    music_rig_jack_midi_output *host = context;

    if (!valid_attached_host(host) ||
        !atomic_load_explicit(&host->active, memory_order_acquire) ||
        !generation_ports_match(host, generation)) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->prepared_generation = generation;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_adopted(
    void *context,
    const music_rig_generation *generation,
    uint64_t *adopted_at_ns
)
{
    music_rig_jack_midi_output *host = context;

    if (!valid_attached_host(host) || generation == NULL ||
        adopted_at_ns == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (atomic_load_explicit(
            &host->adopted_generation_id, memory_order_acquire
        ) != generation->id) {
        *adopted_at_ns = UINT64_C(0);
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    *adopted_at_ns = atomic_load_explicit(
        &host->adopted_at_ns, memory_order_acquire
    );
    return *adopted_at_ns == UINT64_C(0)
        ? MUSIC_RIG_RESULT_NOT_FOUND : MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_output_init(
    music_rig_jack_midi_output *host,
    music_rig_generation_slot *generations,
    const music_rig_compiled_tables *tables
)
{
    music_rig_result result;

    if (host == NULL || generations == NULL || tables == NULL ||
        !music_rig_generation_slot_is_lock_free(generations)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(host, 0, sizeof(*host));
    result = music_rig_device_port_catalogue_build(tables, &host->ports);
    if (result != MUSIC_RIG_RESULT_OK || host->ports.count == 0U) {
        return result == MUSIC_RIG_RESULT_OK
            ? MUSIC_RIG_RESULT_INVALID_STATE : result;
    }
    host->abi_version = MUSIC_RIG_JACK_MIDI_OUTPUT_ABI_VERSION;
    host->generations = generations;
    host->port_count = tables->device_profile_count;
    atomic_init(&host->last_process_result, MUSIC_RIG_RESULT_OK);
    atomic_init(&host->active, false);
    atomic_init(&host->backend_shutdown, false);
    atomic_init(&host->adopted_generation_id, UINT64_C(0));
    atomic_init(&host->adopted_at_ns, UINT64_C(0));
    if (!atomic_is_lock_free(&host->last_process_result) ||
        !atomic_is_lock_free(&host->active) ||
        !atomic_is_lock_free(&host->backend_shutdown) ||
        !atomic_is_lock_free(&host->adopted_generation_id) ||
        !atomic_is_lock_free(&host->adopted_at_ns)) {
        memset(host, 0, sizeof(*host));
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_output_observer_init(
    music_rig_jack_midi_output *host,
    music_rig_device_midi_shadow_observer *observer
)
{
    if (!valid_host(host) || observer == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(observer, 0, sizeof(*observer));
    observer->abi_version = MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION;
    observer->context = host;
    observer->output_midi = output_midi;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_output_attach_shadow(
    music_rig_jack_midi_output *host,
    music_rig_device_midi_shadow *shadow
)
{
    const music_rig_generation *published;

    if (!valid_host(host) || shadow == NULL ||
        music_rig_device_midi_shadow_slot_count(shadow) != host->port_count ||
        shadow->observer.output_midi != output_midi) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    host->shadow = shadow;
    published = atomic_load_explicit(
        &host->generations->published, memory_order_acquire
    );
    if (!generation_ports_match(host, published)) {
        host->shadow = NULL;
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_output_adoption_adapter music_rig_jack_midi_output_adapter(
    music_rig_jack_midi_output *host
)
{
    music_rig_output_adoption_adapter adapter;

    memset(&adapter, 0, sizeof(adapter));
    adapter.abi_version = MUSIC_RIG_OUTPUT_ADOPTION_ADAPTER_ABI_VERSION;
    adapter.context = host;
    adapter.prepare = output_prepare;
    adapter.confirm = output_confirm;
    adapter.rollback = output_rollback;
    adapter.adopted = output_adopted;
    return adapter;
}

music_rig_result music_rig_jack_midi_output_start(
    music_rig_jack_midi_output *host
)
{
    jack_client_t *client;
    jack_status_t status = UINT32_C(0);
    size_t index;

    if (!valid_attached_host(host) || host->shadow->observer.output_midi !=
            output_midi) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (host->client != NULL || atomic_load_explicit(&host->active,
            memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    client = jack_client_open(
        "music-rigd-device-output", JACK_NO_START_SERVER, &status
    );
    if (client == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->client = client;
    if (jack_set_process_callback(client, process_cycle, host) != 0) {
        (void)jack_client_close(client);
        host->client = NULL;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    jack_on_shutdown(client, backend_shutdown, host);
    for (index = 0U; index < host->port_count; ++index) {
        host->input_ports[index] = jack_port_register(
            client,
            host->ports.ports[index * 2U].id,
            jack_default_midi_type,
            JACK_PORT_IS_INPUT,
            0UL
        );
        host->output_ports[index] = jack_port_register(
            client,
            host->ports.ports[index * 2U + 1U].id,
            jack_default_midi_type,
            JACK_PORT_IS_OUTPUT,
            0UL
        );
        if (host->input_ports[index] == NULL ||
            host->output_ports[index] == NULL) {
            (void)jack_client_close(client);
            host->client = NULL;
            memset(host->input_ports, 0, sizeof(host->input_ports));
            memset(host->output_ports, 0, sizeof(host->output_ports));
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    atomic_store_explicit(
        &host->backend_shutdown, false, memory_order_release
    );
    atomic_store_explicit(&host->active, true, memory_order_release);
    if (jack_activate(client) != 0) {
        atomic_store_explicit(&host->active, false, memory_order_release);
        (void)jack_client_close(client);
        host->client = NULL;
        memset(host->input_ports, 0, sizeof(host->input_ports));
        memset(host->output_ports, 0, sizeof(host->output_ports));
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_output_reconnect(
    music_rig_jack_midi_output *host
)
{
    jack_client_t *client;

    if (!valid_attached_host(host) ||
        !atomic_load_explicit(&host->backend_shutdown, memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    client = host->client;
    if (client != NULL) {
        if (jack_client_close(client) != 0) {
            host->client = NULL;
            memset(host->input_ports, 0, sizeof(host->input_ports));
            memset(host->output_ports, 0, sizeof(host->output_ports));
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        host->client = NULL;
    }
    memset(host->input_ports, 0, sizeof(host->input_ports));
    memset(host->output_ports, 0, sizeof(host->output_ports));
    memset(host->output_buffers, 0, sizeof(host->output_buffers));
    atomic_store_explicit(
        &host->adopted_generation_id, UINT64_C(0), memory_order_release
    );
    atomic_store_explicit(&host->adopted_at_ns, UINT64_C(0),
        memory_order_release);
    {
        music_rig_result result = music_rig_jack_midi_output_start(host);

        if (result == MUSIC_RIG_RESULT_OK &&
            host->metrics.reconnects != UINT64_MAX) {
            host->metrics.reconnects += UINT64_C(1);
        }
        return result;
    }
}

music_rig_result music_rig_jack_midi_output_stop(
    music_rig_jack_midi_output *host
)
{
    music_rig_result result = MUSIC_RIG_RESULT_OK;
    jack_client_t *client;

    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    client = host->client;
    if (client == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (atomic_load_explicit(&host->active, memory_order_acquire) &&
        jack_deactivate(client) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    atomic_store_explicit(&host->active, false, memory_order_release);
    if (jack_client_close(client) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->client = NULL;
    memset(host->input_ports, 0, sizeof(host->input_ports));
    memset(host->output_ports, 0, sizeof(host->output_ports));
    memset(host->output_buffers, 0, sizeof(host->output_buffers));
    return result;
}

const music_rig_jack_midi_output_metrics *
music_rig_jack_midi_output_metrics_read(
    const music_rig_jack_midi_output *host
)
{
    return valid_host(host) ? &host->metrics : NULL;
}
