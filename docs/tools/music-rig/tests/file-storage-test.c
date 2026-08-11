#include "music_rig/file_storage.h"
#include "music_rig/state.h"

#include <stdio.h>
#include <string.h>

#define DOCUMENT_CAPACITY ((size_t)131072)

static uint8_t document[DOCUMENT_CAPACITY];

static int test_definition_read(
    music_rig_storage_adapter *adapter,
    const char *fixture_path
)
{
    uint8_t small[8];
    size_t size = 0;

    if (adapter->read(
            adapter->context,
            MUSIC_RIG_STORAGE_COMPILED_DEFINITION,
            document,
            sizeof(document),
            &size
        ) != MUSIC_RIG_RESULT_OK ||
        size <= sizeof(small) || document[0] != (uint8_t)0x7b ||
        adapter->read(
            adapter->context,
            MUSIC_RIG_STORAGE_COMPILED_DEFINITION,
            small,
            sizeof(small),
            &size
        ) != MUSIC_RIG_RESULT_BUFFER_TOO_SMALL ||
        size <= sizeof(small)) {
        fprintf(stderr, "definition read failed for %s\n", fixture_path);
        return 1;
    }
    return 0;
}

static int test_state_replace(
    music_rig_storage_adapter *adapter,
    const char *state_path
)
{
    uint8_t first[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    uint8_t second[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    uint8_t actual[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    size_t size = 0;
    size_t index;

    (void)remove(state_path);
    if (adapter->read(
            adapter->context,
            MUSIC_RIG_STORAGE_RUNTIME_STATE,
            actual,
            sizeof(actual),
            &size
        ) != MUSIC_RIG_RESULT_NOT_FOUND) {
        fputs("missing state was not reported\n", stderr);
        return 1;
    }
    for (index = 0; index < sizeof(first); ++index) {
        first[index] = (uint8_t)index;
        second[index] = (uint8_t)(0xffU - (unsigned int)index);
    }
    if (adapter->atomic_replace(
            adapter->context,
            MUSIC_RIG_STORAGE_RUNTIME_STATE,
            first,
            sizeof(first)
        ) != MUSIC_RIG_RESULT_OK ||
        adapter->atomic_replace(
            adapter->context,
            MUSIC_RIG_STORAGE_RUNTIME_STATE,
            second,
            sizeof(second)
        ) != MUSIC_RIG_RESULT_OK ||
        adapter->read(
            adapter->context,
            MUSIC_RIG_STORAGE_RUNTIME_STATE,
            actual,
            sizeof(actual),
            &size
        ) != MUSIC_RIG_RESULT_OK ||
        size != sizeof(second) || memcmp(actual, second, sizeof(second)) != 0) {
        fputs("atomic state replacement failed\n", stderr);
        (void)remove(state_path);
        return 1;
    }
    if (remove(state_path) != 0) {
        fputs("state test cleanup failed\n", stderr);
        return 1;
    }
    return 0;
}

static int test_failures(
    music_rig_file_storage *storage,
    music_rig_storage_adapter *adapter,
    const char *fixture_path,
    const char *failure_path
)
{
    uint8_t buffer[8];
    size_t size = 0;

    if (adapter->read(
            adapter->context,
            (music_rig_storage_object)99,
            buffer,
            sizeof(buffer),
            &size
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        adapter->read(
            adapter->context,
            MUSIC_RIG_STORAGE_COMPILED_DEFINITION,
            NULL,
            sizeof(buffer),
            &size
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        adapter->atomic_replace(
            adapter->context,
            MUSIC_RIG_STORAGE_COMPILED_DEFINITION,
            buffer,
            sizeof(buffer)
        ) != MUSIC_RIG_RESULT_INVALID_ARGUMENT ||
        music_rig_file_storage_init(
            storage,
            fixture_path,
            failure_path,
            adapter
        ) != MUSIC_RIG_RESULT_OK ||
        adapter->atomic_replace(
            adapter->context,
            MUSIC_RIG_STORAGE_RUNTIME_STATE,
            buffer,
            sizeof(buffer)
        ) != MUSIC_RIG_RESULT_ADAPTER_FAILURE ||
        music_rig_file_storage_init(storage, fixture_path, NULL, adapter) !=
            MUSIC_RIG_RESULT_OK ||
        adapter->atomic_replace(
            adapter->context,
            MUSIC_RIG_STORAGE_RUNTIME_STATE,
            buffer,
            sizeof(buffer)
        ) != MUSIC_RIG_RESULT_NOT_FOUND ||
        music_rig_file_storage_init(storage, NULL, failure_path, adapter) !=
            MUSIC_RIG_RESULT_OK ||
        adapter->read(
            adapter->context,
            MUSIC_RIG_STORAGE_COMPILED_DEFINITION,
            buffer,
            sizeof(buffer),
            &size
        ) != MUSIC_RIG_RESULT_NOT_FOUND ||
        music_rig_file_storage_init(storage, NULL, NULL, adapter) !=
            MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("file storage failure contract failed\n", stderr);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    music_rig_file_storage storage;
    music_rig_storage_adapter adapter;

    if (argc != 4 || music_rig_file_storage_init(
            &storage,
            argv[1],
            argv[2],
            &adapter
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("file storage test setup failed\n", stderr);
        return 1;
    }
    if (test_definition_read(&adapter, argv[1]) != 0 ||
        test_state_replace(&adapter, argv[2]) != 0 ||
        test_failures(&storage, &adapter, argv[1], argv[3]) != 0) {
        return 1;
    }
    return 0;
}
