#include "music_rig/smc_mixer_relay.h"

#include <limits.h>
#include <string.h>

static const char SLOT[] = "smc-mixer-main";
static const char PROFILE[] = "eight-band-eq";
static const char PRESET[] = "smc-mixer-current-cc";
static const char *const MAPPINGS[] = {
    "band-63-hz-gain",
    "band-125-hz-gain",
    "band-250-hz-gain",
    "band-500-hz-gain",
    "band-1000-hz-gain",
    "band-2000-hz-gain",
    "band-4000-hz-gain",
    "band-8000-hz-gain"
};
static const char *const TARGETS[] = {
    "equalizer.band-63-hz.gain",
    "equalizer.band-125-hz.gain",
    "equalizer.band-250-hz.gain",
    "equalizer.band-500-hz.gain",
    "equalizer.band-1000-hz.gain",
    "equalizer.band-2000-hz.gain",
    "equalizer.band-4000-hz.gain",
    "equalizer.band-8000-hz.gain"
};

static void increment(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        *value += UINT64_C(1);
    }
}

static bool valid_relay(const music_rig_smc_mixer_relay *relay)
{
    return relay != NULL &&
        relay->abi_version == MUSIC_RIG_SMC_MIXER_RELAY_ABI_VERSION;
}

static music_rig_result validate_parity_tables(
    const music_rig_compiled_tables *tables,
    uint16_t *profile_index,
    music_rig_device_port_catalogue *ports
)
{
    const music_rig_compiled_device_profile *profile;
    size_t profile_mapping_count = 0U;
    size_t index;
    music_rig_result result;

    result = music_rig_compiled_tables_validate(
        tables,
        tables != NULL ? tables->device_profile_count : UINT32_C(0),
        tables != NULL ? tables->mapping_count : UINT32_C(0),
        tables != NULL ? tables->target_binding_count : UINT32_C(0),
        tables != NULL ? tables->ownership_count : UINT32_C(0)
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = music_rig_compiled_profile_index(tables, SLOT, profile_index);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    profile = &tables->device_profiles[*profile_index];
    if (strcmp(profile->profile, PROFILE) != 0 ||
        strcmp(profile->hardware_preset, PRESET) != 0 ||
        profile->readiness != MUSIC_RIG_READINESS_CONTROL_ONLY ||
        tables->input_bindings[*profile_index].status !=
            MUSIC_RIG_BINDING_STATUS_AVAILABLE) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    for (index = 0U; index < tables->mapping_count; ++index) {
        if (tables->mappings[index].profile_index == *profile_index) {
            profile_mapping_count += 1U;
        }
    }
    if (profile_mapping_count != sizeof(MAPPINGS) / sizeof(MAPPINGS[0])) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    for (index = 0U; index < sizeof(MAPPINGS) / sizeof(MAPPINGS[0]); ++index) {
        const music_rig_compiled_mapping *mapping =
            music_rig_compiled_mapping_lookup(
                tables,
                *profile_index,
                MUSIC_RIG_MIDI_EVENT_CC,
                UINT8_C(1),
                (uint8_t)(UINT8_C(40) + (uint8_t)index)
            );
        const music_rig_compiled_ownership *ownership =
            music_rig_compiled_ownership_lookup(
                tables,
                MUSIC_RIG_OWNERSHIP_KIND_PARAMETER,
                TARGETS[index]
            );
        char expected_control[sizeof("fader-8")];

        expected_control[0] = 'f';
        expected_control[1] = 'a';
        expected_control[2] = 'd';
        expected_control[3] = 'e';
        expected_control[4] = 'r';
        expected_control[5] = '-';
        expected_control[6] = (char)('1' + (char)index);
        expected_control[7] = '\0';
        if (mapping == NULL || strcmp(mapping->mapping, MAPPINGS[index]) != 0 ||
            strcmp(mapping->control, expected_control) != 0 ||
            strcmp(mapping->target, TARGETS[index]) != 0 ||
            mapping->edge != MUSIC_RIG_MIDI_EDGE_CHANGE ||
            mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE ||
            mapping->transform != MUSIC_RIG_TRANSFORM_DIRECT ||
            mapping->relative_encoding != MUSIC_RIG_RELATIVE_ENCODING_NONE ||
            mapping->takeover != MUSIC_RIG_TAKEOVER_PICKUP ||
            ownership == NULL ||
            ownership->mode != MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE ||
            ownership->owner_count != UINT16_C(1) ||
            ownership->owners[0].scope !=
                MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE ||
            ownership->owners[0].profile_index != *profile_index) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    return music_rig_device_port_catalogue_build(tables, ports);
}

void music_rig_smc_mixer_relay_config_init(
    music_rig_smc_mixer_relay_config *config
)
{
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->abi_version = MUSIC_RIG_SMC_MIXER_RELAY_ABI_VERSION;
    config->output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
}

music_rig_result music_rig_smc_mixer_relay_init(
    music_rig_smc_mixer_relay *relay,
    const music_rig_smc_mixer_relay_config *config
)
{
    const music_rig_generation *generation;
    const music_rig_device_port *input;
    const music_rig_device_port *output;
    music_rig_device_port_catalogue ports;
    uint16_t profile_index;
    music_rig_result result;

    if (relay == NULL || config == NULL ||
        config->abi_version != MUSIC_RIG_SMC_MIXER_RELAY_ABI_VERSION ||
        config->generations == NULL || config->emit == NULL ||
        config->output_mode != MUSIC_RIG_OUTPUT_ENABLED ||
        !music_rig_generation_slot_is_lock_free(config->generations)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    generation = music_rig_generation_slot_adopted(config->generations);
    if (generation == NULL || generation->mapping == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = validate_parity_tables(
        generation->mapping,
        &profile_index,
        &ports
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    input = music_rig_device_port_lookup(
        &ports,
        SLOT,
        MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT
    );
    output = music_rig_device_port_lookup(
        &ports,
        SLOT,
        MUSIC_RIG_DEVICE_PORT_DIRECTION_OUTPUT
    );
    if (input == NULL || output == NULL) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    memset(relay, 0, sizeof(*relay));
    relay->abi_version = MUSIC_RIG_SMC_MIXER_RELAY_ABI_VERSION;
    relay->generations = config->generations;
    relay->current_generation = generation;
    relay->current_generation_id = generation->id;
    relay->tables = generation->mapping;
    relay->emit = config->emit;
    relay->emit_context = config->emit_context;
    relay->profile_index = profile_index;
    memcpy(relay->input_port_id, input->id, sizeof(relay->input_port_id));
    memcpy(relay->output_port_id, output->id, sizeof(relay->output_port_id));
    atomic_init(&relay->prepared_generation, generation);
    if (!atomic_is_lock_free(&relay->prepared_generation)) {
        memset(relay, 0, sizeof(*relay));
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_smc_mixer_relay_prepare_generation(
    music_rig_smc_mixer_relay *relay,
    const music_rig_generation *generation
)
{
    const music_rig_device_port *input;
    const music_rig_device_port *output;
    music_rig_device_port_catalogue ports;
    uint16_t profile_index;
    music_rig_result result;

    if (!valid_relay(relay) || generation == NULL ||
        generation->mapping == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = validate_parity_tables(
        generation->mapping,
        &profile_index,
        &ports
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    input = music_rig_device_port_lookup(
        &ports,
        SLOT,
        MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT
    );
    output = music_rig_device_port_lookup(
        &ports,
        SLOT,
        MUSIC_RIG_DEVICE_PORT_DIRECTION_OUTPUT
    );
    if (profile_index != relay->profile_index || input == NULL ||
        output == NULL || strcmp(input->id, relay->input_port_id) != 0 ||
        strcmp(output->id, relay->output_port_id) != 0) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    atomic_store_explicit(
        &relay->prepared_generation,
        generation,
        memory_order_release
    );
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_smc_mixer_relay_begin_cycle(
    music_rig_smc_mixer_relay *relay
)
{
    const music_rig_generation *generation;
    const music_rig_generation *prepared;

    if (!valid_relay(relay)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    generation = music_rig_generation_slot_adopt(relay->generations);
    if (generation == NULL || generation->mapping == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    increment(&relay->metrics.cycles);
    if (generation == relay->current_generation &&
        generation->id == relay->current_generation_id &&
        relay->tables == generation->mapping) {
        return MUSIC_RIG_RESULT_OK;
    }
    prepared = atomic_load_explicit(
        &relay->prepared_generation,
        memory_order_acquire
    );
    if (generation != prepared ||
        generation->mapping == NULL ||
        ((const music_rig_compiled_tables *)generation->mapping)->
            prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION) {
        relay->current_generation = generation;
        relay->current_generation_id = generation->id;
        relay->tables = NULL;
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    relay->current_generation = generation;
    relay->current_generation_id = generation->id;
    relay->tables = generation->mapping;
    increment(&relay->metrics.generation_adoptions);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_smc_mixer_relay_process(
    music_rig_smc_mixer_relay *relay,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    const music_rig_compiled_mapping *mapping;
    music_rig_result result;

    if (!valid_relay(relay) || message == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (relay->tables == NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    increment(&relay->metrics.input_events);
    if (message_size != 3U || message[0] < UINT8_C(0x80) ||
        message[1] > UINT8_C(127) || message[2] > UINT8_C(127)) {
        increment(&relay->metrics.malformed_events);
        return MUSIC_RIG_RESULT_OK;
    }
    if (message[0] != UINT8_C(0xb0)) {
        increment(&relay->metrics.unmapped_events);
        return MUSIC_RIG_RESULT_OK;
    }
    mapping = music_rig_compiled_mapping_lookup(
        relay->tables,
        relay->profile_index,
        MUSIC_RIG_MIDI_EVENT_CC,
        UINT8_C(1),
        message[1]
    );
    if (mapping == NULL || mapping->edge != MUSIC_RIG_MIDI_EDGE_CHANGE) {
        increment(&relay->metrics.unmapped_events);
        return MUSIC_RIG_RESULT_OK;
    }
    increment(&relay->metrics.mapped_events);
    increment(
        &relay->metrics.control_mapped_events[
            (size_t)(message[1] - UINT8_C(40))
        ]
    );
    result = relay->emit(
        relay->emit_context,
        frame,
        message,
        message_size
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        increment(&relay->metrics.emitted_events);
    } else {
        increment(&relay->metrics.adapter_failures);
    }
    return result;
}

const char *music_rig_smc_mixer_relay_input_port_id(
    const music_rig_smc_mixer_relay *relay
)
{
    return valid_relay(relay) ? relay->input_port_id : NULL;
}

const char *music_rig_smc_mixer_relay_output_port_id(
    const music_rig_smc_mixer_relay *relay
)
{
    return valid_relay(relay) ? relay->output_port_id : NULL;
}

const music_rig_smc_mixer_relay_metrics *
music_rig_smc_mixer_relay_metrics_read(
    const music_rig_smc_mixer_relay *relay
)
{
    return valid_relay(relay) ? &relay->metrics : NULL;
}
