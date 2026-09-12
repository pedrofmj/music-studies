#define _POSIX_C_SOURCE 200809L

#include "music_rig/synthv1_process.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void wait_milliseconds(long milliseconds)
{
    struct timespec delay = {
        milliseconds / 1000L,
        (milliseconds % 1000L) * 1000000L
    };
    (void)nanosleep(&delay, NULL);
}

music_rig_result music_rig_synthv1_process_init(
    music_rig_synthv1_process *process,
    const char *binary_path,
    const char *client_name,
    const char *jack_server
)
{
    if (process == NULL || binary_path == NULL || client_name == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(process, 0, sizeof(*process));
    process->binary_path = binary_path;
    process->client_name = client_name;
    process->jack_server = jack_server;
    process->pid = (pid_t)-1;
    return MUSIC_RIG_RESULT_OK;
}

bool music_rig_synthv1_process_is_healthy(
    music_rig_synthv1_process *process
)
{
    int status;

    if (process == NULL || process->pid <= (pid_t)0) {
        return false;
    }
    if (waitpid(process->pid, &status, WNOHANG) == process->pid) {
        process->pid = (pid_t)-1;
        return false;
    }
    return kill(process->pid, 0) == 0;
}

music_rig_result music_rig_synthv1_process_start(
    void *context,
    const char *preset_path
)
{
    music_rig_synthv1_process *process = context;
    pid_t child;

    if (process == NULL || preset_path == NULL ||
        process->pid > (pid_t)0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    child = fork();
    if (child < (pid_t)0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (child == (pid_t)0) {
        (void)setenv("QT_QPA_PLATFORM", "offscreen", 1);
        if (process->jack_server != NULL) {
            (void)setenv("JACK_DEFAULT_SERVER", process->jack_server, 1);
        }
        execl(
            process->binary_path,
            process->binary_path,
            "--no-gui",
            "--client-name",
            process->client_name,
            preset_path,
            (char *)NULL
        );
        _exit(127);
    }
    process->pid = child;
    wait_milliseconds(100L);
    if (!music_rig_synthv1_process_is_healthy(process)) {
        (void)music_rig_synthv1_process_stop(process);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_synthv1_process_stop(void *context)
{
    music_rig_synthv1_process *process = context;
    int status;
    unsigned int attempt;

    if (process == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (process->pid <= (pid_t)0) {
        return MUSIC_RIG_RESULT_OK;
    }
    (void)kill(process->pid, SIGTERM);
    for (attempt = 0U; attempt < 20U; ++attempt) {
        if (waitpid(process->pid, &status, WNOHANG) == process->pid) {
            process->pid = (pid_t)-1;
            return MUSIC_RIG_RESULT_OK;
        }
        wait_milliseconds(50L);
    }
    (void)kill(process->pid, SIGKILL);
    if (waitpid(process->pid, &status, 0) != process->pid) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    process->pid = (pid_t)-1;
    return MUSIC_RIG_RESULT_OK;
}
