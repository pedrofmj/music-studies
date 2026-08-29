#include "music_rig/control.h"
#include "music_rig/device_ports.h"

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
    const music_rig_compiled_device_profile *source,
    uint32_t flags
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
    target->flags = flags;
    response->profile_count += UINT32_C(1);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result append_profiles(
    const music_rig_compiled_tables *tables,
    const char *slot,
    uint32_t flags,
    music_rig_protocol_response *response
)
{
    size_t index;

    for (index = 0; index < tables->device_profile_count; ++index) {
        const music_rig_compiled_device_profile *profile =
            &tables->device_profiles[index];

        if (slot[0] == '\0' || strcmp(slot, profile->slot) == 0) {
            music_rig_result result = append_profile(response, profile, flags);

            if (result != MUSIC_RIG_RESULT_OK) {
                return result;
            }
        }
    }
    return response->profile_count == UINT32_C(0)
        ? MUSIC_RIG_RESULT_NOT_FOUND
        : MUSIC_RIG_RESULT_OK;
}

static music_rig_result append_profile_unique(
    music_rig_protocol_response *response,
    const music_rig_compiled_device_profile *source,
    uint32_t flags
)
{
    size_t index;

    for (index = 0U; index < response->profile_count; ++index) {
        music_rig_protocol_profile *existing = &response->profiles[index];

        if (strcmp(existing->device_slot, source->slot) == 0 &&
            strcmp(existing->profile, source->profile) == 0) {
            if (existing->readiness != (uint32_t)source->readiness) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
            existing->flags |= flags;
            return MUSIC_RIG_RESULT_OK;
        }
    }
    return append_profile(response, source, flags);
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

music_rig_result music_rig_control_prepared_definitions_validate(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count
)
{
    music_rig_device_port_catalogue active_ports;
    music_rig_result result;
    size_t index;

    if (snapshot == NULL || snapshot->generation_id == UINT64_C(0) ||
        !bounded_text(snapshot->active_rig_profile, true) ||
        !table_header_is_valid(snapshot->tables) ||
        prepared_definition_count > MUSIC_RIG_PREPARED_DEFINITION_CAPACITY ||
        (prepared_definition_count != 0U &&
            prepared_definitions == NULL)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_device_port_catalogue_build(
        snapshot->tables,
        &active_ports
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    for (index = 0U; index < prepared_definition_count; ++index) {
        const music_rig_prepared_definition *prepared =
            &prepared_definitions[index];
        music_rig_device_port_catalogue candidate_ports;
        music_rig_generation generation;
        size_t previous;

        if (prepared->definition == NULL || prepared->tables == NULL) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        result = music_rig_definition_generation_init(
            prepared->definition,
            prepared->tables,
            &generation
        );
        if (result != MUSIC_RIG_RESULT_OK ||
            generation.mapping != prepared->tables) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        for (previous = 0U; previous < index; ++previous) {
            if (strcmp(
                    prepared_definitions[previous].definition->
                        active_rig_profile,
                    prepared->definition->active_rig_profile
                ) == 0) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
        result = music_rig_device_port_catalogue_build(
            prepared->tables,
            &candidate_ports
        );
        if (result != MUSIC_RIG_RESULT_OK ||
            !music_rig_device_port_catalogues_match(
                &active_ports,
                &candidate_ports
            )) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static const music_rig_compiled_tables *find_rig_tables(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const char *profile,
    uint32_t *profile_flags
)
{
    size_t index;

    if (strcmp(profile, snapshot->active_rig_profile) == 0) {
        *profile_flags = MUSIC_RIG_PROFILE_ACTIVE;
        return snapshot->tables;
    }
    for (index = 0U; index < prepared_definition_count; ++index) {
        if (strcmp(
                profile,
                prepared_definitions[index].definition->active_rig_profile
            ) == 0) {
            *profile_flags = UINT32_C(0);
            return prepared_definitions[index].tables;
        }
    }
    return NULL;
}

static const music_rig_compiled_device_profile *
find_available_device_profile(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const char *slot,
    const char *profile,
    uint32_t *profile_flags
)
{
    const music_rig_compiled_device_profile *found;
    size_t index;

    found = find_device_profile(snapshot->tables, slot, profile);
    if (found != NULL) {
        *profile_flags = MUSIC_RIG_PROFILE_ACTIVE;
        return found;
    }
    for (index = 0U; index < prepared_definition_count; ++index) {
        found = find_device_profile(
            prepared_definitions[index].tables,
            slot,
            profile
        );
        if (found != NULL) {
            *profile_flags = UINT32_C(0);
            return found;
        }
    }
    return NULL;
}

static music_rig_result append_available_profiles(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const char *slot,
    music_rig_protocol_response *response
)
{
    music_rig_result result;
    size_t definition_index;

    result = append_profiles(
        snapshot->tables,
        slot,
        MUSIC_RIG_PROFILE_ACTIVE,
        response
    );
    if (result != MUSIC_RIG_RESULT_OK &&
        result != MUSIC_RIG_RESULT_NOT_FOUND) {
        return result;
    }
    for (definition_index = 0U;
         definition_index < prepared_definition_count;
         ++definition_index) {
        const music_rig_compiled_tables *tables =
            prepared_definitions[definition_index].tables;
        size_t profile_index;

        for (profile_index = 0U;
             profile_index < tables->device_profile_count;
             ++profile_index) {
            const music_rig_compiled_device_profile *profile =
                &tables->device_profiles[profile_index];

            if (slot[0] == '\0' || strcmp(slot, profile->slot) == 0) {
                result = append_profile_unique(
                    response,
                    profile,
                    UINT32_C(0)
                );
                if (result != MUSIC_RIG_RESULT_OK) {
                    return result;
                }
            }
        }
    }
    return response->profile_count == UINT32_C(0)
        ? MUSIC_RIG_RESULT_NOT_FOUND
        : MUSIC_RIG_RESULT_OK;
}

static music_rig_result dispatch_global_dry_run(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    const music_rig_compiled_tables *tables;
    music_rig_result result;
    uint32_t profile_flags;

    copy_text(response->selected_profile, request->profile);
    tables = find_rig_tables(
        snapshot,
        prepared_definitions,
        prepared_definition_count,
        request->profile,
        &profile_flags
    );
    if (tables == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }

    result = append_profiles(tables, "", profile_flags, response);
    if (result == MUSIC_RIG_RESULT_OK) {
        set_readiness(response, aggregate_readiness(tables));
    }
    return result;
}

static music_rig_result dispatch_device_dry_run(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    const music_rig_compiled_device_profile *profile;
    uint32_t profile_flags;

    profile = find_available_device_profile(
        snapshot,
        prepared_definitions,
        prepared_definition_count,
        request->device_slot,
        request->profile,
        &profile_flags
    );
    copy_text(response->selected_device_slot, request->device_slot);
    copy_text(response->selected_profile, request->profile);
    if (profile == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    set_readiness(response, profile->readiness);
    return append_profile(response, profile, profile_flags);
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
            return append_profile(response, profile, MUSIC_RIG_PROFILE_ACTIVE);
        }
    }
    return MUSIC_RIG_RESULT_NOT_FOUND;
}

music_rig_result music_rig_control_dispatch_prepared(
    const music_rig_control_snapshot *snapshot,
    const music_rig_prepared_definition *prepared_definitions,
    size_t prepared_definition_count,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    uint8_t validated_request[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
    music_rig_result operation_result = MUSIC_RIG_RESULT_OK;
    music_rig_result validation_result;
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

    validation_result = music_rig_control_prepared_definitions_validate(
        snapshot,
        prepared_definitions,
        prepared_definition_count
    );
    if (validation_result != MUSIC_RIG_RESULT_OK) {
        return validation_result;
    }

    memset(response, 0, sizeof(*response));
    response->protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    response->operation = request->operation;
    response->flags = MUSIC_RIG_RESPONSE_OUTPUT_SUPPRESSED;
    response->request_id = request->request_id;
    response->previous_generation = snapshot->generation_id;
    response->resulting_generation = snapshot->generation_id;
    response->rollback_status = (uint32_t)MUSIC_RIG_ROLLBACK_NOT_REQUIRED;
    response->output_mode = snapshot->output_mode;
    copy_text(response->active_rig_profile, snapshot->active_rig_profile);

    if (snapshot->output_mode != MUSIC_RIG_OUTPUT_SUPPRESSED &&
        snapshot->output_mode != MUSIC_RIG_OUTPUT_ENABLED) {
        operation_result = MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    } else if (request->expected_generation != UINT64_C(0) &&
        request->expected_generation != snapshot->generation_id) {
        operation_result = MUSIC_RIG_RESULT_GENERATION_CONFLICT;
    } else {
        is_dry_run = request->flags == MUSIC_RIG_REQUEST_DRY_RUN;
        switch ((music_rig_operation)request->operation) {
        case MUSIC_RIG_OPERATION_STATUS:
            operation_result = append_profiles(
                snapshot->tables,
                "",
                MUSIC_RIG_PROFILE_ACTIVE,
                response
            );
            if (operation_result == MUSIC_RIG_RESULT_OK) {
                set_readiness(response, aggregate_readiness(snapshot->tables));
            }
            break;
        case MUSIC_RIG_OPERATION_LIST_PROFILES:
            operation_result = append_available_profiles(
                snapshot,
                prepared_definitions,
                prepared_definition_count,
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
                    MUSIC_RIG_PROFILE_ACTIVE,
                    response
                );
            }
            break;
        case MUSIC_RIG_OPERATION_PREPARE_GLOBAL:
        case MUSIC_RIG_OPERATION_SWITCH_GLOBAL:
            operation_result = is_dry_run
                ? dispatch_global_dry_run(
                    snapshot,
                    prepared_definitions,
                    prepared_definition_count,
                    request,
                    response
                )
                : MUSIC_RIG_RESULT_UNSUPPORTED;
            break;
        case MUSIC_RIG_OPERATION_PREPARE_DEVICE:
        case MUSIC_RIG_OPERATION_SWITCH_DEVICE:
            operation_result = is_dry_run
                ? dispatch_device_dry_run(
                    snapshot,
                    prepared_definitions,
                    prepared_definition_count,
                    request,
                    response
                )
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

music_rig_result music_rig_control_dispatch(
    const music_rig_control_snapshot *snapshot,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    return music_rig_control_dispatch_prepared(
        snapshot,
        NULL,
        0U,
        request,
        response
    );
}
