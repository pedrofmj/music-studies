#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <json-c/json.h>
#include <json-c/json_c_version.h>

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MUSIC_RIG_COMPILER_ID
#define MUSIC_RIG_COMPILER_ID "unknown"
#endif
#ifndef MUSIC_RIG_COMPILER_VERSION
#define MUSIC_RIG_COMPILER_VERSION "unknown"
#endif
#ifndef MUSIC_RIG_JSON_C_LINKAGE
#define MUSIC_RIG_JSON_C_LINKAGE "unknown"
#endif

#define PARSE_ITERATIONS UINT64_C(10000)

#if JSON_C_VERSION_NUM < (17 << 8)
#error "json-c 0.17 or newer is required"
#endif

static int elapsed_ns(
    LARGE_INTEGER started,
    LARGE_INTEGER finished,
    LARGE_INTEGER frequency,
    uint64_t *value
)
{
    uint64_t ticks;
    uint64_t ticks_per_second;

    if (finished.QuadPart < started.QuadPart || frequency.QuadPart <= 0 ||
        value == NULL) {
        return 0;
    }

    ticks = (uint64_t)(finished.QuadPart - started.QuadPart);
    ticks_per_second = (uint64_t)frequency.QuadPart;
    *value = ticks / ticks_per_second * UINT64_C(1000000000) +
        ticks % ticks_per_second * UINT64_C(1000000000) / ticks_per_second;
    return 1;
}

static const char *architecture_name(void)
{
#if defined(_M_X64)
    return "x86_64";
#elif defined(_M_ARM64)
    return "aarch64";
#else
    return "unknown";
#endif
}

static char *read_document(const char *path, size_t *size)
{
    FILE *stream = NULL;
    long length;
    char *content;

    if (fopen_s(&stream, path, "rb") != 0 || stream == NULL ||
        fseek(stream, 0, SEEK_END) != 0 ||
        (length = ftell(stream)) < 0 ||
        fseek(stream, 0, SEEK_SET) != 0 ||
        (unsigned long)length > (unsigned long)INT_MAX) {
        if (stream != NULL) {
            fclose(stream);
        }
        return NULL;
    }

    content = malloc((size_t)length + 1U);
    if (content == NULL ||
        fread(content, 1U, (size_t)length, stream) != (size_t)length) {
        free(content);
        fclose(stream);
        return NULL;
    }

    content[length] = '\0';
    fclose(stream);
    *size = (size_t)length;
    return content;
}

static int document_is_valid(struct json_object *root)
{
    struct json_object *schema;
    struct json_object *profiles;
    struct json_object *mappings;

    return json_object_is_type(root, json_type_object) &&
        json_object_object_get_ex(root, "schema", &schema) &&
        json_object_is_type(schema, json_type_string) &&
        strcmp(
            json_object_get_string(schema),
            "music-studies/compiled-performance-rig/v1"
        ) == 0 &&
        json_object_object_get_ex(root, "device_profiles", &profiles) &&
        json_object_is_type(profiles, json_type_array) &&
        json_object_array_length(profiles) == 5U &&
        json_object_object_get_ex(root, "mappings", &mappings) &&
        json_object_is_type(mappings, json_type_array) &&
        json_object_array_length(mappings) == 8U;
}

static int parse_and_validate(
    struct json_tokener *tokener,
    const char *document,
    size_t document_size
)
{
    struct json_object *root;

    json_tokener_reset(tokener);
    root = json_tokener_parse_ex(tokener, document, (int)document_size);
    if (json_tokener_get_error(tokener) != json_tokener_success ||
        !document_is_valid(root)) {
        json_object_put(root);
        return 0;
    }

    json_object_put(root);
    return 1;
}

static int file_size(const char *path, uint64_t *size)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    ULARGE_INTEGER combined_size;

    if (size == NULL ||
        !GetFileAttributesExA(
            path,
            GetFileExInfoStandard,
            &attributes
        )) {
        return 0;
    }

    combined_size.HighPart = attributes.nFileSizeHigh;
    combined_size.LowPart = attributes.nFileSizeLow;
    *size = combined_size.QuadPart;
    return 1;
}

int main(int argc, char **argv)
{
    struct json_tokener *tokener;
    PROCESS_MEMORY_COUNTERS process_memory;
    LARGE_INTEGER frequency;
    LARGE_INTEGER started;
    LARGE_INTEGER finished;
    char *document;
    size_t document_size;
    uint64_t elapsed;
    uint64_t iteration;
    uint64_t executable_bytes;
    uint64_t baseline_executable_bytes;

    if (argc != 3) {
        fprintf(
            stderr,
            "Usage: %s COMPILED-RUNTIME.json BASELINE-EXECUTABLE\n",
            argv[0]
        );
        return 2;
    }

    if (json_c_version_num() < (17 << 8)) {
        fputs("json-c 0.17 or newer is required\n", stderr);
        return 1;
    }

    document = read_document(argv[1], &document_size);
    if (document == NULL) {
        fputs("json-c fixture read failed\n", stderr);
        return 1;
    }

    tokener = json_tokener_new_ex(32);
    if (tokener == NULL) {
        fputs("json-c tokener allocation failed\n", stderr);
        free(document);
        return 1;
    }

    if (!parse_and_validate(tokener, document, document_size)) {
        fputs("json-c fixture validation failed\n", stderr);
        free(document);
        json_tokener_free(tokener);
        return 1;
    }

    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&started)) {
        fputs("performance counter initialization failed\n", stderr);
        free(document);
        json_tokener_free(tokener);
        return 1;
    }
    for (iteration = UINT64_C(0);
         iteration < PARSE_ITERATIONS;
         ++iteration) {
        if (!parse_and_validate(tokener, document, document_size)) {
            fputs("json-c repeated parse failed\n", stderr);
            free(document);
            json_tokener_free(tokener);
            return 1;
        }
    }
    if (!QueryPerformanceCounter(&finished) ||
        !elapsed_ns(started, finished, frequency, &elapsed)) {
        fputs("performance counter read failed\n", stderr);
        free(document);
        json_tokener_free(tokener);
        return 1;
    }

    process_memory.cb = (DWORD)sizeof(process_memory);
    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            &process_memory,
            process_memory.cb
        ) ||
        !file_size(argv[0], &executable_bytes) ||
        !file_size(argv[2], &baseline_executable_bytes) ||
        executable_bytes < baseline_executable_bytes) {
        fputs("json-c footprint measurement failed\n", stderr);
        free(document);
        json_tokener_free(tokener);
        return 1;
    }

    printf(
        "{\"schema\":\"music-studies/json-c-dependency-spike/v1\","
        "\"platform\":\"windows\",\"architecture\":\"%s\","
        "\"compiler\":\"%s\",\"compiler_version\":\"%s\","
        "\"json_c_version\":\"%s\",\"linkage\":\"%s\","
        "\"input_bytes\":%zu,\"parse_iterations\":%" PRIu64 ","
        "\"elapsed_ns\":%" PRIu64 ",\"average_parse_ns\":%" PRIu64 ","
        "\"peak_working_set_bytes\":%" PRIu64 ","
        "\"executable_bytes\":%" PRIu64 ","
        "\"baseline_executable_bytes\":%" PRIu64 ","
        "\"static_link_delta_bytes\":%" PRIu64 ",\"failures\":0}\n",
        architecture_name(),
        MUSIC_RIG_COMPILER_ID,
        MUSIC_RIG_COMPILER_VERSION,
        json_c_version(),
        MUSIC_RIG_JSON_C_LINKAGE,
        document_size,
        PARSE_ITERATIONS,
        elapsed,
        elapsed / PARSE_ITERATIONS,
        (uint64_t)process_memory.PeakWorkingSetSize,
        executable_bytes,
        baseline_executable_bytes,
        executable_bytes - baseline_executable_bytes
    );

    free(document);
    json_tokener_free(tokener);
    return 0;
}
