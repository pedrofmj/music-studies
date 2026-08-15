#include "music_rig/cli.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct output_builder {
    char *output;
    size_t capacity;
    size_t size;
    bool failed;
} output_builder;

static bool identifier_is_valid(const char *value)
{
    size_t index;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (index = 0; index < MUSIC_RIG_PROTOCOL_IDENTIFIER_CAPACITY; ++index) {
        unsigned char character = (unsigned char)value[index];

        if (character == (unsigned char)'\0') {
            return true;
        }
        if (!((character >= (unsigned char)'a' &&
                character <= (unsigned char)'z') ||
            (character >= (unsigned char)'A' &&
                character <= (unsigned char)'Z') ||
            (character >= (unsigned char)'0' &&
                character <= (unsigned char)'9') ||
            character == (unsigned char)'-' ||
            character == (unsigned char)'_' ||
            character == (unsigned char)'.')) {
            return false;
        }
    }
    return false;
}

static void copy_identifier(char *target, const char *source)
{
    memcpy(target, source, strlen(source) + 1U);
}

static bool parse_u64(const char *text, uint64_t *value)
{
    uint64_t parsed = UINT64_C(0);
    size_t index;

    if (text == NULL || text[0] == '\0' || value == NULL) {
        return false;
    }
    for (index = 0; text[index] != '\0'; ++index) {
        uint64_t digit;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (uint64_t)(text[index] - '0');
        if (parsed > (UINT64_MAX - digit) / UINT64_C(10)) {
            return false;
        }
        parsed = parsed * UINT64_C(10) + digit;
    }
    *value = parsed;
    return true;
}

static bool parse_common_option(
    int argc,
    char *const argv[],
    int *index,
    music_rig_cli_command *command,
    bool *seen_json,
    bool *seen_generation
)
{
    if (strcmp(argv[*index], "--json") == 0 && !*seen_json) {
        command->format = MUSIC_RIG_CLI_FORMAT_JSON;
        *seen_json = true;
        return true;
    }
    if (strcmp(argv[*index], "--expected-generation") == 0 &&
        !*seen_generation && *index + 1 < argc &&
        parse_u64(
            argv[*index + 1],
            &command->request.expected_generation
        )) {
        *seen_generation = true;
        *index += 1;
        return true;
    }
    return false;
}

static music_rig_result parse_read_command(
    int argc,
    char *const argv[],
    int first_option,
    bool allow_device,
    music_rig_cli_command *command
)
{
    bool seen_json = false;
    bool seen_generation = false;
    bool seen_device = false;
    int index;

    for (index = first_option; index < argc; ++index) {
        if (parse_common_option(
                argc,
                argv,
                &index,
                command,
                &seen_json,
                &seen_generation
            )) {
            continue;
        }
        if (allow_device && strcmp(argv[index], "--device") == 0 &&
            !seen_device && index + 1 < argc &&
            identifier_is_valid(argv[index + 1])) {
            copy_identifier(command->request.device_slot, argv[index + 1]);
            seen_device = true;
            index += 1;
            continue;
        }
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result parse_switch(
    int argc,
    char *const argv[],
    music_rig_cli_command *command
)
{
    bool seen_json = false;
    bool seen_generation = false;
    bool seen_dry_run = false;
    bool seen_global = false;
    bool seen_device = false;
    bool seen_profile = false;
    int index;

    for (index = 2; index < argc; ++index) {
        if (parse_common_option(
                argc,
                argv,
                &index,
                command,
                &seen_json,
                &seen_generation
            )) {
            continue;
        }
        if (strcmp(argv[index], "--dry-run") == 0 && !seen_dry_run) {
            seen_dry_run = true;
            command->request.flags = MUSIC_RIG_REQUEST_DRY_RUN;
        } else if (strcmp(argv[index], "--global") == 0 &&
            !seen_global && !seen_device && index + 1 < argc &&
            identifier_is_valid(argv[index + 1])) {
            copy_identifier(command->request.profile, argv[index + 1]);
            seen_global = true;
            index += 1;
        } else if (strcmp(argv[index], "--device") == 0 &&
            !seen_device && !seen_global && index + 1 < argc &&
            identifier_is_valid(argv[index + 1])) {
            copy_identifier(
                command->request.device_slot,
                argv[index + 1]
            );
            seen_device = true;
            index += 1;
        } else if (strcmp(argv[index], "--profile") == 0 &&
            !seen_profile && index + 1 < argc &&
            identifier_is_valid(argv[index + 1])) {
            copy_identifier(command->request.profile, argv[index + 1]);
            seen_profile = true;
            index += 1;
        } else {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
    }

    if (!seen_dry_run || seen_global == seen_device ||
        (seen_global && seen_profile) || (seen_device && !seen_profile)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    command->request.operation = seen_global
        ? (uint32_t)MUSIC_RIG_OPERATION_SWITCH_GLOBAL
        : (uint32_t)MUSIC_RIG_OPERATION_SWITCH_DEVICE;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result parse_prepare(
    int argc,
    char *const argv[],
    music_rig_cli_command *command
)
{
    music_rig_result result = parse_switch(argc, argv, command);

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    command->request.operation = command->request.device_slot[0] == '\0'
        ? (uint32_t)MUSIC_RIG_OPERATION_PREPARE_GLOBAL
        : (uint32_t)MUSIC_RIG_OPERATION_PREPARE_DEVICE;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result parse_reset(
    int argc,
    char *const argv[],
    music_rig_cli_command *command
)
{
    bool seen_device = false;
    bool seen_dry_run = false;
    bool seen_json = false;
    bool seen_generation = false;
    int index;

    for (index = 2; index < argc; ++index) {
        if (parse_common_option(
                argc, argv, &index, command, &seen_json, &seen_generation
            )) {
            continue;
        }
        if (strcmp(argv[index], "--dry-run") == 0 && !seen_dry_run) {
            seen_dry_run = true;
            command->request.flags = MUSIC_RIG_REQUEST_DRY_RUN;
        } else if (strcmp(argv[index], "--device") == 0 &&
            !seen_device && index + 1 < argc &&
            identifier_is_valid(argv[index + 1])) {
            copy_identifier(command->request.device_slot, argv[index + 1]);
            seen_device = true;
            index += 1;
        } else {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
    }
    if (!seen_device || !seen_dry_run) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    command->request.operation =
        (uint32_t)MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_cli_parse(
    int argc,
    char *const argv[],
    uint64_t request_id,
    music_rig_cli_command *command
)
{
    music_rig_result result;

    if (argc < 2 || argv == NULL || command == NULL ||
        request_id == UINT64_C(0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(command, 0, sizeof(*command));
    command->request.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
    command->request.request_id = request_id;
    command->format = MUSIC_RIG_CLI_FORMAT_HUMAN;

    if (strcmp(argv[1], "status") == 0) {
        command->request.operation = (uint32_t)MUSIC_RIG_OPERATION_STATUS;
        result = parse_read_command(argc, argv, 2, false, command);
    } else if (strcmp(argv[1], "profiles") == 0 && argc >= 3 &&
        strcmp(argv[2], "list") == 0) {
        command->request.operation =
            (uint32_t)MUSIC_RIG_OPERATION_LIST_PROFILES;
        result = parse_read_command(argc, argv, 3, true, command);
    } else if (strcmp(argv[1], "validate") == 0) {
        command->request.operation =
            (uint32_t)MUSIC_RIG_OPERATION_VALIDATE_ACTIVE;
        result = parse_read_command(argc, argv, 2, false, command);
    } else if (strcmp(argv[1], "switch") == 0) {
        result = parse_switch(argc, argv, command);
    } else if (strcmp(argv[1], "prepare") == 0) {
        result = parse_prepare(argc, argv, command);
    } else if (strcmp(argv[1], "reset") == 0) {
        result = parse_reset(argc, argv, command);
    } else {
        result = MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

static const char *operation_name(uint32_t operation)
{
    switch ((music_rig_operation)operation) {
    case MUSIC_RIG_OPERATION_STATUS:
        return "status";
    case MUSIC_RIG_OPERATION_LIST_PROFILES:
        return "list-profiles";
    case MUSIC_RIG_OPERATION_PREPARE_GLOBAL:
        return "prepare-global";
    case MUSIC_RIG_OPERATION_PREPARE_DEVICE:
        return "prepare-device";
    case MUSIC_RIG_OPERATION_SWITCH_GLOBAL:
        return "switch-global";
    case MUSIC_RIG_OPERATION_SWITCH_DEVICE:
        return "switch-device";
    case MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE:
        return "reset-device-override";
    case MUSIC_RIG_OPERATION_RELOAD_COMPILED_DEFINITION:
        return "reload-compiled-definition";
    case MUSIC_RIG_OPERATION_VALIDATE_ACTIVE:
        return "validate-active";
    default:
        return "invalid";
    }
}

static const char *result_name(uint32_t result)
{
    static const char *const names[] = {
        "ok", "unsupported", "invalid-argument", "invalid-state",
        "adapter-failure", "generation-conflict", "not-found",
        "invalid-data", "buffer-too-small"
    };

    return result <= (uint32_t)MUSIC_RIG_RESULT_BUFFER_TOO_SMALL
        ? names[result]
        : "invalid-result";
}

static const char *readiness_name(uint32_t readiness)
{
    static const char *const names[] = {
        "not-evaluated", "control-only", "prepared", "cold"
    };

    return readiness <= UINT32_C(3) ? names[readiness] : "invalid";
}

static void append_format(output_builder *builder, const char *format, ...)
{
    va_list arguments;
    int written;

    if (builder->failed || builder->size >= builder->capacity) {
        builder->failed = true;
        return;
    }
    va_start(arguments, format);
    written = vsnprintf(
        builder->output + builder->size,
        builder->capacity - builder->size,
        format,
        arguments
    );
    va_end(arguments);
    if (written < 0 || (size_t)written >= builder->capacity - builder->size) {
        builder->failed = true;
        return;
    }
    builder->size += (size_t)written;
}

static void render_human(
    const music_rig_protocol_response *response,
    output_builder *builder
)
{
    size_t index;

    append_format(builder, "operation %s\n", operation_name(response->operation));
    append_format(builder, "result %s\n", result_name(response->result_code));
    append_format(builder, "generation %llu\n",
        (unsigned long long)response->resulting_generation);
    append_format(builder, "rig-profile %s\n", response->active_rig_profile);
    append_format(builder, "readiness %s\n",
        readiness_name(response->readiness));
    append_format(builder, "output-mode suppressed\n");
    append_format(builder, "dry-run %s\n",
        (response->flags & MUSIC_RIG_RESPONSE_DRY_RUN) != UINT32_C(0)
            ? "yes" : "no");
    append_format(builder, "graph-delta %s\n",
        (response->flags & MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY) != UINT32_C(0)
            ? "empty" : "not-evaluated");
    append_format(builder, "profiles %u\n", response->profile_count);
    for (index = 0; index < response->profile_count; ++index) {
        const music_rig_protocol_profile *profile = &response->profiles[index];

        append_format(
            builder,
            "profile %s %s %s active\n",
            profile->device_slot,
            profile->profile,
            readiness_name(profile->readiness)
        );
    }
}

static void render_json(
    const music_rig_protocol_response *response,
    output_builder *builder
)
{
    size_t index;

    append_format(
        builder,
        "{\"schema\":\"music-studies/music-rig-cli-response/v1\"," 
        "\"protocol_version\":%u,\"request_id\":%llu,"
        "\"operation\":\"%s\",\"result\":\"%s\","
        "\"previous_generation\":%llu,\"resulting_generation\":%llu,"
        "\"active_rig_profile\":\"%s\",\"readiness\":\"%s\","
        "\"output_mode\":\"suppressed\",\"dry_run\":%s,"
        "\"valid\":%s,\"graph_delta\":\"%s\","
        "\"control_duration_ns\":%llu,\"adopted_at_ns\":%llu,"
        "\"rollback\":\"not-required\",\"warning_flags\":%u,"
        "\"profiles\":[",
        response->protocol_version,
        (unsigned long long)response->request_id,
        operation_name(response->operation),
        result_name(response->result_code),
        (unsigned long long)response->previous_generation,
        (unsigned long long)response->resulting_generation,
        response->active_rig_profile,
        readiness_name(response->readiness),
        (response->flags & MUSIC_RIG_RESPONSE_DRY_RUN) != UINT32_C(0)
            ? "true" : "false",
        (response->flags & MUSIC_RIG_RESPONSE_VALID) != UINT32_C(0)
            ? "true" : "false",
        (response->flags & MUSIC_RIG_RESPONSE_GRAPH_DELTA_EMPTY) != UINT32_C(0)
            ? "empty" : "not-evaluated",
        (unsigned long long)response->control_duration_ns,
        (unsigned long long)response->adopted_at_ns,
        response->warning_flags
    );
    for (index = 0; index < response->profile_count; ++index) {
        const music_rig_protocol_profile *profile = &response->profiles[index];

        append_format(
            builder,
            "%s{\"device\":\"%s\",\"profile\":\"%s\","
            "\"readiness\":\"%s\",\"active\":true,"
            "\"override\":%s}",
            index == 0 ? "" : ",",
            profile->device_slot,
            profile->profile,
            readiness_name(profile->readiness),
            (profile->flags & MUSIC_RIG_PROFILE_OVERRIDE) != UINT32_C(0)
                ? "true" : "false"
        );
    }
    append_format(builder, "]}\n");
}

music_rig_result music_rig_cli_render(
    const music_rig_protocol_response *response,
    music_rig_cli_format format,
    char *output,
    size_t output_capacity,
    size_t *output_size
)
{
    uint8_t validated[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
    output_builder builder;

    if (response == NULL || output == NULL || output_capacity == 0U ||
        output_size == NULL ||
        (format != MUSIC_RIG_CLI_FORMAT_HUMAN &&
            format != MUSIC_RIG_CLI_FORMAT_JSON) ||
        music_rig_protocol_encode_response(
            response,
            validated,
            sizeof(validated)
        ) != MUSIC_RIG_RESULT_OK) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    builder.output = output;
    builder.capacity = output_capacity;
    builder.size = 0U;
    builder.failed = false;
    output[0] = '\0';
    if (format == MUSIC_RIG_CLI_FORMAT_JSON) {
        render_json(response, &builder);
    } else {
        render_human(response, &builder);
    }
    if (builder.failed) {
        output[0] = '\0';
        *output_size = 0U;
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    *output_size = builder.size;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_cli_execute(
    const music_rig_cli_command *command,
    const music_rig_client_transport *transport,
    char *output,
    size_t output_capacity,
    size_t *output_size
)
{
    music_rig_protocol_response response;
    music_rig_result result;

    if (command == NULL || transport == NULL || transport->exchange == NULL ||
        output == NULL || output_size == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *output_size = 0U;
    result = transport->exchange(
        transport->context,
        &command->request,
        &response
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    if (response.protocol_version != command->request.protocol_version ||
        response.operation != command->request.operation ||
        response.request_id != command->request.request_id) {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    result = music_rig_cli_render(
        &response,
        command->format,
        output,
        output_capacity,
        output_size
    );
    return result == MUSIC_RIG_RESULT_OK
        ? (music_rig_result)response.result_code
        : result;
}
