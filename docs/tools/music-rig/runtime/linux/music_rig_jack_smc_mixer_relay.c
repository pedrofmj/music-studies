#include "music_rig/jack_smc_mixer_relay.h"

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
#define MUSIC_RIG_SMC_MIXER_RELAY_COALESCE_CYCLE_PERIOD UINT32_C(1)

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
extern void jack_midi_clear_buffer(void *port_buffer);
extern int jack_midi_event_write(
    void *port_buffer,
    jack_nframes_t time,
    const jack_midi_data_t *data,
    size_t data_size
);

static bool valid_host(const music_rig_jack_smc_mixer_relay *host)
{
    return host != NULL &&
        host->abi_version == MUSIC_RIG_JACK_SMC_MIXER_RELAY_ABI_VERSION;
}

static music_rig_result emit_output(
    void *opaque,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_jack_smc_mixer_relay *host = opaque;

    if (!valid_host(host) || host->cycle_output_buffer == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    return jack_midi_event_write(
        host->cycle_output_buffer,
        frame,
        message,
        message_size
    ) == 0
        ? MUSIC_RIG_RESULT_OK
        : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static int process_cycle(jack_nframes_t frame_count, void *opaque)
{
    music_rig_jack_smc_mixer_relay *host = opaque;
    music_rig_result result = MUSIC_RIG_RESULT_INVALID_ARGUMENT;

    if (valid_host(host)) {
        void *output_buffer = jack_port_get_buffer(
            host->output_port,
            frame_count
        );

        if (output_buffer == NULL) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        } else {
            void *input_buffer;
            uint32_t event_count;
            uint32_t event_index;

            jack_midi_clear_buffer(output_buffer);
            host->cycle_output_buffer = output_buffer;
            result = music_rig_smc_mixer_relay_begin_cycle(&host->relay);
            input_buffer = result == MUSIC_RIG_RESULT_OK
                ? jack_port_get_buffer(host->input_port, frame_count)
                : NULL;
            if (result == MUSIC_RIG_RESULT_OK && input_buffer == NULL) {
                result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            }
            event_count = input_buffer != NULL
                ? jack_midi_get_event_count(input_buffer)
                : UINT32_C(0);
            for (event_index = 0U;
                 result == MUSIC_RIG_RESULT_OK && event_index < event_count;
                 ++event_index) {
                jack_midi_event_t event;

                if (jack_midi_event_get(
                        &event,
                        input_buffer,
                        event_index
                    ) != 0 || event.buffer == NULL) {
                    result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
                    break;
                }
                result = music_rig_smc_mixer_relay_process(
                    &host->relay,
                    event.time,
                    event.buffer,
                    event.size
                );
            }
            if (result == MUSIC_RIG_RESULT_OK) {
                result = music_rig_smc_mixer_relay_end_cycle(&host->relay);
            }
            host->cycle_output_buffer = NULL;
        }
        if (result != MUSIC_RIG_RESULT_OK) {
            atomic_store_explicit(
                &host->last_process_result,
                result,
                memory_order_relaxed
            );
        }
    }
    return 0;
}

static void backend_shutdown(void *opaque)
{
    music_rig_jack_smc_mixer_relay *host = opaque;

    if (valid_host(host)) {
        host->cycle_output_buffer = NULL;
        atomic_store_explicit(
            &host->backend_shutdown,
            true,
            memory_order_release
        );
        atomic_store_explicit(&host->active, false, memory_order_release);
    }
}

music_rig_result music_rig_jack_smc_mixer_relay_init(
    music_rig_jack_smc_mixer_relay *host,
    music_rig_generation_slot *generations
)
{
    music_rig_smc_mixer_relay_config config;
    music_rig_result result;

    if (host == NULL || generations == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(host, 0, sizeof(*host));
    host->abi_version = MUSIC_RIG_JACK_SMC_MIXER_RELAY_ABI_VERSION;
    music_rig_smc_mixer_relay_config_init(&config);
    config.generations = generations;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.coalesce_per_cycle = true;
    config.coalesce_cycle_period =
        MUSIC_RIG_SMC_MIXER_RELAY_COALESCE_CYCLE_PERIOD;
    config.emit = emit_output;
    config.emit_context = host;
    result = music_rig_smc_mixer_relay_init(&host->relay, &config);
    if (result != MUSIC_RIG_RESULT_OK) {
        memset(host, 0, sizeof(*host));
        return result;
    }
    atomic_init(&host->last_process_result, MUSIC_RIG_RESULT_OK);
    atomic_init(&host->active, false);
    atomic_init(&host->backend_shutdown, false);
    if (!atomic_is_lock_free(&host->last_process_result) ||
        !atomic_is_lock_free(&host->active) ||
        !atomic_is_lock_free(&host->backend_shutdown)) {
        memset(host, 0, sizeof(*host));
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_smc_mixer_relay_start(
    music_rig_jack_smc_mixer_relay *host
)
{
    jack_client_t *client;
    jack_status_t status = UINT32_C(0);

    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (host->client != NULL ||
        atomic_load_explicit(&host->active, memory_order_acquire)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    client = jack_client_open(
        "music-rigd-smc-mixer",
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
    host->input_port = jack_port_register(
        client,
        music_rig_smc_mixer_relay_input_port_id(&host->relay),
        jack_default_midi_type,
        JACK_PORT_IS_INPUT,
        0UL
    );
    host->output_port = jack_port_register(
        client,
        music_rig_smc_mixer_relay_output_port_id(&host->relay),
        jack_default_midi_type,
        JACK_PORT_IS_OUTPUT,
        0UL
    );
    if (host->input_port == NULL || host->output_port == NULL) {
        (void)jack_client_close(client);
        host->client = NULL;
        host->input_port = NULL;
        host->output_port = NULL;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    atomic_store_explicit(
        &host->backend_shutdown,
        false,
        memory_order_release
    );
    atomic_store_explicit(
        &host->last_process_result,
        MUSIC_RIG_RESULT_OK,
        memory_order_relaxed
    );
    atomic_store_explicit(&host->active, true, memory_order_release);
    if (jack_activate(client) != 0) {
        atomic_store_explicit(&host->active, false, memory_order_release);
        (void)jack_client_close(client);
        host->client = NULL;
        host->input_port = NULL;
        host->output_port = NULL;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_jack_smc_mixer_relay_stop(
    music_rig_jack_smc_mixer_relay *host
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
    host->cycle_output_buffer = NULL;
    if (jack_client_close(client) != 0 && result == MUSIC_RIG_RESULT_OK) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->client = NULL;
    host->input_port = NULL;
    host->output_port = NULL;
    return result;
}
