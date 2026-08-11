#include "music_rig/core.h"

#include <stdio.h>
#include <string.h>

static int test_build_info(void)
{
    const music_rig_build_info *build_info = music_rig_get_build_info();

    if (build_info == NULL) {
        fputs("build info is null\n", stderr);
        return 1;
    }
    if (strcmp(build_info->core_version, MUSIC_RIG_CORE_VERSION) != 0) {
        fputs("core version mismatch\n", stderr);
        return 1;
    }
    if (build_info->protocol_version != MUSIC_RIG_PROTOCOL_VERSION) {
        fputs("protocol version mismatch\n", stderr);
        return 1;
    }
    if (build_info->profile_schema_version !=
        MUSIC_RIG_PROFILE_SCHEMA_VERSION) {
        fputs("profile schema version mismatch\n", stderr);
        return 1;
    }
    if (MUSIC_RIG_RESULT_OK != 0 ||
        MUSIC_RIG_RESULT_UNSUPPORTED != 1 ||
        MUSIC_RIG_RESULT_INVALID_ARGUMENT != 2 ||
        MUSIC_RIG_RESULT_INVALID_STATE != 3 ||
        MUSIC_RIG_RESULT_ADAPTER_FAILURE != 4 ||
        MUSIC_RIG_RESULT_GENERATION_CONFLICT != 5) {
        fputs("result code contract mismatch\n", stderr);
        return 1;
    }

    return 0;
}

static int test_generation_slot(void)
{
    static const int initial_mapping = 10;
    static const int next_mapping = 20;
    const music_rig_generation initial = {
        UINT64_C(1),
        &initial_mapping
    };
    const music_rig_generation next = {
        UINT64_C(2),
        &next_mapping
    };
    music_rig_generation_slot slot;
    const music_rig_generation *adopted;

    if (music_rig_generation_slot_init(&slot, &initial) !=
        MUSIC_RIG_RESULT_OK) {
        fputs("generation slot initialization failed\n", stderr);
        return 1;
    }
    if (!music_rig_generation_slot_is_lock_free(&slot)) {
        fputs("generation pointer atomics are not lock-free\n", stderr);
        return 1;
    }
    if (music_rig_generation_slot_publish(&slot, &next) !=
        MUSIC_RIG_RESULT_OK) {
        fputs("generation publication failed\n", stderr);
        return 1;
    }

    adopted = music_rig_generation_slot_adopt(&slot);
    if (adopted != &next || adopted->mapping != &next_mapping) {
        fputs("generation adoption returned the wrong mapping\n", stderr);
        return 1;
    }
    if (music_rig_generation_slot_adopted(&slot) != &next) {
        fputs("adopted generation was not observable\n", stderr);
        return 1;
    }
    if (music_rig_generation_slot_publish(&slot, &initial) !=
        MUSIC_RIG_RESULT_GENERATION_CONFLICT) {
        fputs("stale generation was not rejected\n", stderr);
        return 1;
    }
    if (music_rig_generation_slot_init(NULL, &initial) !=
        MUSIC_RIG_RESULT_INVALID_ARGUMENT) {
        fputs("null slot was not rejected\n", stderr);
        return 1;
    }

    return 0;
}

int main(void)
{
    if (test_build_info() != 0) {
        return 1;
    }
    return test_generation_slot();
}
