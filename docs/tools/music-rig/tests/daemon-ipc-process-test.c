#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>

static int run_command(
    const char *program,
    char *const arguments[],
    const char *name,
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
    if (child < 0 || waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
        return 0;
    }
    if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "%s returned %d\n", name, WEXITSTATUS(status));
        return 0;
    }
    return 1;
}

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

static int connect_socket(const char *path)
{
    struct sockaddr_un address;
    size_t path_size = strlen(path);
    int descriptor;

    if (path_size >= sizeof(address.sun_path)) {
        return -1;
    }
    descriptor = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (descriptor < 0) {
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, path_size + 1U);
    if (connect(descriptor, (const struct sockaddr *)&address,
            sizeof(address.sun_family) + path_size + 1U) != 0) {
        (void)close(descriptor);
        return -1;
    }
    return descriptor;
}

static int send_malformed_request(const char *path)
{
    uint8_t frame[8] = {0};
    int descriptor = connect_socket(path);
    ssize_t sent;

    if (descriptor < 0) {
        return 0;
    }
    sent = send(descriptor, frame, sizeof(frame), MSG_NOSIGNAL);
    (void)close(descriptor);
    return sent == (ssize_t)sizeof(frame);
}

int main(int argc, char **argv)
{
    char root[] = "/tmp/music-rig-daemon-ipc-XXXXXX";
    char runtime_home[512];
    char state_home[512];
    char socket_path[1024];
    char error_path[512];
    char *daemon_arguments[11];
    char *status_arguments[] = {(char *)"music-rig", (char *)"status",
        (char *)"--json", NULL};
    char *status_generation_arguments[] = {(char *)"music-rig",
        (char *)"status", (char *)"--expected-generation", (char *)"2",
        (char *)"--json", NULL};
    char *switch_arguments[] = {(char *)"music-rig", (char *)"switch",
        (char *)"--device", (char *)"smc-pad-main", (char *)"--profile",
        (char *)"pad-layer-controller", (char *)"--json", NULL};
    char *reset_arguments[] = {(char *)"music-rig", (char *)"reset",
        (char *)"--device", (char *)"smc-pad-main", (char *)"--json", NULL};
    pid_t daemon;
    int status;
    struct stat socket_status;

    if (argc != 7 || mkdtemp(root) == NULL) {
        return 1;
    }
    (void)snprintf(runtime_home, sizeof(runtime_home), "%s/runtime", root);
    (void)snprintf(state_home, sizeof(state_home), "%s/state", root);
    (void)snprintf(socket_path, sizeof(socket_path),
        "%s/music-rig/control.sock", runtime_home);
    (void)snprintf(error_path, sizeof(error_path), "%s/daemon.err", root);
    if (mkdir(runtime_home, 0700) != 0 || mkdir(state_home, 0700) != 0) {
        return 1;
    }

    daemon_arguments[0] = argv[1];
    daemon_arguments[1] = (char *)"run";
    daemon_arguments[2] = (char *)"--definition";
    daemon_arguments[3] = argv[3];
    daemon_arguments[4] = (char *)"--expected-fingerprint";
    daemon_arguments[5] = argv[4];
    daemon_arguments[6] = (char *)"--prepared-definition";
    daemon_arguments[7] = argv[5];
    daemon_arguments[8] = (char *)"--prepared-fingerprint";
    daemon_arguments[9] = argv[6];
    daemon_arguments[10] = NULL;
    daemon = fork();
    if (daemon == 0) {
        (void)setenv("HOME", root, 1);
        (void)setenv("XDG_RUNTIME_DIR", runtime_home, 1);
        (void)setenv("XDG_STATE_HOME", state_home, 1);
        (void)setenv("XDG_CONFIG_HOME", root, 1);
        if (freopen(error_path, "wb", stderr) == NULL) {
            _exit(126);
        }
        execv(argv[1], daemon_arguments);
        _exit(127);
    }
    if (daemon < 0 || !wait_for_socket(socket_path) ||
        stat(socket_path, &socket_status) != 0 ||
        (socket_status.st_mode & 0777) != 0600) {
        if (daemon > 0) {
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
        }
        return 1;
    }
    {
        pid_t duplicate = fork();
        int duplicate_status;
        struct timespec delay = {0, 100000000L};

        if (duplicate == 0) {
            (void)setenv("HOME", root, 1);
            (void)setenv("XDG_RUNTIME_DIR", runtime_home, 1);
            (void)setenv("XDG_STATE_HOME", state_home, 1);
            (void)setenv("XDG_CONFIG_HOME", root, 1);
            execv(argv[1], daemon_arguments);
            _exit(127);
        }
        (void)nanosleep(&delay, NULL);
        if (duplicate < 0 || waitpid(duplicate, &duplicate_status, WNOHANG) == 0) {
            if (duplicate > 0) {
                (void)kill(duplicate, SIGTERM);
                (void)waitpid(duplicate, NULL, 0);
            }
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
            return 1;
        }
        if (!WIFEXITED(duplicate_status) || WEXITSTATUS(duplicate_status) == 0) {
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
            return 1;
        }
    }
    status_arguments[0] = argv[2];
    if (!run_command(argv[2], status_arguments, "status", root, runtime_home, state_home) ||
        !run_command(argv[2], status_arguments, "status-reconnect", root,
            runtime_home, state_home)) {
        (void)kill(daemon, SIGTERM);
        (void)waitpid(daemon, NULL, 0);
        fprintf(stderr, "%s", "daemon diagnostic output:\n");
        {
            FILE *file = fopen(error_path, "rb");
            char line[256];
            if (file != NULL) {
                while (fgets(line, sizeof(line), file) != NULL) {
                    fputs(line, stderr);
                }
                fclose(file);
            }
        }
        return 1;
    }
    if (!run_command(argv[2], switch_arguments, "switch-pad-main", root,
            runtime_home, state_home) ||
        !run_command(argv[2], status_generation_arguments,
            "status-after-switch", root, runtime_home, state_home)) {
        (void)kill(daemon, SIGTERM);
        (void)waitpid(daemon, NULL, 0);
        return 1;
    }
    (void)kill(daemon, SIGTERM);
    if (waitpid(daemon, &status, 0) < 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0 || access(socket_path, F_OK) == 0) {
        return 1;
    }
    daemon = fork();
    if (daemon == 0) {
        (void)setenv("HOME", root, 1);
        (void)setenv("XDG_RUNTIME_DIR", runtime_home, 1);
        (void)setenv("XDG_STATE_HOME", state_home, 1);
        (void)setenv("XDG_CONFIG_HOME", root, 1);
        if (freopen(error_path, "ab", stderr) == NULL) {
            _exit(126);
        }
        execv(argv[1], daemon_arguments);
        _exit(127);
    }
    if (daemon < 0 || !wait_for_socket(socket_path) ||
        !run_command(argv[2], status_generation_arguments,
            "status-after-restart", root, runtime_home, state_home) ||
        !run_command(argv[2], reset_arguments, "reset-pad-main", root,
            runtime_home, state_home)) {
        if (daemon > 0) {
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
        }
        return 1;
    }
    if (!send_malformed_request(socket_path) ||
        !run_command(argv[2], status_arguments, "status-after-malformed", root,
            runtime_home, state_home)) {
        (void)kill(daemon, SIGTERM);
        (void)waitpid(daemon, NULL, 0);
        return 1;
    }
    {
        int disconnected = connect_socket(socket_path);
        if (disconnected < 0) {
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
            return 1;
        }
        (void)close(disconnected);
        if (!run_command(argv[2], status_arguments, "status-after-disconnect",
                root, runtime_home, state_home)) {
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
            return 1;
        }
    }
    {
        int held = connect_socket(socket_path);
        pid_t takeover = fork();

        if (held < 0 || takeover < 0) {
            if (held >= 0) (void)close(held);
            (void)kill(daemon, SIGTERM);
            (void)waitpid(daemon, NULL, 0);
            return 1;
        }
        if (takeover == 0) {
            (void)close(held);
            (void)setenv("HOME", root, 1);
            (void)setenv("XDG_RUNTIME_DIR", runtime_home, 1);
            (void)setenv("XDG_STATE_HOME", state_home, 1);
            (void)setenv("XDG_CONFIG_HOME", root, 1);
            execv(argv[2], status_arguments);
            _exit(127);
        }
        {
            struct timespec delay = {0, 100000000L};
            int takeover_status;
            (void)nanosleep(&delay, NULL);
            if (waitpid(takeover, &takeover_status, WNOHANG) != 0) {
                (void)close(held);
                (void)kill(daemon, SIGTERM);
                (void)waitpid(daemon, NULL, 0);
                return 1;
            }
            (void)close(held);
            if (waitpid(takeover, &takeover_status, 0) < 0 ||
                !WIFEXITED(takeover_status) ||
                WEXITSTATUS(takeover_status) != 0) {
                (void)kill(daemon, SIGTERM);
                (void)waitpid(daemon, NULL, 0);
                return 1;
            }
        }
    }
    (void)kill(daemon, SIGTERM);
    if (waitpid(daemon, &status, 0) < 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0 || access(socket_path, F_OK) == 0) {
        return 1;
    }
    return 0;
}
