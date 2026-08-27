#define _POSIX_C_SOURCE 200809L

#include "music_rig/runtime.h"
#include "compiled-tables-fixture.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SWITCH_SAMPLE_COUNT 1000U
#define CONTROL_COMMIT_P95_LIMIT_NS UINT64_C(20000000)
#define PROCESSING_PERIOD_NS UINT64_C(42666667)
#define ADOPTION_MARGIN_NS UINT64_C(5000000)
#define ADOPTION_MAX_LIMIT_NS (PROCESSING_PERIOD_NS + ADOPTION_MARGIN_NS)

typedef struct benchmark_storage {
    uint8_t frame[MUSIC_RIG_RUNTIME_STATE_FRAME_SIZE];
    bool exists;
} benchmark_storage;

static uint64_t monotonic_raw_ns(void *opaque)
{
    struct timespec value;

    (void)opaque;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &value) != 0) {
        return UINT64_C(0);
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
        (uint64_t)value.tv_nsec;
}

static music_rig_result storage_read(
    void *opaque,
    music_rig_storage_object object,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size
)
{
    benchmark_storage *storage = opaque;

    if (storage == NULL || output == NULL || output_size == NULL ||
        object != MUSIC_RIG_STORAGE_RUNTIME_STATE || !storage->exists) {
        return MUSIC_RIG_RESULT_NOT_FOUND;
    }
    if (output_capacity < sizeof(storage->frame)) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }
    memcpy(output, storage->frame, sizeof(storage->frame));
    *output_size = sizeof(storage->frame);
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result storage_replace(
    void *opaque,
    music_rig_storage_object object,
    const uint8_t *input,
    size_t input_size
)
{
    benchmark_storage *storage = opaque;

    if (storage == NULL || input == NULL ||
        object != MUSIC_RIG_STORAGE_RUNTIME_STATE ||
        input_size != sizeof(storage->frame)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memcpy(storage->frame, input, input_size);
    storage->exists = true;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result control_start(void *opaque)
{
    (void)opaque;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_control_poll control_poll(
    void *opaque,
    music_rig_protocol_request *request
)
{
    (void)opaque;
    (void)request;
    return MUSIC_RIG_CONTROL_STOP;
}

static music_rig_result control_wait(void *opaque)
{
    (void)opaque;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result control_respond(
    void *opaque,
    const music_rig_protocol_response *response
)
{
    (void)opaque;
    (void)response;
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result control_stop(void *opaque)
{
    (void)opaque;
    return MUSIC_RIG_RESULT_OK;
}

static int compare_u64(const void *left, const void *right)
{
    const uint64_t left_value = *(const uint64_t *)left;
    const uint64_t right_value = *(const uint64_t *)right;

    return left_value < right_value ? -1 : left_value > right_value;
}

static void do_synthetic_load(unsigned int rounds)
{
    volatile uint64_t value = UINT64_C(0);
    unsigned int index;

    for (index = 0U; index < rounds; ++index) {
        value = (value * UINT64_C(16777619)) ^ (uint64_t)index;
    }
}

static int run_scenario(const char *name, unsigned int synthetic_load)
{
    static const uint8_t fingerprint[MUSIC_RIG_DEFINITION_FINGERPRINT_SIZE] = {
        0x5a
    };
    static music_rig_compiled_tables base_tables;
    static music_rig_compiled_tables alternate_tables;
    static music_rig_compiled_definition alternate_definition;
    static music_rig_prepared_definition prepared;
    static music_rig_runtime runtime;
    static benchmark_storage storage_context;
    music_rig_generation initial;
    music_rig_runtime_config config;
    music_rig_platform_interfaces interfaces;
    uint64_t control_samples[SWITCH_SAMPLE_COUNT];
    uint64_t effective_adoption_samples[SWITCH_SAMPLE_COUNT];
    uint64_t commit_to_adoption_samples[SWITCH_SAMPLE_COUNT];
    uint64_t started_ns;
    uint64_t published_ns;
    uint64_t adopted_ns;
    uint64_t finished_ns;
    uint64_t control_p95;
    uint64_t effective_adoption_p95;
    uint64_t commit_to_adoption_p95;
    uint64_t control_maximum;
    uint64_t effective_adoption_maximum;
    uint64_t commit_to_adoption_maximum;
    size_t index;

    if (init_compiled_tables_fixture(&base_tables) != MUSIC_RIG_RESULT_OK ||
        init_alternate_prepared_definition_fixture(
            &alternate_tables, &alternate_definition, &prepared
        ) != MUSIC_RIG_RESULT_OK) {
        return 1;
    }
    memset(&storage_context, 0, sizeof(storage_context));
    memset(&interfaces, 0, sizeof(interfaces));
    interfaces.abi_version = MUSIC_RIG_RUNTIME_ABI_VERSION;
    interfaces.clock.now_ns = monotonic_raw_ns;
    interfaces.control.start = control_start;
    interfaces.control.poll = control_poll;
    interfaces.control.wait = control_wait;
    interfaces.control.respond = control_respond;
    interfaces.control.stop = control_stop;
    interfaces.storage.abi_version = MUSIC_RIG_STORAGE_ABI_VERSION;
    interfaces.storage.context = &storage_context;
    interfaces.storage.read = storage_read;
    interfaces.storage.atomic_replace = storage_replace;
    initial.id = UINT64_C(1);
    initial.mapping = &base_tables;
    memset(&config, 0, sizeof(config));
    config.initial_generation = &initial;
    config.definition_fingerprint = fingerprint;
    config.definition_fingerprint_size = sizeof(fingerprint);
    config.active_rig_profile = "full-live-rack";
    config.prepared_definitions = &prepared;
    config.prepared_definition_count = 1U;
    config.output_mode = MUSIC_RIG_OUTPUT_SUPPRESSED;
    if (music_rig_runtime_init(&runtime, &config, &interfaces) !=
            MUSIC_RIG_RESULT_OK) {
        return 1;
    }

    for (index = 0U; index < SWITCH_SAMPLE_COUNT; ++index) {
        music_rig_protocol_request request;
        music_rig_protocol_response response;
        bool use_alternate = (index & 1U) == 0U;

        do_synthetic_load(synthetic_load);
        memset(&request, 0, sizeof(request));
        request.protocol_version = MUSIC_RIG_PROTOCOL_VERSION;
        request.request_id = (uint64_t)index + UINT64_C(1);
        request.expected_generation = runtime.state.generation_id;
        request.operation = use_alternate
            ? (uint32_t)MUSIC_RIG_OPERATION_SWITCH_DEVICE
            : (uint32_t)MUSIC_RIG_OPERATION_RESET_DEVICE_OVERRIDE;
        fixture_copy(request.device_slot, "smc-mixer-main");
        if (use_alternate) {
            fixture_copy(request.profile, "multilevel-volume");
        }
        started_ns = monotonic_raw_ns(NULL);
        if (music_rig_runtime_dispatch(&runtime, &request, &response) !=
                MUSIC_RIG_RESULT_OK || response.result_code !=
                (uint32_t)MUSIC_RIG_RESULT_OK) {
            fprintf(stderr, "%s scenario switch failed at sample %zu\n",
                name, index);
            return 1;
        }
        finished_ns = monotonic_raw_ns(NULL);
        published_ns = finished_ns;
        control_samples[index] = finished_ns >= started_ns
            ? finished_ns - started_ns : UINT64_C(0);
        if (music_rig_generation_slot_adopt(&runtime.generations) !=
                runtime.control_generation) {
            fprintf(stderr, "%s scenario generation adoption failed at sample %zu\n",
                name, index);
            return 1;
        }
        adopted_ns = monotonic_raw_ns(NULL);
        effective_adoption_samples[index] = adopted_ns >= started_ns
            ? adopted_ns - started_ns : UINT64_C(0);
        commit_to_adoption_samples[index] = adopted_ns >= published_ns
            ? adopted_ns - published_ns : UINT64_C(0);
        (void)music_rig_runtime_reclaim_generation(&runtime);
    }
    qsort(control_samples, SWITCH_SAMPLE_COUNT, sizeof(control_samples[0]),
        compare_u64);
    qsort(effective_adoption_samples, SWITCH_SAMPLE_COUNT,
        sizeof(effective_adoption_samples[0]), compare_u64);
    qsort(commit_to_adoption_samples, SWITCH_SAMPLE_COUNT,
        sizeof(commit_to_adoption_samples[0]), compare_u64);
    control_p95 = control_samples[SWITCH_SAMPLE_COUNT * 95U / 100U - 1U];
    effective_adoption_p95 = effective_adoption_samples[
        SWITCH_SAMPLE_COUNT * 95U / 100U - 1U
    ];
    commit_to_adoption_p95 = commit_to_adoption_samples[
        SWITCH_SAMPLE_COUNT * 95U / 100U - 1U
    ];
    control_maximum = control_samples[SWITCH_SAMPLE_COUNT - 1U];
    effective_adoption_maximum = effective_adoption_samples[
        SWITCH_SAMPLE_COUNT - 1U
    ];
    commit_to_adoption_maximum = commit_to_adoption_samples[
        SWITCH_SAMPLE_COUNT - 1U
    ];
    printf(
        "{\"schema\":\"music-studies/music-rig-switch-performance/v1\","
        "\"scenario\":\"%s\",\"sample_count\":%u,"
        "\"control_commit_p95_ns\":%" PRIu64 ","
        "\"control_commit_maximum_ns\":%" PRIu64 ","
        "\"effective_adoption_p95_ns\":%" PRIu64 ","
        "\"effective_adoption_maximum_ns\":%" PRIu64 ","
        "\"commit_to_adoption_p95_ns\":%" PRIu64 ","
        "\"commit_to_adoption_maximum_ns\":%" PRIu64 ","
        "\"timing_pass\":%s}\n",
        name,
        SWITCH_SAMPLE_COUNT,
        control_p95,
        control_maximum,
        effective_adoption_p95,
        effective_adoption_maximum,
        commit_to_adoption_p95,
        commit_to_adoption_maximum,
        control_p95 <= CONTROL_COMMIT_P95_LIMIT_NS &&
            commit_to_adoption_maximum <= ADOPTION_MAX_LIMIT_NS
            ? "true" : "false"
    );
    return control_p95 <= CONTROL_COMMIT_P95_LIMIT_NS &&
        commit_to_adoption_maximum <= ADOPTION_MAX_LIMIT_NS ? 0 : 1;
}

int main(void)
{
    return run_scenario("idle", 0U) != 0 ||
        run_scenario("normal-performance", 100U) != 0 ||
        run_scenario("high-midi-load", 1000U) != 0;
}
