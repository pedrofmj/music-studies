#include "music_rig/diagnostics.h"
#include "music_rig/host_paths.h"
#include "music_rig/journal_diagnostics.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int test_paths(void)
{
    const music_rig_linux_path_environment custom = {
        "/home/tester",
        "/test/config/",
        "/test/cache",
        "/test/state",
        "/run/user/1000"
    };
    const music_rig_linux_path_environment fallback = {
        "/home/tester",
        "relative-config",
        NULL,
        "",
        "/run/user/1000/"
    };
    const music_rig_linux_path_environment root = {
        "/", "/", "/", "/", "/"
    };
    music_rig_linux_path_environment invalid = custom;
    music_rig_host_paths paths;

    if (music_rig_linux_host_paths_resolve(&custom, &paths) !=
            MUSIC_RIG_RESULT_OK ||
        strcmp(paths.config_directory, "/test/config/music-rig") != 0 ||
        strcmp(paths.config_file,
            "/test/config/music-rig/config.json") != 0 ||
        strcmp(paths.compiled_cache_directory,
            "/test/cache/music-rig/compiled") != 0 ||
        strcmp(paths.active_state_file,
            "/test/state/music-rig/active.state") != 0 ||
        strcmp(paths.device_state_directory,
            "/test/state/music-rig/device-state") != 0 ||
        strcmp(paths.control_socket,
            "/run/user/1000/music-rig/control.sock") != 0) {
        fputs("custom XDG path resolution failed\n", stderr);
        return 1;
    }
    if (music_rig_linux_host_paths_resolve(&fallback, &paths) !=
            MUSIC_RIG_RESULT_OK ||
        strcmp(paths.config_directory,
            "/home/tester/.config/music-rig") != 0 ||
        strcmp(paths.cache_directory,
            "/home/tester/.cache/music-rig") != 0 ||
        strcmp(paths.state_directory,
            "/home/tester/.local/state/music-rig") != 0 ||
        strcmp(paths.runtime_directory,
            "/run/user/1000/music-rig") != 0) {
        fputs("XDG fallback path resolution failed\n", stderr);
        return 1;
    }
    if (music_rig_linux_host_paths_resolve(&root, &paths) !=
            MUSIC_RIG_RESULT_OK ||
        strcmp(paths.config_file, "/music-rig/config.json") != 0 ||
        strcmp(paths.control_socket, "/music-rig/control.sock") != 0) {
        fputs("root XDG path resolution failed\n", stderr);
        return 1;
    }

    invalid.xdg_runtime_dir = NULL;
    if (music_rig_linux_host_paths_resolve(&invalid, &paths) !=
            MUSIC_RIG_RESULT_NOT_FOUND || paths.config_directory[0] != '\0') {
        fputs("missing XDG runtime directory was accepted\n", stderr);
        return 1;
    }
    invalid.xdg_runtime_dir = "relative-runtime";
    if (music_rig_linux_host_paths_resolve(&invalid, &paths) !=
            MUSIC_RIG_RESULT_INVALID_DATA ||
        music_rig_linux_host_paths_resolve(NULL, &paths) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_linux_host_paths_resolve(&custom, NULL) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("invalid XDG path input was accepted\n", stderr);
        return 1;
    }
    return 0;
}

static int test_journal_sink(void)
{
    int descriptors[2];
    char output[1024];
    ssize_t count;
    music_rig_journal_diagnostics journal;
    music_rig_diagnostic_sink sink;
    music_rig_diagnostic_record invalid_record;
    music_rig_diagnostics diagnostics;
    char unterminated[MUSIC_RIG_DIAGNOSTIC_CODE_CAPACITY];

    if (pipe(descriptors) != 0 ||
        music_rig_journal_diagnostics_init(
            &journal,
            descriptors[1],
            &sink
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("journal diagnostic setup failed\n", stderr);
        return 1;
    }
    memset(unterminated, 'a', sizeof(unterminated));
    invalid_record.timestamp_ns = UINT64_C(0);
    invalid_record.suppressed_before = UINT64_C(0);
    invalid_record.severity = MUSIC_RIG_DIAGNOSTIC_INFO;
    invalid_record.code = unterminated;
    invalid_record.message = "invalid\nmessage";
    if (sink.write(sink.context, &invalid_record) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_diagnostics_init(
            &diagnostics,
            UINT64_C(100),
            UINT32_C(1),
            &sink
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(10), MUSIC_RIG_DIAGNOSTIC_INFO,
            "runtime.started", "ready"
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(11), MUSIC_RIG_DIAGNOSTIC_WARNING,
            "runtime.retry", "suppressed"
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_diagnostics_emit(
            &diagnostics, UINT64_C(110), MUSIC_RIG_DIAGNOSTIC_ERROR,
            "runtime.failed", "stopped"
        ) != MUSIC_RIG_RESULT_OK ||
        close(descriptors[1]) != 0) {
        fputs("journal diagnostic setup failed\n", stderr);
        return 1;
    }
    count = read(descriptors[0], output, sizeof(output) - 1U);
    (void)close(descriptors[0]);
    if (count <= 0) {
        fputs("journal diagnostic output was empty\n", stderr);
        return 1;
    }
    output[(size_t)count] = '\0';
    if (strstr(output,
            "severity=info code=runtime.started timestamp_ns=10 "
            "suppressed=0 message=ready\n") == NULL ||
        strstr(output,
            "severity=error code=runtime.failed timestamp_ns=110 "
            "suppressed=1 message=stopped\n") == NULL ||
        strstr(output, "message=suppressed") != NULL ||
        music_rig_journal_diagnostics_init(NULL, 2, &sink) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("journal diagnostic output is incorrect\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_paths() != 0) {
        return 1;
    }
    return test_journal_sink();
}
