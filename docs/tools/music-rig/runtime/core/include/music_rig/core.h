#ifndef MUSIC_RIG_CORE_H
#define MUSIC_RIG_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>

#define MUSIC_RIG_CORE_VERSION "0.1.0"
#define MUSIC_RIG_PROTOCOL_VERSION UINT32_C(2)
#define MUSIC_RIG_PROFILE_SCHEMA_VERSION UINT32_C(1)
#define MUSIC_RIG_RETIRED_GENERATION_CAPACITY ((size_t)8)

typedef enum music_rig_result {
    MUSIC_RIG_RESULT_OK = 0,
    MUSIC_RIG_RESULT_UNSUPPORTED = 1,
    MUSIC_RIG_RESULT_INVALID_ARGUMENT = 2,
    MUSIC_RIG_RESULT_INVALID_STATE = 3,
    MUSIC_RIG_RESULT_ADAPTER_FAILURE = 4,
    MUSIC_RIG_RESULT_GENERATION_CONFLICT = 5,
    MUSIC_RIG_RESULT_NOT_FOUND = 6,
    MUSIC_RIG_RESULT_INVALID_DATA = 7,
    MUSIC_RIG_RESULT_BUFFER_TOO_SMALL = 8
} music_rig_result;

typedef struct music_rig_build_info {
    const char *core_version;
    uint32_t protocol_version;
    uint32_t profile_schema_version;
} music_rig_build_info;

typedef struct music_rig_generation {
    uint64_t id;
    const void *mapping;
} music_rig_generation;

/*
 * Generation storage is owned by the caller. Published generations must remain
 * alive until the control thread has observed adoption and completed reclamation.
 */
typedef struct music_rig_generation_slot {
    _Atomic(const music_rig_generation *) published;
    _Atomic(const music_rig_generation *) adopted;
    const music_rig_generation *retired[MUSIC_RIG_RETIRED_GENERATION_CAPACITY];
    size_t retired_head;
    size_t retired_count;
} music_rig_generation_slot;

const music_rig_build_info *music_rig_get_build_info(void);

music_rig_result music_rig_generation_slot_init(
    music_rig_generation_slot *slot,
    const music_rig_generation *initial_generation
);

bool music_rig_generation_slot_is_lock_free(
    const music_rig_generation_slot *slot
);

music_rig_result music_rig_generation_slot_publish(
    music_rig_generation_slot *slot,
    const music_rig_generation *next_generation
);

const music_rig_generation *music_rig_generation_slot_adopt(
    music_rig_generation_slot *slot
);

const music_rig_generation *music_rig_generation_slot_adopted(
    const music_rig_generation_slot *slot
);

/*
 * Called only by the publishing control thread. A returned generation is no
 * longer observable by the real-time adopter and its caller-owned storage may
 * be reused. NULL means adoption has not advanced far enough.
 */
const music_rig_generation *music_rig_generation_slot_reclaim(
    music_rig_generation_slot *slot
);

size_t music_rig_generation_slot_retired_count(
    const music_rig_generation_slot *slot
);

#endif
