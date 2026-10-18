#include "music_rig/win32_soundfont_paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

static bool directory_exists(const wchar_t *path)
{
    DWORD attributes = GetFileAttributesW(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
}

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

static bool copy_path(wchar_t *output, size_t capacity, const wchar_t *path)
{
    size_t length;

    if (output == NULL || path == NULL || capacity == 0U) {
        return false;
    }
    length = wcslen(path);
    if (length >= capacity) {
        return false;
    }
    wmemcpy(output, path, length + 1U);
    return true;
}

static bool find_account_root(
    const wchar_t *insync_root,
    wchar_t *output,
    size_t capacity
)
{
    WIN32_FIND_DATAW data;
    wchar_t pattern[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    HANDLE search;

    if (!append_path(pattern, sizeof(pattern) / sizeof(pattern[0]),
            insync_root, L"*")) {
        return false;
    }
    search = FindFirstFileW(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) {
        return false;
    }
    do {
        wchar_t account_root[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
        wchar_t candidate[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
            wcscmp(data.cFileName, L".") == 0 ||
            wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        if (!append_path(account_root, sizeof(account_root) / sizeof(account_root[0]),
                insync_root, data.cFileName) ||
            !append_path(candidate, sizeof(candidate) / sizeof(candidate[0]),
                account_root,
                L"Google Drive\\Flash\\PED\\MIDI\\Pack de Timbres\\Library")) {
            continue;
        }
        if (directory_exists(candidate) && copy_path(output, capacity, candidate)) {
            FindClose(search);
            return true;
        }
    } while (FindNextFileW(search, &data) != 0);
    FindClose(search);
    return false;
}

music_rig_result music_rig_win32_soundfont_root_resolve(
    const wchar_t *profile_root,
    wchar_t *root,
    size_t root_capacity
)
{
    PWSTR known_profile = NULL;
    wchar_t insync_root[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    wchar_t direct_root[MUSIC_RIG_WIN32_SOUNDFONT_PATH_CAPACITY];
    HRESULT status;
    bool found = false;

    if (root == NULL || root_capacity == 0U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    root[0] = L'\0';
    if (profile_root == NULL) {
        status = SHGetKnownFolderPath(&FOLDERID_Profile, KF_FLAG_DEFAULT,
            NULL, &known_profile);
        if (status != S_OK || known_profile == NULL) {
            return MUSIC_RIG_RESULT_NOT_FOUND;
        }
        profile_root = known_profile;
    }
    if (!append_path(insync_root, sizeof(insync_root) / sizeof(insync_root[0]),
            profile_root, L"Insync") ||
        !append_path(direct_root, sizeof(direct_root) / sizeof(direct_root[0]),
            profile_root, L"Flash\\PED\\MIDI\\Pack de Timbres\\Library")) {
        status = E_INVALIDARG;
        goto cleanup;
    }
    if (directory_exists(direct_root)) {
        found = copy_path(root, root_capacity, direct_root);
    } else if (directory_exists(insync_root)) {
        found = find_account_root(insync_root, root, root_capacity);
    }
    status = found ? S_OK : HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);

cleanup:
    if (known_profile != NULL) {
        CoTaskMemFree(known_profile);
    }
    if (status == E_INVALIDARG) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return found ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_NOT_FOUND;
}

music_rig_result music_rig_win32_soundfont_path_join(
    const wchar_t *root,
    const wchar_t *relative_path,
    wchar_t *output,
    size_t output_capacity
)
{
    if (root == NULL || relative_path == NULL || output == NULL ||
        root[0] == L'\0' || relative_path[0] == L'\0' ||
        relative_path[0] == L'\\' || relative_path[0] == L'/' ||
        wcschr(relative_path, L':') != NULL ||
        wcsstr(relative_path, L"..") != NULL ||
        !append_path(output, output_capacity, root, relative_path)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return MUSIC_RIG_RESULT_OK;
}
