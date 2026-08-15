#include "music_rig/cli.h"
#include "music_rig/control.h"
#include "compiled-tables-fixture.h"

#include <stdio.h>
#include <string.h>

typedef struct mock_transport {
    music_rig_control_snapshot snapshot;
    bool corrupt_response;
} mock_transport;

static music_rig_result exchange(
    void *opaque,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    mock_transport *transport = opaque;
    music_rig_protocol_request decoded_request;
    music_rig_protocol_response dispatched_response;
    uint8_t request_frame[MUSIC_RIG_PROTOCOL_REQUEST_SIZE];
    uint8_t response_frame[MUSIC_RIG_PROTOCOL_RESPONSE_SIZE];
    music_rig_result result;

    result = music_rig_protocol_encode_request(
        request,
        request_frame,
        sizeof(request_frame)
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_protocol_decode_request(
            request_frame,
            sizeof(request_frame),
            &decoded_request
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_control_dispatch(
            &transport->snapshot,
            &decoded_request,
            &dispatched_response
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_protocol_encode_response(
            &dispatched_response,
            response_frame,
            sizeof(response_frame)
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = music_rig_protocol_decode_response(
            response_frame,
            sizeof(response_frame),
            response
        );
    }
    if (result == MUSIC_RIG_RESULT_OK && transport->corrupt_response) {
        response->request_id += UINT64_C(1);
    }
    return result;
}

static int run_command(
    int argc,
    char **argv,
    mock_transport *mock,
    music_rig_result expected,
    const char *expected_text
)
{
    music_rig_cli_command command;
    music_rig_client_transport transport;
    char output[8192];
    size_t output_size = 0U;
    music_rig_result result;

    result = music_rig_cli_parse(argc, argv, UINT64_C(73), &command);
    if (result != MUSIC_RIG_RESULT_OK) {
        fputs("valid CLI command did not parse\n", stderr);
        return 1;
    }
    transport.context = mock;
    transport.exchange = exchange;
    result = music_rig_cli_execute(
        &command,
        &transport,
        output,
        sizeof(output),
        &output_size
    );
    if (result != expected || output_size == 0U ||
        strstr(output, expected_text) == NULL) {
        fputs("CLI command response failed\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    static music_rig_compiled_tables tables;
    mock_transport mock;
    music_rig_cli_command command;
    music_rig_client_transport transport;
    char output[8192];
    size_t output_size;
    char *status[] = {
        "music-rig", "status", "--json", "--expected-generation", "41"
    };
    char *profiles[] = {
        "music-rig", "profiles", "list", "--device", "smc-mixer-main"
    };
    char *validate[] = {"music-rig", "validate", "--json"};
    char *global[] = {
        "music-rig", "switch", "--global", "full-live-rack",
        "--dry-run", "--json"
    };
    char *mixed_global[] = {
        "music-rig", "switch", "--global",
        "multilevel-volume-mixed-pads", "--dry-run", "--json"
    };
    char *device[] = {
        "music-rig", "switch", "--device", "smc-mixer-main",
        "--profile", "eight-band-eq", "--dry-run"
    };
    char *reset[] = {
        "music-rig", "reset", "--device", "smc-mixer-main", "--dry-run",
        "--json"
    };
    char *missing[] = {
        "music-rig", "switch", "--global", "modeled-piano", "--dry-run"
    };
    char *unsafe[] = {
        "music-rig", "switch", "--global", "full-live-rack"
    };
    char *ambiguous[] = {
        "music-rig", "switch", "--global", "full-live-rack",
        "--device", "smc-mixer-main", "--dry-run"
    };

    if (init_compiled_tables_fixture(&tables) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    memset(&mock, 0, sizeof(mock));
    mock.snapshot.generation_id = UINT64_C(41);
    mock.snapshot.active_rig_profile = "full-live-rack";
    mock.snapshot.tables = &tables;
    mock.snapshot.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;

    if (run_command(5, status, &mock, MUSIC_RIG_RESULT_OK,
            "\"active_rig_profile\":\"full-live-rack\"") ||
        music_rig_cli_parse(6, mixed_global, UINT64_C(73), &command) !=
            MUSIC_RIG_RESULT_OK ||
        command.request.operation != MUSIC_RIG_OPERATION_SWITCH_GLOBAL ||
        strcmp(command.request.profile, "multilevel-volume-mixed-pads") != 0 ||
        command.request.flags != MUSIC_RIG_REQUEST_DRY_RUN ||
        run_command(5, profiles, &mock, MUSIC_RIG_RESULT_OK,
            "profile smc-mixer-main eight-band-eq control-only active") ||
        run_command(3, validate, &mock, MUSIC_RIG_RESULT_OK,
            "\"valid\":true") ||
        run_command(6, global, &mock, MUSIC_RIG_RESULT_OK,
            "\"graph_delta\":\"empty\"") ||
        run_command(7, device, &mock, MUSIC_RIG_RESULT_OK,
            "dry-run yes") ||
        run_command(6, reset, &mock, MUSIC_RIG_RESULT_OK,
            "reset-device-override") ||
        run_command(5, missing, &mock, MUSIC_RIG_RESULT_NOT_FOUND,
            "result not-found")) {
        return 1;
    }

    if (music_rig_cli_parse(4, unsafe, UINT64_C(1), &command) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_cli_parse(7, ambiguous, UINT64_C(1), &command) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("unsafe CLI switch was accepted\n", stderr);
        return 1;
    }

    mock.corrupt_response = true;
    transport.context = &mock;
    transport.exchange = exchange;
    if (music_rig_cli_parse(3, validate, UINT64_C(73), &command) !=
            MUSIC_RIG_RESULT_OK ||
        music_rig_cli_execute(
            &command,
            &transport,
            output,
            sizeof(output),
            &output_size
        ) != MUSIC_RIG_RESULT_INVALID_DATA ||
        output_size != 0U) {
        fputs("mismatched CLI response was accepted\n", stderr);
        return 1;
    }
    return 0;
}
