#ifndef MUSIC_RIG_COMPILED_TABLES_H
#define MUSIC_RIG_COMPILED_TABLES_H

#include "music_rig/core.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUSIC_RIG_COMPILED_TABLES_VERSION UINT32_C(1)
#define MUSIC_RIG_IDENTIFIER_CAPACITY ((size_t)65)
#define MUSIC_RIG_SEMANTIC_ID_CAPACITY ((size_t)129)
#define MUSIC_RIG_ADAPTER_ID_CAPACITY ((size_t)129)
#define MUSIC_RIG_IDENTITY_VALUE_CAPACITY ((size_t)257)
#define MUSIC_RIG_LOCATOR_CAPACITY ((size_t)1025)

#define MUSIC_RIG_DEVICE_PROFILE_CAPACITY ((size_t)16)
#define MUSIC_RIG_INPUT_ENDPOINT_CAPACITY ((size_t)4)
#define MUSIC_RIG_MAPPING_CAPACITY ((size_t)128)
#define MUSIC_RIG_MAPPING_DISPATCH_CAPACITY ((size_t)256)
#define MUSIC_RIG_TARGET_BINDING_CAPACITY ((size_t)128)
#define MUSIC_RIG_OWNERSHIP_CAPACITY ((size_t)128)
#define MUSIC_RIG_OWNERS_PER_ENTRY_CAPACITY ((size_t)8)
#define MUSIC_RIG_TABLE_INDEX_NONE UINT16_MAX

typedef enum music_rig_readiness {
    MUSIC_RIG_READINESS_INVALID = 0,
    MUSIC_RIG_READINESS_CONTROL_ONLY = 1,
    MUSIC_RIG_READINESS_PREPARED = 2,
    MUSIC_RIG_READINESS_COLD = 3
} music_rig_readiness;

typedef enum music_rig_midi_event_type {
    MUSIC_RIG_MIDI_EVENT_INVALID = 0,
    MUSIC_RIG_MIDI_EVENT_CC = 1,
    MUSIC_RIG_MIDI_EVENT_NOTE = 2,
    MUSIC_RIG_MIDI_EVENT_PROGRAM_CHANGE = 3
} music_rig_midi_event_type;

typedef enum music_rig_midi_edge {
    MUSIC_RIG_MIDI_EDGE_INVALID = 0,
    MUSIC_RIG_MIDI_EDGE_ANY = 1,
    MUSIC_RIG_MIDI_EDGE_CHANGE = 2,
    MUSIC_RIG_MIDI_EDGE_PRESS = 3,
    MUSIC_RIG_MIDI_EDGE_RELEASE = 4
} music_rig_midi_edge;

typedef enum music_rig_control_behavior {
    MUSIC_RIG_CONTROL_BEHAVIOR_INVALID = 0,
    MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE = 1,
    MUSIC_RIG_CONTROL_BEHAVIOR_MOMENTARY = 2,
    MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE = 3,
    MUSIC_RIG_CONTROL_BEHAVIOR_TOGGLE = 4
} music_rig_control_behavior;

typedef enum music_rig_transform_type {
    MUSIC_RIG_TRANSFORM_INVALID = 0,
    MUSIC_RIG_TRANSFORM_DIRECT = 1,
    MUSIC_RIG_TRANSFORM_SCALE = 2,
    MUSIC_RIG_TRANSFORM_TOGGLE = 3,
    MUSIC_RIG_TRANSFORM_RELATIVE = 4
} music_rig_transform_type;

typedef enum music_rig_relative_encoding {
    MUSIC_RIG_RELATIVE_ENCODING_NONE = 0,
    MUSIC_RIG_RELATIVE_ENCODING_BINARY_OFFSET = 1,
    MUSIC_RIG_RELATIVE_ENCODING_SIGN_MAGNITUDE = 2,
    MUSIC_RIG_RELATIVE_ENCODING_TWOS_COMPLEMENT = 3
} music_rig_relative_encoding;

typedef enum music_rig_takeover_mode {
    MUSIC_RIG_TAKEOVER_NONE = 0,
    MUSIC_RIG_TAKEOVER_JUMP = 1,
    MUSIC_RIG_TAKEOVER_PICKUP = 2,
    MUSIC_RIG_TAKEOVER_SCALED_PICKUP = 3,
    MUSIC_RIG_TAKEOVER_IGNORE_UNTIL_MOVED = 4
} music_rig_takeover_mode;

typedef enum music_rig_binding_status {
    MUSIC_RIG_BINDING_STATUS_INVALID = 0,
    MUSIC_RIG_BINDING_STATUS_AVAILABLE = 1,
    MUSIC_RIG_BINDING_STATUS_UNAVAILABLE = 2
} music_rig_binding_status;

typedef enum music_rig_ownership_kind {
    MUSIC_RIG_OWNERSHIP_KIND_INVALID = 0,
    MUSIC_RIG_OWNERSHIP_KIND_EFFECT = 1,
    MUSIC_RIG_OWNERSHIP_KIND_ENGINE = 2,
    MUSIC_RIG_OWNERSHIP_KIND_FEEDBACK = 3,
    MUSIC_RIG_OWNERSHIP_KIND_HELPER = 4,
    MUSIC_RIG_OWNERSHIP_KIND_MIDI_EVENT = 5,
    MUSIC_RIG_OWNERSHIP_KIND_PARAMETER = 6,
    MUSIC_RIG_OWNERSHIP_KIND_PORT = 7,
    MUSIC_RIG_OWNERSHIP_KIND_ROUTE = 8,
    MUSIC_RIG_OWNERSHIP_KIND_SEMANTIC_CONTROL = 9,
    MUSIC_RIG_OWNERSHIP_KIND_STATE_KEY = 10
} music_rig_ownership_kind;

typedef enum music_rig_ownership_mode {
    MUSIC_RIG_OWNERSHIP_MODE_INVALID = 0,
    MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE = 1,
    MUSIC_RIG_OWNERSHIP_MODE_SHARED_EVENT_DESTINATION = 2,
    MUSIC_RIG_OWNERSHIP_MODE_READ_ONLY = 3
} music_rig_ownership_mode;

typedef enum music_rig_owner_scope {
    MUSIC_RIG_OWNER_SCOPE_INVALID = 0,
    MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE = 1,
    MUSIC_RIG_OWNER_SCOPE_RIG_PROFILE = 2
} music_rig_owner_scope;

typedef struct music_rig_compiled_device_profile {
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char profile[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char hardware_preset[MUSIC_RIG_IDENTIFIER_CAPACITY];
    music_rig_readiness readiness;
} music_rig_compiled_device_profile;

typedef struct music_rig_compiled_input_endpoint {
    char purpose[MUSIC_RIG_SEMANTIC_ID_CAPACITY];
    char locator[MUSIC_RIG_LOCATOR_CAPACITY];
} music_rig_compiled_input_endpoint;

typedef struct music_rig_compiled_input_binding {
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char adapter[MUSIC_RIG_ADAPTER_ID_CAPACITY];
    char identity_strategy[MUSIC_RIG_ADAPTER_ID_CAPACITY];
    char identity_value[MUSIC_RIG_IDENTITY_VALUE_CAPACITY];
    music_rig_binding_status status;
    uint16_t endpoint_count;
    music_rig_compiled_input_endpoint endpoints[
        MUSIC_RIG_INPUT_ENDPOINT_CAPACITY
    ];
} music_rig_compiled_input_binding;

typedef struct music_rig_compiled_mapping {
    char mapping[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char control[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char target[MUSIC_RIG_SEMANTIC_ID_CAPACITY];
    uint16_t profile_index;
    music_rig_midi_event_type event_type;
    music_rig_midi_edge edge;
    uint8_t channel;
    uint8_t number;
    music_rig_control_behavior behavior;
    music_rig_transform_type transform;
    music_rig_relative_encoding relative_encoding;
    music_rig_takeover_mode takeover;
    bool has_switch_values;
    uint8_t off_value;
    uint8_t on_value;
    double input_min;
    double input_max;
    double output_min;
    double output_max;
} music_rig_compiled_mapping;

typedef struct music_rig_compiled_target_binding {
    char target[MUSIC_RIG_SEMANTIC_ID_CAPACITY];
    char adapter[MUSIC_RIG_ADAPTER_ID_CAPACITY];
    char locator[MUSIC_RIG_LOCATOR_CAPACITY];
    music_rig_binding_status status;
} music_rig_compiled_target_binding;

typedef struct music_rig_compiled_owner {
    music_rig_owner_scope scope;
    uint16_t profile_index;
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char profile[MUSIC_RIG_IDENTIFIER_CAPACITY];
} music_rig_compiled_owner;

typedef struct music_rig_compiled_ownership {
    music_rig_ownership_kind kind;
    music_rig_ownership_mode mode;
    char target[MUSIC_RIG_SEMANTIC_ID_CAPACITY];
    uint16_t owner_count;
    music_rig_compiled_owner owners[MUSIC_RIG_OWNERS_PER_ENTRY_CAPACITY];
} music_rig_compiled_ownership;

typedef struct music_rig_compiled_tables {
    uint32_t prepared_version;
    uint32_t device_profile_count;
    uint32_t input_binding_count;
    uint32_t mapping_count;
    uint32_t target_binding_count;
    uint32_t ownership_count;
    music_rig_compiled_device_profile device_profiles[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    music_rig_compiled_input_binding input_bindings[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    music_rig_compiled_mapping mappings[MUSIC_RIG_MAPPING_CAPACITY];
    music_rig_compiled_target_binding target_bindings[
        MUSIC_RIG_TARGET_BINDING_CAPACITY
    ];
    music_rig_compiled_ownership ownership[MUSIC_RIG_OWNERSHIP_CAPACITY];
    uint16_t mapping_dispatch[MUSIC_RIG_MAPPING_DISPATCH_CAPACITY];
} music_rig_compiled_tables;

music_rig_result music_rig_compiled_tables_prepare(
    music_rig_compiled_tables *tables,
    uint32_t expected_device_profiles,
    uint32_t expected_mappings,
    uint32_t expected_target_bindings,
    uint32_t expected_ownership
);

music_rig_result music_rig_compiled_tables_validate(
    const music_rig_compiled_tables *tables,
    uint32_t expected_device_profiles,
    uint32_t expected_mappings,
    uint32_t expected_target_bindings,
    uint32_t expected_ownership
);

music_rig_result music_rig_compiled_profile_index(
    const music_rig_compiled_tables *tables,
    const char *slot,
    uint16_t *profile_index
);

const music_rig_compiled_mapping *music_rig_compiled_mapping_lookup(
    const music_rig_compiled_tables *tables,
    uint16_t profile_index,
    music_rig_midi_event_type event_type,
    uint8_t channel,
    uint8_t number
);

const music_rig_compiled_target_binding *music_rig_compiled_target_lookup(
    const music_rig_compiled_tables *tables,
    const char *target
);

const music_rig_compiled_ownership *music_rig_compiled_ownership_lookup(
    const music_rig_compiled_tables *tables,
    music_rig_ownership_kind kind,
    const char *target
);

#endif
