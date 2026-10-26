#include "music_rig/win32_soundfont_paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
#define MUSIC_RIG_TEST_DIRECTORY_CAPACITY ((size_t)16)

typedef struct test_directories {
    wchar_t root[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    wchar_t paths[MUSIC_RIG_TEST_DIRECTORY_CAPACITY]
                 [MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    size_t count;
    bool root_created;
} test_directories;

static bool append_path(
    wchar_t *output,
    size_t capacity,
    const wchar_t *base,
    const wchar_t *suffix
)
{
    int written = _snwprintf_s(
        output, capacity, _TRUNCATE, L"%ls\\%ls", base, suffix
    );

    return written >= 0 && (size_t)written < capacity;
}

static bool make_directory_tree(
    test_directories *directories,
    const wchar_t *relative_path,
    wchar_t *final_path,
    size_t final_capacity
)
{
    wchar_t current[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    const wchar_t *segment = relative_path;
    const wchar_t *cursor;

    if (wcslen(directories->root) >= sizeof(current) / sizeof(current[0])) {
        return false;
    }
    wcscpy_s(current, sizeof(current) / sizeof(current[0]), directories->root);
    for (cursor = relative_path;; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'\0') {
            wchar_t component[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
            size_t component_size = (size_t)(cursor - segment);
            wchar_t next[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];

            if (component_size == 0U ||
                component_size >= sizeof(component) / sizeof(component[0]) ||
                directories->count >= MUSIC_RIG_TEST_DIRECTORY_CAPACITY) {
                return false;
            }
            wmemcpy(component, segment, component_size);
            component[component_size] = L'\0';
            if (!append_path(next, sizeof(next) / sizeof(next[0]), current,
                    component) || !CreateDirectoryW(next, NULL)) {
                return false;
            }
            wcscpy_s(current, sizeof(current) / sizeof(current[0]), next);
            wcscpy_s(directories->paths[directories->count],
                sizeof(directories->paths[directories->count]) /
                    sizeof(directories->paths[directories->count][0]),
                current);
            directories->count += 1U;
            if (*cursor == L'\0') {
                return wcslen(current) < final_capacity &&
                    wcscpy_s(final_path, final_capacity, current) == 0;
            }
            segment = cursor + 1;
        }
    }
}

static void remove_directories_from(test_directories *directories, size_t first)
{
    while (directories->count > first) {
        directories->count -= 1U;
        (void)RemoveDirectoryW(directories->paths[directories->count]);
    }
}

static void cleanup_directories(test_directories *directories)
{
    remove_directories_from(directories, 0U);
    if (directories->root_created) {
        (void)RemoveDirectoryW(directories->root);
        directories->root_created = false;
    }
}

static bool create_test_root(test_directories *directories)
{
    wchar_t temp_path[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    DWORD length = GetTempPathW(
        (DWORD)(sizeof(temp_path) / sizeof(temp_path[0])), temp_path
    );

    if (length == 0U || length >= sizeof(temp_path) / sizeof(temp_path[0]) ||
        GetTempFileNameW(temp_path, L"mrg", 0U, directories->root) == 0U) {
        return false;
    }
    if (!DeleteFileW(directories->root) ||
        !CreateDirectoryW(directories->root, NULL)) {
        return false;
    }
    directories->root_created = true;
    return true;
}

int main(void)
{
    wchar_t output[64];
    wchar_t small_output[4];
    wchar_t expected_root[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    wchar_t resolved_root[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    test_directories directories = {0};
    int result = 0;

    CHECK(music_rig_win32_soundfont_path_join(
            L"C:\\Library", L"Organ.sf2", output,
            sizeof(output) / sizeof(output[0])
        ) == MUSIC_RIG_RESULT_OK &&
        wcscmp(output, L"C:\\Library\\Organ.sf2") == 0);
    CHECK(music_rig_win32_soundfont_path_join(
            L"C:\\Library", L"strings..sf2", output,
            sizeof(output) / sizeof(output[0])
        ) == MUSIC_RIG_RESULT_OK &&
        wcscmp(output, L"C:\\Library\\strings..sf2") == 0);
    CHECK(music_rig_win32_soundfont_path_join(
            L"C:\\Library", L"subfolder/../Organ.sf2", output,
            sizeof(output) / sizeof(output[0])
        ) == MUSIC_RIG_RESULT_INVALID_ARGUMENT);
    CHECK(music_rig_win32_soundfont_path_join(
            L"C:\\Library", L"subfolder/.. /Organ.sf2", output,
            sizeof(output) / sizeof(output[0])
        ) == MUSIC_RIG_RESULT_INVALID_ARGUMENT);
    CHECK(music_rig_win32_soundfont_path_join(
            L"C:\\Library", L"Organ.sf2", small_output,
            sizeof(small_output) / sizeof(small_output[0])
        ) == MUSIC_RIG_RESULT_BUFFER_TOO_SMALL);

#define REQUIRE(condition) do { \
    if (!(condition)) { result = __LINE__; goto cleanup; } \
} while (0)
    REQUIRE(create_test_root(&directories));
    REQUIRE(make_directory_tree(&directories,
        L"Flash\\PED\\MIDI\\Pack de Timbres\\Library", expected_root,
        sizeof(expected_root) / sizeof(expected_root[0])));
    REQUIRE(music_rig_win32_soundfont_root_resolve(
            directories.root, small_output,
            sizeof(small_output) / sizeof(small_output[0])
        ) == MUSIC_RIG_RESULT_BUFFER_TOO_SMALL && small_output[0] == L'\0');
    REQUIRE(music_rig_win32_soundfont_root_resolve(
            directories.root, resolved_root,
            sizeof(resolved_root) / sizeof(resolved_root[0])
        ) == MUSIC_RIG_RESULT_OK &&
        wcscmp(resolved_root, expected_root) == 0);

    remove_directories_from(&directories, 0U);
    REQUIRE(make_directory_tree(&directories,
        L"Insync\\account\\Google Drive\\Flash\\PED\\MIDI\\Pack de Timbres\\Library",
        expected_root, sizeof(expected_root) / sizeof(expected_root[0])));
    REQUIRE(music_rig_win32_soundfont_root_resolve(
            directories.root, resolved_root,
            sizeof(resolved_root) / sizeof(resolved_root[0])
        ) == MUSIC_RIG_RESULT_OK &&
        wcscmp(resolved_root, expected_root) == 0);

    remove_directories_from(&directories, 0U);
    REQUIRE(music_rig_win32_soundfont_root_resolve(
            directories.root, resolved_root,
            sizeof(resolved_root) / sizeof(resolved_root[0])
        ) == MUSIC_RIG_RESULT_NOT_FOUND && resolved_root[0] == L'\0');

cleanup:
    cleanup_directories(&directories);
#undef REQUIRE
    return result;
}
