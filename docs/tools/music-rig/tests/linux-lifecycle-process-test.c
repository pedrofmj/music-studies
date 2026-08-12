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

#define OUTPUT_CAPACITY ((size_t)2048)

int main(int argc, char **argv)
{
    char root[256];
    char output[OUTPUT_CAPACITY];
    int descriptors[2];
    struct pollfd ready;
    pid_t child;
    size_t used = 0U;
    int status;

    if (argc != 2 || snprintf(root, sizeof(root),
            "/tmp/music-rig-lifecycle-%ld", (long)getpid()) < 0 ||
        pipe(descriptors) != 0) {
        fputs("lifecycle process test setup failed\n", stderr);
        return 1;
    }
    child = fork();
    if (child == 0) {
        (void)close(descriptors[0]);
        if (dup2(descriptors[1], STDERR_FILENO) < 0 ||
            close(descriptors[1]) != 0 ||
            setenv("HOME", root, 1) != 0 ||
            setenv("XDG_CONFIG_HOME", root, 1) != 0 ||
            setenv("XDG_CACHE_HOME", root, 1) != 0 ||
            setenv("XDG_STATE_HOME", root, 1) != 0 ||
            setenv("XDG_RUNTIME_DIR", root, 1) != 0) {
            _exit(120);
        }
        execl(
            argv[1],
            argv[1],
            "run-shadow",
            "--output-suppressed",
            (char *)NULL
        );
        _exit(121);
    }
    (void)close(descriptors[1]);
    if (child < 0) {
        (void)close(descriptors[0]);
        fputs("lifecycle process fork failed\n", stderr);
        return 1;
    }

    ready.fd = descriptors[0];
    ready.events = POLLIN;
    ready.revents = 0;
    if (poll(&ready, 1, 2000) <= 0 ||
        (ready.revents & POLLIN) == 0) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        (void)close(descriptors[0]);
        fputs("shadow host did not report readiness\n", stderr);
        return 1;
    }
    {
        ssize_t count = read(
            descriptors[0], output, sizeof(output) - used - 1U
        );
        if (count <= 0) {
            (void)kill(child, SIGKILL);
            (void)waitpid(child, &status, 0);
            (void)close(descriptors[0]);
            fputs("shadow host readiness was empty\n", stderr);
            return 1;
        }
        used += (size_t)count;
    }
    if (kill(child, SIGTERM) != 0 || waitpid(child, &status, 0) != child) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        (void)close(descriptors[0]);
        fputs("shadow host termination failed\n", stderr);
        return 1;
    }
    while (used + 1U < sizeof(output)) {
        ssize_t count = read(
            descriptors[0], output + used, sizeof(output) - used - 1U
        );
        if (count > 0) {
            used += (size_t)count;
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            (void)close(descriptors[0]);
            fputs("shadow host diagnostic read failed\n", stderr);
            return 1;
        }
    }
    (void)close(descriptors[0]);
    output[used] = '\0';
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        strstr(output, "code=lifecycle.started") == NULL ||
        strstr(output, "code=lifecycle.stopped") == NULL ||
        strstr(output, "output-suppressed shadow host") == NULL ||
        access(root, F_OK) == 0 || errno != ENOENT) {
        fputs("shadow host lifecycle contract failed\n", stderr);
        return 1;
    }
    return 0;
}
