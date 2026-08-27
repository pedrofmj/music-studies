#include "music_rig/device_midi_shadow.h"

#include <limits.h>
#include <string.h>

_Static_assert(
    sizeof(music_rig_device_midi_shadow) <=
        MUSIC_RIG_DEVICE_MIDI_SHADOW_STORAGE_MAX,
    "device/MIDI shadow state exceeds its fixed storage contract"
);

typedef enum parsed_event_result {
    PARSED_EVENT_NOT_MAPPABLE = 0,
    PARSED_EVENT_VALID = 1,
    PARSED_EVENT_MALFORMED = 2
} parsed_event_result;

typedef struct parsed_event {
    music_rig_midi_event_type type;
    uint8_t channel;
    uint8_t number;
    uint8_t value;
    bool pressed;
    bool released;
} parsed_event;

typedef struct smk25_emit_context {
    music_rig_device_midi_shadow *shadow;
    size_t slot_index;
} smk25_emit_context;

static void increment(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        *value += UINT64_C(1);
    }
}

static bool valid_shadow(const music_rig_device_midi_shadow *shadow)
{
    return shadow != NULL &&
        shadow->abi_version == MUSIC_RIG_DEVICE_MIDI_SHADOW_ABI_VERSION;
}

static bool valid_behavior(music_rig_device_midi_shadow_behavior behavior)
{
    return behavior >= MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_NONE &&
        behavior <= MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25;
}

static parsed_event_result parse_event(
    const uint8_t *message,
    size_t message_size,
    parsed_event *event
)
{
    uint8_t status;
    uint8_t type;

    if (message == NULL || message_size == 0U) {
        return PARSED_EVENT_MALFORMED;
    }
    status = message[0];
    if (status < UINT8_C(0x80)) {
        return PARSED_EVENT_MALFORMED;
    }
    if (status >= UINT8_C(0xf0)) {
        return PARSED_EVENT_NOT_MAPPABLE;
    }
    type = status & UINT8_C(0xf0);
    if (type == UINT8_C(0xc0)) {
        if (message_size != 2U || message[1] > UINT8_C(127)) {
            return PARSED_EVENT_MALFORMED;
        }
        event->type = MUSIC_RIG_MIDI_EVENT_PROGRAM_CHANGE;
        event->channel = (uint8_t)((status & UINT8_C(0x0f)) + UINT8_C(1));
        event->number = message[1];
        event->value = message[1];
        event->pressed = true;
        event->released = false;
        return PARSED_EVENT_VALID;
    }
    if (message_size != 3U || message[1] > UINT8_C(127) ||
        message[2] > UINT8_C(127)) {
        return PARSED_EVENT_MALFORMED;
    }
    event->channel = (uint8_t)((status & UINT8_C(0x0f)) + UINT8_C(1));
    event->number = message[1];
    event->value = message[2];
    if (type == UINT8_C(0xb0)) {
        event->type = MUSIC_RIG_MIDI_EVENT_CC;
        event->pressed = message[2] >= UINT8_C(64);
        event->released = message[2] < UINT8_C(64);
        return PARSED_EVENT_VALID;
    }
    if (type == UINT8_C(0x80) || type == UINT8_C(0x90)) {
        bool note_on = type == UINT8_C(0x90) && message[2] > UINT8_C(0);

        event->type = MUSIC_RIG_MIDI_EVENT_NOTE;
        event->pressed = note_on;
        event->released = !note_on;
        if (!note_on) {
            event->value = UINT8_C(0);
        }
        return PARSED_EVENT_VALID;
    }
    return PARSED_EVENT_NOT_MAPPABLE;
}

static bool matches_edge(
    const music_rig_compiled_mapping *mapping,
    const parsed_event *event
)
{
    switch (mapping->edge) {
    case MUSIC_RIG_MIDI_EDGE_ANY:
    case MUSIC_RIG_MIDI_EDGE_CHANGE:
        return true;
    case MUSIC_RIG_MIDI_EDGE_PRESS:
        return event->pressed;
    case MUSIC_RIG_MIDI_EDGE_RELEASE:
        return event->released;
    default:
        return false;
    }
}

static void observe_suppressed(
    music_rig_device_midi_shadow *shadow,
    size_t slot_index,
    size_t route_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_device_midi_suppressed_event event;

    increment(&shadow->metrics.suppressed_midi_events);
    increment(
        &shadow->metrics.slots[slot_index].suppressed_midi_events
    );
    if (shadow->observer.suppressed_midi == NULL) {
        return;
    }
    event.generation_id = shadow->current_generation->id;
    event.slot_index = slot_index;
    event.route_index = route_index;
    event.frame = frame;
    event.message = message;
    event.message_size = message_size;
    shadow->observer.suppressed_midi(shadow->observer.context, &event);
}

static void observe_output(
    music_rig_device_midi_shadow *shadow,
    size_t slot_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    if (shadow->observer.output_midi != NULL) {
        shadow->observer.output_midi(
            shadow->observer.context, slot_index, frame, message, message_size
        );
    }
}

static void smk25_emit(
    void *opaque,
    size_t route_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    smk25_emit_context *context = opaque;

    if (context->shadow->observer.output_midi != NULL) {
        observe_output(context->shadow, context->slot_index, frame,
            message, message_size);
    } else {
        observe_suppressed(context->shadow, context->slot_index, route_index,
            frame, message, message_size);
    }
}

void music_rig_device_midi_shadow_config_init(
    music_rig_device_midi_shadow_config *config
)
{
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->abi_version = MUSIC_RIG_DEVICE_MIDI_SHADOW_ABI_VERSION;
    config->output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    config->observer.abi_version =
        MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION;
    config->arturia_initial_volume = 3;
}

music_rig_result music_rig_device_midi_shadow_configure_behavior(
    music_rig_device_midi_shadow_config *config,
    const music_rig_compiled_tables *tables,
    const char *slot,
    music_rig_device_midi_shadow_behavior behavior
)
{
    uint16_t profile_index;
    music_rig_result result;

    if (config == NULL || !valid_behavior(behavior)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_compiled_profile_index(tables, slot, &profile_index);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    config->behaviors[profile_index] = behavior;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_device_midi_shadow_init(
    music_rig_device_midi_shadow *shadow,
    const music_rig_device_midi_shadow_config *config
)
{
    music_rig_device_port_catalogue ports;
    const music_rig_generation *generation;
    const music_rig_compiled_tables *tables;
    music_rig_result result;
    size_t index;

    if (shadow == NULL || config == NULL ||
        config->abi_version != MUSIC_RIG_DEVICE_MIDI_SHADOW_ABI_VERSION ||
        config->generations == NULL ||
        config->output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED ||
        config->observer.abi_version !=
            MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION ||
        !music_rig_generation_slot_is_lock_free(config->generations)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    generation = music_rig_generation_slot_adopted(config->generations);
    if (generation == NULL || generation->mapping == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    tables = generation->mapping;
    result = music_rig_compiled_tables_validate(
        tables,
        tables->device_profile_count,
        tables->mapping_count,
        tables->target_binding_count,
        tables->ownership_count
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_device_port_catalogue_build(tables, &ports);
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    memset(shadow, 0, sizeof(*shadow));
    shadow->abi_version = MUSIC_RIG_DEVICE_MIDI_SHADOW_ABI_VERSION;
    shadow->generations = config->generations;
    shadow->current_generation = generation;
    shadow->tables = tables;
    shadow->observer = config->observer;
    shadow->slot_count = tables->device_profile_count;
    for (index = 0U; index < shadow->slot_count; ++index) {
        music_rig_device_midi_shadow_slot *shadow_slot = &shadow->slots[index];
        const music_rig_device_port *port = &ports.ports[index * 2U];

        if (!valid_behavior(config->behaviors[index]) ||
            port->direction != MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT) {
            memset(shadow, 0, sizeof(*shadow));
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        memcpy(shadow_slot->slot, port->slot, sizeof(shadow_slot->slot));
        memcpy(
            shadow_slot->input_port_id,
            port->id,
            sizeof(shadow_slot->input_port_id)
        );
        shadow_slot->profile_index = (uint16_t)index;
        shadow_slot->behavior = config->behaviors[index];
        if (shadow_slot->behavior ==
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA) {
            music_rig_current_arturia_config arturia;

            music_rig_current_arturia_config_init(&arturia);
            result = music_rig_current_arturia_init(
                &shadow_slot->state.arturia,
                &arturia,
                config->arturia_initial_volume,
                config->arturia_initial_mute
            );
        } else if (shadow_slot->behavior ==
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25) {
            music_rig_current_smk25_config smk25;

            music_rig_current_smk25_config_init(&smk25);
            result = music_rig_current_smk25_init(
                &shadow_slot->state.smk25,
                &smk25
            );
        }
        if (result != MUSIC_RIG_RESULT_OK) {
            memset(shadow, 0, sizeof(*shadow));
            return result;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_device_midi_shadow_begin_cycle(
    music_rig_device_midi_shadow *shadow
)
{
    const music_rig_generation *generation;
    const music_rig_compiled_tables *tables;

    if (!valid_shadow(shadow)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    generation = music_rig_generation_slot_adopt(shadow->generations);
    if (generation == NULL || generation->mapping == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    tables = generation->mapping;
    if (tables->prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION ||
        tables->device_profile_count != shadow->slot_count) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    increment(&shadow->metrics.cycles);
    if (generation != shadow->current_generation) {
        shadow->current_generation = generation;
        shadow->tables = tables;
        increment(&shadow->metrics.generation_adoptions);
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_device_midi_shadow_process(
    music_rig_device_midi_shadow *shadow,
    size_t slot_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_device_midi_shadow_slot *slot;
    parsed_event event;
    parsed_event_result parsed;
    const music_rig_compiled_mapping *mapping = NULL;
    music_rig_result result = MUSIC_RIG_RESULT_OK;

    if (!valid_shadow(shadow) || message == NULL ||
        slot_index >= shadow->slot_count) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    slot = &shadow->slots[slot_index];
    increment(&shadow->metrics.input_events);
    increment(&shadow->metrics.slots[slot_index].input_events);
    parsed = parse_event(message, message_size, &event);
    if (parsed == PARSED_EVENT_MALFORMED) {
        increment(&shadow->metrics.malformed_events);
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (parsed == PARSED_EVENT_VALID) {
        music_rig_device_midi_mapping_decision decision;

        increment(&shadow->metrics.parsed_events);
        mapping = music_rig_compiled_mapping_lookup(
            shadow->tables,
            slot->profile_index,
            event.type,
            event.channel,
            event.number
        );
        if (mapping != NULL && matches_edge(mapping, &event)) {
            decision.generation_id = shadow->current_generation->id;
            decision.slot_index = slot_index;
            decision.profile_index = slot->profile_index;
            decision.mapping_index = (uint16_t)(mapping - shadow->tables->mappings);
            decision.event_type = event.type;
            decision.channel = event.channel;
            decision.number = event.number;
            decision.value = event.value;
            decision.pressed = event.pressed;
            decision.released = event.released;
            decision.mapping = mapping;
            increment(&shadow->metrics.mapping_decisions);
            increment(
                &shadow->metrics.slots[slot_index].mapping_decisions
            );
            if (shadow->observer.mapping_decision != NULL) {
                shadow->observer.mapping_decision(
                    shadow->observer.context,
                    &decision
                );
            }
        } else {
            increment(&shadow->metrics.unmapped_events);
        }
    } else {
        increment(&shadow->metrics.unmapped_events);
    }

    if (slot->behavior ==
        MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA) {
        music_rig_current_arturia_decision decision;

        result = music_rig_current_arturia_process_midi(
            &slot->state.arturia,
            message,
            message_size,
            &decision
        );
        if (result == MUSIC_RIG_RESULT_OK && decision.output_ready) {
            if (shadow->observer.output_midi != NULL) {
                observe_output(shadow, slot_index, frame, decision.output,
                    sizeof(decision.output));
            } else {
                observe_suppressed(shadow, slot_index, 0U, frame,
                    decision.output, sizeof(decision.output));
            }
        }
    } else if (slot->behavior ==
        MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25) {
        smk25_emit_context context;

        context.shadow = shadow;
        context.slot_index = slot_index;
        result = music_rig_current_smk25_process_midi(
            &slot->state.smk25,
            message,
            message_size,
            frame,
            smk25_emit,
            &context
        );
    }
    return result;
}

size_t music_rig_device_midi_shadow_slot_count(
    const music_rig_device_midi_shadow *shadow
)
{
    return valid_shadow(shadow) ? shadow->slot_count : 0U;
}

const char *music_rig_device_midi_shadow_input_port_id(
    const music_rig_device_midi_shadow *shadow,
    size_t slot_index
)
{
    if (!valid_shadow(shadow) || slot_index >= shadow->slot_count) {
        return NULL;
    }
    return shadow->slots[slot_index].input_port_id;
}

const char *music_rig_device_midi_shadow_slot_name(
    const music_rig_device_midi_shadow *shadow,
    size_t slot_index
)
{
    if (!valid_shadow(shadow) || slot_index >= shadow->slot_count) {
        return NULL;
    }
    return shadow->slots[slot_index].slot;
}

const music_rig_device_midi_shadow_metrics *
music_rig_device_midi_shadow_metrics_read(
    const music_rig_device_midi_shadow *shadow
)
{
    return valid_shadow(shadow) ? &shadow->metrics : NULL;
}
