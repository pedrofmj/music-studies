#ifndef MUSIC_RIG_JACK_MIDI_OUTPUT_H
#define MUSIC_RIG_JACK_MIDI_OUTPUT_H

#include "music_rig/device_midi_shadow.h"
#include "music_rig/runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>

#define MUSIC_RIG_JACK_MIDI_OUTPUT_ABI_VERSION UINT32_C(1)

typedef struct music_rig_jack_midi_output_metrics {
    uint64_t output_events;
    uint64_t output_reserve_failures;
} music_rig_jack_midi_output_metrics;

/* Caller-owned Linux JACK host. Read status and metrics only after stop. */
typedef struct music_rig_jack_midi_output {
    uint32_t abi_version;
    music_rig_generation_slot *generations;
    music_rig_device_port_catalogue ports;
    music_rig_device_midi_shadow *shadow;
    void *client;
    void *input_ports[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    void *output_ports[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    void *output_buffers[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    const music_rig_generation *prepared_generation;
    music_rig_jack_midi_output_metrics metrics;
    size_t port_count;
    _Atomic music_rig_result last_process_result;
    _Atomic bool active;
    _Atomic bool backend_shutdown;
    bool output_failed;
} music_rig_jack_midi_output;

music_rig_result music_rig_jack_midi_output_init(
    music_rig_jack_midi_output *host,
    music_rig_generation_slot *generations,
    const music_rig_compiled_tables *tables
);

music_rig_result music_rig_jack_midi_output_observer_init(
    music_rig_jack_midi_output *host,
    music_rig_device_midi_shadow_observer *observer
);

music_rig_result music_rig_jack_midi_output_attach_shadow(
    music_rig_jack_midi_output *host,
    music_rig_device_midi_shadow *shadow
);

music_rig_output_adoption_adapter music_rig_jack_midi_output_adapter(
    music_rig_jack_midi_output *host
);

music_rig_result music_rig_jack_midi_output_start(
    music_rig_jack_midi_output *host
);

/* Reopens the client after the JACK server has signalled backend shutdown. */
music_rig_result music_rig_jack_midi_output_reconnect(
    music_rig_jack_midi_output *host
);

music_rig_result music_rig_jack_midi_output_stop(
    music_rig_jack_midi_output *host
);

const music_rig_jack_midi_output_metrics *
music_rig_jack_midi_output_metrics_read(
    const music_rig_jack_midi_output *host
);

#endif
