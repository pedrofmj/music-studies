#include "music_rig/control.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <string.h>

static music_rig_protocol_request request(
    music_rig_operation operation,
    uint32_t flags,
    const char *slot,
    const char *profile
)
{
    music_rig_protocol_request value = {0};

    value.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    value.operation = (uint32_t)operation;
    value.flags = flags;
    value.request_id = UINT64_C(73);
    value.expected_generation = UINT64_C(41);
    if (slot != NULL) {
        fixture_copy(value.device_slot, slot);
    }
    if (profile != NULL) {
        fixture_copy(value.profile, profile);
    }
    return value;
}

static int dispatch_expect(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *value,
    music_rig_result expected,
    music_rig_protocol_response *response
)
{
    if (music_rig_control_dispatch(snapshot, value, response) !=
            MUSIC_RIG_RESULT_OK ||
        response->result_code != (uint32_t)expected ||
        response->request_id != value->request_id ||
        response->operation != value->operation ||
        response->previous_generation != snapshot->generation_id ||
        response->resulting_generation != snapshot->generation_id ||
        response->adopted_at_ns != UINT64_C(0) ||
        response->output_mode != snapshot->output_mode ||
        response->rollback_status !=
            (uint32_t)MUSIC_RIG_ROLLBACK_NOT_REQUIRED) {
        fputs("control response contract failed\n", stderr);
        return 1;
    }
    return 0;
}

static int dispatch_prepared_expect(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const music_rig_protocol_request *value,
    music_rig_result expected,
    music_rig_protocol_response *response
)
{
    if (music_rig_control_dispatch_prepared(
            snapshot,
            prepared_definitions,
            prepared_definition_count,
            value,
            response
        ) != MUSIC_RIG_RESULT_OK ||
        response->result_code != (uint32_t)expected ||
        response->previous_generation != snapshot->generation_id ||
        response->resulting_generation != snapshot->generation_id ||
        response->output_mode != snapshot->output_mode) {
        fputs("prepared control response contract failed\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    static music_rig_compiled_tables tables;
    static music_rig_compiled_tables alternate_tables;
    static music_rig_compiled_tables incompatible_tables;
    music_rig_compiled_definition alternate_definition;
    music_rig_prepared_definition prepared[2];
    music_rig_control_snapshot snapshot;
    music_rig_protocol_request value;
    music_rig_protocol_response response;

    if (init_compiled_tables_fixture(&tables) != MUSIC_RIG_RESULT_OK ||
        init_alternate_prepared_definition_fixture(
            &alternate_tables,
            &alternate_definition,
            &prepared[0]
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("could not prepare control fixture\n", stderr);
        return 1;
    }
    snapshot.generation_id = UINT64_C(41);
    snapshot.active_rig_profile = "full-live-rack";
    snapshot.tables = &tables;
    snapshot.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;

    value = request(MUSIC_RIG_OPERATION_STATUS, UINT32_C(0), NULL, NULL);
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        response.profile_count != UINT32_C(2) ||
        response.readiness != (uint32_t)MUSIC_RIG_READINESS_CONTROL_ONLY ||
        response.flags != MUSIC_RIG_RESPONSE_OUTPUT_SUPPRESSED) {
        fputs("status inspection failed\n", stderr);
        return 1;
    }

    value = request(
        MUSIC_RIG_OPERATION_LIST_PROFILES,
        UINT32_C(0),
        "smc-mixer-main",
        NULL
    );
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        response.profile_count != UINT32_C(1) ||
        strcmp(response.profiles[0].profile, "eight-band-eq") != 0) {
        fputs("filtered profile inspection failed\n", stderr);
        return 1;
    }

    if (dispatch_prepared_expect(
            &snapshot,
            prepared,
            1U,
            &value,
            MUSIC_RIG_RESULT_OK,
            &response
        ) ||
        response.profile_count != UINT32_C(2) ||
        strcmp(response.profiles[0].profile, "eight-band-eq") != 0 ||
        response.profiles[0].flags != MUSIC_RIG_PROFILE_ACTIVE ||
        strcmp(response.profiles[1].profile, "multilevel-volume") != 0 ||
        response.profiles[1].flags != UINT32_C(0)) {
        fputs("prepared profile inventory failed\n", stderr);
        return 1;
    }

    value = request(
        MUSIC_RIG_OPERATION_SWITCH_GLOBAL,
        MUSIC_RIG_REQUEST_DRY_RUN,
        NULL,
        "multilevel-volume-mixed-pads"
    );
    if (dispatch_prepared_expect(
            &snapshot,
            prepared,
            1U,
            &value,
            MUSIC_RIG_RESULT_OK,
            &response
        ) ||
        response.profile_count != UINT32_C(2) ||
        strcmp(
            response.selected_profile,
            "multilevel-volume-mixed-pads"
        ) != 0 ||
        response.profiles[0].flags != UINT32_C(0) ||
        response.profiles[1].flags != UINT32_C(0)) {
        fputs("prepared global dry-run failed\n", stderr);
        return 1;
    }

    value = request(
        MUSIC_RIG_OPERATION_SWITCH_DEVICE,
        MUSIC_RIG_REQUEST_DRY_RUN,
        "smc-mixer-main",
        "multilevel-volume"
    );
    if (dispatch_prepared_expect(
            &snapshot,
            prepared,
            1U,
            &value,
            MUSIC_RIG_RESULT_OK,
            &response
        ) ||
        response.profile_count != UINT32_C(1) ||
        strcmp(response.profiles[0].profile, "multilevel-volume") != 0 ||
        response.profiles[0].flags != UINT32_C(0)) {
        fputs("prepared device dry-run failed\n", stderr);
        return 1;
    }

    prepared[1] = prepared[0];
    if (music_rig_control_prepared_definitions_validate(
            &snapshot,
            prepared,
            2U
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("duplicate prepared Rig Profile was accepted\n", stderr);
        return 1;
    }
    incompatible_tables = alternate_tables;
    fixture_copy(
        incompatible_tables.device_profiles[1].slot,
        "smc-mixer-other"
    );
    fixture_copy(
        incompatible_tables.input_bindings[1].slot,
        "smc-mixer-other"
    );
    fixture_copy(
        incompatible_tables.ownership[1].owners[0].slot,
        "smc-mixer-other"
    );
    prepared[0].tables = &incompatible_tables;
    if (music_rig_control_prepared_definitions_validate(
            &snapshot,
            prepared,
            1U
        ) != MUSIC_RIG_RESULT_INVALID_DATA) {
        fputs("prepared stable-port drift was accepted\n", stderr);
        return 1;
    }
    prepared[0].tables = &alternate_tables;

    value = request(
        MUSIC_RIG_OPERATION_VALIDATE_ACTIVE,
        UINT32_C(0),
        NULL,
        NULL
    );
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        (response.flags & MUSIC_RIG_RESPONSE_VALID) == UINT32_C(0)) {
        fputs("active validation failed\n", stderr);
        return 1;
    }

    value = request(
        MUSIC_RIG_OPERATION_SWITCH_GLOBAL,
        MUSIC_RIG_REQUEST_DRY_RUN,
        NULL,
        "full-live-rack"
    );
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        response.profile_count != UINT32_C(2) ||
        (response.flags & (MUSIC_RIG_RESPONSE_DRY_RUN |
            MUSIC_RIG_RESPONSE_VALID |
            MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY)) !=
            (MUSIC_RIG_RESPONSE_DRY_RUN | MUSIC_RIG_RESPONSE_VALID |
                MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY)) {
        fputs("global dry-run failed\n", stderr);
        return 1;
    }

    value = request(
        MUSIC_RIG_OPERATION_SWITCH_DEVICE,
        MUSIC_RIG_REQUEST_DRY_RUN,
        "smc-mixer-main",
        "eight-band-eq"
    );
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        response.profile_count != UINT32_C(1) ||
        strcmp(response.selected_device_slot, "smc-mixer-main") != 0) {
        fputs("device dry-run failed\n", stderr);
        return 1;
    }

    value.operation = (uint32_t)MUSIC_RIG_OPERATION_PREPARE_DEVICE;
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response)) {
        return 1;
    }
    value = request(
        MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE,
        MUSIC_RIG_REQUEST_DRY_RUN,
        "smc-mixer-main",
        NULL
    );
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        strcmp(response.selected_profile, "eight-band-eq") != 0) {
        fputs("reset dry-run failed\n", stderr);
        return 1;
    }
    value = request(
        MUSIC_RIG_OPERATION_RELOAD_COMPILED_DEFINITION,
        MUSIC_RIG_REQUEST_DRY_RUN,
        NULL,
        NULL
    );
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        (response.flags & MUSIC_RIG_RESPONSE_VALID) == UINT32_C(0)) {
        fputs("reload dry-run validation failed\n", stderr);
        return 1;
    }

    value = request(
        MUSIC_RIG_OPERATION_SWITCH_GLOBAL,
        UINT32_C(0),
        NULL,
        "full-live-rack"
    );
    if (dispatch_expect(
            &snapshot,
            &value,
            MUSIC_RIG_RESULT_UNSUPPORTED,
            &response
        )) {
        return 1;
    }
    value.flags = MUSIC_RIG_REQUEST_DRY_RUN;
    fixture_copy(value.profile, "modeled-piano");
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_NOT_FOUND, &response)) {
        return 1;
    }
    value.expected_generation = UINT64_C(40);
    if (dispatch_expect(
            &snapshot,
            &value,
            MUSIC_RIG_RESULT_GENERATION_CONFLICT,
            &response
        )) {
        return 1;
    }

    tables.device_profiles[1].readiness = MUSIC_RIG_READINESS_COLD;
    value = request(MUSIC_RIG_OPERATION_STATUS, UINT32_C(0), NULL, NULL);
    if (dispatch_expect(&snapshot, &value, MUSIC_RIG_RESULT_OK, &response) ||
        response.readiness != (uint32_t)MUSIC_RIG_READINESS_COLD ||
        response.warning_flags != MUSIC_RIG_WARNING_COLD_REQUIRED) {
        fputs("cold readiness warning failed\n", stderr);
        return 1;
    }
    tables.device_profiles[1].readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;

    value.operation = UINT32_C(99);
    if (music_rig_control_dispatch(&snapshot, &value, &response) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid control request was accepted\n", stderr);
        return 1;
    }
    return 0;
}
