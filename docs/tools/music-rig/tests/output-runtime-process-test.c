#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define OUTPUT_CAPACITY ((size_t)8192)

static int wait_for_socket(const char *path)
{
    struct timespec delay = {0, 10000000L};
    size_t attempt;

    for (attempt = 0U; attempt < 200U; ++attempt) {
        if (access(path, F_OK) == 0) {
            return 1;
        }
        (void)nanosleep(&delay, NULL);
    }
    return 0;
}

static int run_cli(
    const char *program,
    char *const arguments[],
    const char *home,
    const char *runtime_home,
    const char *state_home
)
{
    pid_t child = fork();
    int status;

    if (child == 0) {
        (void)setenv("HOME", home, 1);
        (void)setenv("XDG_RUNTIME_DIR", runtime_home, 1);
        (void)setenv("XDG_STATE_HOME", state_home, 1);
        (void)setenv("XDG_CONFIG_HOME", home, 1);
        execv(program, arguments);
        _exit(127);
    }
    return child > 0 && waitpid(child, &status, 0) == child &&
        WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int read_ready(
    int descriptor,
    char *output,
    size_t *used,
    const char *needle
)
{
    struct pollfd ready = {descriptor, POLLIN, 0};
    int attempts;

    for (attempts = 0; attempts < 200; ++attempts) {
        ssize_t count;

        ready.revents = 0;
        if (poll(&ready, 1, 10) <= 0 || (ready.revents & POLLIN) == 0) {
            continue;
        }
        count = read(descriptor, output + *used,
            OUTPUT_CAPACITY - *used - 1U);
        if (count <= 0) {
            return 0;
        }
        *used += (size_t)count;
        output[*used] = '\0';
        if (strstr(output, needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    char root[] = "/tmp/music-rig-output-runtime-XXXXXX";
    char runtime_home[512];
    char state_home[512];
    char socket_path[1024];
    char output[OUTPUT_CAPACITY] = {0};
    char *status_arguments[] = {(char *)"music-rig", (char *)"status",
        (char *)"--expected-generation", (char *)"1", (char *)"--json", NULL};
    char *switch_arguments[] = {(char *)"music-rig", (char *)"switch",
        (char *)"--global", (char *)"multilevel-volume-mixed-pads",
        (char *)"--json", NULL};
    char *switch_back_arguments[] = {(char *)"music-rig", (char *)"switch",
        (char *)"--global", (char *)"full-live-rack", (char *)"--json", NULL};
    char *status_after_arguments[] = {(char *)"music-rig", (char *)"status",
        (char *)"--expected-generation", (char *)"3", (char *)"--json", NULL};
    int descriptors[2];
    pid_t daemon;
    size_t used = 0U;
    int status;
    const char *failure = NULL;

    if (argc != 8 || mkdtemp(root) == NULL || pipe(descriptors) != 0) {
        return 1;
    }
    (void)snprintf(runtime_home, sizeof(runtime_home), "%s/runtime", root);
    (void)snprintf(state_home, sizeof(state_home), "%s/state", root);
    (void)snprintf(socket_path, sizeof(socket_path),
        "%s/music-rig/control.sock", runtime_home);
    if (mkdir(runtime_home, 0700) != 0 || mkdir(state_home, 0700) != 0) {
        return 1;
    }
    daemon = fork();
    if (daemon == 0) {
        (void)close(descriptors[0]);
        if (dup2(descriptors[1], STDERR_FILENO) < 0 ||
            dup2(descriptors[1], STDOUT_FILENO) < 0 ||
            close(descriptors[1]) != 0 ||
            setenv("HOME", root, 1) != 0 ||
            setenv("XDG_RUNTIME_DIR", runtime_home, 1) != 0 ||
            setenv("XDG_STATE_HOME", state_home, 1) != 0 ||
            setenv("XDG_CONFIG_HOME", root, 1) != 0 ||
            setenv("LD_PRELOAD", argv[7], 1) != 0) {
            _exit(120);
        }
        execl(argv[1], argv[1],
            "run-output", "--definition", argv[3],
            "--expected-fingerprint", argv[4],
            "--prepared-definition", argv[5],
            "--prepared-fingerprint", argv[6],
            "--acknowledge-output", (char *)NULL);
        _exit(121);
    }
    (void)close(descriptors[1]);
    if (daemon < 0) {
        failure = "fork";
    } else if (!wait_for_socket(socket_path)) {
        failure = "socket startup";
    } else if (!read_ready(descriptors[0], output, &used,
            "output-runtime ready")) {
        failure = "lifecycle startup";
    } else if (!run_cli(argv[2], status_arguments, root, runtime_home,
            state_home)) {
        failure = "initial status";
    } else if (!run_cli(argv[2], switch_arguments, root, runtime_home,
            state_home)) {
        failure = "global switch";
    } else if (!run_cli(argv[2], switch_back_arguments, root, runtime_home,
            state_home)) {
        failure = "global switch-back";
    } else if (!run_cli(argv[2], status_after_arguments, root, runtime_home,
            state_home)) {
        failure = "final status";
    }
    if (failure != NULL) {
        fputs(output, stderr);
        fprintf(stderr, "MIDI output runtime process stage failed: %s\n", failure);
        if (daemon > 0) {
            (void)kill(daemon, SIGKILL);
            (void)waitpid(daemon, &status, 0);
        }
        (void)close(descriptors[0]);
        return 1;
    }
    if (kill(daemon, SIGTERM) != 0 || waitpid(daemon, &status, 0) != daemon) {
        (void)close(descriptors[0]);
        return 1;
    }
    while (used + 1U < sizeof(output)) {
        ssize_t count = read(descriptors[0], output + used,
            sizeof(output) - used - 1U);

        if (count > 0) {
            used += (size_t)count;
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            (void)close(descriptors[0]);
            return 1;
        }
    }
    (void)close(descriptors[0]);
    output[used] = '\0';
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        strstr(output, "output-runtime ready") == NULL ||
        strstr(output, "output-mode enabled") == NULL) {
        fputs(output, stderr);
        return 1;
    }
    return 0;
}
