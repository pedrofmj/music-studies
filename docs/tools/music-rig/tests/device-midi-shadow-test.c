#include "music_rig/device_midi_shadow.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <string.h>

#define CAPTURE_CAPACITY ((size_t)128)

typedef struct observer_capture {
    music_rig_device_midi_mapping_decision mappings[CAPTURE_CAPACITY];
    music_rig_device_midi_suppressed_event suppressed[CAPTURE_CAPACITY];
    uint8_t suppressed_messages[CAPTURE_CAPACITY][16];
    size_t mapping_count;
    size_t suppressed_count;
    bool overflow;
} observer_capture;

static void capture_mapping(
    void *opaque,
    const music_rig_device_midi_mapping_decision *decision
)
{
    observer_capture *capture = opaque;

    if (capture->mapping_count >= CAPTURE_CAPACITY) {
        capture->overflow = true;
        return;
    }
    capture->mappings[capture->mapping_count] = *decision;
    capture->mapping_count += 1U;
}

static void capture_suppressed(
    void *opaque,
    const music_rig_device_midi_suppressed_event *event
)
{
    observer_capture *capture = opaque;
    music_rig_device_midi_suppressed_event *stored;

    if (capture->suppressed_count >= CAPTURE_CAPACITY ||
        event->message_size > sizeof(capture->suppressed_messages[0])) {
        capture->overflow = true;
        return;
    }
    stored = &capture->suppressed[capture->suppressed_count];
    *stored = *event;
    memcpy(
        capture->suppressed_messages[capture->suppressed_count],
        event->message,
        event->message_size
    );
    stored->message =
        capture->suppressed_messages[capture->suppressed_count];
    capture->suppressed_count += 1U;
}

static music_rig_result init_shadow_tables(music_rig_compiled_tables *tables)
{
    music_rig_result result = init_compiled_tables_fixture(tables);

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    fixture_copy(tables->device_profiles[0].hardware_preset,
        "arturia-current-rack");
    tables->mappings[0].number = UINT8_C(114);
    tables->mappings[0].behavior = MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE;
    tables->mappings[0].transform = MUSIC_RIG_TRANSFORM_RELATIVE;
    tables->mappings[0].relative_encoding =
        MUSIC_RIG_RELATIVE_ENCODING_BINARY_OFFSET;
    tables->mappings[0].takeover = MUSIC_RIG_TAKEOVER_NONE;

    fixture_copy(tables->device_profiles[1].slot, "smk25-main");
    fixture_copy(tables->device_profiles[1].profile, "ambient-pad-layers");
    fixture_copy(tables->device_profiles[1].hardware_preset,
        "smk25-current-pad-layers");
    fixture_copy(tables->input_bindings[1].slot, "smk25-main");
    fixture_copy(tables->input_bindings[1].identity_value, "smk25-main");
    fixture_copy(tables->ownership[1].owners[0].slot, "smk25-main");
    fixture_copy(
        tables->ownership[1].owners[0].profile,
        "ambient-pad-layers"
    );
    tables->mappings[1].number = UINT8_C(20);
    return music_rig_compiled_tables_prepare(
        tables,
        UINT32_C(2),
        UINT32_C(2),
        UINT32_C(2),
        UINT32_C(2)
    );
}

static int test_shadow_processing(void)
{
    static music_rig_compiled_tables tables;
    static music_rig_compiled_tables next_tables;
    music_rig_generation first;
    music_rig_generation second;
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow_config config;
    music_rig_device_midi_shadow shadow;
    observer_capture capture = {0};
    const music_rig_device_midi_shadow_metrics *metrics;
    uint8_t event[3];
    size_t before;

    if (init_shadow_tables(&tables) != MUSIC_RIG_RESULT_OK) {
        fputs("shadow tables failed\n", stderr);
        return 1;
    }
    first.id = UINT64_C(1);
    first.mapping = &tables;
    if (music_rig_generation_slot_init(&generations, &first) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("shadow generation failed\n", stderr);
        return 1;
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = &generations;
    config.arturia_initial_volume = 64;
    config.observer.context = &capture;
    config.observer.mapping_decision = capture_mapping;
    config.observer.suppressed_midi = capture_suppressed;
    if (music_rig_device_midi_shadow_configure_behavior(
            &config,
            &tables,
            "arturia-main",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_configure_behavior(
            &config,
            &tables,
            "smk25-main",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_init(&shadow, &config) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_slot_count(&shadow) != 2U ||
        strcmp(
            music_rig_device_midi_shadow_input_port_id(&shadow, 0U),
            "device.arturia-main.midi-input"
        ) != 0 ||
        strcmp(
            music_rig_device_midi_shadow_input_port_id(&shadow, 1U),
            "device.smk25-main.midi-input"
        ) != 0) {
        fputs("shadow initialization failed\n", stderr);
        return 1;
    }
    if (music_rig_device_midi_shadow_begin_cycle(&shadow) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("shadow cycle failed\n", stderr);
        return 1;
    }

    event[0] = UINT8_C(0xb0);
    event[1] = UINT8_C(114);
    event[2] = UINT8_C(65);
    if (music_rig_device_midi_shadow_process(
            &shadow, 0U, UINT32_C(7), event, sizeof(event)
        ) != MUSIC_RIG_RESULT_OK || capture.mapping_count != 1U ||
        capture.suppressed_count != 1U ||
        capture.mappings[0].mapping_index != UINT16_C(0) ||
        capture.mappings[0].value != UINT8_C(65) ||
        capture.suppressed[0].slot_index != 0U ||
        capture.suppressed[0].frame != UINT32_C(7) ||
        capture.suppressed_messages[0][1] != UINT8_C(119) ||
        capture.suppressed_messages[0][2] != UINT8_C(65)) {
        fputs("Arturia shadow decision failed\n", stderr);
        return 1;
    }

    event[0] = UINT8_C(0xb0);
    event[1] = UINT8_C(20);
    event[2] = UINT8_C(91);
    if (music_rig_device_midi_shadow_process(
            &shadow, 1U, UINT32_C(8), event, sizeof(event)
        ) != MUSIC_RIG_RESULT_OK || capture.mapping_count != 2U ||
        capture.suppressed_count != 2U ||
        capture.mappings[1].slot_index != 1U ||
        capture.suppressed[1].route_index != 0U ||
        capture.suppressed_messages[1][1] != UINT8_C(20) ||
        capture.suppressed_messages[1][2] != UINT8_C(91)) {
        fputs("SMK-25 knob shadow decision failed\n", stderr);
        return 1;
    }

    before = capture.suppressed_count;
    event[0] = UINT8_C(0x90);
    event[1] = UINT8_C(60);
    event[2] = UINT8_C(100);
    if (music_rig_device_midi_shadow_process(
            &shadow, 1U, UINT32_C(9), event, sizeof(event)
        ) != MUSIC_RIG_RESULT_OK || capture.mapping_count != 2U ||
        capture.suppressed_count != before + 8U) {
        fputs("SMK-25 passthrough suppression failed\n", stderr);
        return 1;
    }

    event[0] = UINT8_C(0xb0);
    event[1] = UINT8_C(20);
    event[2] = UINT8_C(128);
    if (music_rig_device_midi_shadow_process(
            &shadow, 1U, UINT32_C(10), event, sizeof(event)
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("malformed shadow MIDI was accepted\n", stderr);
        return 1;
    }

    next_tables = tables;
    next_tables.mappings[0].number = UINT8_C(115);
    if (music_rig_compiled_tables_prepare(
            &next_tables,
            UINT32_C(2), UINT32_C(2), UINT32_C(2), UINT32_C(2)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("next shadow table failed\n", stderr);
        return 1;
    }
    second.id = UINT64_C(2);
    second.mapping = &next_tables;
    if (music_rig_generation_slot_publish(&generations, &second) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_begin_cycle(&shadow) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("shadow generation adoption failed\n", stderr);
        return 1;
    }
    event[0] = UINT8_C(0xb0);
    event[1] = UINT8_C(115);
    event[2] = UINT8_C(127);
    if (music_rig_device_midi_shadow_process(
            &shadow, 0U, UINT32_C(11), event, sizeof(event)
        ) != MUSIC_RIG_RESULT_OK || capture.mapping_count != 3U ||
        capture.mappings[2].generation_id != UINT64_C(2) ||
        capture.suppressed_messages[capture.suppressed_count - 1U][1] !=
            UINT8_C(118)) {
        fputs("adopted mapping decision failed\n", stderr);
        return 1;
    }

    metrics = music_rig_device_midi_shadow_metrics_read(&shadow);
    if (metrics == NULL || metrics->cycles != UINT64_C(2) ||
        metrics->generation_adoptions != UINT64_C(1) ||
        metrics->input_events != UINT64_C(5) ||
        metrics->parsed_events != UINT64_C(4) ||
        metrics->mapping_decisions != UINT64_C(3) ||
        metrics->unmapped_events != UINT64_C(1) ||
        metrics->malformed_events != UINT64_C(1) ||
        metrics->suppressed_midi_events != capture.suppressed_count ||
        capture.overflow) {
        fputs("shadow metrics failed\n", stderr);
        return 1;
    }
    return 0;
}

static int test_fail_closed_boundaries(void)
{
    static music_rig_compiled_tables tables;
    music_rig_generation generation;
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow_config config;
    music_rig_device_midi_shadow shadow;

    if (init_shadow_tables(&tables) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    generation.id = UINT64_C(1);
    generation.mapping = &tables;
    if (music_rig_generation_slot_init(&generations, &generation) !=
            MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = &generations;
    config.output_mode = MUSIC_RIG_OUTPUT_ENABLED;
    if (music_rig_device_midi_shadow_init(&shadow, &config) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("output-enabled shadow was accepted\n", stderr);
        return 1;
    }
    config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    config.observer.abi_version = UINT32_C(2);
    if (music_rig_device_midi_shadow_init(&shadow, &config) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_device_midi_shadow_configure_behavior(
            &config, &tables, "missing",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_NONE
        ) != MUSIC_RIG_RESULT_NOT_FOUND ||
        music_rig_device_midi_shadow_input_port_id(NULL, 0U) != NULL) {
        fputs("invalid shadow boundary was accepted\n", stderr);
        return 1;
    }
    config.observer.abi_version =
        MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION;
    config.behaviors[0] = (music_rig_device_midi_shadow_behavior)99;
    if (music_rig_device_midi_shadow_init(&shadow, &config) !=
            MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_device_midi_shadow_slot_count(&shadow) != 0U) {
        fputs("partial shadow initialization did not fail closed\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_shadow_processing() != 0 ||
        test_fail_closed_boundaries() != 0) {
        return 1;
    }
    printf(
        "Device/MIDI shadow test: OK (storage=%zu bytes)\n",
        sizeof(music_rig_device_midi_shadow)
    );
    return 0;
}
