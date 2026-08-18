#include "music_rig/definition.h"
#include "music_rig/definition_json.h"
#include "music_rig/device_midi_shadow.h"
#include "music_rig/device_ports.h"

#include <stdio.h>
#include <string.h>

#define DOCUMENT_CAPACITY ((size_t)131072)

typedef struct document_source {
    uint8_t content[DOCUMENT_CAPACITY];
    size_t size;
} document_source;

static uint8_t *find_text(
    uint8_t *document,
    size_t document_size,
    const char *text
)
{
    size_t text_size = strlen(text);
    size_t index;

    if (text_size == 0 || text_size > document_size) {
        return NULL;
    }
    for (index = 0; index <= document_size - text_size; ++index) {
        if (memcmp(&document[index], text, text_size) == 0) {
            return &document[index];
        }
    }
    return NULL;
}

static int read_file(const char *path, document_source *source)
{
    FILE *stream;
    long length;

#if defined(_MSC_VER)
    if (fopen_s(&stream, path, "rb") != 0) {
        stream = NULL;
    }
#else
    stream = fopen(path, "rb");
#endif
    if (stream == NULL || fseek(stream, 0, SEEK_END) != 0 ||
        (length = ftell(stream)) <= 0 ||
        (unsigned long)length >= (unsigned long)sizeof(source->content) ||
        fseek(stream, 0, SEEK_SET) != 0 ||
        fread(source->content, 1U, (size_t)length, stream) != (size_t)length) {
        if (stream != NULL) {
            fclose(stream);
        }
        return 0;
    }
    fclose(stream);
    source->size = (size_t)length;
    return 1;
}

static music_rig_result source_read(
    void *opaque,
    music_rig_storage_object object,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
)
{
    document_source *source = opaque;

    if (object != MUSIC_RIG_STORAGE_COMPILED_DEFINITION) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (output_capacity < source->size) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, source->content, source->size);
    *output_size = source->size;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result source_replace(
    void *opaque,
    music_rig_storage_object object,
    const uint8_t *input,
    size_t input_size
)
{
    (void)opaque;
    (void)object;
    (void)input;
    (void)input_size;
    return MUSIC_RIG_RESULT_UNSUPPORTED;
}

static int test_full_definition_shadow(
    const music_rig_compiled_tables *tables,
    const music_rig_generation *generation
)
{
    music_rig_generation_slot generations;
    music_rig_device_midi_shadow_config config;
    music_rig_device_midi_shadow shadow;
    const music_rig_device_midi_shadow_metrics *metrics;
    static const char *const expected_ports[] = {
        "device.arturia-main.midi-input",
        "device.smc-mixer-main.midi-input",
        "device.smc-pad-main.midi-input",
        "device.smc-pad-pocket.midi-input",
        "device.smk25-main.midi-input"
    };
    static const uint8_t events[][3] = {
        {UINT8_C(0xb0), UINT8_C(114), UINT8_C(65)},
        {UINT8_C(0xb0), UINT8_C(40), UINT8_C(64)},
        {UINT8_C(0x99), UINT8_C(36), UINT8_C(100)},
        {UINT8_C(0x99), UINT8_C(36), UINT8_C(100)},
        {UINT8_C(0xb0), UINT8_C(20), UINT8_C(91)}
    };
    size_t index;

    if (music_rig_generation_slot_init(&generations, generation) !=
            MUSIC_RIG_RESULT_OK) {
        return 0;
    }
    music_rig_device_midi_shadow_config_init(&config);
    config.generations = &generations;
    if (music_rig_device_midi_shadow_configure_behavior(
            &config,
            tables,
            "arturia-main",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_ARTURIA
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_configure_behavior(
            &config,
            tables,
            "smk25-main",
            MUSIC_RIG_DEVICE_MIDI_SHADOW_BEHAVIOR_CURRENT_SMK25
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_init(&shadow, &config) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_device_midi_shadow_slot_count(&shadow) !=
            sizeof(expected_ports) / sizeof(expected_ports[0])) {
        return 0;
    }
    for (index = 0U;
         index < sizeof(expected_ports) / sizeof(expected_ports[0]);
         ++index) {
        if (strcmp(
                music_rig_device_midi_shadow_input_port_id(&shadow, index),
                expected_ports[index]
            ) != 0) {
            return 0;
        }
    }
    if (music_rig_device_midi_shadow_begin_cycle(&shadow) !=
            MUSIC_RIG_RESULT_OK) {
        return 0;
    }
    for (index = 0U; index < sizeof(events) / sizeof(events[0]); ++index) {
        if (music_rig_device_midi_shadow_process(
                &shadow,
                index,
                (uint32_t)index,
                events[index],
                sizeof(events[index])
            ) != MUSIC_RIG_RESULT_OK) {
            return 0;
        }
    }
    metrics = music_rig_device_midi_shadow_metrics_read(&shadow);
    return metrics != NULL && metrics->cycles == UINT64_C(1) &&
        metrics->input_events == UINT64_C(5) &&
        metrics->parsed_events == UINT64_C(5) &&
        metrics->mapping_decisions == UINT64_C(5) &&
        metrics->unmapped_events == UINT64_C(0) &&
        metrics->malformed_events == UINT64_C(0) &&
        metrics->suppressed_midi_events == UINT64_C(2) &&
        metrics->slots[0].mapping_decisions == UINT64_C(1) &&
        metrics->slots[1].mapping_decisions == UINT64_C(1) &&
        metrics->slots[2].mapping_decisions == UINT64_C(1) &&
        metrics->slots[3].mapping_decisions == UINT64_C(1) &&
        metrics->slots[4].mapping_decisions == UINT64_C(1) &&
        metrics->slots[0].suppressed_midi_events == UINT64_C(1) &&
        metrics->slots[4].suppressed_midi_events == UINT64_C(1);
}

int main(int argc, char **argv)
{
    static const char fingerprint_text[] =
        "sha256:e43fa6ad1c16b3672a997e9f620448fd"
        "bbc1fb0c141361049ca67d49faab5114";
    static uint8_t workspace[DOCUMENT_CAPACITY];
    static document_source source;
    static music_rig_compiled_tables tables;
    music_rig_storage_adapter storage;
    music_rig_definition_decoder decoder;
    music_rig_compiled_definition definition;
    music_rig_generation generation;
    music_rig_device_port_catalogue ports;
    const music_rig_compiled_mapping *mapping;
    const music_rig_compiled_target_binding *target;
    const music_rig_compiled_ownership *ownership;
    uint8_t *mutation;
    uint16_t profile_index;
    uint8_t expected_fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE];

    if (argc != 2 || !read_file(argv[1], &source)) {
        fputs("compiled definition fixture read failed\n", stderr);
        return 1;
    }
    if (music_rig_definition_fingerprint_parse(
            fingerprint_text,
            sizeof(fingerprint_text) - 1U,
            expected_fingerprint,
            sizeof(expected_fingerprint)
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("compiled definition expected fingerprint is invalid\n", stderr);
        return 1;
    }

    storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    storage.context = &source;
    storage.read = source_read;
    storage.atomic_replace = source_replace;
    decoder.context = NULL;
    decoder.decode = music_rig_definition_json_decode;
    if (music_rig_definition_load(
            &storage,
            &decoder,
            workspace,
            sizeof(workspace),
            expected_fingerprint,
            sizeof(expected_fingerprint),
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_definition_generation_init(
            &definition,
            &tables,
            &generation
        ) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_device_port_catalogue_build(&tables, &ports) !=
            MUSIC_RIG_RESULT_OK ||
        generation.id != UINT64_C(1) || generation.mapping != &tables ||
        strcmp(definition.rig_id, "pedro-performance-rig") != 0 ||
        strcmp(definition.active_rig_profile, "full-live-rack") != 0 ||
        strcmp(definition.platform_binding_id, "airstar-current") != 0 ||
        strcmp(definition.platform, "linux") != 0 ||
        definition.device_profile_count != UINT32_C(5) ||
        definition.mapping_count != UINT32_C(72) ||
        definition.target_binding_count != UINT32_C(71) ||
        definition.ownership_count != UINT32_C(57) ||
        tables.prepared_version != MUSIC_RIG_COMPILED_TABLES_VERSION ||
        tables.input_binding_count != UINT32_C(5) || ports.count != 10U ||
        strcmp(ports.ports[0].id,
            "device.arturia-main.midi-input") != 0 ||
        strcmp(ports.ports[9].id,
            "device.smk25-main.midi-output") != 0) {
        fputs("compiled definition load contract failed\n", stderr);
        return 1;
    }

    if (music_rig_compiled_profile_index(
            &tables,
            "arturia-main",
            &profile_index
        ) != MUSIC_RIG_RESULT_OK ||
        profile_index != UINT16_C(0) ||
        (mapping = music_rig_compiled_mapping_lookup(
            &tables,
            profile_index,
            MUSIC_RIG_MIDI_EVENT_CC,
            UINT8_C(1),
            UINT8_C(114)
        )) == NULL ||
        strcmp(mapping->mapping, "master-volume") != 0 ||
        strcmp(mapping->target, "master.volume") != 0 ||
        mapping->behavior != MUSIC_RIG_CONTROL_BEHAVIOR_RELATIVE ||
        mapping->transform != MUSIC_RIG_TRANSFORM_RELATIVE ||
        mapping->relative_encoding !=
            MUSIC_RIG_RELATIVE_ENCODING_BINARY_OFFSET ||
        music_rig_compiled_mapping_lookup(
            &tables,
            profile_index,
            MUSIC_RIG_MIDI_EVENT_CC,
            UINT8_C(1),
            UINT8_C(113)
        ) != NULL) {
        fputs("Arturia direct dispatch table is invalid\n", stderr);
        return 1;
    }

    if (music_rig_compiled_profile_index(
            &tables,
            "smc-pad-pocket",
            &profile_index
        ) != MUSIC_RIG_RESULT_OK ||
        (mapping = music_rig_compiled_mapping_lookup(
            &tables,
            profile_index,
            MUSIC_RIG_MIDI_EVENT_NOTE,
            UINT8_C(10),
            UINT8_C(36)
        )) == NULL ||
        strcmp(mapping->control, "performance-pad-13") != 0 ||
        strcmp(mapping->target, "drum-set.notes") != 0) {
        fputs("Pocket direct dispatch table is invalid\n", stderr);
        return 1;
    }

    target = music_rig_compiled_target_lookup(
        &tables,
        "parameter.master-volume"
    );
    ownership = music_rig_compiled_ownership_lookup(
        &tables,
        MUSIC_RIG_OWNERSHIP_KIND_ENGINE,
        "engine.drum-set"
    );
    if (target == NULL ||
        strcmp(target->adapter, "linux.current-rack-controls") != 0 ||
        strcmp(
            target->locator,
            "arturia-main-volume-encoder.service:master-volume"
        ) != 0 ||
        ownership == NULL ||
        ownership->mode != MUSIC_RIG_OWNERSHIP_MODE_READ_ONLY ||
        ownership->owner_count != UINT16_C(2) ||
        tables.input_bindings[4].endpoint_count != UINT16_C(2) ||
        strcmp(tables.input_bindings[4].slot, "smk25-main") != 0) {
        fputs("compiled target, ownership, or input table is invalid\n",
            stderr);
        return 1;
    }
    if (!test_full_definition_shadow(&tables, &generation)) {
        fputs("full five-device shadow processing failed\n", stderr);
        return 1;
    }

    workspace[source.size] = (uint8_t)'x';
    if (music_rig_definition_json_decode(
            NULL,
            workspace,
            source.size + 1U,
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("compiled definition trailing data was accepted\n", stderr);
        return 1;
    }

    mutation = find_text(
        workspace,
        source.size,
        "arturia-current-rack"
    );
    if (mutation == NULL) {
        fputs("compiled preset mutation point was not found\n", stderr);
        return 1;
    }
    mutation[sizeof("arturia-current-rack") - 2U] = (uint8_t)'x';
    if (music_rig_definition_json_decode(
            NULL,
            workspace,
            source.size,
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("mapping with a mismatched Hardware Preset was accepted\n",
            stderr);
        return 1;
    }
    mutation[sizeof("arturia-current-rack") - 2U] = (uint8_t)'k';

    mutation = find_text(
        workspace,
        source.size,
        "\"arturia-main|cc|1|114\": 0"
    );
    if (mutation == NULL) {
        fputs("compiled mapping-index mutation point was not found\n", stderr);
        return 1;
    }
    mutation[sizeof("\"arturia-main|cc|1|114\": 0") - 2U] = (uint8_t)'1';
    if (music_rig_definition_json_decode(
            NULL,
            workspace,
            source.size,
            &definition,
            &tables
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("misdirected compiler mapping index was accepted\n", stderr);
        return 1;
    }
    return 0;
}
