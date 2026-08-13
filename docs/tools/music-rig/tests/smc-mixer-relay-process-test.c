#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define OUTPUT_CAPACITY ((size_t)4096)

static int read_ready(
    int descriptor,
    char *output,
    size_t *used,
    const char *needle
)
{
    struct pollfd ready;
    int attempts;

    ready.fd = descriptor;
    ready.events = POLLIN;
    for (attempts = 0; attempts < 20; ++attempts) {
        ssize_t count;

        ready.revents = 0;
        if (poll(&ready, 1, 100) <= 0 || (ready.revents & POLLIN) == 0) {
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
    char root[256];
    char output[OUTPUT_CAPACITY] = {0};
    int descriptors[2];
    pid_t child;
    size_t used = 0U;
    int status;

    if (argc != 5 || snprintf(root, sizeof(root),
            "/tmp/music-rig-smc-mixer-relay-%ld", (long)getpid()) < 0 ||
        pipe(descriptors) != 0) {
        fputs("SMC-Mixer relay process setup failed\n", stderr);
        return 1;
    }
    child = fork();
    if (child == 0) {
        (void)close(descriptors[0]);
        if (dup2(descriptors[1], STDERR_FILENO) < 0 ||
            dup2(descriptors[1], STDOUT_FILENO) < 0 ||
            close(descriptors[1]) != 0 ||
            setenv("ASAN_OPTIONS", "verify_asan_link_order=0", 0) != 0 ||
            setenv("LD_PRELOAD", argv[4], 1) != 0 ||
            setenv("HOME", root, 1) != 0 ||
            setenv("XDG_CONFIG_HOME", root, 1) != 0 ||
            setenv("XDG_CACHE_HOME", root, 1) != 0 ||
            setenv("XDG_STATE_HOME", root, 1) != 0 ||
            setenv("XDG_RUNTIME_DIR", root, 1) != 0) {
            _exit(120);
        }
        execl(
            argv[1], argv[1],
            "run-smc-mixer-relay",
            "--definition", argv[2],
            "--expected-fingerprint", argv[3],
            "--output-enabled",
            "--acknowledge-smc-mixer-cutover",
            (char *)NULL
        );
        _exit(121);
    }
    (void)close(descriptors[1]);
    if (child < 0) {
        (void)close(descriptors[0]);
        fputs("SMC-Mixer relay process fork failed\n", stderr);
        return 1;
    }
    if (!read_ready(descriptors[0], output, &used,
            "code=lifecycle.started")) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        (void)close(descriptors[0]);
        if (used > 0U) {
            (void)fwrite(output, 1U, used, stderr);
        }
        fputs("SMC-Mixer relay did not report readiness\n", stderr);
        return 1;
    }
    if (kill(child, SIGTERM) != 0 || waitpid(child, &status, 0) != child) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        (void)close(descriptors[0]);
        fputs("SMC-Mixer relay termination failed\n", stderr);
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
            fputs("SMC-Mixer relay diagnostic read failed\n", stderr);
            return 1;
        }
    }
    (void)close(descriptors[0]);
    output[used] = '\0';
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        strstr(output, "code=lifecycle.started") == NULL ||
        strstr(output, "code=lifecycle.stopped") == NULL ||
        strstr(output, "definition-generation 1") == NULL ||
        strstr(output, "slot smc-mixer-main") == NULL ||
        strstr(output, "input-ports 1") == NULL ||
        strstr(output, "output-ports 1") == NULL ||
        strstr(output, "input-events 0") == NULL ||
        strstr(output, "control-cc-40-mapped-events 0") == NULL ||
        strstr(output, "control-cc-47-mapped-events 0") == NULL ||
        strstr(output, "emitted-events 0") == NULL ||
        strstr(output, "output-mode enabled-smc-mixer-only") == NULL ||
        access(root, F_OK) == 0 || errno != ENOENT) {
        fputs("SMC-Mixer relay process contract failed\n", stderr);
        return 1;
    }
    return 0;
}
