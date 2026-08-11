#include "music_rig/control.h"

#include <stdbool.h>
#include <string.h>

_Static_assert(
    MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY == MUSIC_RIG_IDENTIFIER_CAPACITY,
    "protocol and compiled-table identifiers must have equal capacity"
);
_Static_assert(
    MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY == MUSIC_RIG_DEVICE_PROFILE_CAPACITY,
    "protocol response must hold every compiled device profile"
);

static bool bounded_text(const char *value, bool required)
{
    size_t index;

    if (value == NULL) {
        return false;
    }
    for (index = 0; index < MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY; ++index) {
        if (value[index] == '\0') {
            return !required || index != 0;
        }
    }
    return false;
}

static void copy_text(char *target, const char *source)
{
    size_t size = strlen(source);

    memcpy(target, source, size + 1U);
}

static bool table_header_is_valid(const music_rig_compiled_tables *tables)
{
    return tables != NULL &&
        tables->prepared_version == MUSIC_RIG_COMPILED_TABLES_VERSION &&
        tables->device_profile_count > UINT32_C(0) &&
        tables->device_profile_count <= MUSIC_RIG_DEVICE_PROFILE_CAPACITY &&
        tables->input_binding_count == tables->device_profile_count &&
        tables->mapping_count > UINT32_C(0) &&
        tables->mapping_count <= MUSIC_RIG_MAPPING_CAPACITY &&
        tables->target_binding_count > UINT32_C(0) &&
        tables->target_binding_count <= MUSIC_RIG_TARGET_BINDING_CAPACITY &&
        tables->ownership_count > UINT32_C(0) &&
        tables->ownership_count <= MUSIC_RIG_OWNERSHIP_CAPACITY;
}

static music_rig_readiness aggregate_readiness(
    const music_rig_compiled_tables *tables
)
{
    music_rig_readiness readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;
    size_t index;

    for (index = 0; index < tables->device_profile_count; ++index) {
        if (tables->device_profiles[index].readiness > readiness) {
            readiness = tables->device_profiles[index].readiness;
        }
    }
    return readiness;
}

static music_rig_readiness response_readiness(
    const music_rig_protocol_response *response
)
{
    music_rig_readiness readiness = MUSIC_RIG_READINESS_CONTROL_ONLY;
    size_t index;

    for (index = 0; index < response->profile_count; ++index) {
        if ((music_rig_readiness)response->profiles[index].readiness >
            readiness) {
            readiness = (music_rig_readiness)
                response->profiles[index].readiness;
        }
    }
    return readiness;
}

static void set_readiness(
    music_rig_protocol_response *response,
    music_rig_readiness readiness
)
{
    response->readiness = (uint32_t)readiness;
    if (readiness == MUSIC_RIG_READINESS_COLD) {
        response->warning_flags |= MUSIC_RIG_WARNING_COLD_REQUIRED;
    }
}

static music_rig_result append_profile(
    music_rig_protocol_response *response,
    const music_rig_compiled_device_profile *source
)
{
    music_rig_protocol_profile *target;

    if (response->profile_count >= MUSIC_RIG_PROTOCOL_PROFILE_CAPACITY) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    target = &response->profiles[response->profile_count];
    copy_text(target->device_slot, source->slot);
    copy_text(target->profile, source->profile);
    target->readiness = (uint32_t)source->readiness;
    target->flags = MUSIC_RIG_PROFILE_ACTIVE;
    response->profile_count += UINT32_C(1);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result append_profiles(
    const music_rig_compiled_tables *tables,
    const char *slot,
    music_rig_protocol_response *response
)
{
    size_t index;

    for (index = 0; index < tables->device_profile_count; ++index) {
        const music_rig_compiled_device_profile *profile =
            &tables->device_profiles[index];

        if (slot[0] == '\0' || strcmp(slot, profile->slot) == 0) {
            music_rig_result result = append_profile(response, profile);

            if (result != MUSIC_RIG_RESULT_OK) {
                return result;
            }
        }
    }
    return response->profile_count == UINT32_C(0)
        ? MUSIC_RIG_RESULT_NOT_FOUND
        : MUSIC_RIG_RESULT_OK;
}

static const music_rig_compiled_device_profile *find_device_profile(
    const music_rig_compiled_tables *tables,
    const char *slot,
    const char *profile
)
{
    size_t index;

    for (index = 0; index < tables->device_profile_count; ++index) {
        const music_rig_compiled_device_profile *candidate =
            &tables->device_profiles[index];

        if (strcmp(candidate->slot, slot) == 0 &&
            strcmp(candidate->profile, profile) == 0) {
            return candidate;
        }
    }
    return NULL;
}

static music_rig_result validate_tables(
    const music_rig_compiled_tables *tables
)
{
    return music_rig_compiled_tables_validate(
        tables,
        tables->device_profile_count,
        tables->mapping_count,
        tables->target_binding_count,
        tables->ownership_count
    );
}

static music_rig_result dispatch_global_dry_run(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    music_rig_result result;

    copy_text(response->selected_profile, request->profile);
    if (strcmp(request->profile, snapshot->active_rig_profile) != 0) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }

    result = append_profiles(snapshot->tables, "", response);
    if (result == MUSIC_RIG_RESULT_OK) {
        set_readiness(response, aggregate_readiness(snapshot->tables));
    }
    return result;
}

static music_rig_result dispatch_device_dry_run(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    const music_rig_compiled_device_profile *profile = find_device_profile(
        snapshot->tables,
        request->device_slot,
        request->profile
    );

    copy_text(response->selected_device_slot, request->device_slot);
    copy_text(response->selected_profile, request->profile);
    if (profile == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    set_readiness(response, profile->readiness);
    return append_profile(response, profile);
}

static music_rig_result dispatch_reset_dry_run(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    size_t index;

    copy_text(response->selected_device_slot, request->device_slot);
    for (index = 0; index < snapshot->tables->device_profile_count; ++index) {
        const music_rig_compiled_device_profile *profile =
            &snapshot->tables->device_profiles[index];

        if (strcmp(profile->slot, request->device_slot) == 0) {
            copy_text(response->selected_profile, profile->profile);
            set_readiness(response, profile->readiness);
            return append_profile(response, profile);
        }
    }
    return MUSIC_RIG_RESULT_NOT_FOUND;
}

music_rig_result music_rig_control_dispatch(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    uint8_t validated_request[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
    music_rig_result operation_result = MUSIC_RIG_RESULT_OK;
    bool is_dry_run;

    if (snapshot == NULL || request == NULL || response == NULL ||
        snapshot->generation_id == UINT64_C(0) ||
        !bounded_text(snapshot->active_rig_profile, true) ||
        !table_header_is_valid(snapshot->tables) ||
        music_rig_protocol_encode_request(
            request,
            validated_request,
            sizeof(validated_request)
        ) != MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    memset(response, 0, sizeof(*response));
    response->protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    response->operation = request->operation;
    response->flags = MUSIC_RIG_RESPONSE_OUTPUT_SUPPRESSED;
    response->request_id = request->request_id;
    response->previous_generation = snapshot->generation_id;
    response->resulting_generation = snapshot->generation_id;
    response->rollback_status = (uint32_t)MUSIC_RIG_ROLLBACK_NOT_REQUIRED;
    copy_text(response->active_rig_profile, snapshot->active_rig_profile);

    if (snapshot->output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED) {
        operation_result = MUSIC_RIG_RESULT_UNSUPPORTED;
    } else if (request->expected_generation != UINT64_C(0) &&
        request->expected_generation != snapshot->generation_id) {
        operation_result = MUSIC_RIG_RESULT_GENERATION_CONFLICT;
    } else {
        is_dry_run = request->flags == MUSIC_RIG_REQUEST_DRY_RUN;
        switch ((music_rig_operation)request->operation) {
        case MUSIC_RIG_OPERATION_STATUS:
            operation_result = append_profiles(snapshot->tables, "", response);
            if (operation_result == MUSIC_RIG_RESULT_OK) {
                set_readiness(response, aggregate_readiness(snapshot->tables));
            }
            break;
        case MUSIC_RIG_OPERATION_LIST_PROFILES:
            operation_result = append_profiles(
                snapshot->tables,
                request->device_slot,
                response
            );
            if (operation_result == MUSIC_RIG_RESULT_OK) {
                set_readiness(response, response_readiness(response));
            }
            break;
        case MUSIC_RIG_OPERATION_VALIDATE_ACTIVE:
            operation_result = validate_tables(snapshot->tables);
            if (operation_result == MUSIC_RIG_RESULT_OK) {
                response->flags |= MUSIC_RIG_RESPONSE_VALID;
                set_readiness(response, aggregate_readiness(snapshot->tables));
                operation_result = append_profiles(
                    snapshot->tables,
                    "",
                    response
                );
            }
            break;
        case MUSIC_RIG_OPERATION_PREPARE_GLOBAL:
        case MUSIC_RIG_OPERATION_SWITCH_GLOBAL:
            operation_result = is_dry_run
                ? dispatch_global_dry_run(snapshot, request, response)
                : MUSIC_RIG_RESULT_UNSUPPORTED;
            break;
        case MUSIC_RIG_OPERATION_PREPARE_DEVICE:
        case MUSIC_RIG_OPERATION_SWITCH_DEVICE:
            operation_result = is_dry_run
                ? dispatch_device_dry_run(snapshot, request, response)
                : MUSIC_RIG_RESULT_UNSUPPORTED;
            break;
        case MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE:
            operation_result = is_dry_run
                ? dispatch_reset_dry_run(snapshot, request, response)
                : MUSIC_RIG_RESULT_UNSUPPORTED;
            break;
        case MUSIC_RIG_OPERATION_RELOAD_COMPILED_DEFINITION:
            operation_result = is_dry_run
                ? validate_tables(snapshot->tables)
                : MUSIC_RIG_RESULT_UNSUPPORTED;
            if (operation_result == MUSIC_RIG_RESULT_OK) {
                response->flags |= MUSIC_RIG_RESPONSE_VALID;
                set_readiness(response, aggregate_readiness(snapshot->tables));
            }
            break;
        default:
            operation_result = MUSIC_RIG_RESULT_INVALID_ARGUMENT;
            break;
        }
        if (is_dry_run && operation_result == MUSIC_RIG_RESULT_OK) {
            response->flags |= MUSIC_RIG_RESPONSE_DRY_RUN |
                MUSIC_RIG_RESPONSE_VALID |
                MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY;
        }
    }

    response->result_code = (uint32_t)operation_result;
    return MUSIC_RIG_RESULT_OK;
}
