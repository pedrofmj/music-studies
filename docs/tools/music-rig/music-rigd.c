#include "music_rig/core.h"
#include "music_rig/runtime.h"

#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s --version\n", program);
}

int main(int argc, char **argv)
{
    const music_rig_build_info *build_info;

    if (argc == 2 &&
        (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0)) {
        build_info = music_rig_get_build_info();
        printf("music-rigd %s\n", build_info->core_version);
        printf("protocol %u\n", build_info->protocol_version);
        printf("runtime-state %u\n", MUSIC_RIG_RUNTIME_STATE_VERSION);
        printf("output-mode suppressed-only\n");
        return MUSIC_RIG_RESULT_OK;
    }

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(stdout, argv[0]);
        return MUSIC_RIG_RESULT_OK;
    }

    print_usage(stderr, argv[0]);
    return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
}
