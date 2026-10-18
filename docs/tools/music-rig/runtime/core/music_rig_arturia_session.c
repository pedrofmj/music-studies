#include "music_rig/arturia_session.h"

#include <string.h>

static bool engine_ops_valid(const music_rig_arturia_engine_ops *engine)
{
    return engine != NULL && engine->activate_patch != NULL &&
        engine->deactivate_patch != NULL &&
        engine->set_patch_volume != NULL &&
        engine->set_patch_reverb != NULL && engine->send_note != NULL &&
        engine->send_controller != NULL && engine->send_pitch_bend != NULL &&
        engine->set_master_volume != NULL && engine->set_master_mute != NULL;
}

static music_rig_result callback_result(music_rig_result result)
{
    if (result < MUSIC_RIG_RESULT_OK ||
        result > MUSIC_RIG_RESULT_BUFFER_TOO_SMALL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return result;
}

static bool plan_has_patch(
    const music_rig_arturia_shared_sf2_plan *plan,
    uint16_t patch_index
)
{
    size_t index;

    for (index = 0U; index < plan->route_count; ++index) {
        if (plan->routes[index].patch_index == patch_index) {
            return true;
        }
    }
    return false;
}

static music_rig_result build_plan(
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    music_rig_arturia_shared_sf2_plan *plan
)
{
    music_rig_result result;
    size_t index;

    result = music_rig_arturia_shared_sf2_plan_build(
        patch_indexes,
        patch_count,
        active_layers,
        active_layer_count,
        plan
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    for (index = 0U; index < plan->route_count; ++index) {
        size_t previous;

        for (previous = 0U; previous < index; ++previous) {
            if (plan->routes[previous].patch_index ==
                plan->routes[index].patch_index) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result apply_route_controls(
    music_rig_arturia_session *session,
    const music_rig_arturia_shared_sf2_route *route,
    const music_rig_arturia_control_state *state
)
{
    size_t layer_index = (size_t)route->layer - 1U;
    music_rig_result result;

    result = callback_result(session->engine.set_patch_volume(
        session->engine_context,
        route->patch_index,
        state->values[layer_index]
    ));
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    return callback_result(session->engine.set_patch_reverb(
        session->engine_context,
        route->patch_index,
        state->values[layer_index + MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT]
    ));
}

static bool rollback_switch(
    music_rig_arturia_session *session,
    const music_rig_arturia_shared_sf2_plan *previous,
    const uint16_t *deactivated,
    size_t deactivated_count,
    const uint16_t *activated,
    size_t activated_count
)
{
    bool complete = true;

    while (activated_count != 0U) {
        music_rig_result result;

        activated_count -= 1U;
        result = callback_result(session->engine.deactivate_patch(
            session->engine_context,
            activated[activated_count]
        ));
        complete = complete && result == MUSIC_RIG_RESULT_OK;
    }
    while (deactivated_count != 0U) {
        music_rig_result result;

        deactivated_count -= 1U;
        result = callback_result(session->engine.activate_patch(
            session->engine_context,
            deactivated[deactivated_count]
        ));
        complete = complete && result == MUSIC_RIG_RESULT_OK;
    }
    for (size_t index = 0U; index < previous->route_count; ++index) {
        music_rig_result result = apply_route_controls(
            session,
            &previous->routes[index],
            &session->control_state
        );

        complete = complete && result == MUSIC_RIG_RESULT_OK;
    }
    return complete;
}

static music_rig_result select_plan(
    music_rig_arturia_session *session,
    const music_rig_arturia_shared_sf2_plan *next
)
{
    uint16_t deactivated[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    uint16_t activated[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    size_t deactivated_count = 0U;
    size_t activated_count = 0U;
    size_t index;
    music_rig_result result = MUSIC_RIG_RESULT_OK;

    session->rollback_incomplete = false;
    for (index = 0U; index < session->plan.route_count; ++index) {
        uint16_t patch_index = session->plan.routes[index].patch_index;

        if (!plan_has_patch(next, patch_index)) {
            result = callback_result(session->engine.deactivate_patch(
                session->engine_context,
                patch_index
            ));
            if (result != MUSIC_RIG_RESULT_OK) {
                goto rollback;
            }
            deactivated[deactivated_count++] = patch_index;
        }
    }
    for (index = 0U; index < next->route_count; ++index) {
        const music_rig_arturia_shared_sf2_route *route = &next->routes[index];

        if (!plan_has_patch(&session->plan, route->patch_index)) {
            result = callback_result(session->engine.activate_patch(
                session->engine_context,
                route->patch_index
            ));
            if (result != MUSIC_RIG_RESULT_OK) {
                goto rollback;
            }
            activated[activated_count++] = route->patch_index;
        }
    }
    for (index = 0U; index < next->route_count; ++index) {
        result = apply_route_controls(
            session,
            &next->routes[index],
            &session->control_state
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            goto rollback;
        }
    }
    session->plan = *next;
    return MUSIC_RIG_RESULT_OK;

rollback:
    session->rollback_incomplete = !rollback_switch(
        session,
        &session->plan,
        deactivated,
        deactivated_count,
        activated,
        activated_count
    );
    return result;
}

music_rig_result music_rig_arturia_session_init(
    music_rig_arturia_session *session,
    const music_rig_arturia_engine_ops *engine,
    void *engine_context,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count,
    uint8_t initial_master_volume
)
{
    music_rig_arturia_shared_sf2_plan initial_plan;
    music_rig_result result;

    if (session == NULL || !engine_ops_valid(engine) ||
        initial_master_volume > UINT8_C(127)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = build_plan(
        patch_indexes,
        patch_count,
        active_layers,
        active_layer_count,
        &initial_plan
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    memset(session, 0, sizeof(*session));
    session->engine = *engine;
    session->engine_context = engine_context;
    session->master_volume = initial_master_volume;
    music_rig_arturia_control_state_init(&session->control_state);
    session->initialized = true;

    result = callback_result(engine->set_master_volume(
        engine_context,
        initial_master_volume
    ));
    if (result != MUSIC_RIG_RESULT_OK) {
        session->initialized = false;
        return result;
    }
    result = callback_result(engine->set_master_mute(engine_context, false));
    if (result != MUSIC_RIG_RESULT_OK) {
        session->initialized = false;
        return result;
    }
    result = select_plan(session, &initial_plan);
    if (result != MUSIC_RIG_RESULT_OK) {
        session->initialized = false;
    }
    return result;
}

music_rig_result music_rig_arturia_session_select_profile(
    music_rig_arturia_session *session,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
)
{
    music_rig_arturia_shared_sf2_plan next;
    music_rig_result result;

    if (session == NULL || !session->initialized) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    result = build_plan(
        patch_indexes,
        patch_count,
        active_layers,
        active_layer_count,
        &next
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    return select_plan(session, &next);
}

static music_rig_result apply_cached_control(
    music_rig_arturia_session *session,
    int control_index,
    uint8_t controller,
    uint8_t value
)
{
    music_rig_arturia_control_state next_state = session->control_state;
    size_t route_index;

    if (!music_rig_arturia_control_state_update(
            &next_state,
            UINT8_C(0xb0),
            controller,
            value
        )) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    for (route_index = 0U; route_index < session->plan.route_count;
         ++route_index) {
        const music_rig_arturia_shared_sf2_route *route =
            &session->plan.routes[route_index];
        uint8_t routed_cc = (size_t)control_index <
                MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT
            ? route->fader_cc
            : route->knob_cc;

        if (routed_cc == controller) {
            music_rig_result result;
            size_t value_index = (size_t)control_index;

            session->rollback_incomplete = false;
            if (value_index < MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT) {
                result = callback_result(session->engine.set_patch_volume(
                    session->engine_context,
                    route->patch_index,
                    value
                ));
                if (result != MUSIC_RIG_RESULT_OK) {
                    music_rig_result restored = callback_result(
                        session->engine.set_patch_volume(
                            session->engine_context,
                            route->patch_index,
                            session->control_state.values[value_index]
                        )
                    );
                    session->rollback_incomplete =
                        restored != MUSIC_RIG_RESULT_OK;
                    return result;
                }
            } else {
                value_index -= MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT;
                result = callback_result(session->engine.set_patch_reverb(
                    session->engine_context,
                    route->patch_index,
                    value
                ));
                if (result != MUSIC_RIG_RESULT_OK) {
                    music_rig_result restored = callback_result(
                        session->engine.set_patch_reverb(
                            session->engine_context,
                            route->patch_index,
                            session->control_state.values[
                                value_index + MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT
                            ]
                        )
                    );
                    session->rollback_incomplete =
                        restored != MUSIC_RIG_RESULT_OK;
                    return result;
                }
            }
            break;
        }
    }
    session->control_state = next_state;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result set_master_volume(
    music_rig_arturia_session *session,
    uint8_t relative_value
)
{
    int volume = (int)session->master_volume + (int)relative_value - 64;
    uint8_t next_volume;
    music_rig_result result;

    if (volume < 0) {
        volume = 0;
    } else if (volume > 127) {
        volume = 127;
    }
    next_volume = (uint8_t)volume;
    if (next_volume == session->master_volume) {
        return MUSIC_RIG_RESULT_OK;
    }
    result = callback_result(session->engine.set_master_volume(
        session->engine_context,
        next_volume
    ));
    if (result == MUSIC_RIG_RESULT_OK) {
        session->master_volume = next_volume;
    }
    return result;
}

static music_rig_result toggle_master_mute(music_rig_arturia_session *session)
{
    bool next_muted = !session->master_muted;
    music_rig_result result = callback_result(session->engine.set_master_mute(
        session->engine_context,
        next_muted
    ));

    if (result == MUSIC_RIG_RESULT_OK) {
        session->master_muted = next_muted;
        session->mute_button_down = true;
    }
    return result;
}

static music_rig_result broadcast_controller(
    music_rig_arturia_session *session,
    uint8_t controller,
    uint8_t value
)
{
    size_t index;

    for (index = 0U; index < session->plan.route_count; ++index) {
        music_rig_result result = callback_result(
            session->engine.send_controller(
                session->engine_context,
                session->plan.routes[index].patch_index,
                controller,
                value
            )
        );

        if (result != MUSIC_RIG_RESULT_OK) {
            session->rollback_incomplete = index != 0U;
            return result;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_arturia_session_process_midi(
    music_rig_arturia_session *session,
    const uint8_t *message,
    size_t message_size
)
{
    uint8_t status;
    uint8_t command;
    uint8_t channel;

    if (session == NULL || message == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (!session->initialized) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (message_size == 0U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    status = message[0];
    if (status >= UINT8_C(0xf0)) {
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    command = status & UINT8_C(0xf0);
    channel = status & UINT8_C(0x0f);
    if (command == UINT8_C(0xc0) || command == UINT8_C(0xd0)) {
        if (message_size != 2U || message[1] > UINT8_C(127)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    if (message_size != 3U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (message[1] > UINT8_C(127) || message[2] > UINT8_C(127)) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    if (channel != 0U) {
        return MUSIC_RIG_RESULT_OK;
    }

    switch (command) {
    case UINT8_C(0x80):
    case UINT8_C(0x90):
    {
        bool note_on = command == UINT8_C(0x90) && message[2] != 0U;
        size_t index;

        for (index = 0U; index < session->plan.route_count; ++index) {
            music_rig_result result = callback_result(session->engine.send_note(
                session->engine_context,
                session->plan.routes[index].patch_index,
                note_on,
                message[1],
                message[2]
            ));

            if (result != MUSIC_RIG_RESULT_OK) {
                session->rollback_incomplete = index != 0U;
                return result;
            }
        }
        return MUSIC_RIG_RESULT_OK;
    }
    case UINT8_C(0xb0):
        if (message[1] == MUSIC_RIG_ARTURIA_MASTER_VOLUME_CC) {
            return set_master_volume(session, message[2]);
        }
        if (message[1] == MUSIC_RIG_ARTURIA_MASTER_MUTE_CC) {
            bool pressed = message[2] >= UINT8_C(64);

            if (!pressed) {
                session->mute_button_down = false;
                return MUSIC_RIG_RESULT_OK;
            }
            if (session->mute_button_down) {
                return MUSIC_RIG_RESULT_OK;
            }
            return toggle_master_mute(session);
        }
        if (message[1] == MUSIC_RIG_ARTURIA_MODULATION_CC ||
            message[1] == MUSIC_RIG_ARTURIA_SUSTAIN_CC) {
            return broadcast_controller(session, message[1], message[2]);
        }
        {
            int control_index = music_rig_arturia_control_state_index(
                message[1]
            );

            if (control_index >= 0) {
                return apply_cached_control(
                    session,
                    control_index,
                    message[1],
                    message[2]
                );
            }
        }
        return MUSIC_RIG_RESULT_OK;
    case UINT8_C(0xe0):
        {
            uint16_t pitch_value = (uint16_t)(
                (uint16_t)message[1] | ((uint16_t)message[2] << 7U)
            );
            size_t index;

            for (index = 0U; index < session->plan.route_count; ++index) {
                music_rig_result result = callback_result(
                    session->engine.send_pitch_bend(
                        session->engine_context,
                        session->plan.routes[index].patch_index,
                        pitch_value
                    )
                );

                if (result != MUSIC_RIG_RESULT_OK) {
                    session->rollback_incomplete = index != 0U;
                    return result;
                }
            }
            return MUSIC_RIG_RESULT_OK;
        }
    default:
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
}
