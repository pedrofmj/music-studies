#include "music_rig/core.h"
#include "music_rig/cli.h"

#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s --version\n", program);
    fprintf(stream, "       %s status [--json] [--expected-generation ID]\n",
        program);
    fprintf(stream, "       %s profiles list [--device SLOT] [--json]\n",
        program);
    fprintf(stream, "       %s validate [--json]\n", program);
    fprintf(stream, "       %s switch --global PROFILE --dry-run [--json]\n",
        program);
    fprintf(stream,
        "       %s switch --device SLOT --profile PROFILE --dry-run "
        "[--json]\n",
        program
    );
    fprintf(stream, "       %s reset --device SLOT --dry-run [--json]\n",
        program);
}

static music_rig_result unavailable_exchange(
    void *context,
    const music_rig_protocol_request *request,
    music_rig_protocol_response *response
)
{
    (void)context;
    (void)request;
    (void)response;
    return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

int main(int argc, char **argv)
{
    const music_rig_build_info *build_info;
    music_rig_cli_command command;
    music_rig_client_transport transport;
    char output[8192];
    size_t output_size;
    music_rig_result result;

    if (argc == 2 &&
        (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0)) {
        build_info = music_rig_get_build_info();
        printf("music-rig %s\n", build_info->core_version);
        printf("protocol %u\n", build_info->protocol_version);
        printf("profile-schema %u\n", build_info->profile_schema_version);
        return MUSIC_RIG_RESULT_OK;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout, argv[0]);
        return MUSIC_RIG_RESULT_OK;
    }

    result = music_rig_cli_parse(
        argc,
        argv,
        UINT64_C(1),
        &command
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        transport.context = NULL;
        transport.exchange = unavailable_exchange;
        result = music_rig_cli_execute(
            &command,
            &transport,
            output,
            sizeof(output),
            &output_size
        );
        if (result == MUSIC_RIG_RESULT_OK) {
            fwrite(output, 1U, output_size, stdout);
        } else if (result == MUSIC_RIG_RESULT_ADAPTER_FAILURE) {
            fputs(
                "music-rig: control transport is not configured; "
                "no request was sent\n",
                stderr
            );
        }
        return (int)result;
    }

    print_usage(stderr, argv[0]);
    return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}
