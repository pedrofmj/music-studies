#include <json-c/json.h>
#include <json-c/json_c_version.h>

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>

#ifndef MUSIC_RIG_COMPILER_ID
#define MUSIC_RIG_COMPILER_ID "unknown"
#endif
#ifndef MUSIC_RIG_COMPILER_VERSION
#define MUSIC_RIG_COMPILER_VERSION "unknown"
#endif
#ifndef MUSIC_RIG_JSON_C_LIBRARY_PATH
#define MUSIC_RIG_JSON_C_LIBRARY_PATH ""
#endif

#define PARSE_ITERATIONS UINT64_C(10000)

#if JSON_C_VERSION_NUM < (17 << 8)
#error "json-c 0.17 or newer is required"
#endif

static uint64_t monotonic_ns(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &value) != 0) {
        return UINT64_C(0);
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
        (uint64_t)value.tv_nsec;
}

static const char *architecture_name(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#else
    return "unknown";
#endif
}

static char *read_document(const char *path, size_t *size)
{
    FILE *stream;
    long length;
    char *content;

    stream = fopen(path, "rb");
    if (stream == NULL ||
        fseek(stream, 0, SEEK_END) != 0 ||
        (length = ftell(stream)) < 0 ||
        fseek(stream, 0, SEEK_SET) != 0 ||
        (unsigned long)length > (unsigned long)INT_MAX) {
        if (stream != NULL) {
            fclose(stream);
        }
        return NULL;
    }

    content = malloc((size_t)length + 1);
    if (content == NULL ||
        fread(content, 1, (size_t)length, stream) != (size_t)length) {
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
        json_object_array_length(profiles) == 5 &&
        json_object_object_get_ex(root, "mappings", &mappings) &&
        json_object_is_type(mappings, json_type_array) &&
        json_object_array_length(mappings) == 8;
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

int main(int argc, char **argv)
{
    struct json_tokener *tokener;
    struct rusage usage;
    struct stat executable_stat;
    struct stat library_stat;
    char *document;
    size_t document_size;
    uint64_t started_ns;
    uint64_t finished_ns;
    uint64_t elapsed_ns;
    uint64_t iteration;
    uint64_t peak_rss_bytes;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s COMPILED-RUNTIME.json\n", argv[0]);
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

    started_ns = monotonic_ns();
    if (started_ns == UINT64_C(0)) {
        fputs("monotonic clock read failed\n", stderr);
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
    finished_ns = monotonic_ns();
    if (finished_ns < started_ns) {
        fputs("monotonic clock read failed\n", stderr);
        free(document);
        json_tokener_free(tokener);
        return 1;
    }
    elapsed_ns = finished_ns - started_ns;

    if (getrusage(RUSAGE_SELF, &usage) != 0 ||
        stat(argv[0], &executable_stat) != 0 ||
        stat(MUSIC_RIG_JSON_C_LIBRARY_PATH, &library_stat) != 0) {
        fputs("json-c footprint measurement failed\n", stderr);
        free(document);
        json_tokener_free(tokener);
        return 1;
    }

    peak_rss_bytes = (uint64_t)usage.ru_maxrss * UINT64_C(1024);
    printf(
        "{\"schema\":\"music-studies/json-c-dependency-spike/v1\","
        "\"platform\":\"linux\",\"architecture\":\"%s\","
        "\"compiler\":\"%s\",\"compiler_version\":\"%s\","
        "\"json_c_version\":\"%s\",\"input_bytes\":%zu,"
        "\"parse_iterations\":%" PRIu64 ",\"elapsed_ns\":%" PRIu64 ","
        "\"average_parse_ns\":%" PRIu64 ",\"peak_rss_bytes\":%" PRIu64 ","
        "\"executable_bytes\":%" PRIu64 ",\"shared_library_bytes\":%"
        PRIu64 ",\"failures\":0}\n",
        architecture_name(),
        MUSIC_RIG_COMPILER_ID,
        MUSIC_RIG_COMPILER_VERSION,
        json_c_version(),
        document_size,
        PARSE_ITERATIONS,
        elapsed_ns,
        elapsed_ns / PARSE_ITERATIONS,
        peak_rss_bytes,
        (uint64_t)executable_stat.st_size,
        (uint64_t)library_stat.st_size
    );

    free(document);
    json_tokener_free(tokener);
    return 0;
}
