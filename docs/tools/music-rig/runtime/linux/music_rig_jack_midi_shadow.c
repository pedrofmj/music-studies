#include "music_rig/jack_midi_shadow.h"

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

static bool valid_host(const music_rig_jack_midi_shadow *host)
{
    return host != NULL &&
        host->abi_version == MUSIC_RIG_JACK_MIDI_SHADOW_ABI_VERSION &&
        host->shadow != NULL;
}

static bool valid_output_host(const music_rig_jack_midi_output *host);

static music_rig_result output_prepare(
    void *context,
    const music_rig_generation *generation
)
{
    music_rig_jack_midi_output *host = context;
    (void)generation;
    return valid_output_host(host) ? MUSIC_RIG_RESULT_OK
        : MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}

static music_rig_result output_confirm(
    void *context,
    const music_rig_generation *generation
)
{
    music_rig_jack_midi_output *host = context;
    (void)generation;
    return valid_output_host(host) && host->client != NULL
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static int process_cycle(jack_nframes_t frame_count, void *opaque)
{
    music_rig_jack_midi_shadow *host = opaque;
    music_rig_result result;
    size_t slot_index;

    if (!valid_host(host)) {
        return 0;
    }
    result = music_rig_device_midi_shadow_begin_cycle(host->shadow);
    for (slot_index = 0U;
         result == MUSIC_RIG_RESULT_OK && slot_index < host->port_count;
         ++slot_index) {
        jack_port_t *port = host->input_ports[slot_index];
        void *buffer = jack_port_get_buffer(port, frame_count);
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
            if (result != MUSIC_RIG_RESULT_OK) {
                break;
            }
        }
    }
    atomic_store_explicit(
        &host->last_process_result,
        result,
        memory_order_relaxed
    );
    return 0;
}

static int output_process_cycle(jack_nframes_t frame_count, void *opaque)
{
    music_rig_jack_midi_output *host = opaque;
    music_rig_result result;
    size_t slot_index;

    if (!valid_output_host(host)) {
        return 0;
    }
    result = music_rig_device_midi_shadow_begin_cycle(host->shadow);
    for (slot_index = 0U;
         result == MUSIC_RIG_RESULT_OK && slot_index < host->port_count;
         ++slot_index) {
        host->output_buffers[slot_index] = jack_port_get_buffer(
            host->output_ports[slot_index], frame_count
        );
        if (host->output_buffers[slot_index] == NULL) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    for (slot_index = 0U;
         result == MUSIC_RIG_RESULT_OK && slot_index < host->port_count;
         ++slot_index) {
        void *buffer = jack_port_get_buffer(host->input_ports[slot_index],
            frame_count);
        uint32_t count;
        uint32_t event_index;

        if (buffer == NULL) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            break;
        }
        count = jack_midi_get_event_count(buffer);
        for (event_index = 0U; event_index < count; ++event_index) {
            jack_midi_event_t event;

            if (jack_midi_event_get(&event, buffer, event_index) != 0 ||
                event.buffer == NULL) {
                result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
                break;
            }
            result = music_rig_device_midi_shadow_process(host->shadow,
                slot_index, event.time, event.buffer, event.size);
            if (result == MUSIC_RIG_RESULT_INVALID_DATA) {
                result = MUSIC_RIG_RESULT_OK;
            }
        }
    }
    atomic_store_explicit(&host->last_process_result, result,
        memory_order_relaxed);
    return 0;
}

static void backend_shutdown(void *opaque)
{
    music_rig_jack_midi_shadow *host = opaque;

    if (valid_host(host)) {
        atomic_store_explicit(
            &host->backend_shutdown,
            true,
            memory_order_release
        );
        atomic_store_explicit(&host->active, false, memory_order_release);
    }
}

music_rig_result music_rig_jack_midi_shadow_init(
    music_rig_jack_midi_shadow *host,
    music_rig_device_midi_shadow *shadow
)
{
    size_t port_count;
    size_t index;

    if (host == NULL || shadow == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    port_count = music_rig_device_midi_shadow_slot_count(shadow);
    if (port_count == 0U ||
        port_count > MUSIC_RIG_DEVICE_PROFILE_CAPACITY) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    for (index = 0U; index < port_count; ++index) {
        if (music_rig_device_midi_shadow_input_port_id(shadow, index) == NULL) {
            return MUSIC_RIG_RESULT_INVALID_STATE;
        }
    }
    memset(host, 0, sizeof(*host));
    host->abi_version = MUSIC_RIG_JACK_MIDI_SHADOW_ABI_VERSION;
    host->shadow = shadow;
    host->port_count = port_count;
    atomic_init(&host->last_process_result, MUSIC_RIG_RESULT_OK);
    atomic_init(&host->active, false);
    atomic_init(&host->backend_shutdown, false);
    if (!atomic_is_lock_free(&host->last_process_result) ||
        !atomic_is_lock_free(&host->active) ||
        !atomic_is_lock_free(&host->backend_shutdown)) {
        host->abi_version = UINT32_C(0);
        host->shadow = NULL;
        host->port_count = 0U;
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_shadow_start(
    music_rig_jack_midi_shadow *host
)
{
    jack_client_t *client;
    jack_status_t status = UINT32_C(0);
    size_t index;

    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (host->client != NULL ||
        atomic_load_explicit(&host->active, memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    client = jack_client_open(
        "music-rigd-shadow",
        JACK_NO_START_SERVER,
        &status
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
        const char *port_id = music_rig_device_midi_shadow_input_port_id(
            host->shadow,
            index
        );

        host->input_ports[index] = jack_port_register(
            client,
            port_id,
            jack_default_midi_type,
            JACK_PORT_IS_INPUT,
            0UL
        );
        if (host->input_ports[index] == NULL) {
            (void)jack_client_close(client);
            host->client = NULL;
            memset(host->input_ports, 0, sizeof(host->input_ports));
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    atomic_store_explicit(
        &host->backend_shutdown,
        false,
        memory_order_release
    );
    atomic_store_explicit(&host->active, true, memory_order_release);
    if (jack_activate(client) != 0) {
        atomic_store_explicit(&host->active, false, memory_order_release);
        (void)jack_client_close(client);
        host->client = NULL;
        memset(host->input_ports, 0, sizeof(host->input_ports));
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_shadow_stop(
    music_rig_jack_midi_shadow *host
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
    if (jack_client_close(client) != 0 && result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->client = NULL;
    memset(host->input_ports, 0, sizeof(host->input_ports));
    return result;
}

static bool valid_output_host(const music_rig_jack_midi_output *host)
{
    return host != NULL &&
        host->abi_version == MUSIC_RIG_JACK_MIDI_SHADOW_ABI_VERSION &&
        host->shadow != NULL;
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

    if (!valid_output_host(host) || slot_index >= host->port_count ||
        host->output_buffers[slot_index] == NULL || message == NULL) {
        return;
    }
    target = jack_midi_event_reserve(
        host->output_buffers[slot_index], frame, message_size
    );
    if (target != NULL) {
        memcpy(target, message, message_size);
    }
}

music_rig_result music_rig_jack_midi_output_init(
    music_rig_jack_midi_output *host,
    music_rig_device_midi_shadow *shadow
)
{
    size_t index;

    if (host == NULL || shadow == NULL ||
        music_rig_device_midi_shadow_slot_count(shadow) == 0U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(host, 0, sizeof(*host));
    host->abi_version = MUSIC_RIG_JACK_MIDI_SHADOW_ABI_VERSION;
    host->shadow = shadow;
    host->port_count = music_rig_device_midi_shadow_slot_count(shadow);
    atomic_init(&host->last_process_result, MUSIC_RIG_RESULT_OK);
    atomic_init(&host->active, false);
    atomic_init(&host->backend_shutdown, false);
    for (index = 0U; index < host->port_count; ++index) {
        if (music_rig_device_midi_shadow_input_port_id(shadow, index) == NULL) {
            memset(host, 0, sizeof(*host));
            return MUSIC_RIG_RESULT_INVALID_STATE;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_midi_output_start(
    music_rig_jack_midi_output *host
)
{
    jack_client_t *client;
    jack_status_t status = UINT32_C(0);
    size_t index;

    if (!valid_output_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (host->client != NULL || atomic_load_explicit(&host->active,
            memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    client = jack_client_open("music-rigd-output", JACK_NO_START_SERVER,
        &status);
    if (client == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->client = client;
    if (jack_set_process_callback(client, output_process_cycle, host) != 0) {
        (void)jack_client_close(client);
        host->client = NULL;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    jack_on_shutdown(client, (void (*)(void *))backend_shutdown, host);
    for (index = 0U; index < host->port_count; ++index) {
        host->input_ports[index] = jack_port_register(client,
            music_rig_device_midi_shadow_input_port_id(host->shadow, index),
            jack_default_midi_type, JACK_PORT_IS_INPUT, 0UL);
        host->output_ports[index] = jack_port_register(client,
            music_rig_device_midi_shadow_slot_name(host->shadow, index),
            jack_default_midi_type, JACK_PORT_IS_OUTPUT, 0UL);
        if (host->input_ports[index] == NULL || host->output_ports[index] == NULL) {
            (void)jack_client_close(client);
            host->client = NULL;
            memset(host->input_ports, 0, sizeof(host->input_ports));
            memset(host->output_ports, 0, sizeof(host->output_ports));
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    host->observer.abi_version = MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION;
    host->observer.context = host;
    host->observer.output_midi = output_midi;
    atomic_store_explicit(&host->active, true, memory_order_release);
    if (jack_activate(client) != 0) {
        atomic_store_explicit(&host->active, false, memory_order_release);
        (void)jack_client_close(client);
        host->client = NULL;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
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
    return adapter;
}

music_rig_result music_rig_jack_midi_output_stop(
    music_rig_jack_midi_output *host
)
{
    if (!valid_output_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (host->client == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (atomic_load_explicit(&host->active, memory_order_acquire)) {
        (void)jack_deactivate((jack_client_t *)host->client);
    }
    atomic_store_explicit(&host->active, false, memory_order_release);
    (void)jack_client_close((jack_client_t *)host->client);
    host->client = NULL;
    memset(host->input_ports, 0, sizeof(host->input_ports));
    memset(host->output_ports, 0, sizeof(host->output_ports));
    memset(host->output_buffers, 0, sizeof(host->output_buffers));
    return MUSIC_RIG_RESULT_OK;
}
