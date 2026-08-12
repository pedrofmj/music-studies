#define main smk25_protected_main
#include "../../smk25-pad-layers/smk25-pad-layers.c"
#undef main

#include "music_rig/current_smk25.h"

#include <string.h>

typedef struct portable_output {
    test_event_t events[MAX_TEST_EVENTS];
    size_t count;
    bool overflow;
} portable_output;

static void portable_emit(
    void *opaque,
    size_t layer,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    portable_output *output = opaque;
    test_event_t *event;

    (void)frame;
    if (output->count >= MAX_TEST_EVENTS ||
        message_size > sizeof(output->events[0].data)) {
        output->overflow = true;
        return;
    }
    event = &output->events[output->count];
    event->layer = (int)layer;
    event->size = message_size;
    memcpy(event->data, message, message_size);
    output->count += 1U;
}

static int compare_state(
    const music_rig_current_smk25_behavior *portable
)
{
    size_t layer;
    size_t note;

    if ((size_t)physical_note_count != portable->physical_note_count ||
        atomic_load(&state_generation) != portable->generation) {
        return -1;
    }
    for (note = 0U; note < MIDI_NOTE_COUNT; ++note) {
        if (physical_notes[note] != portable->physical_notes[note]) {
            return -1;
        }
    }
    for (layer = 0U; layer < LAYER_COUNT; ++layer) {
        if (atomic_load(&layers[layer].enabled) !=
                portable->layers[layer].enabled ||
            atomic_load(&layers[layer].paused) !=
                portable->layers[layer].paused ||
            layers[layer].pad_down != portable->layers[layer].pad_down ||
            atomic_load(&knob_values[layer]) !=
                portable->knob_values[layer] ||
            output_connection_counts[layer] !=
                portable->output_connection_counts[layer]) {
            return -1;
        }
        for (note = 0U; note < MIDI_NOTE_COUNT; ++note) {
            if (atomic_load(&layers[layer].last_velocity[note]) !=
                    portable->layers[layer].last_velocity[note] ||
                layers[layer].active_velocity[note] !=
                    portable->layers[layer].active_velocity[note]) {
                return -1;
            }
        }
    }
    return 0;
}

static int compare_output(
    const test_output_context_t *legacy,
    const portable_output *portable
)
{
    size_t index;

    if (portable->overflow || legacy->count != portable->count) {
        return -1;
    }
    for (index = 0U; index < legacy->count; ++index) {
        if (legacy->events[index].layer != portable->events[index].layer ||
            legacy->events[index].size != portable->events[index].size ||
            memcmp(
                legacy->events[index].data,
                portable->events[index].data,
                legacy->events[index].size
            ) != 0) {
            return -1;
        }
    }
    return 0;
}

static int run_event(
    music_rig_current_smk25_behavior *portable,
    const uint8_t *message,
    size_t message_size,
    uint32_t frame
)
{
    test_output_context_t legacy_output = {0};
    portable_output portable_output_value = {0};

    process_event(
        message,
        message_size,
        (jack_nframes_t)frame,
        test_emit,
        &legacy_output
    );
    if (music_rig_current_smk25_process_midi(
            portable,
            message,
            message_size,
            frame,
            portable_emit,
            &portable_output_value
        ) != MUSIC_RIG_RESULT_OK ||
        compare_output(&legacy_output, &portable_output_value) != 0 ||
        compare_state(portable) != 0) {
        return -1;
    }
    return 0;
}

static int run_connection_change(
    music_rig_current_smk25_behavior *portable,
    size_t layer,
    int32_t connection_count,
    uint32_t frame
)
{
    test_output_context_t legacy_output = {0};
    portable_output portable_output_value = {0};

    if (connection_count > 0 &&
        connection_count > output_connection_counts[layer]) {
        const unsigned char volume[3] = {
            (unsigned char)(0xb0 | configuration.knob_channels[layer]),
            (unsigned char)configuration.knobs[layer],
            atomic_load(&knob_values[layer]),
        };

        test_emit(&legacy_output, (int)layer, frame, volume, sizeof(volume));
    }
    output_connection_counts[layer] = connection_count;
    if (music_rig_current_smk25_output_connections(
            portable,
            layer,
            connection_count,
            frame,
            portable_emit,
            &portable_output_value
        ) != MUSIC_RIG_RESULT_OK ||
        compare_output(&legacy_output, &portable_output_value) != 0 ||
        compare_state(portable) != 0) {
        return -1;
    }
    return 0;
}

static void configure_legacy(
    const music_rig_current_smk25_config *portable_config
)
{
    size_t index;

    initialize_configuration();
    configuration.channel = portable_config->channel;
    configuration.pad_type = CONTROL_CC;
    configuration.pad_behavior = PAD_VALUE;
    configuration.pad_on_minimum = portable_config->pad_on_minimum;
    configuration.play.type = CONTROL_NOTE;
    configuration.play.number = portable_config->play.number;
    configuration.stop.type = CONTROL_NOTE;
    configuration.stop.number = portable_config->stop.number;
    for (index = 0U; index < LAYER_COUNT; ++index) {
        configuration.pads[index] = portable_config->pads[index];
        configuration.pad_channels[index] =
            portable_config->pad_channels[index];
        configuration.knobs[index] = portable_config->knobs[index];
        configuration.knob_channels[index] =
            portable_config->knob_channels[index];
    }
    configuration.default_chord_count =
        (int)portable_config->default_chord_count;
    for (index = 0U; index < portable_config->default_chord_count; ++index) {
        configuration.default_chord[index] =
            portable_config->default_chord[index];
    }
    initialize_state();
}

static int test_event_parity(void)
{
    static const uint8_t messages[][8] = {
        {0xb0, 23, 0},
        {0xb0, 40, 127},
        {0x90, 60, 100},
        {0x80, 60, 0},
        {0x90, 64, 90},
        {0x80, 64, 0},
        {0x90, 93, 127},
        {0x80, 93, 64},
        {0x90, 94, 127},
        {0x80, 94, 64},
        {0xb0, 40, 0},
        {0xb0, 41, 127},
        {0xb1, 41, 127},
        {0xfa},
        {0xfc},
        {0xf0, 0x7f, 0x7f, 0x06, 0x02, 0xf7},
    };
    static const size_t sizes[] = {
        3U, 3U, 3U, 3U, 3U, 3U, 3U, 3U,
        3U, 3U, 3U, 3U, 3U, 1U, 1U, 6U
    };
    music_rig_current_smk25_config config;
    music_rig_current_smk25_behavior portable;
    size_t index;

    music_rig_current_smk25_config_init(&config);
    configure_legacy(&config);
    if (music_rig_current_smk25_init(&portable, &config) !=
            MUSIC_RIG_RESULT_OK) {
        return -1;
    }
    for (index = 0U; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        if (run_event(
                &portable, messages[index], sizes[index], (uint32_t)index
            ) != 0) {
            fprintf(stderr, "SMK-25 event parity failed at index %zu\n", index);
            return -1;
        }
    }
    if (run_connection_change(&portable, 3U, 0, UINT32_C(20)) != 0 ||
        run_connection_change(&portable, 3U, 1, UINT32_C(21)) != 0 ||
        run_connection_change(&portable, 3U, 1, UINT32_C(22)) != 0 ||
        run_connection_change(&portable, 3U, 0, UINT32_C(23)) != 0 ||
        run_connection_change(&portable, 3U, 1, UINT32_C(24)) != 0) {
        fputs("SMK-25 connection replay parity failed\n", stderr);
        return -1;
    }
    return 0;
}

static int test_state_parity(void)
{
    static const char state_text[] =
        "SMK25_PAD_LAYERS 1\n"
        "KNOBS 0 1 2 3 4 5 6 127\n"
        "LAYER 1 1 0 60:100 64:90 invalid\n"
        "LAYER 2 0 1 48:96\n";
    char path[] = "/tmp/music-rig-smk25-parity.XXXXXX";
    music_rig_current_smk25_config config;
    music_rig_current_smk25_behavior portable;
    int descriptor;
    ssize_t written;

    music_rig_current_smk25_config_init(&config);
    configure_legacy(&config);
    if (music_rig_current_smk25_init(&portable, &config) !=
            MUSIC_RIG_RESULT_OK) {
        return -1;
    }
    descriptor = mkstemp(path);
    if (descriptor < 0) {
        return -1;
    }
    written = write(descriptor, state_text, sizeof(state_text) - 1U);
    if (written != (ssize_t)(sizeof(state_text) - 1U) ||
        close(descriptor) != 0) {
        (void)unlink(path);
        return -1;
    }
    load_state(path);
    (void)unlink(path);
    if (music_rig_current_smk25_restore_legacy_text(
            &portable, state_text, sizeof(state_text) - 1U
        ) != MUSIC_RIG_RESULT_OK ||
        compare_state(&portable) != 0) {
        return -1;
    }
    return 0;
}

int main(void)
{
    if (test_event_parity() != 0 || test_state_parity() != 0) {
        fputs("SMK-25 current behavior parity failed\n", stderr);
        return 1;
    }
    puts("SMK-25 current behavior parity: OK");
    return 0;
}
