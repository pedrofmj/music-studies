#ifndef MUSIC_RIG_DEVICE_MIDI_SHADOW_H
#define MUSIC_RIG_DEVICE_MIDI_SHADOW_H

#include "music_rig/compiled_tables.h"
#include "music_rig/current_arturia.h"
#include "music_rig/current_smk25.h"
#include "music_rig/device_ports.h"
#include "music_rig/state.h"

#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_DEVICE_MIDI_SHADOW_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_DEVICE_MIDI_SHADOW_STORAGE_MAX ((size_t)65536)

typedef enum music_rig_device_midi_shadow_behavior {
    MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_NONE = 0,
    MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA = 1,
    MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25 = 2
} music_rig_device_midi_shadow_behavior;

typedef struct music_rig_device_midi_mapping_decision {
    uint64_t generation_id;
    size_t slot_index;
    uint16_t profile_index;
    uint16_t mapping_index;
    music_rig_midi_event_type event_type;
    uint8_t channel;
    uint8_t number;
    uint8_t value;
    bool pressed;
    bool released;
    const music_rig_compiled_mapping *mapping;
} music_rig_device_midi_mapping_decision;

typedef struct music_rig_device_midi_suppressed_event {
    uint64_t generation_id;
    size_t slot_index;
    size_t route_index;
    uint32_t frame;
    const uint8_t *message;
    size_t message_size;
} music_rig_device_midi_suppressed_event;

typedef void (*music_rig_device_midi_mapping_observer_fn)(
    void *context,
    const music_rig_device_midi_mapping_decision *decision
);

typedef void (*music_rig_device_midi_suppressed_observer_fn)(
    void *context,
    const music_rig_device_midi_suppressed_event *event
);

typedef struct music_rig_device_midi_shadow_observer {
    uint32_t abi_version;
    void *context;
    music_rig_device_midi_mapping_observer_fn mapping_decision;
    music_rig_device_midi_suppressed_observer_fn suppressed_midi;
} music_rig_device_midi_shadow_observer;

typedef struct music_rig_device_midi_shadow_config {
    uint32_t abi_version;
    music_rig_generation_slot *generations;
    music_rig_output_mode output_mode;
    music_rig_device_midi_shadow_observer observer;
    music_rig_device_midi_shadow_behavior behaviors[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    int32_t arturia_initial_volume;
    int32_t arturia_initial_mute;
} music_rig_device_midi_shadow_config;

typedef struct music_rig_device_midi_shadow_metrics {
    uint64_t cycles;
    uint64_t generation_adoptions;
    uint64_t input_events;
    uint64_t parsed_events;
    uint64_t mapping_decisions;
    uint64_t unmapped_events;
    uint64_t malformed_events;
    uint64_t suppressed_midi_events;
} music_rig_device_midi_shadow_metrics;

typedef union music_rig_device_midi_shadow_behavior_state {
    music_rig_current_arturia_behavior arturia;
    music_rig_current_smk25_behavior smk25;
} music_rig_device_midi_shadow_behavior_state;

typedef struct music_rig_device_midi_shadow_slot {
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char input_port_id[MUSIC_RIG_DEVICE_PORT_ID_CAPACITY];
    uint16_t profile_index;
    music_rig_device_midi_shadow_behavior behavior;
    music_rig_device_midi_shadow_behavior_state state;
} music_rig_device_midi_shadow_slot;

/* Caller-owned, single real-time callback state. Read metrics only when stopped. */
typedef struct music_rig_device_midi_shadow {
    uint32_t abi_version;
    music_rig_generation_slot *generations;
    const music_rig_generation *current_generation;
    const music_rig_compiled_tables *tables;
    music_rig_device_midi_shadow_observer observer;
    size_t slot_count;
    music_rig_device_midi_shadow_slot slots[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    music_rig_device_midi_shadow_metrics metrics;
} music_rig_device_midi_shadow;

void music_rig_device_midi_shadow_config_init(
    music_rig_device_midi_shadow_config *config
);

music_rig_result music_rig_device_midi_shadow_configure_behavior(
    music_rig_device_midi_shadow_config *config,
    const music_rig_compiled_tables *tables,
    const char *slot,
    music_rig_device_midi_shadow_behavior behavior
);

music_rig_result music_rig_device_midi_shadow_init(
    music_rig_device_midi_shadow *shadow,
    const music_rig_device_midi_shadow_config *config
);

music_rig_result music_rig_device_midi_shadow_begin_cycle(
    music_rig_device_midi_shadow *shadow
);

music_rig_result music_rig_device_midi_shadow_process(
    music_rig_device_midi_shadow *shadow,
    size_t slot_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
);

size_t music_rig_device_midi_shadow_slot_count(
    const music_rig_device_midi_shadow *shadow
);

const char *music_rig_device_midi_shadow_input_port_id(
    const music_rig_device_midi_shadow *shadow,
    size_t slot_index
);

const music_rig_device_midi_shadow_metrics *
music_rig_device_midi_shadow_metrics_read(
    const music_rig_device_midi_shadow *shadow
);

#endif
