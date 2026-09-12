#include "music_rig/synthv1_preset_adapter.h"

#include <stdio.h>
#include <string.h>

static int write_preset(const char *path, bool complete)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        return 1;
    }
    fputs("<!DOCTYPE synthv1>\n<preset><params>\n", file);
    fputs("<param index=\"0\" name=\"DCO1_BALANCE\">0.5</param>\n", file);
    fputs("<param index=\"1\" name=\"DCF1_CUTOFF\">0.5</param>\n", file);
    if (complete) {
        fputs("<param index=\"2\" name=\"OUT1_VOLUME\">0.5</param>\n", file);
    }
    fputs("</params></preset>\n", file);
    return fclose(file) != 0;
}

typedef struct process_mock {
    unsigned int starts;
    unsigned int stops;
    bool fail_start;
} process_mock;

static music_rig_result process_start(void *opaque, const char *preset_path)
{
    process_mock *mock = opaque;
    (void)preset_path;
    mock->starts += 1U;
    return mock->fail_start
        ? MUSIC_RIG_RESULT_ADAPTER_FAILURE
        : MUSIC_RIG_RESULT_OK;
}

static music_rig_result process_stop(void *opaque)
{
    process_mock *mock = opaque;
    mock->stops += 1U;
    return MUSIC_RIG_RESULT_OK;
}

int main(void)
{
    static const char *const required[] = {
        "DCO1_BALANCE", "DCF1_CUTOFF", "OUT1_VOLUME"
    };
    const music_rig_generation previous = {UINT64_C(1), NULL};
    const music_rig_generation candidate = {UINT64_C(2), NULL};
    music_rig_synthv1_preset_adapter resource;
    music_rig_prepared_engine_adapter interfaces;
    music_rig_prepared_engine_transaction transaction;
    process_mock process = {0};
    const char *path = "/tmp/music-rig-synthv1-preset-adapter-test.synthv1";

    if (write_preset(path, true) != 0 ||
        music_rig_synthv1_preset_adapter_init(
            &resource, path, required, sizeof(required) / sizeof(required[0])
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_synthv1_preset_adapter_interfaces(
            &resource, &interfaces
        ) != MUSIC_RIG_RESULT_OK) {
        fputs("synthv1 preset adapter setup failed\n", stderr);
        return 1;
    }
    music_rig_synthv1_preset_adapter_set_process_hooks(
        &resource, &process, process_start, process_stop
    );
    if (
        music_rig_prepared_engine_transaction_begin(
            &transaction, &interfaces, &previous, &candidate
        ) != MUSIC_RIG_RESULT_OK ||
        music_rig_prepared_engine_transaction_commit(&transaction) !=
            MUSIC_RIG_RESULT_OK || !resource.committed ||
        music_rig_prepared_engine_transaction_rollback(&transaction) !=
            MUSIC_RIG_RESULT_OK || resource.committed || resource.staged ||
        process.starts != 1U || process.stops != 1U) {
        fputs("synthv1 preset adapter transaction failed\n", stderr);
        return 1;
    }
    if (write_preset(path, false) != 0 ||
        music_rig_prepared_engine_transaction_begin(
            &transaction, &interfaces, &previous, &candidate
        ) != MUSIC_RIG_RESULT_INVALID_DATA || resource.staged) {
        fputs("synthv1 preset adapter validation failure was accepted\n", stderr);
        return 1;
    }
    if (write_preset(path, true) != 0) {
        return 1;
    }
    process.fail_start = true;
    if (music_rig_prepared_engine_transaction_begin(
            &transaction, &interfaces, &previous, &candidate
        ) != MUSIC_RIG_RESULT_ADAPTER_FAILURE || resource.staged ||
        process.starts != 2U || process.stops != 1U) {
        fputs("synthv1 process-start failure was accepted\n", stderr);
        return 1;
    }
    puts("Synthv1 preset adapter tests: PASS");
    return 0;
}
