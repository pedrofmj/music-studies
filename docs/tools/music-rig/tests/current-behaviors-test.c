#include "music_rig/current_arturia.h"
#include "music_rig/current_smk25.h"

#include <stdio.h>
#include <string.h>

#define EVENT_CAPACITY ((size_t)2048)
#define MESSAGE_CAPACITY ((size_t)16)

typedef struct captured_event {
    size_t layer;
    uint32_t frame;
    size_t size;
    uint8_t data[MESSAGE_CAPACITY];
} captured_event;

typedef struct captured_output {
    captured_event events[EVENT_CAPACITY];
    size_t count;
    bool overflow;
} captured_output;

static void capture(
    void *opaque,
    size_t layer,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    captured_output *output = opaque;
    captured_event *event;

    if (output->count >= EVENT_CAPACITY || message_size > MESSAGE_CAPACITY) {
        output->overflow = true;
        return;
    }
    event = &output->events[output->count];
    event->layer = layer;
    event->frame = frame;
    event->size = message_size;
    memcpy(event->data, message, message_size);
    output->count += 1U;
}

static bool event_is(
    const captured_output *output,
    size_t index,
    size_t layer,
    uint8_t status,
    uint8_t data1,
    uint8_t data2
)
{
    const captured_event *event;

    if (index >= output->count) {
        return false;
    }
    event = &output->events[index];
    return event->layer == layer && event->size == 3U &&
        event->data[0] == status && event->data[1] == data1 &&
        event->data[2] == data2;
}

static int test_arturia(void)
{
    static const uint8_t relative_output[3] = {
        UINT8_C(0xb0), UINT8_C(119), UINT8_C(65)
    };
    music_rig_current_arturia_config config;
    music_rig_current_arturia_behavior behavior;
    music_rig_current_arturia_decision decision;
    music_rig_current_arturia_snapshot snapshot;
    uint8_t message[3];
    float left;
    float right;

    music_rig_current_arturia_config_init(&config);
    if (config.channel != 0U || config.volume_input_cc != UINT8_C(114) ||
        config.button_input_cc != UINT8_C(115) ||
        music_rig_current_arturia_init(&behavior, &config, 64, 0) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("Arturia current configuration failed\n", stderr);
        return 1;
    }

    message[0] = UINT8_C(0xb0);
    message[1] = UINT8_C(114);
    message[2] = UINT8_C(65);
    if (music_rig_current_arturia_process_midi(
            &behavior, message, sizeof(message), &decision
        ) != MUSIC_RIG_RESULT_OK ||
        !decision.handled || !decision.output_ready ||
        memcmp(decision.output, relative_output, sizeof(relative_output)) != 0 ||
        behavior.volume != UINT8_C(65) || behavior.generation != UINT32_C(1)) {
        fputs("Arturia relative volume failed\n", stderr);
        return 1;
    }
    message[2] = UINT8_C(64);
    if (music_rig_current_arturia_process_midi(
            &behavior, message, sizeof(message), &decision
        ) != MUSIC_RIG_RESULT_OK ||
        !decision.handled || decision.output_ready ||
        behavior.generation != UINT32_C(1)) {
        fputs("Arturia neutral relative value changed state\n", stderr);
        return 1;
    }

    message[1] = UINT8_C(115);
    message[2] = UINT8_C(127);
    if (music_rig_current_arturia_process_midi(
            &behavior, message, sizeof(message), &decision
        ) != MUSIC_RIG_RESULT_OK ||
        behavior.mute != UINT8_C(127) || !decision.output_ready ||
        decision.output[1] != UINT8_C(118) ||
        decision.output[2] != UINT8_C(127) ||
        behavior.generation != UINT32_C(2)) {
        fputs("Arturia mute rising edge failed\n", stderr);
        return 1;
    }
    if (music_rig_current_arturia_process_midi(
            &behavior, message, sizeof(message), &decision
        ) != MUSIC_RIG_RESULT_OK || decision.output_ready ||
        behavior.generation != UINT32_C(2)) {
        fputs("Arturia held mute toggled twice\n", stderr);
        return 1;
    }
    message[2] = UINT8_C(0);
    if (music_rig_current_arturia_process_midi(
            &behavior, message, sizeof(message), &decision
        ) != MUSIC_RIG_RESULT_OK || decision.output_ready ||
        behavior.button_down) {
        fputs("Arturia mute release failed\n", stderr);
        return 1;
    }

    if (music_rig_current_arturia_output_connections(
            &behavior, 1, &decision
        ) != MUSIC_RIG_RESULT_OK || !decision.output_ready ||
        decision.output[1] != UINT8_C(119) ||
        decision.output[2] != behavior.volume ||
        music_rig_current_arturia_output_connections(
            &behavior, 1, &decision
        ) != MUSIC_RIG_RESULT_OK || decision.output_ready) {
        fputs("Arturia connection replay failed\n", stderr);
        return 1;
    }

    behavior.audio_gain = 1.0f;
    if (music_rig_current_arturia_apply_audio_frame(
            &behavior, 0.25f, 1.0f, 0.5f, &left, &right
        ) != MUSIC_RIG_RESULT_OK || left != 0.75f || right != 0.375f ||
        music_rig_current_arturia_apply_audio_frame(
            &behavior, 0.25f, 1.0f, 0.5f, &left, &right
        ) != MUSIC_RIG_RESULT_OK || left != 0.5f || right != 0.25f) {
        fputs("Arturia mute audio ramp failed\n", stderr);
        return 1;
    }

    if (music_rig_current_arturia_restore_legacy_text(
            &behavior, "150 64 ignored", 14U, 3
        ) != MUSIC_RIG_RESULT_OK || behavior.volume != UINT8_C(127) ||
        behavior.mute != UINT8_C(127) || behavior.audio_gain != 0.0f ||
        music_rig_current_arturia_snapshot_read(&behavior, &snapshot) !=
            MUSIC_RIG_RESULT_OK ||
        snapshot.version != MUSIC_RIG_CURRENT_ARTURIA_SNAPSHOT_VERSION) {
        fputs("Arturia legacy state recall failed\n", stderr);
        return 1;
    }
    snapshot.volume = UINT8_C(91);
    snapshot.mute = UINT8_C(0);
    if (music_rig_current_arturia_snapshot_restore(&behavior, &snapshot) !=
            MUSIC_RIG_RESULT_OK ||
        behavior.volume != UINT8_C(91) || behavior.mute != UINT8_C(0) ||
        behavior.generation != UINT32_C(0)) {
        fputs("Arturia snapshot restore failed\n", stderr);
        return 1;
    }
    snapshot.version = UINT32_C(2);
    config.channel = UINT8_C(16);
    message[0] = UINT8_C(0xb0);
    message[1] = UINT8_C(114);
    message[2] = UINT8_C(128);
    if (music_rig_current_arturia_snapshot_restore(&behavior, &snapshot) !=
            MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_current_arturia_process_midi(
            &behavior, message, sizeof(message), &decision
        ) != MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_current_arturia_init(&behavior, &config, 0, 0) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_current_arturia_apply_audio_frame(
            &behavior, 0.0f, 0.0f, 0.0f, &left, &right
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("Arturia invalid boundary failed\n", stderr);
        return 1;
    }
    return 0;
}

static int test_smk25_current_trace(void)
{
    music_rig_current_smk25_config config;
    music_rig_current_smk25_behavior behavior;
    captured_output output = {0};
    uint8_t event[3];
    size_t before;

    music_rig_current_smk25_config_init(&config);
    if (music_rig_current_smk25_init(&behavior, &config) !=
            MUSIC_RIG_RESULT_OK ||
        config.pads[0] != UINT8_C(40) ||
        config.pad_channels[7] != UINT8_C(7) ||
        config.knobs[7] != UINT8_C(27)) {
        fputs("SMK-25 current configuration failed\n", stderr);
        return 1;
    }

    event[0] = UINT8_C(0xb0);
    event[1] = UINT8_C(23);
    event[2] = UINT8_C(0);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(1), capture, &output
        ) != MUSIC_RIG_RESULT_OK || output.count != 1U ||
        !event_is(&output, 0U, 3U, UINT8_C(0xb0), UINT8_C(23), UINT8_C(0)) ||
        behavior.knob_values[3] != UINT8_C(0)) {
        fputs("SMK-25 knob routing failed\n", stderr);
        return 1;
    }
    before = output.count;
    if (music_rig_current_smk25_output_connections(
            &behavior, 3U, 1, UINT32_C(2), capture, &output
        ) != MUSIC_RIG_RESULT_OK || output.count != before + 1U ||
        !event_is(
            &output,
            before,
            3U,
            UINT8_C(0xb0),
            UINT8_C(23),
            UINT8_C(0)
        ) ||
        music_rig_current_smk25_output_connections(
            &behavior, 3U, 1, UINT32_C(3), capture, &output
        ) != MUSIC_RIG_RESULT_OK || output.count != before + 1U) {
        fputs("SMK-25 connection replay failed\n", stderr);
        return 1;
    }

    event[1] = UINT8_C(40);
    event[2] = UINT8_C(127);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(2), capture, &output
        ) != MUSIC_RIG_RESULT_OK || !behavior.layers[0].enabled) {
        fputs("SMK-25 pad value enable failed\n", stderr);
        return 1;
    }

    before = output.count;
    event[0] = UINT8_C(0x90);
    event[1] = UINT8_C(60);
    event[2] = UINT8_C(100);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(3), capture, &output
        ) != MUSIC_RIG_RESULT_OK || output.count != before + 8U ||
        behavior.layers[0].active_velocity[60] != UINT8_C(100) ||
        behavior.layers[0].last_velocity[60] != UINT8_C(100)) {
        fputs("SMK-25 note latch failed\n", stderr);
        return 1;
    }
    before = output.count;
    event[0] = UINT8_C(0x80);
    event[2] = UINT8_C(0);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(4), capture, &output
        ) != MUSIC_RIG_RESULT_OK || output.count != before + 7U ||
        behavior.layers[0].active_velocity[60] != UINT8_C(100)) {
        fputs("SMK-25 latched note release failed\n", stderr);
        return 1;
    }

    event[0] = UINT8_C(0x90);
    event[1] = UINT8_C(64);
    event[2] = UINT8_C(90);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(5), capture, &output
        ) != MUSIC_RIG_RESULT_OK ||
        behavior.layers[0].active_velocity[60] != UINT8_C(0) ||
        behavior.layers[0].active_velocity[64] != UINT8_C(90)) {
        fputs("SMK-25 new gesture replacement failed\n", stderr);
        return 1;
    }
    event[0] = UINT8_C(0x80);
    event[2] = UINT8_C(0);
    (void)music_rig_current_smk25_process_midi(
        &behavior, event, 3U, UINT32_C(6), capture, &output
    );

    event[0] = UINT8_C(0x90);
    event[1] = UINT8_C(93);
    event[2] = UINT8_C(127);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(7), capture, &output
        ) != MUSIC_RIG_RESULT_OK ||
        behavior.layers[0].active_velocity[64] != UINT8_C(0) ||
        !behavior.layers[0].paused) {
        fputs("SMK-25 Stop failed\n", stderr);
        return 1;
    }
    before = output.count;
    event[0] = UINT8_C(0x80);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(8), capture, &output
        ) != MUSIC_RIG_RESULT_OK || output.count != before) {
        fputs("SMK-25 transport release leaked\n", stderr);
        return 1;
    }
    event[0] = UINT8_C(0x90);
    event[1] = UINT8_C(94);
    if (music_rig_current_smk25_process_midi(
            &behavior, event, 3U, UINT32_C(9), capture, &output
        ) != MUSIC_RIG_RESULT_OK ||
        behavior.layers[0].active_velocity[64] != UINT8_C(90) ||
        behavior.layers[0].paused) {
        fputs("SMK-25 Play failed\n", stderr);
        return 1;
    }

    {
        const uint8_t stop = UINT8_C(0xfc);
        const uint8_t play = UINT8_C(0xfa);
        const uint8_t mmc[6] = {
            UINT8_C(0xf0), UINT8_C(0x7f), UINT8_C(0x7f),
            UINT8_C(0x06), UINT8_C(0x02), UINT8_C(0xf7)
        };
        uint32_t generation = behavior.generation;

        (void)music_rig_current_smk25_process_midi(
            &behavior, &stop, 1U, UINT32_C(10), capture, &output
        );
        (void)music_rig_current_smk25_process_midi(
            &behavior, &play, 1U, UINT32_C(11), capture, &output
        );
        (void)music_rig_current_smk25_process_midi(
            &behavior, mmc, sizeof(mmc), UINT32_C(12), capture, &output
        );
        if (behavior.generation != generation + UINT32_C(3) ||
            output.overflow) {
            fputs("SMK-25 standard transport failed\n", stderr);
            return 1;
        }
    }
    return 0;
}

static int test_smk25_state_and_boundaries(void)
{
    static const char legacy_state[] =
        "SMK25_PAD_LAYERS 1\n"
        "KNOBS 0 1 2 3 4 5 6 127\n"
        "LAYER 1 1 0 60:100 64:90 invalid\n"
        "LAYER 2 0 1 48:96\n";
    music_rig_current_smk25_config config;
    music_rig_current_smk25_behavior behavior;
    music_rig_current_smk25_snapshot snapshot;
    captured_output output = {0};
    uint8_t event[3] = {UINT8_C(0x90), UINT8_C(128), UINT8_C(1)};

    music_rig_current_smk25_config_init(&config);
    if (music_rig_current_smk25_init(&behavior, &config) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_current_smk25_restore_legacy_text(
            &behavior, legacy_state, sizeof(legacy_state) - 1U
        ) != MUSIC_RIG_RESULT_OK ||
        behavior.knob_values[7] != UINT8_C(127) ||
        !behavior.layers[0].enabled || !behavior.layers[0].paused ||
        behavior.layers[0].last_velocity[60] != UINT8_C(100) ||
        behavior.layers[0].last_velocity[64] != UINT8_C(90) ||
        behavior.layers[1].enabled || !behavior.layers[1].paused ||
        behavior.layers[1].last_velocity[48] != UINT8_C(96) ||
        music_rig_current_smk25_snapshot_read(&behavior, &snapshot) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("SMK-25 legacy state recall failed\n", stderr);
        return 1;
    }
    behavior.layers[0].enabled = false;
    behavior.layers[0].last_velocity[60] = UINT8_C(0);
    if (music_rig_current_smk25_snapshot_restore(&behavior, &snapshot) !=
            MUSIC_RIG_RESULT_OK ||
        !behavior.layers[0].enabled || !behavior.layers[0].paused ||
        behavior.layers[0].last_velocity[60] != UINT8_C(100) ||
        behavior.generation != UINT32_C(0)) {
        fputs("SMK-25 snapshot restore failed\n", stderr);
        return 1;
    }
    snapshot.version = UINT32_C(2);
    if (music_rig_current_smk25_snapshot_restore(&behavior, &snapshot) !=
            MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_current_smk25_process_midi(
            &behavior, event, sizeof(event), UINT32_C(0), capture, &output
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("SMK-25 invalid state or MIDI was accepted\n", stderr);
        return 1;
    }

    config.pad_behavior = MUSIC_RIG_CURRENT_SMK25_PAD_TOGGLE;
    if (music_rig_current_smk25_init(&behavior, &config) !=
            MUSIC_RIG_RESULT_OK) {
        fputs("SMK-25 toggle configuration failed\n", stderr);
        return 1;
    }
    event[0] = UINT8_C(0xb0);
    event[1] = UINT8_C(40);
    event[2] = UINT8_C(127);
    (void)music_rig_current_smk25_process_midi(
        &behavior, event, 3U, UINT32_C(1), capture, &output
    );
    (void)music_rig_current_smk25_process_midi(
        &behavior, event, 3U, UINT32_C(2), capture, &output
    );
    if (!behavior.layers[0].enabled || behavior.generation != UINT32_C(1)) {
        fputs("SMK-25 toggle hold repeated\n", stderr);
        return 1;
    }
    event[2] = UINT8_C(0);
    (void)music_rig_current_smk25_process_midi(
        &behavior, event, 3U, UINT32_C(3), capture, &output
    );
    event[2] = UINT8_C(127);
    (void)music_rig_current_smk25_process_midi(
        &behavior, event, 3U, UINT32_C(4), capture, &output
    );
    if (behavior.layers[0].enabled || behavior.generation != UINT32_C(2)) {
        fputs("SMK-25 toggle second edge failed\n", stderr);
        return 1;
    }

    music_rig_current_smk25_config_init(&config);
    config.pads[1] = config.pads[0];
    if (music_rig_current_smk25_init(&behavior, &config) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_current_smk25_init(NULL, &config) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_current_smk25_output_connections(
            &behavior, MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT, 0,
            UINT32_C(0), capture, &output
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("SMK-25 invalid configuration was accepted\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_arturia() != 0 || test_smk25_current_trace() != 0 ||
        test_smk25_state_and_boundaries() != 0) {
        return 1;
    }
    printf(
        "Current behavior tests: OK "
        "(Arturia behavior=%zu snapshot=%zu; "
        "SMK-25 behavior=%zu snapshot=%zu bytes)\n",
        sizeof(music_rig_current_arturia_behavior),
        sizeof(music_rig_current_arturia_snapshot),
        sizeof(music_rig_current_smk25_behavior),
        sizeof(music_rig_current_smk25_snapshot)
    );
    return 0;
}
