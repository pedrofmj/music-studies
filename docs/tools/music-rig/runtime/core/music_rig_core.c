#include "music_rig/core.h"

#include <stddef.h>
#include <string.h>

static const music_rig_build_info BUILD_INFO = {
    MUSIC_RIG_CORE_VERSION,
    MUSIC_RIG_PROTOCOL_VERSION,
    MUSIC_RIG_PROFILE_SCHEMA_VERSION
};

const music_rig_build_info *music_rig_get_build_info(void)
{
    return &BUILD_INFO;
}

music_rig_result music_rig_generation_slot_init(
    music_rig_generation_slot *slot,
    const music_rig_generation *initial_generation
)
{
    if (slot == NULL || initial_generation == NULL ||
        initial_generation->id == UINT64_C(0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }

    atomic_init(&slot->published, initial_generation);
    atomic_init(&slot->adopted, initial_generation);
    memset(slot->retired, 0, sizeof(slot->retired));
    slot->retired_head = 0U;
    slot->retired_count = 0U;
    return MUSIC_RIG_RESULT_OK;
}

bool music_rig_generation_slot_is_lock_free(
    const music_rig_generation_slot *slot
)
{
    if (slot == NULL) {
        return false;
    }

    return atomic_is_lock_free(&slot->published) &&
        atomic_is_lock_free(&slot->adopted);
}

music_rig_result music_rig_generation_slot_publish(
    music_rig_generation_slot *slot,
    const music_rig_generation *next_generation
)
{
    const music_rig_generation *current_generation;

    if (slot == NULL || next_generation == NULL ||
        next_generation->id == UINT64_C(0)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    current_generation = atomic_load_explicit(
        &slot->published,
        memory_order_relaxed
    );
    if (next_generation->id <= current_generation->id) {
        return MUSIC_RIG_RESULT_GENERATION_CONFLICT;
    }
    if (slot->retired_count == MUSIC_RIG_RETIRED_GENERATION_CAPACITY) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }

    atomic_store_explicit(
        &slot->published,
        next_generation,
        memory_order_release
    );
    slot->retired[(slot->retired_head + slot->retired_count) %
        MUSIC_RIG_RETIRED_GENERATION_CAPACITY] = current_generation;
    slot->retired_count += 1U;
    return MUSIC_RIG_RESULT_OK;
}

const music_rig_generation *music_rig_generation_slot_adopt(
    music_rig_generation_slot *slot
)
{
    const music_rig_generation *generation;

    if (slot == NULL) {
        return NULL;
    }

    generation = atomic_load_explicit(&slot->published, memory_order_acquire);
    atomic_store_explicit(&slot->adopted, generation, memory_order_release);
    return generation;
}

const music_rig_generation *music_rig_generation_slot_adopted(
    const music_rig_generation_slot *slot
)
{
    if (slot == NULL) {
        return NULL;
    }

    return atomic_load_explicit(&slot->adopted, memory_order_acquire);
}

const music_rig_generation *music_rig_generation_slot_reclaim(
    music_rig_generation_slot *slot
)
{
    const music_rig_generation *adopted;
    const music_rig_generation *retired;

    if (slot == NULL || slot->retired_count == 0U) {
        return NULL;
    }

    adopted = atomic_load_explicit(&slot->adopted, memory_order_acquire);
    retired = slot->retired[slot->retired_head];
    if (adopted == NULL || retired == NULL || retired->id >= adopted->id) {
        return NULL;
    }

    slot->retired[slot->retired_head] = NULL;
    slot->retired_head = (slot->retired_head + 1U) %
        MUSIC_RIG_RETIRED_GENERATION_CAPACITY;
    slot->retired_count -= 1U;
    return retired;
}

size_t music_rig_generation_slot_retired_count(
    const music_rig_generation_slot *slot
)
{
    return slot == NULL ? 0U : slot->retired_count;
}
