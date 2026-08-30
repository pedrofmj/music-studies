#ifndef MUSIC_RIG_JACK_SMC_MIXER_RELAY_H
#define MUSIC_RIG_JACK_SMC_MIXER_RELAY_H

#include "music_rig/smc_mixer_relay.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

#define MUSIC_RIG_JACK_SMC_MIXER_RELAY_ABI_VERSION UINT32_C(2)

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
