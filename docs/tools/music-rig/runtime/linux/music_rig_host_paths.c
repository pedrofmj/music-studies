#define _POSIX_C_SOURCE 200809L

#include "music_rig/host_paths.h"

#include <stdlib.h>
#include <string.h>

static bool absolute_path(const char *value)
{
    size_t index;

    if (value == NULL || value[0] != '/') {
        return false;
    }
    for (index = 0U; index < MUSIC_RIG_HOST_PATH_CAPACITY; ++index) {
        unsigned char byte = (unsigned char)value[index];

        if (byte == '\0') {
            return true;
        }
        if (byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
    return false;
}

static const char *optional_absolute(const char *value)
{
    return value != NULL && value[0] != '\0' && absolute_path(value)
        ? value
        : NULL;
}

static music_rig_result append_path(
    char *output,
    const char *base,
    const char *suffix
)
{
    size_t base_size;
    size_t suffix_offset;
    size_t suffix_size = strlen(suffix);

    if (!absolute_path(base) || suffix[0] != '/') {
        return MUSIC_RIG_RESULT_INVALID_DATA;
    }
    base_size = strlen(base);
    while (base_size > 1U && base[base_size - 1U] == '/') {
        base_size -= 1U;
    }
    suffix_offset = base_size == 1U ? 1U : 0U;
    if (base_size + suffix_size - suffix_offset + 1U >
            MUSIC_RIG_HOST_PATH_CAPACITY) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, base, base_size);
    memcpy(
        output + base_size,
        suffix + suffix_offset,
        suffix_size - suffix_offset + 1U
    );
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result resolve_base(
    char *output,
    const char *xdg_value,
    const char *home,
    const char *fallback_suffix
)
{
    const char *base = optional_absolute(xdg_value);

    if (base != NULL) {
        return append_path(output, base, "/music-rig");
    }
    if (!absolute_path(home)) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    return append_path(output, home, fallback_suffix);
}

music_rig_result music_rig_linux_host_paths_resolve(
    const music_rig_linux_path_environment *environment,
    music_rig_host_paths *paths
)
{
    const char *runtime_base;
    music_rig_result result;

    if (environment == NULL || paths == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(paths, 0, sizeof(*paths));

    result = resolve_base(
        paths->config_directory,
        environment->xdg_config_home,
        environment->home,
        "/.config/music-rig"
    );
    if (result == MUSIC_RIG_RESULT_OK) {
        result = resolve_base(
            paths->cache_directory,
            environment->xdg_cache_home,
            environment->home,
            "/.cache/music-rig"
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = resolve_base(
            paths->state_directory,
            environment->xdg_state_home,
            environment->home,
            "/.local/state/music-rig"
        );
    }
    runtime_base = optional_absolute(environment->xdg_runtime_dir);
    if (result == MUSIC_RIG_RESULT_OK && runtime_base == NULL) {
        result = environment->xdg_runtime_dir != NULL &&
                environment->xdg_runtime_dir[0] != '\0'
            ? MUSIC_RIG_RESULT_INVALID_DATA
            : MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = append_path(
            paths->runtime_directory,
            runtime_base,
            "/music-rig"
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = append_path(
            paths->config_file,
            paths->config_directory,
            "/config.json"
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = append_path(
            paths->compiled_cache_directory,
            paths->cache_directory,
            "/compiled"
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = append_path(
            paths->active_state_file,
            paths->state_directory,
            "/active.state"
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = append_path(
            paths->device_state_directory,
            paths->state_directory,
            "/device-state"
        );
    }
    if (result == MUSIC_RIG_RESULT_OK) {
        result = append_path(
            paths->control_socket,
            paths->runtime_directory,
            "/control.sock"
        );
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        memset(paths, 0, sizeof(*paths));
    }
    return result;
}

music_rig_result music_rig_linux_host_paths_from_process(
    music_rig_host_paths *paths
)
{
    music_rig_linux_path_environment environment;

    environment.home = getenv("HOME");
    environment.xdg_config_home = getenv("XDG_CONFIG_HOME");
    environment.xdg_cache_home = getenv("XDG_CACHE_HOME");
    environment.xdg_state_home = getenv("XDG_STATE_HOME");
    environment.xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    return music_rig_linux_host_paths_resolve(&environment, paths);
}
