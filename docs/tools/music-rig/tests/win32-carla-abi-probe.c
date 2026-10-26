#include "music_rig/win32_carla_host.h"

#include <stdio.h>

int wmain(int argc, wchar_t **argv)
{
    char error_message[512];
    music_rig_result result;

    if (argc != 2) {
        (void)fprintf(stderr, "Usage: carla-abi-probe.exe <Carla DLL path>\n");
        return 2;
    }
    result = music_rig_win32_carla_host_probe_library(
        argv[1], error_message, sizeof(error_message)
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)fprintf(stderr, "Carla API probe failed (%d): %s\n",
            (int)result, error_message);
        return 1;
    }
    (void)puts("Carla DLL loaded and required Host API exports resolved.");
    return 0;
}
