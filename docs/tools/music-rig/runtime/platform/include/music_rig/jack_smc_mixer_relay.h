#ifndef MUSIC_RIG_JACK_SMC_MIXER_RELAY_H
#define MUSIC_RIG_JACK_SMC_MIXER_RELAY_H

#include "music_rig/smc_mixer_relay.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

#define MUSIC_RIG_JACK_SMC_MIXER_RELAY_ABI_VERSION UINT32_C(3)
#define MUSIC_RIG_JACK_SMC_MIXER_RELAY_TRACE_SECOND_CAP UINT32_C(900)

typedef struct music_rig_jack_smc_mixer_relay_trace {
    uint64_t second;
    uint64_t input_events;
    uint64_t mapped_events;
    uint64_t emitted_events;
    uint64_t coalesced_events;
    uint64_t unmapped_events;
    uint64_t malformed_events;
    uint64_t adapter_failures;
} music_rig_jack_smc_mixer_relay_trace;

/* Caller-owned Linux JACK host. Read metrics only after stop. */
typedef struct music_rig_jack_smc_mixer_relay {
    uint32_t abi_version;
    music_rig_smc_mixer_relay relay;
    void *client;
    void *input_port;
    void *output_port;
    void *cycle_output_buffer;
    _Atomic music_rig_result last_process_result;
    _Atomic bool active;
    _Atomic bool backend_shutdown;
    uint32_t trace_sample_rate;
    uint64_t trace_start_epoch;
    uint64_t trace_elapsed_frames;
    uint64_t trace_last_second;
    music_rig_jack_smc_mixer_relay_trace trace[
        MUSIC_RIG_JACK_SMC_MIXER_RELAY_TRACE_SECOND_CAP
    ];
} music_rig_jack_smc_mixer_relay;

music_rig_result music_rig_jack_smc_mixer_relay_init(
    music_rig_jack_smc_mixer_relay *host,
    music_rig_generation_slot *generations
);

music_rig_result music_rig_jack_smc_mixer_relay_start(
    music_rig_jack_smc_mixer_relay *host
);

music_rig_result music_rig_jack_smc_mixer_relay_stop(
    music_rig_jack_smc_mixer_relay *host
);

#endif
