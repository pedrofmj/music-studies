#define _POSIX_C_SOURCE 200809L

#include "music_rig/file_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEMP_PATH_CAPACITY ((size_t)4096)

static music_rig_result resolve_path(
    const music_rig_file_storage *storage,
    music_rig_storage_object object,
    const char **path
)
{
    if (storage == NULL || path == NULL ||
        storage->abi_version != MUSIC_RIG_FILE_STORAGE_ABI_VERSION) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (object == MUSIC_RIG_STORAGE_COMPILED_DEFINITION) {
        *path = storage->definition_path;
    } else if (object == MUSIC_RIG_STORAGE_RUNTIME_STATE) {
        *path = storage->state_path;
    } else {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (*path == NULL) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result file_read(
    void *opaque,
    music_rig_storage_object object,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
)
{
    const music_rig_file_storage *storage = opaque;
    const char *path;
    size_t total = 0;
    music_rig_result result;
    int descriptor;

    if (output == NULL || output_capacity == 0 || output_size == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *output_size = 0;
    result = resolve_path(storage, object, &path);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    descriptor = open(path, O_RDONLY);
    if (descriptor < 0) {
        return errno == ENOENT ? MUSIC_RIG_RESULT_NOT_FOUND :
            MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    while (total < output_capacity) {
        ssize_t count = read(descriptor, output + total, output_capacity - total);

        if (count > 0) {
            total += (size_t)count;
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            (void)close(descriptor);
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    if (total == output_capacity) {
        uint8_t extra;
        ssize_t count;

        do {
            count = read(descriptor, &extra, 1U);
        } while (count < 0 && errno == EINTR);
        if (count < 0) {
            (void)close(descriptor);
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        if (count > 0) {
            *output_size = output_capacity == SIZE_MAX ?
                output_capacity : output_capacity + 1U;
            (void)close(descriptor);
            return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
        }
    }
    if (close(descriptor) != 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *output_size = total;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result write_all(
    int descriptor,
    const uint8_t *input,
    size_t input_size
)
{
    size_t total = 0;

    while (total < input_size) {
        size_t remaining = input_size - total;
        size_t chunk = remaining > (size_t)0x7fffffff ?
            (size_t)0x7fffffff : remaining;
        ssize_t count = write(descriptor, input + total, chunk);

        if (count > 0) {
            total += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result file_atomic_replace(
    void *opaque,
    music_rig_storage_object object,
    const uint8_t *input,
    size_t input_size
)
{
    const music_rig_file_storage *storage = opaque;
    const char *path;
    char temporary_path[TEMP_PATH_CAPACITY];
    music_rig_result result;
    int descriptor;
    int length;

    if (object != MUSIC_RIG_STORAGE_RUNTIME_STATE ||
        (input == NULL && input_size != 0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = resolve_path(storage, object, &path);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    length = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s.tmp.XXXXXX",
        path
    );
    if (length < 0 || (size_t)length >= sizeof(temporary_path)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    descriptor = mkstemp(temporary_path);
    if (descriptor < 0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    result = write_all(descriptor, input, input_size);
    if (result == MUSIC_RIG_RESULT_OK && fsync(descriptor) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (close(descriptor) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK && rename(temporary_path, path) != 0) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)unlink(temporary_path);
    }
    return result;
}

music_rig_result music_rig_file_storage_init(
    music_rig_file_storage *storage,
    const char *definition_path,
    const char *state_path,
    music_rig_storage_adapter *adapter
)
{
    if (storage == NULL || adapter == NULL ||
        (definition_path == NULL && state_path == NULL) ||
        (definition_path != NULL && definition_path[0] == 0) ||
        (state_path != NULL && state_path[0] == 0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    storage->abi_version = MUSIC_RIG_FILE_STORAGE_ABI_VERSION;
    storage->definition_path = definition_path;
    storage->state_path = state_path;
    adapter->abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    adapter->context = storage;
    adapter->read = file_read;
    adapter->atomic_replace = file_atomic_replace;
    return MUSIC_RIG_RESULT_OK;
}
