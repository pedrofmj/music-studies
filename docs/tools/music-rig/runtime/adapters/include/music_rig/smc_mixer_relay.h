#ifndef MUSIC_RIG_SMC_MIXER_RELAY_H
#define MUSIC_RIG_SMC_MIXER_RELAY_H

#include "music_rig/compiled_tables.h"
#include "music_rig/device_ports.h"
#include "music_rig/state.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

#define MUSIC_RIG_SMC_MIXER_RELAY_ABI_VERSION UINT32_C(4)
#define MUSIC_RIG_SMC_MIXER_CONTROL_COUNT UINT32_C(8)

typedef music_rig_result (*music_rig_smc_mixer_emit_fn)(
    void *context,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
);

typedef struct music_rig_smc_mixer_relay_config {
    uint32_t abi_version;
    music_rig_generation_slot *generations;
    music_rig_output_mode output_mode;
    bool coalesce_per_cycle;
    music_rig_smc_mixer_emit_fn emit;
    void *emit_context;
} music_rig_smc_mixer_relay_config;

typedef struct music_rig_smc_mixer_relay_metrics {
    uint64_t cycles;
    uint64_t generation_adoptions;
    uint64_t input_events;
    uint64_t mapped_events;
    uint64_t control_mapped_events[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT];
    uint64_t emitted_events;
    uint64_t coalesced_events;
    uint64_t duplicate_events;
    uint64_t unmapped_events;
    uint64_t malformed_events;
    uint64_t adapter_failures;
} music_rig_smc_mixer_relay_metrics;

/* Caller-owned, single real-time callback state. Read metrics only when stopped. */
typedef struct music_rig_smc_mixer_relay {
    uint32_t abi_version;
    music_rig_generation_slot *generations;
    const music_rig_generation *current_generation;
    uint64_t current_generation_id;
    _Atomic(const music_rig_generation *) prepared_generation;
    _Atomic(const music_rig_generation *) adopted_generation;
    const music_rig_compiled_tables *tables;
    music_rig_smc_mixer_emit_fn emit;
    void *emit_context;
    uint16_t profile_index;
    bool coalesce_per_cycle;
    bool pending_controls[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT];
    uint8_t pending_order[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT];
    uint8_t pending_count;
    uint32_t pending_frames[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT];
    uint8_t pending_messages[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT][3];
    bool last_emitted_controls[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT];
    uint8_t last_emitted_messages[MUSIC_RIG_SMC_MIXER_CONTROL_COUNT][3];
    char input_port_id[MUSIC_RIG_DEVICE_PORT_ID_CAPACITY];
    char output_port_id[MUSIC_RIG_DEVICE_PORT_ID_CAPACITY];
    music_rig_smc_mixer_relay_metrics metrics;
} music_rig_smc_mixer_relay;

void music_rig_smc_mixer_relay_config_init(
    music_rig_smc_mixer_relay_config *config
);

music_rig_result music_rig_smc_mixer_relay_init(
    music_rig_smc_mixer_relay *relay,
    const music_rig_smc_mixer_relay_config *config
);

music_rig_result music_rig_smc_mixer_relay_begin_cycle(
    music_rig_smc_mixer_relay *relay
);

music_rig_result music_rig_smc_mixer_relay_end_cycle(
    music_rig_smc_mixer_relay *relay
);

/* Control-thread validation required before publishing another generation. */
music_rig_result music_rig_smc_mixer_relay_prepare_generation(
    music_rig_smc_mixer_relay *relay,
    const music_rig_generation *generation
);

music_rig_result music_rig_smc_mixer_relay_process(
    music_rig_smc_mixer_relay *relay,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
);

const char *music_rig_smc_mixer_relay_input_port_id(
    const music_rig_smc_mixer_relay *relay
);
bool music_rig_smc_mixer_relay_generation_is_adopted(
    const music_rig_smc_mixer_relay *relay,
    const music_rig_generation *generation
);


const char *music_rig_smc_mixer_relay_output_port_id(
    const music_rig_smc_mixer_relay *relay
);

const music_rig_smc_mixer_relay_metrics *
music_rig_smc_mixer_relay_metrics_read(
    const music_rig_smc_mixer_relay *relay
);

#endif
