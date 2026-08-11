#include "music_rig/file_storage.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <limits.h>
#include <wchar.h>

#define NATIVE_PATH_CAPACITY ((size_t)4096)

static volatile LONG temporary_sequence = 0;

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

static music_rig_result convert_path(
    const char *path,
    wchar_t *native_path,
    size_t native_capacity
)
{
    int capacity;

    if (path == NULL || native_path == NULL || native_capacity == 0 ||
        native_capacity > (size_t)INT_MAX) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    capacity = (int)native_capacity;
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            -1,
            native_path,
            capacity
        ) == 0) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
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
    wchar_t native_path[NATIVE_PATH_CAPACITY];
    LARGE_INTEGER file_size;
    size_t total = 0;
    music_rig_result result;
    HANDLE file;

    if (output == NULL || output_capacity == 0 || output_size == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *output_size = 0;
    result = resolve_path(storage, object, &path);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = convert_path(path, native_path, NATIVE_PATH_CAPACITY);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    file = CreateFileW(
        native_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();

        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            MUSIC_RIG_RESULT_NOT_FOUND : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0) {
        (void)CloseHandle(file);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if ((uint64_t)file_size.QuadPart > (uint64_t)SIZE_MAX ||
        (uint64_t)file_size.QuadPart > (uint64_t)output_capacity) {
        if ((uint64_t)file_size.QuadPart <= (uint64_t)SIZE_MAX) {
            *output_size = (size_t)file_size.QuadPart;
        }
        (void)CloseHandle(file);
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }

    while (total < (size_t)file_size.QuadPart) {
        size_t remaining = (size_t)file_size.QuadPart - total;
        DWORD chunk = remaining > (size_t)MAXDWORD ?
            MAXDWORD : (DWORD)remaining;
        DWORD count = 0;

        if (!ReadFile(file, output + total, chunk, &count, NULL)) {
            (void)CloseHandle(file);
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        if (count == 0) {
            break;
        }
        total += (size_t)count;
    }
    if (total == (size_t)file_size.QuadPart) {
        uint8_t extra;
        DWORD count = 0;

        if (!ReadFile(file, &extra, 1U, &count, NULL)) {
            (void)CloseHandle(file);
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        if (count != 0) {
            *output_size = output_capacity == SIZE_MAX ?
                output_capacity : output_capacity + 1U;
            (void)CloseHandle(file);
            return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
        }
    }
    if (!CloseHandle(file)) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *output_size = total;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result write_all(
    HANDLE file,
    const uint8_t *input,
    size_t input_size
)
{
    size_t total = 0;

    while (total < input_size) {
        size_t remaining = input_size - total;
        DWORD chunk = remaining > (size_t)MAXDWORD ?
            MAXDWORD : (DWORD)remaining;
        DWORD count = 0;

        if (!WriteFile(file, input + total, chunk, &count, NULL) || count == 0) {
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        total += (size_t)count;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result create_temporary_file(
    const wchar_t *target_path,
    wchar_t *temporary_path,
    size_t temporary_capacity,
    HANDLE *file
)
{
    unsigned int attempt;

    for (attempt = 0; attempt < 64U; ++attempt) {
        LONG sequence = InterlockedIncrement(&temporary_sequence);
        int length = swprintf_s(
            temporary_path,
            temporary_capacity,
            L"%ls.tmp.%lu.%ld",
            target_path,
            GetCurrentProcessId(),
            sequence
        );

        if (length < 0 || (size_t)length >= temporary_capacity) {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
        *file = CreateFileW(
            temporary_path,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (*file != INVALID_HANDLE_VALUE) {
            return MUSIC_RIG_RESULT_OK;
        }
        {
            DWORD error = GetLastError();

            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
                return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            }
        }
    }
    return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
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
    wchar_t native_path[NATIVE_PATH_CAPACITY];
    wchar_t temporary_path[NATIVE_PATH_CAPACITY];
    music_rig_result result;
    HANDLE file = INVALID_HANDLE_VALUE;

    if (object != MUSIC_RIG_STORAGE_RUNTIME_STATE ||
        (input == NULL && input_size != 0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = resolve_path(storage, object, &path);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = convert_path(path, native_path, NATIVE_PATH_CAPACITY);
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    result = create_temporary_file(
        native_path,
        temporary_path,
        NATIVE_PATH_CAPACITY,
        &file
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }

    result = write_all(file, input, input_size);
    if (result == MUSIC_RIG_RESULT_OK && !FlushFileBuffers(file)) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (!CloseHandle(file)) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result == MUSIC_RIG_RESULT_OK && !MoveFileExW(
            temporary_path,
            native_path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)DeleteFileW(temporary_path);
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
