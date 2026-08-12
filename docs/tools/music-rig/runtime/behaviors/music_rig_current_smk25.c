#include "music_rig/current_smk25.h"

#include <limits.h>
#include <string.h>

#define LEGACY_LINE_CAPACITY ((size_t)2048)

_Static_assert(
    sizeof(music_rig_current_smk25_behavior) <=
        MUSIC_RIG_CURRENT_SMK25_BEHAVIOR_STORAGE_MAX,
    "SMK-25 current behavior exceeds its fixed storage contract"
);
_Static_assert(
    sizeof(music_rig_current_smk25_snapshot) <=
        MUSIC_RIG_CURRENT_SMK25_SNAPSHOT_STORAGE_MAX,
    "SMK-25 current snapshot exceeds its fixed storage contract"
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

static bool valid_control(const music_rig_current_smk25_control *control)
{
    if (control->type == MUSIC_RIG_CURRENT_SMK25_CONTROL_NONE) {
        return true;
    }
    return (control->type == MUSIC_RIG_CURRENT_SMK25_CONTROL_CC ||
            control->type == MUSIC_RIG_CURRENT_SMK25_CONTROL_NOTE) &&
        control->number <= UINT8_C(127);
}

static bool unique_values(const uint8_t *values)
{
    size_t left;
    size_t right;

    for (left = 0U; left < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++left) {
        for (right = left + 1U;
             right < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT;
             ++right) {
            if (values[left] == values[right]) {
                return false;
            }
        }
    }
    return true;
}

static bool valid_config(const music_rig_current_smk25_config *config)
{
    size_t chord;
    size_t pad;
    size_t knob;

    if (config == NULL || config->channel > UINT8_C(15) ||
        (config->pad_type != MUSIC_RIG_CURRENT_SMK25_CONTROL_CC &&
         config->pad_type != MUSIC_RIG_CURRENT_SMK25_CONTROL_NOTE) ||
        (config->pad_behavior != MUSIC_RIG_CURRENT_SMK25_PAD_VALUE &&
         config->pad_behavior != MUSIC_RIG_CURRENT_SMK25_PAD_TOGGLE) ||
        config->pad_on_minimum == 0U ||
        config->pad_on_minimum > UINT8_C(127) ||
        config->default_chord_count == 0U ||
        config->default_chord_count > MUSIC_RIG_CURRENT_SMK25_CHORD_CAPACITY ||
        !valid_control(&config->play) || !valid_control(&config->stop) ||
        !unique_values(config->pads) || !unique_values(config->knobs)) {
        return false;
    }
    for (chord = 0U; chord < config->default_chord_count; ++chord) {
        if (config->default_chord[chord] > UINT8_C(127)) {
            return false;
        }
    }
    for (pad = 0U; pad < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++pad) {
        if (config->pads[pad] > UINT8_C(127) ||
            config->pad_channels[pad] > UINT8_C(15) ||
            config->knobs[pad] > UINT8_C(127) ||
            config->knob_channels[pad] > UINT8_C(15)) {
            return false;
        }
    }
    if (config->pad_type == MUSIC_RIG_CURRENT_SMK25_CONTROL_CC) {
        for (pad = 0U; pad < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++pad) {
            for (knob = 0U;
                 knob < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT;
                 ++knob) {
                if (config->pads[pad] == config->knobs[knob] &&
                    config->pad_channels[pad] ==
                        config->knob_channels[knob]) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool valid_behavior(const music_rig_current_smk25_behavior *behavior)
{
    return behavior != NULL &&
        behavior->abi_version == MUSIC_RIG_CURRENT_SMK25_ABI_VERSION;
}

static void reset_state(music_rig_current_smk25_behavior *behavior)
{
    memset(behavior->layers, 0, sizeof(behavior->layers));
    memset(behavior->physical_notes, 0, sizeof(behavior->physical_notes));
    memset(behavior->knob_values, 0, sizeof(behavior->knob_values));
    behavior->physical_note_count = 0U;
    behavior->generation = UINT32_C(0);
}

static void emit_message(
    music_rig_current_smk25_behavior *behavior,
    size_t layer,
    uint32_t frame,
    uint8_t status,
    uint8_t data1,
    uint8_t data2,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    const uint8_t message[3] = {status, data1, data2};

    (void)behavior;
    emit(emit_context, layer, frame, message, sizeof(message));
}

static void clear_last_chord(
    music_rig_current_smk25_behavior *behavior,
    size_t layer
)
{
    memset(
        behavior->layers[layer].last_velocity,
        0,
        sizeof(behavior->layers[layer].last_velocity)
    );
}

static bool has_last_chord(
    const music_rig_current_smk25_behavior *behavior,
    size_t layer
)
{
    size_t note;

    for (note = 0U; note < MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT; ++note) {
        if (behavior->layers[layer].last_velocity[note] != 0U) {
            return true;
        }
    }
    return false;
}

static void release_active(
    music_rig_current_smk25_behavior *behavior,
    size_t layer,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    size_t note;

    for (note = 0U; note < MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT; ++note) {
        if (behavior->layers[layer].active_velocity[note] == 0U) {
            continue;
        }
        emit_message(
            behavior,
            layer,
            frame,
            (uint8_t)(UINT8_C(0x80) | behavior->config.channel),
            (uint8_t)note,
            UINT8_C(0),
            emit,
            emit_context
        );
        behavior->layers[layer].active_velocity[note] = UINT8_C(0);
    }
}

static void start_last_chord(
    music_rig_current_smk25_behavior *behavior,
    size_t layer,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    size_t note;

    if (!has_last_chord(behavior, layer)) {
        size_t index;

        for (index = 0U; index < behavior->config.default_chord_count; ++index) {
            behavior->layers[layer].last_velocity
                [behavior->config.default_chord[index]] = UINT8_C(96);
        }
    }
    for (note = 0U; note < MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT; ++note) {
        uint8_t velocity = behavior->layers[layer].last_velocity[note];

        if (velocity == 0U) {
            continue;
        }
        emit_message(
            behavior,
            layer,
            frame,
            (uint8_t)(UINT8_C(0x90) | behavior->config.channel),
            (uint8_t)note,
            velocity,
            emit,
            emit_context
        );
        behavior->layers[layer].active_velocity[note] = velocity;
    }
    behavior->layers[layer].paused = false;
}

static void set_layer_enabled(
    music_rig_current_smk25_behavior *behavior,
    size_t layer,
    bool enabled,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    if (behavior->layers[layer].enabled == enabled) {
        return;
    }
    release_active(behavior, layer, frame, emit, emit_context);
    clear_last_chord(behavior, layer);
    behavior->layers[layer].enabled = enabled;
    behavior->layers[layer].paused = false;
    behavior->generation += UINT32_C(1);
}

static void stop_layers(
    music_rig_current_smk25_behavior *behavior,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    size_t layer;

    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        release_active(behavior, layer, frame, emit, emit_context);
        emit_message(
            behavior,
            layer,
            frame,
            (uint8_t)(UINT8_C(0xb0) | behavior->config.channel),
            UINT8_C(123),
            UINT8_C(0),
            emit,
            emit_context
        );
        if (behavior->layers[layer].enabled) {
            behavior->layers[layer].paused = true;
        }
    }
    behavior->generation += UINT32_C(1);
}

static void play_layers(
    music_rig_current_smk25_behavior *behavior,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    size_t layer;

    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        if (!behavior->layers[layer].enabled) {
            continue;
        }
        release_active(behavior, layer, frame, emit, emit_context);
        start_last_chord(behavior, layer, frame, emit, emit_context);
    }
    behavior->generation += UINT32_C(1);
}

static bool is_control_message(
    const music_rig_current_smk25_behavior *behavior,
    const music_rig_current_smk25_control *control,
    const uint8_t *message,
    size_t message_size
)
{
    uint8_t type;

    if (control->type == MUSIC_RIG_CURRENT_SMK25_CONTROL_NONE ||
        message_size != 3U ||
        (message[0] & UINT8_C(0x0f)) != behavior->config.channel ||
        message[1] != control->number) {
        return false;
    }
    type = message[0] & UINT8_C(0xf0);
    if (control->type == MUSIC_RIG_CURRENT_SMK25_CONTROL_CC) {
        return type == UINT8_C(0xb0);
    }
    return type == UINT8_C(0x80) || type == UINT8_C(0x90);
}

static bool matches_control(
    const music_rig_current_smk25_behavior *behavior,
    const music_rig_current_smk25_control *control,
    const uint8_t *message,
    size_t message_size
)
{
    if (!is_control_message(behavior, control, message, message_size)) {
        return false;
    }
    if (control->type == MUSIC_RIG_CURRENT_SMK25_CONTROL_CC) {
        return message[2] >= UINT8_C(64);
    }
    return (message[0] & UINT8_C(0xf0)) == UINT8_C(0x90) &&
        message[2] > UINT8_C(0);
}

static bool handle_standard_transport(
    music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    size_t message_size,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    if (message_size == 1U && message[0] == UINT8_C(0xfc)) {
        stop_layers(behavior, frame, emit, emit_context);
        return true;
    }
    if (message_size == 1U &&
        (message[0] == UINT8_C(0xfa) || message[0] == UINT8_C(0xfb))) {
        play_layers(behavior, frame, emit, emit_context);
        return true;
    }
    if (message_size >= 6U && message[0] == UINT8_C(0xf0) &&
        message[1] == UINT8_C(0x7f) && message[3] == UINT8_C(0x06) &&
        message[message_size - 1U] == UINT8_C(0xf7)) {
        if (message[4] == UINT8_C(0x01)) {
            stop_layers(behavior, frame, emit, emit_context);
            return true;
        }
        if (message[4] == UINT8_C(0x02) || message[4] == UINT8_C(0x03)) {
            play_layers(behavior, frame, emit, emit_context);
            return true;
        }
    }
    return false;
}

static int matching_pad(
    const music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    size_t message_size
)
{
    uint8_t type;
    uint8_t channel;
    size_t index;

    if (message_size != 3U) {
        return -1;
    }
    type = message[0] & UINT8_C(0xf0);
    channel = message[0] & UINT8_C(0x0f);
    if (behavior->config.pad_type == MUSIC_RIG_CURRENT_SMK25_CONTROL_CC &&
        type != UINT8_C(0xb0)) {
        return -1;
    }
    if (behavior->config.pad_type == MUSIC_RIG_CURRENT_SMK25_CONTROL_NOTE &&
        type != UINT8_C(0x80) && type != UINT8_C(0x90)) {
        return -1;
    }
    for (index = 0U; index < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++index) {
        if (message[1] == behavior->config.pads[index] &&
            channel == behavior->config.pad_channels[index]) {
            return (int)index;
        }
    }
    return -1;
}

static bool handle_pad(
    music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    size_t message_size,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    int matched = matching_pad(behavior, message, message_size);
    size_t layer;
    bool pressed;

    if (matched < 0) {
        return false;
    }
    layer = (size_t)matched;
    pressed = (message[0] & UINT8_C(0xf0)) == UINT8_C(0x90)
        ? message[2] > UINT8_C(0)
        : message[2] >= behavior->config.pad_on_minimum;
    if (behavior->config.pad_behavior == MUSIC_RIG_CURRENT_SMK25_PAD_VALUE) {
        set_layer_enabled(
            behavior, layer, pressed, frame, emit, emit_context
        );
    } else {
        bool was_down = behavior->layers[layer].pad_down;

        behavior->layers[layer].pad_down = pressed;
        if (pressed && !was_down) {
            set_layer_enabled(
                behavior,
                layer,
                !behavior->layers[layer].enabled,
                frame,
                emit,
                emit_context
            );
        }
    }
    return true;
}

static int matching_knob(
    const music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    size_t message_size
)
{
    size_t index;

    if (message_size != 3U ||
        (message[0] & UINT8_C(0xf0)) != UINT8_C(0xb0)) {
        return -1;
    }
    for (index = 0U; index < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++index) {
        if (message[1] == behavior->config.knobs[index] &&
            (message[0] & UINT8_C(0x0f)) ==
                behavior->config.knob_channels[index]) {
            return (int)index;
        }
    }
    return -1;
}

static void handle_note(
    music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    uint8_t type = message[0] & UINT8_C(0xf0);
    uint8_t note = message[1];
    uint8_t velocity = message[2];
    bool note_on = type == UINT8_C(0x90) && velocity > UINT8_C(0);
    size_t layer;

    if (note_on) {
        bool new_gesture = behavior->physical_note_count == 0U;

        if (!behavior->physical_notes[note]) {
            behavior->physical_notes[note] = true;
            behavior->physical_note_count += 1U;
        }
        if (new_gesture) {
            for (layer = 0U;
                 layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT;
                 ++layer) {
                if (!behavior->layers[layer].enabled) {
                    continue;
                }
                release_active(behavior, layer, frame, emit, emit_context);
                clear_last_chord(behavior, layer);
                behavior->layers[layer].paused = false;
            }
        }
        for (layer = 0U;
             layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT;
             ++layer) {
            emit(emit_context, layer, frame, message, 3U);
            behavior->layers[layer].active_velocity[note] = velocity;
            if (behavior->layers[layer].enabled) {
                behavior->layers[layer].last_velocity[note] = velocity;
            }
        }
        behavior->generation += UINT32_C(1);
        return;
    }

    if (behavior->physical_notes[note]) {
        behavior->physical_notes[note] = false;
        if (behavior->physical_note_count > 0U) {
            behavior->physical_note_count -= 1U;
        }
    }
    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        if (behavior->layers[layer].enabled) {
            continue;
        }
        emit(emit_context, layer, frame, message, 3U);
        behavior->layers[layer].active_velocity[note] = UINT8_C(0);
    }
}

static void parse_legacy_line(
    music_rig_current_smk25_behavior *behavior,
    char *line
)
{
    const char *cursor;
    const char *end = line + strlen(line);
    int32_t values[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT];
    int32_t layer_number;
    int32_t enabled;
    int32_t paused;
    size_t index;

    if (strncmp(line, "KNOBS ", 6U) == 0) {
        cursor = line + 6;
        for (index = 0U;
             index < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT;
             ++index) {
            if (!read_integer(&cursor, end, &values[index])) {
                return;
            }
        }
        for (index = 0U;
             index < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT;
             ++index) {
            if (values[index] >= 0 && values[index] <= 127) {
                behavior->knob_values[index] = (uint8_t)values[index];
            }
        }
        return;
    }

    if (strncmp(line, "LAYER", 5U) != 0) {
        return;
    }
    cursor = line + 5;
    if (!read_integer(&cursor, end, &layer_number) ||
        !read_integer(&cursor, end, &enabled) ||
        !read_integer(&cursor, end, &paused) ||
        layer_number < 1 ||
        layer_number > (int32_t)MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT) {
        return;
    }
    index = (size_t)(layer_number - 1);
    behavior->layers[index].enabled = enabled != 0;
    behavior->layers[index].paused = enabled != 0 ? true : paused != 0;
    while (cursor < end && *cursor != '\0') {
        int32_t note;
        int32_t velocity;

        while (cursor < end && *cursor != '\0' && ascii_space(*cursor)) {
            cursor += 1;
        }
        if (!read_integer(&cursor, end, &note) || cursor >= end ||
            *cursor != ':') {
            break;
        }
        cursor += 1;
        if (!read_integer(&cursor, end, &velocity)) {
            break;
        }
        if (note >= 0 && note < 128 && velocity > 0 && velocity <= 127) {
            behavior->layers[index].last_velocity[note] = (uint8_t)velocity;
        }
    }
}

void music_rig_current_smk25_config_init(
    music_rig_current_smk25_config *config
)
{
    static const uint8_t pads[MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT] = {
        UINT8_C(40), UINT8_C(41), UINT8_C(42), UINT8_C(43),
        UINT8_C(36), UINT8_C(37), UINT8_C(38), UINT8_C(39)
    };
    size_t index;

    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->channel = UINT8_C(0);
    config->pad_type = MUSIC_RIG_CURRENT_SMK25_CONTROL_CC;
    config->pad_behavior = MUSIC_RIG_CURRENT_SMK25_PAD_VALUE;
    config->pad_on_minimum = UINT8_C(64);
    config->play.type = MUSIC_RIG_CURRENT_SMK25_CONTROL_NOTE;
    config->play.number = UINT8_C(94);
    config->stop.type = MUSIC_RIG_CURRENT_SMK25_CONTROL_NOTE;
    config->stop.number = UINT8_C(93);
    for (index = 0U; index < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++index) {
        config->pads[index] = pads[index];
        config->pad_channels[index] = (uint8_t)index;
        config->knobs[index] = (uint8_t)(UINT8_C(20) + (uint8_t)index);
        config->knob_channels[index] = UINT8_C(0);
    }
    config->default_chord[0] = UINT8_C(48);
    config->default_chord[1] = UINT8_C(52);
    config->default_chord[2] = UINT8_C(55);
    config->default_chord_count = 3U;
}

music_rig_result music_rig_current_smk25_init(
    music_rig_current_smk25_behavior *behavior,
    const music_rig_current_smk25_config *config
)
{
    size_t layer;

    if (behavior == NULL || !valid_config(config)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(behavior, 0, sizeof(*behavior));
    behavior->abi_version = MUSIC_RIG_CURRENT_SMK25_ABI_VERSION;
    behavior->config = *config;
    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        behavior->output_connection_counts[layer] = -1;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_restore_legacy_text(
    music_rig_current_smk25_behavior *behavior,
    const char *text,
    size_t text_size
)
{
    size_t offset = 0U;

    if (behavior == NULL || (text == NULL && text_size != 0U)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!valid_behavior(behavior) || !valid_config(&behavior->config)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    reset_state(behavior);
    while (text != NULL && offset < text_size && text[offset] != '\0') {
        char line[LEGACY_LINE_CAPACITY];
        size_t length = 0U;

        while (offset < text_size && text[offset] != '\0' &&
               text[offset] != '\n' && length + 1U < sizeof(line)) {
            line[length] = text[offset];
            length += 1U;
            offset += 1U;
        }
        if (offset < text_size && text[offset] == '\n') {
            offset += 1U;
        }
        line[length] = '\0';
        parse_legacy_line(behavior, line);
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_snapshot_read(
    const music_rig_current_smk25_behavior *behavior,
    music_rig_current_smk25_snapshot *snapshot
)
{
    size_t layer;

    if (!valid_behavior(behavior) || snapshot == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->version = MUSIC_RIG_CURRENT_SMK25_SNAPSHOT_VERSION;
    memcpy(
        snapshot->knob_values,
        behavior->knob_values,
        sizeof(snapshot->knob_values)
    );
    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        snapshot->enabled[layer] = behavior->layers[layer].enabled;
        snapshot->paused[layer] = behavior->layers[layer].paused;
        memcpy(
            snapshot->last_velocity[layer],
            behavior->layers[layer].last_velocity,
            sizeof(snapshot->last_velocity[layer])
        );
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_snapshot_restore(
    music_rig_current_smk25_behavior *behavior,
    const music_rig_current_smk25_snapshot *snapshot
)
{
    size_t layer;
    size_t note;

    if (behavior == NULL || snapshot == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!valid_behavior(behavior) || !valid_config(&behavior->config)) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (snapshot->version != MUSIC_RIG_CURRENT_SMK25_SNAPSHOT_VERSION) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        if (snapshot->knob_values[layer] > UINT8_C(127)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        for (note = 0U; note < MUSIC_RIG_CURRENT_SMK25_NOTE_COUNT; ++note) {
            if (snapshot->last_velocity[layer][note] > UINT8_C(127)) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
    }
    reset_state(behavior);
    memcpy(
        behavior->knob_values,
        snapshot->knob_values,
        sizeof(behavior->knob_values)
    );
    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        behavior->layers[layer].enabled = snapshot->enabled[layer];
        behavior->layers[layer].paused = snapshot->enabled[layer]
            ? true
            : snapshot->paused[layer];
        memcpy(
            behavior->layers[layer].last_velocity,
            snapshot->last_velocity[layer],
            sizeof(behavior->layers[layer].last_velocity)
        );
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_process_midi(
    music_rig_current_smk25_behavior *behavior,
    const uint8_t *message,
    size_t message_size,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    uint8_t type;
    uint8_t channel;
    int knob;
    size_t layer;

    if (!valid_behavior(behavior) || message == NULL || emit == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (handle_standard_transport(
            behavior,
            message,
            message_size,
            frame,
            emit,
            emit_context
        )) {
        return MUSIC_RIG_RESULT_OK;
    }
    if (message_size == 3U &&
        (message[0] & UINT8_C(0xf0)) < UINT8_C(0xf0) &&
        (message[1] > UINT8_C(127) || message[2] > UINT8_C(127))) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (is_control_message(
            behavior, &behavior->config.stop, message, message_size
        )) {
        if (matches_control(
                behavior, &behavior->config.stop, message, message_size
            )) {
            stop_layers(behavior, frame, emit, emit_context);
        }
        return MUSIC_RIG_RESULT_OK;
    }
    if (is_control_message(
            behavior, &behavior->config.play, message, message_size
        )) {
        if (matches_control(
                behavior, &behavior->config.play, message, message_size
            )) {
            play_layers(behavior, frame, emit, emit_context);
        }
        return MUSIC_RIG_RESULT_OK;
    }
    if (handle_pad(
            behavior,
            message,
            message_size,
            frame,
            emit,
            emit_context
        )) {
        return MUSIC_RIG_RESULT_OK;
    }
    knob = matching_knob(behavior, message, message_size);
    if (knob >= 0) {
        uint8_t volume[3];

        layer = (size_t)knob;
        volume[0] = (uint8_t)(
            UINT8_C(0xb0) | behavior->config.knob_channels[layer]
        );
        volume[1] = behavior->config.knobs[layer];
        volume[2] = message[2];
        behavior->knob_values[layer] = message[2];
        behavior->generation += UINT32_C(1);
        emit(emit_context, layer, frame, volume, sizeof(volume));
        return MUSIC_RIG_RESULT_OK;
    }
    type = message_size > 0U
        ? message[0] & UINT8_C(0xf0)
        : UINT8_C(0xff);
    channel = message_size > 0U
        ? message[0] & UINT8_C(0x0f)
        : UINT8_C(0xff);
    if (message_size == 3U && channel == behavior->config.channel &&
        (type == UINT8_C(0x80) || type == UINT8_C(0x90))) {
        handle_note(behavior, message, frame, emit, emit_context);
        return MUSIC_RIG_RESULT_OK;
    }
    for (layer = 0U; layer < MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT; ++layer) {
        emit(emit_context, layer, frame, message, message_size);
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_output_connections(
    music_rig_current_smk25_behavior *behavior,
    size_t layer,
    int32_t connection_count,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    if (!valid_behavior(behavior) ||
        layer >= MUSIC_RIG_CURRENT_SMK25_LAYER_COUNT ||
        connection_count < 0 || emit == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (connection_count > 0 &&
        connection_count > behavior->output_connection_counts[layer]) {
        emit_message(
            behavior,
            layer,
            frame,
            (uint8_t)(
                UINT8_C(0xb0) | behavior->config.knob_channels[layer]
            ),
            behavior->config.knobs[layer],
            behavior->knob_values[layer],
            emit,
            emit_context
        );
    }
    behavior->output_connection_counts[layer] = connection_count;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_stop(
    music_rig_current_smk25_behavior *behavior,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    if (!valid_behavior(behavior) || emit == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    stop_layers(behavior, frame, emit, emit_context);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_current_smk25_play(
    music_rig_current_smk25_behavior *behavior,
    uint32_t frame,
    music_rig_current_smk25_emit_fn emit,
    void *emit_context
)
{
    if (!valid_behavior(behavior) || emit == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    play_layers(behavior, frame, emit, emit_context);
    return MUSIC_RIG_RESULT_OK;
}
