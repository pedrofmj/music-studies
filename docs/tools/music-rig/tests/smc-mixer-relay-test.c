#include "music_rig/smc_mixer_relay.h"

#include <stdio.h>
#include <string.h>

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
static const char *const SORTED_TARGETS[] = {
    "equalizer.band-1000-hz.gain",
    "equalizer.band-125-hz.gain",
    "equalizer.band-2000-hz.gain",
    "equalizer.band-250-hz.gain",
    "equalizer.band-4000-hz.gain",
    "equalizer.band-500-hz.gain",
    "equalizer.band-63-hz.gain",
    "equalizer.band-8000-hz.gain"
};

typedef struct emit_capture {
    uint32_t expected_frame;
    uint8_t expected[3];
    size_t count;
    bool mismatch;
    bool fail;
} emit_capture;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "SMC-Mixer relay test failed: %s\n", message); \
        return 1; \
    } \
} while (0)

static void copy_text(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static music_rig_result init_tables(music_rig_compiled_tables *tables)
{
    size_t index;

    memset(tables, 0, sizeof(*tables));
    tables->device_profile_count = UINT32_C(1);
    tables->input_binding_count = UINT32_C(1);
    tables->mapping_count = UINT32_C(8);
    tables->target_binding_count = UINT32_C(8);
    tables->ownership_count = UINT32_C(8);
    copy_text(tables->device_profiles[0].slot, "smc-mixer-main");
    copy_text(tables->device_profiles[0].profile, "eight-band-eq");
    copy_text(
        tables->device_profiles[0].hardware_preset,
        "smc-mixer-current-cc"
    );
    tables->device_profiles[0].readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;
    copy_text(tables->input_bindings[0].slot, "smc-mixer-main");
    copy_text(tables->input_bindings[0].adapter, "mock-midi");
    copy_text(tables->input_bindings[0].identity_strategy, "stable-id");
    copy_text(tables->input_bindings[0].identity_value, "smc-mixer-main");
    tables->input_bindings[0].status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;
    tables->input_bindings[0].endpoint_count = UINT16_C(1);
    copy_text(
        tables->input_bindings[0].endpoints[0].purpose,
        "midi.control-input"
    );
    copy_text(
        tables->input_bindings[0].endpoints[0].locator,
        "mock:smc-mixer"
    );
    for (index = 0U; index < 8U; ++index) {
        music_rig_compiled_mapping *mapping = &tables->mappings[index];
        music_rig_compiled_target_binding *target =
            &tables->target_bindings[index];
        music_rig_compiled_ownership *ownership = &tables->ownership[index];

        copy_text(mapping->mapping, MAPPINGS[index]);
        (void)snprintf(
            mapping->control,
            sizeof(mapping->control),
            "fader-%zu",
            index + 1U
        );
        copy_text(mapping->target, TARGETS[index]);
        mapping->profile_index = UINT16_C(0);
        mapping->event_type = MUSIC_RIG_MIDI_EVENT_CC;
        mapping->edge = MUSIC_RIG_MIDI_EDGE_CHANGE;
        mapping->channel = UINT8_C(1);
        mapping->number = (uint8_t)(UINT8_C(40) + (uint8_t)index);
        mapping->behavior = MUSIC_RIG_CONTROL_BEHAVIOR_ABSOLUTE;
        mapping->transform = MUSIC_RIG_TRANSFORM_DIRECT;
        mapping->relative_encoding = MUSIC_RIG_RELATIVE_ENCODING_NONE;
        mapping->takeover = MUSIC_RIG_TAKEOVER_PICKUP;

        copy_text(target->target, SORTED_TARGETS[index]);
        copy_text(target->adapter, "mock-control");
        copy_text(target->locator, "mock:equalizer");
        target->status = MUSIC_RIG_BINDING_STATUS_AVAILABLE;

        ownership->kind = MUSIC_RIG_OWNERSHIP_KIND_PARAMETER;
        ownership->mode = MUSIC_RIG_OWNERSHIP_MODE_EXCLUSIVE;
        copy_text(ownership->target, TARGETS[index]);
        ownership->owner_count = UINT16_C(1);
        ownership->owners[0].scope =
            MUSIC_RIG_OWNER_SCOPE_DEVICE_PROFILE;
        ownership->owners[0].profile_index = UINT16_C(0);
        copy_text(ownership->owners[0].slot, "smc-mixer-main");
        copy_text(ownership->owners[0].profile, "eight-band-eq");
    }
    return music_rig_compiled_tables_prepare(
        tables,
        UINT32_C(1),
        UINT32_C(8),
        UINT32_C(8),
        UINT32_C(8)
    );
}

static music_rig_result capture_emit(
    void *opaque,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    emit_capture *capture = opaque;

    if (capture->fail) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (frame != capture->expected_frame || message_size != 3U ||
        memcmp(message, capture->expected, 3U) != 0) {
        capture->mismatch = true;
    }
    capture->count += 1U;
    return MUSIC_RIG_RESULT_OK;
}

static int test_exhaustive_parity(void)
{
    static music_rig_compiled_tables tables;
    static music_rig_compiled_tables next_tables;
    static music_rig_compiled_tables invalid_tables;
    music_rig_generation first = {UINT64_C(1), &tables};
    music_rig_generation second = {UINT64_C(2), &next_tables};
    music_rig_generation invalid = {UINT64_C(3), &invalid_tables};
    music_rig_generation_slot generations;
    music_rig_smc_mixer_relay_config config;
    music_rig_smc_mixer_relay relay;
    emit_capture capture = {0};
    const music_rig_smc_mixer_relay_metrics *metrics;
    uint8_t message[3] = {UINT8_C(0xb0), UINT8_C(0), UINT8_C(0)};
    size_t control;
    size_t value;

    CHECK(init_tables(&tables) == MUSIC_RIG_RESULT_OK,
        "fixture initialization");
    CHECK(music_rig_generation_slot_init(&generations, &first) ==
            MUSIC_RIG_RESULT_OK,
        "generation initialization");
    music_rig_smc_mixer_relay_config_init(&config);
    config.generations = &generations;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    config.emit = capture_emit;
    config.emit_context = &capture;
    CHECK(music_rig_smc_mixer_relay_init(&relay, &config) ==
            MUSIC_RIG_RESULT_OK,
        "relay initialization");
    CHECK(strcmp(music_rig_smc_mixer_relay_input_port_id(&relay),
            "device.smc-mixer-main.midi-input") == 0 &&
        strcmp(music_rig_smc_mixer_relay_output_port_id(&relay),
            "device.smc-mixer-main.midi-output") == 0,
        "stable port identities");
    CHECK(music_rig_smc_mixer_relay_begin_cycle(&relay) ==
            MUSIC_RIG_RESULT_OK,
        "first cycle");
    for (control = 0U; control < 8U; ++control) {
        for (value = 0U; value < 128U; ++value) {
            capture.expected_frame = (uint32_t)(control * 128U + value);
            capture.expected[0] = UINT8_C(0xb0);
            capture.expected[1] =
                (uint8_t)(UINT8_C(40) + (uint8_t)control);
            capture.expected[2] = (uint8_t)value;
            memcpy(message, capture.expected, sizeof(message));
            CHECK(music_rig_smc_mixer_relay_process(
                    &relay,
                    capture.expected_frame,
                    message,
                    sizeof(message)
                ) == MUSIC_RIG_RESULT_OK,
                "mapped event relay");
        }
    }
    message[1] = UINT8_C(39);
    CHECK(music_rig_smc_mixer_relay_process(
            &relay, UINT32_C(2000), message, sizeof(message)
        ) == MUSIC_RIG_RESULT_OK,
        "unmapped event rejection");
    message[0] = UINT8_C(0x90);
    message[1] = UINT8_C(40);
    CHECK(music_rig_smc_mixer_relay_process(
            &relay, UINT32_C(2001), message, sizeof(message)
        ) == MUSIC_RIG_RESULT_OK,
        "non-CC event rejection");
    message[0] = UINT8_C(0xb0);
    CHECK(music_rig_smc_mixer_relay_process(
            &relay, UINT32_C(2002), message, 2U
        ) == MUSIC_RIG_RESULT_OK,
        "malformed event rejection");
    capture.fail = true;
    CHECK(music_rig_smc_mixer_relay_process(
            &relay, UINT32_C(2003), message, sizeof(message)
        ) == MUSIC_RIG_RESULT_ADAPTER_FAILURE,
        "output adapter failure propagation");
    capture.fail = false;

    next_tables = tables;
    CHECK(music_rig_smc_mixer_relay_prepare_generation(&relay, &second) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_generation_slot_publish(&generations, &second) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_smc_mixer_relay_begin_cycle(&relay) ==
            MUSIC_RIG_RESULT_OK,
        "valid generation adoption");
    invalid_tables = tables;
    copy_text(invalid_tables.device_profiles[0].profile, "wrong-profile");
    for (control = 0U; control < 8U; ++control) {
        copy_text(
            invalid_tables.ownership[control].owners[0].profile,
            "wrong-profile"
        );
    }
    CHECK(music_rig_compiled_tables_prepare(
            &invalid_tables,
            UINT32_C(1), UINT32_C(8), UINT32_C(8), UINT32_C(8)
        ) == MUSIC_RIG_RESULT_OK &&
        music_rig_smc_mixer_relay_prepare_generation(&relay, &invalid) ==
            MUSIC_RIG_RESULT_INVALID_DATA &&
        music_rig_generation_slot_publish(&generations, &invalid) ==
            MUSIC_RIG_RESULT_OK &&
        music_rig_smc_mixer_relay_begin_cycle(&relay) ==
            MUSIC_RIG_RESULT_INVALID_DATA &&
        relay.tables == NULL &&
        music_rig_smc_mixer_relay_process(
            &relay, UINT32_C(2004), message, sizeof(message)
        ) == MUSIC_RIG_RESULT_INVALID_STATE &&
        music_rig_generation_slot_reclaim(&generations) == &first &&
        music_rig_generation_slot_reclaim(&generations) == &second,
        "non-parity generation fail-closed");

    metrics = music_rig_smc_mixer_relay_metrics_read(&relay);
    CHECK(metrics != NULL && metrics->cycles == UINT64_C(3) &&
        metrics->generation_adoptions == UINT64_C(1) &&
        metrics->input_events == UINT64_C(1028) &&
        metrics->mapped_events == UINT64_C(1025) &&
        metrics->emitted_events == UINT64_C(1024) &&
        metrics->unmapped_events == UINT64_C(2) &&
        metrics->malformed_events == UINT64_C(1) &&
        metrics->adapter_failures == UINT64_C(1) &&
        capture.count == 1024U && !capture.mismatch,
        "exhaustive parity metrics");
    return 0;
}

static int test_fail_closed_initialization(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation = {UINT64_C(1), &tables};
    music_rig_generation_slot generations;
    music_rig_smc_mixer_relay_config config;
    music_rig_smc_mixer_relay relay;
    emit_capture capture = {0};

    CHECK(init_tables(&tables) == MUSIC_RIG_RESULT_OK &&
        music_rig_generation_slot_init(&generations, &generation) ==
            MUSIC_RIG_RESULT_OK,
        "fail-closed fixture");
    music_rig_smc_mixer_relay_config_init(&config);
    config.generations = &generations;
    config.emit = capture_emit;
    config.emit_context = &capture;
    CHECK(music_rig_smc_mixer_relay_init(&relay, &config) ==
            MUSIC_RIG_RESULT_INVALID_ARGUMENT,
        "suppressed mode activation");
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    tables.device_profiles[0].hardware_preset[0] = 'x';
    CHECK(music_rig_smc_mixer_relay_init(&relay, &config) ==
            MUSIC_RIG_RESULT_INVALID_DATA,
        "wrong preset activation");
    CHECK(music_rig_smc_mixer_relay_init(NULL, &config) ==
            MUSIC_RIG_RESULT_INVALID_ARGUMENT &&
        music_rig_smc_mixer_relay_input_port_id(NULL) == NULL &&
        music_rig_smc_mixer_relay_output_port_id(NULL) == NULL &&
        music_rig_smc_mixer_relay_metrics_read(NULL) == NULL,
        "null boundary");
    return 0;
}

int main(void)
{
    if (test_exhaustive_parity() != 0 ||
        test_fail_closed_initialization() != 0) {
        return 1;
    }
    printf(
        "SMC-Mixer compiled relay test: OK (storage=%zu bytes)\n",
        sizeof(music_rig_smc_mixer_relay)
    );
    return 0;
}
