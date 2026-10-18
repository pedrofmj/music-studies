#include "music_rig/arturia_session.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
#define FAKE_CALL_CAPACITY ((size_t)512)

typedef enum fake_call_kind {
    FAKE_ACTIVATE,
    FAKE_DEACTIVATE,
    FAKE_PATCH_VOLUME,
    FAKE_PATCH_REVERB,
    FAKE_NOTE,
    FAKE_CONTROLLER,
    FAKE_PITCH,
    FAKE_MASTER_VOLUME,
    FAKE_MASTER_MUTE
} fake_call_kind;

typedef struct fake_call {
    fake_call_kind kind;
    uint16_t patch;
    uint16_t value;
    uint8_t number;
    bool flag;
} fake_call;

typedef struct fake_engine {
    fake_call calls[FAKE_CALL_CAPACITY];
    size_t call_count;
    size_t invocation_count;
    size_t fail_at;
} fake_engine;

static music_rig_result record_call(
    fake_engine *engine,
    fake_call_kind kind,
    uint16_t patch,
    uint16_t value,
    uint8_t number,
    bool flag
)
{
    fake_call *call;

    if (engine->call_count >= FAKE_CALL_CAPACITY) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    call = &engine->calls[engine->call_count++];
    call->kind = kind;
    call->patch = patch;
    call->value = value;
    call->number = number;
    call->flag = flag;
    engine->invocation_count += 1U;
    return engine->invocation_count == engine->fail_at
        ? MUSIC_RIG_RESULT_ADAPTER_FAILURE
        : MUSIC_RIG_RESULT_OK;
}

static music_rig_result fake_activate(void *context, uint16_t patch)
{
    return record_call(context, FAKE_ACTIVATE, patch, 0U, 0U, false);
}

static music_rig_result fake_deactivate(void *context, uint16_t patch)
{
    return record_call(context, FAKE_DEACTIVATE, patch, 0U, 0U, false);
}

static music_rig_result fake_patch_volume(
    void *context,
    uint16_t patch,
    uint8_t value
)
{
    return record_call(context, FAKE_PATCH_VOLUME, patch, value, 0U, false);
}

static music_rig_result fake_patch_reverb(
    void *context,
    uint16_t patch,
    uint8_t value
)
{
    return record_call(context, FAKE_PATCH_REVERB, patch, value, 0U, false);
}

static music_rig_result fake_note(
    void *context,
    uint16_t patch,
    bool note_on,
    uint8_t note,
    uint8_t velocity
)
{
    return record_call(
        context,
        FAKE_NOTE,
        patch,
        velocity,
        note,
        note_on
    );
}

static music_rig_result fake_controller(
    void *context,
    uint16_t patch,
    uint8_t controller,
    uint8_t value
)
{
    return record_call(
        context,
        FAKE_CONTROLLER,
        patch,
        value,
        controller,
        false
    );
}

static music_rig_result fake_pitch(void *context, uint16_t patch, uint16_t value)
{
    return record_call(context, FAKE_PITCH, patch, value, 0U, false);
}

static music_rig_result fake_master_volume(void *context, uint8_t value)
{
    return record_call(context, FAKE_MASTER_VOLUME, 0U, value, 0U, false);
}

static music_rig_result fake_master_mute(void *context, bool muted)
{
    return record_call(context, FAKE_MASTER_MUTE, 0U, 0U, 0U, muted);
}

static const music_rig_arturia_engine_ops fake_ops = {
    fake_activate,
    fake_deactivate,
    fake_patch_volume,
    fake_patch_reverb,
    fake_note,
    fake_controller,
    fake_pitch,
    fake_master_volume,
    fake_master_mute
};

static void clear_calls(fake_engine *engine)
{
    engine->call_count = 0U;
    engine->invocation_count = 0U;
    engine->fail_at = 0U;
}

static int test_session(void)
{
    static const uint16_t patches[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT] = {
        10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U
    };
    static const uint8_t organ_layer[] = {8U};
    static const uint8_t mode4_layers[] = {1U, 3U, 5U, 8U};
    static const uint8_t mode6_layers[] = {1U, 2U, 3U, 6U, 8U, 9U};
    static const uint8_t failed_layers[] = {2U, 4U};
    fake_engine fake = {{{0}}, 0U, 0U, 0U};
    music_rig_arturia_session session;
    music_rig_arturia_shared_sf2_plan prior_plan;
    music_rig_arturia_control_state prior_controls;
    uint8_t midi[3];
    size_t index;

    CHECK(music_rig_arturia_session_init(
        &session,
        &fake_ops,
        &fake,
        patches,
        UINT16_C(24),
        organ_layer,
        sizeof(organ_layer),
        UINT8_C(100)
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(session.initialized && session.plan.route_count == 1U);
    CHECK(session.plan.routes[0].patch_index == 17U &&
        session.plan.routes[0].layer == 8U &&
        session.plan.routes[0].mixer_slot == 1U);
    CHECK(session.control_state.values[0] == 0U &&
        session.control_state.values[8] == 0U &&
        session.control_state.values[9] == 0U &&
        session.control_state.values[16] == 64U &&
        session.control_state.values[17] == 0U);
    CHECK(fake.call_count == 5U);
    CHECK(fake.calls[0].kind == FAKE_MASTER_VOLUME &&
        fake.calls[0].value == 100U);
    CHECK(fake.calls[1].kind == FAKE_MASTER_MUTE && !fake.calls[1].flag);
    CHECK(fake.calls[2].kind == FAKE_ACTIVATE && fake.calls[2].patch == 17U);
    CHECK(fake.calls[3].kind == FAKE_PATCH_VOLUME &&
        fake.calls[3].patch == 17U && fake.calls[3].value == 0U);
    CHECK(fake.calls[4].kind == FAKE_PATCH_REVERB &&
        fake.calls[4].patch == 17U && fake.calls[4].value == 64U);

    clear_calls(&fake);
    midi[0] = UINT8_C(0xb0);
    midi[1] = UINT8_C(73);
    midi[2] = UINT8_C(11);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    midi[1] = UINT8_C(76);
    midi[2] = UINT8_C(22);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    midi[1] = UINT8_C(93);
    midi[2] = UINT8_C(42);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(fake.call_count == 0U && session.control_state.values[0] == 11U &&
        session.control_state.values[11] == 22U &&
        session.control_state.values[13] == 42U);

    prior_controls = session.control_state;
    midi[1] = MUSIC_RIG_ARTURIA_MASTER_VOLUME_CC;
    midi[2] = UINT8_C(65);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(session.master_volume == 101U &&
        memcmp(&session.control_state, &prior_controls, sizeof(prior_controls)) == 0);
    midi[2] = UINT8_C(64);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    midi[1] = MUSIC_RIG_ARTURIA_MASTER_MUTE_CC;
    midi[2] = UINT8_C(127);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK && session.master_muted);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK && session.master_muted);
    midi[2] = 0U;
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK && !session.mute_button_down);
    midi[2] = UINT8_C(64);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK && !session.master_muted);
    CHECK(memcmp(&session.control_state, &prior_controls, sizeof(prior_controls)) == 0);
    CHECK(fake.calls[0].kind == FAKE_MASTER_VOLUME && fake.calls[0].value == 101U);
    CHECK(fake.calls[1].kind == FAKE_MASTER_MUTE && fake.calls[1].flag);
    CHECK(fake.calls[2].kind == FAKE_MASTER_MUTE && !fake.calls[2].flag);

    clear_calls(&fake);
    CHECK(music_rig_arturia_session_select_profile(
        &session,
        patches,
        UINT16_C(24),
        mode4_layers,
        sizeof(mode4_layers)
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(session.plan.route_count == 4U);
    for (index = 0U; index < session.plan.route_count; ++index) {
        CHECK(session.plan.routes[index].mixer_slot == index + 1U);
        CHECK(session.plan.routes[index].layer == mode4_layers[index]);
    }
    CHECK(session.plan.routes[0].fader_cc == 73U &&
        session.plan.routes[1].fader_cc == 79U &&
        session.plan.routes[2].fader_cc == 80U &&
        session.plan.routes[3].knob_cc == 16U);
    CHECK(fake.calls[0].kind == FAKE_ACTIVATE && fake.calls[0].patch == 10U);
    CHECK(fake.calls[1].kind == FAKE_ACTIVATE && fake.calls[1].patch == 12U);
    CHECK(fake.calls[2].kind == FAKE_ACTIVATE && fake.calls[2].patch == 14U);
    CHECK(fake.calls[3].kind == FAKE_PATCH_VOLUME &&
        fake.calls[3].patch == 10U && fake.calls[3].value == 11U);
    CHECK(fake.calls[4].kind == FAKE_PATCH_REVERB &&
        fake.calls[4].patch == 10U && fake.calls[4].value == 0U);
    CHECK(fake.calls[6].kind == FAKE_PATCH_REVERB &&
        fake.calls[6].patch == 12U && fake.calls[6].value == 22U);
    CHECK(fake.calls[8].kind == FAKE_PATCH_REVERB &&
        fake.calls[8].patch == 14U && fake.calls[8].value == 42U);

    clear_calls(&fake);
    CHECK(music_rig_arturia_session_select_profile(
        &session,
        patches,
        UINT16_C(24),
        mode6_layers,
        sizeof(mode6_layers)
    ) == MUSIC_RIG_RESULT_OK);
    CHECK(session.plan.route_count == 6U);
    for (index = 0U; index < session.plan.route_count; ++index) {
        CHECK(session.plan.routes[index].mixer_slot == index + 1U);
        CHECK(session.plan.routes[index].layer == mode6_layers[index]);
    }
    CHECK(fake.calls[0].kind == FAKE_DEACTIVATE && fake.calls[0].patch == 14U);
    CHECK(fake.calls[1].kind == FAKE_ACTIVATE && fake.calls[1].patch == 11U);
    CHECK(fake.calls[2].kind == FAKE_ACTIVATE && fake.calls[2].patch == 15U);
    CHECK(fake.calls[3].kind == FAKE_ACTIVATE && fake.calls[3].patch == 18U);
    CHECK(fake.calls[4].kind == FAKE_PATCH_VOLUME &&
        fake.calls[4].patch == 10U && fake.calls[4].value == 11U);
    CHECK(fake.calls[7].kind == FAKE_PATCH_REVERB &&
        fake.calls[7].patch == 12U && fake.calls[7].value == 22U);
    CHECK(fake.calls[15].kind == FAKE_PATCH_REVERB &&
        fake.calls[15].patch == 18U && fake.calls[15].value == 0U);

    clear_calls(&fake);
    midi[0] = UINT8_C(0x90);
    midi[1] = UINT8_C(60);
    midi[2] = UINT8_C(90);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(fake.call_count == 6U);
    for (index = 0U; index < 6U; ++index) {
        CHECK(fake.calls[index].kind == FAKE_NOTE &&
            fake.calls[index].patch == session.plan.routes[index].patch_index &&
            fake.calls[index].flag && fake.calls[index].number == 60U &&
            fake.calls[index].value == 90U);
    }
    midi[0] = UINT8_C(0x80);
    midi[2] = UINT8_C(45);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(fake.call_count == 12U && !fake.calls[11].flag);
    midi[0] = UINT8_C(0xb0);
    midi[1] = MUSIC_RIG_ARTURIA_SUSTAIN_CC;
    midi[2] = UINT8_C(127);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    midi[2] = 0U;
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(fake.call_count == 24U);
    for (index = 12U; index < 24U; ++index) {
        CHECK(fake.calls[index].kind == FAKE_CONTROLLER &&
            fake.calls[index].number == MUSIC_RIG_ARTURIA_SUSTAIN_CC &&
            fake.calls[index].patch ==
                session.plan.routes[(index - 12U) % 6U].patch_index);
        CHECK(fake.calls[index].value == (index < 18U ? 127U : 0U));
    }
    midi[1] = MUSIC_RIG_ARTURIA_MODULATION_CC;
    midi[2] = UINT8_C(37);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(fake.calls[24].kind == FAKE_CONTROLLER &&
        fake.calls[24].number == MUSIC_RIG_ARTURIA_MODULATION_CC);
    midi[0] = UINT8_C(0xe0);
    midi[1] = UINT8_C(1);
    midi[2] = UINT8_C(64);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK);
    CHECK(fake.calls[30].kind == FAKE_PITCH &&
        fake.calls[30].value == UINT16_C(8193));

    clear_calls(&fake);
    midi[0] = UINT8_C(0xb1);
    midi[1] = MUSIC_RIG_ARTURIA_SUSTAIN_CC;
    midi[2] = UINT8_C(127);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK && fake.call_count == 0U);
    midi[0] = UINT8_C(0xc0);
    midi[1] = UINT8_C(12);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 2U) ==
        MUSIC_RIG_RESULT_UNSUPPORTED && fake.call_count == 0U);
    midi[0] = UINT8_C(0xf0);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 1U) ==
        MUSIC_RIG_RESULT_UNSUPPORTED && fake.call_count == 0U);
    midi[0] = UINT8_C(0xb0);
    midi[1] = UINT8_C(119);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_OK && fake.call_count == 0U);

    prior_plan = session.plan;
    CHECK(music_rig_arturia_session_select_profile(
        &session, patches, UINT16_C(24), mode6_layers, 0U
    ) == MUSIC_RIG_RESULT_INVALID_ARGUMENT);
    {
        static const uint8_t duplicate_layers[] = {2U, 2U};

        CHECK(music_rig_arturia_session_select_profile(
            &session,
            patches,
            UINT16_C(24),
            duplicate_layers,
            sizeof(duplicate_layers)
        ) == MUSIC_RIG_RESULT_INVALID_DATA);
    }
    CHECK(music_rig_arturia_session_select_profile(
        &session,
        patches,
        UINT16_C(10),
        mode4_layers,
        sizeof(mode4_layers)
    ) == MUSIC_RIG_RESULT_INVALID_DATA);
    CHECK(memcmp(&session.plan, &prior_plan, sizeof(prior_plan)) == 0 &&
        fake.call_count == 0U);

    prior_controls = session.control_state;
    fake.fail_at = 1U;
    midi[0] = UINT8_C(0xb0);
    midi[1] = UINT8_C(73);
    midi[2] = UINT8_C(55);
    CHECK(music_rig_arturia_session_process_midi(&session, midi, 3U) ==
        MUSIC_RIG_RESULT_ADAPTER_FAILURE);
    CHECK(memcmp(&session.control_state, &prior_controls, sizeof(prior_controls)) == 0 &&
        !session.rollback_incomplete && fake.call_count == 2U &&
        fake.calls[0].kind == FAKE_PATCH_VOLUME && fake.calls[0].value == 55U &&
        fake.calls[1].kind == FAKE_PATCH_VOLUME && fake.calls[1].value == 11U);

    clear_calls(&fake);
    fake.fail_at = 7U;
    prior_plan = session.plan;
    CHECK(music_rig_arturia_session_select_profile(
        &session,
        patches,
        UINT16_C(24),
        failed_layers,
        sizeof(failed_layers)
    ) == MUSIC_RIG_RESULT_ADAPTER_FAILURE);
    CHECK(memcmp(&session.plan, &prior_plan, sizeof(prior_plan)) == 0 &&
        !session.rollback_incomplete);
    CHECK(fake.calls[0].kind == FAKE_DEACTIVATE &&
        fake.calls[0].patch == 10U);
    CHECK(fake.calls[5].kind == FAKE_ACTIVATE &&
        fake.calls[5].patch == 13U);
    CHECK(fake.calls[6].kind == FAKE_PATCH_VOLUME &&
        fake.calls[6].patch == 11U);
    CHECK(fake.calls[7].kind == FAKE_DEACTIVATE &&
        fake.calls[7].patch == 13U);
    CHECK(fake.calls[8].kind == FAKE_ACTIVATE &&
        fake.calls[8].patch == 18U);
    return 0;
}

int main(void)
{
    return test_session();
}
