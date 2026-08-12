#include "music_rig/current_arturia.h"

#include <limits.h>
#include <string.h>

_Static_assert(
    sizeof(music_rig_current_arturia_behavior) <=
        MUSIC_RIG_CURRENT_ARTURIA_BEHAVIOR_STORAGE_MAX,
    "Arturia current behavior exceeds its fixed storage contract"
);
_Static_assert(
    sizeof(music_rig_current_arturia_snapshot) <=
        MUSIC_RIG_CURRENT_ARTURIA_SNAPSHOT_STORAGE_MAX,
    "Arturia current snapshot exceeds its fixed storage contract"
);

static bool ascii_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
        value == '\v' || value == '\f';
}

static bool read_integer(
    const char **cursor,
    const char *end,
    int32_t *value
)
{
    const char *position = *cursor;
    uint64_t magnitude = UINT64_C(0);
    bool negative = false;
    bool found = false;

    while (position < end && *position != '\0' && ascii_space(*position)) {
        position += 1;
    }
    if (position < end && (*position == '-' || *position == '+')) {
        negative = *position == '-';
        position += 1;
    }
    while (position < end && *position >= '0' && *position <= '9') {
        uint64_t digit = (uint64_t)(*position - '0');
        uint64_t limit = negative
            ? (uint64_t)INT32_MAX + UINT64_C(1)
            : (uint64_t)INT32_MAX;

        found = true;
        if (magnitude > (limit - digit) / UINT64_C(10)) {
            return false;
        }
        magnitude = magnitude * UINT64_C(10) + digit;
        position += 1;
    }
    if (!found) {
        return false;
    }
    if (negative && magnitude == (uint64_t)INT32_MAX + UINT64_C(1)) {
        *value = INT32_MIN;
    } else {
        *value = negative ? -(int32_t)magnitude : (int32_t)magnitude;
    }
    *cursor = position;
    return true;
}

static uint8_t clamp_midi(int32_t value)
{
    if (value < 0) {
        return UINT8_C(0);
    }
    if (value > 127) {
        return UINT8_C(127);
    }
    return (uint8_t)value;
}

static bool valid_config(const music_rig_current_arturia_config *config)
{
    return config != NULL && config->channel <= UINT8_C(15) &&
        config->volume_input_cc <= UINT8_C(127) &&
        config->button_input_cc <= UINT8_C(127) &&
        config->mute_output_cc <= UINT8_C(127) &&
        config->volume_output_cc <= UINT8_C(127);
}

static bool valid_behavior(
    const music_rig_current_arturia_behavior *behavior
)
{
    return behavior != NULL &&
        behavior->abi_version == MUSIC_RIG_CURRENT_ARTURIA_ABI_VERSION;
}

static void clear_decision(music_rig_current_arturia_decision *decision)
{
    memset(decision, 0, sizeof(*decision));
}

static void set_output(
    const music_rig_current_arturia_behavior *behavior,
    music_rig_current_arturia_decision *decision,
    uint8_t controller,
    uint8_t value
)
{
    decision->output_ready = true;
    decision->output[0] = (uint8_t)(UINT8_C(0xb0) | behavior->config.channel);
    decision->output[1] = controller;
    decision->output[2] = value;
}

void music_rig_current_arturia_config_init(
    music_rig_current_arturia_config *config
)
{
    if (config == NULL) {
        return;
    }
    config->channel = UINT8_C(0);
    config->volume_input_cc = UINT8_C(114);
    config->button_input_cc = UINT8_C(115);
    config->mute_output_cc = UINT8_C(118);
    config->volume_output_cc = UINT8_C(119);
}

music_rig_result music_rig_current_arturia_init(
    music_rig_current_arturia_behavior *behavior,
    const music_rig_current_arturia_config *config,
    int32_t initial_volume,
    int32_t initial_mute
)
{
    if (behavior == NULL || !valid_config(config)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(behavior, 0, sizeof(*behavior));
    behavior->abi_version = MUSIC_RIG_CURRENT_ARTURIA_ABI_VERSION;
    behavior->config = *config;
    behavior->volume = clamp_midi(initial_volume);
    behavior->mute = initial_mute >= 64 ? UINT8_C(127) : UINT8_C(0);
    behavior->output_connection_count = -1;
    behavior->audio_gain = behavior->mute == 0U ? 1.0f : 0.0f;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_arturia_restore_legacy_text(
    music_rig_current_arturia_behavior *behavior,
    const char *text,
    size_t text_size,
    int32_t fallback_volume
)
{
    music_rig_current_arturia_config config;
    const char *cursor = text;
    const char *end;
    int32_t volume;
    int32_t mute = 0;

    if (behavior == NULL || (text == NULL && text_size != 0U)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!valid_behavior(behavior)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    config = behavior->config;
    if (!valid_config(&config)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    volume = fallback_volume;
    if (text != NULL) {
        end = text + text_size;
        if (!read_integer(&cursor, end, &volume)) {
            volume = fallback_volume;
        } else {
            (void)read_integer(&cursor, end, &mute);
        }
    }
    return music_rig_current_arturia_init(behavior, &config, volume, mute);
}

music_rig_result music_rig_current_arturia_snapshot_read(
    const music_rig_current_arturia_behavior *behavior,
    music_rig_current_arturia_snapshot *snapshot
)
{
    if (!valid_behavior(behavior) || snapshot == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    snapshot->version = MUSIC_RIG_CURRENT_ARTURIA_SNAPSHOT_VERSION;
    snapshot->volume = behavior->volume;
    snapshot->mute = behavior->mute;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_arturia_snapshot_restore(
    music_rig_current_arturia_behavior *behavior,
    const music_rig_current_arturia_snapshot *snapshot
)
{
    music_rig_current_arturia_config config;

    if (behavior == NULL || snapshot == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!valid_behavior(behavior)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    config = behavior->config;
    if (!valid_config(&config)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (snapshot->version != MUSIC_RIG_CURRENT_ARTURIA_SNAPSHOT_VERSION ||
        snapshot->volume > UINT8_C(127) ||
        (snapshot->mute != UINT8_C(0) && snapshot->mute != UINT8_C(127))) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    return music_rig_current_arturia_init(
        behavior,
        &config,
        (int32_t)snapshot->volume,
        (int32_t)snapshot->mute
    );
}

music_rig_result music_rig_current_arturia_process_midi(
    music_rig_current_arturia_behavior *behavior,
    const uint8_t *message,
    size_t message_size,
    music_rig_current_arturia_decision *decision
)
{
    int32_t next_volume;

    if (!valid_behavior(behavior) || message == NULL || decision == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    clear_decision(decision);
    if (message_size != 3U || (message[0] & UINT8_C(0xf0)) != UINT8_C(0xb0) ||
        (message[0] & UINT8_C(0x0f)) != behavior->config.channel) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (message[1] > UINT8_C(127) || message[2] > UINT8_C(127)) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (message[1] == behavior->config.volume_input_cc) {
        int32_t delta = (int32_t)message[2] - 64;

        decision->handled = true;
        if (delta == 0) {
            return MUSIC_RIG_RESULT_OK;
        }
        next_volume = (int32_t)behavior->volume + delta;
        behavior->volume = clamp_midi(next_volume);
        behavior->generation += UINT32_C(1);
        set_output(
            behavior,
            decision,
            behavior->config.volume_output_cc,
            behavior->volume
        );
    } else if (message[1] == behavior->config.button_input_cc) {
        bool pressed = message[2] >= UINT8_C(64);
        bool was_down = behavior->button_down;

        decision->handled = true;
        behavior->button_down = pressed;
        if (!pressed || was_down) {
            return MUSIC_RIG_RESULT_OK;
        }
        behavior->mute = behavior->mute == 0U ? UINT8_C(127) : UINT8_C(0);
        behavior->generation += UINT32_C(1);
        set_output(
            behavior,
            decision,
            behavior->config.mute_output_cc,
            behavior->mute
        );
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_arturia_output_connections(
    music_rig_current_arturia_behavior *behavior,
    int32_t connection_count,
    music_rig_current_arturia_decision *decision
)
{
    if (!valid_behavior(behavior) || decision == NULL || connection_count < 0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    clear_decision(decision);
    if (connection_count > 0 &&
        connection_count > behavior->output_connection_count) {
        set_output(
            behavior,
            decision,
            behavior->config.volume_output_cc,
            behavior->volume
        );
    }
    behavior->output_connection_count = connection_count;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_arturia_apply_audio_frame(
    music_rig_current_arturia_behavior *behavior,
    float ramp_step,
    float input_left,
    float input_right,
    float *output_left,
    float *output_right
)
{
    float target;

    if (!valid_behavior(behavior) || output_left == NULL || output_right == NULL ||
        !(ramp_step > 0.0f) || !(behavior->audio_gain >= 0.0f) ||
        !(behavior->audio_gain <= 1.0f)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    target = behavior->mute == 0U ? 1.0f : 0.0f;
    if (behavior->audio_gain < target) {
        behavior->audio_gain += ramp_step;
        if (behavior->audio_gain > target) {
            behavior->audio_gain = target;
        }
    } else if (behavior->audio_gain > target) {
        behavior->audio_gain -= ramp_step;
        if (behavior->audio_gain < target) {
            behavior->audio_gain = target;
        }
    }
    *output_left = input_left * behavior->audio_gain;
    *output_right = input_right * behavior->audio_gain;
    return MUSIC_RIG_RESULT_OK;
}
