#include "music_rig/synthv1_process.h"

#include <stdio.h>

int main(void)
{
    music_rig_synthv1_process process;

    if (music_rig_synthv1_process_init(
            &process,
            "/definitely/missing/synthv1_jack",
            "s2-test",
            "music-rig-s2"
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_synthv1_process_start(
            &process,
            "/tmp/missing-synthv1-preset.synthv1"
        ) != MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        process.pid > (pid_t)0 ||
        music_rig_synthv1_process_stop(&process) != MUSIC_RIG_RESULT_OK) {
        fputs("synthv1 process failure was not fail-closed\n", stderr);
        return 1;
    }
    puts("Synthv1 process adapter tests: PASS");
    return 0;
}
