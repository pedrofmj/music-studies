#include "music_rig/device_ports.h"

#include <string.h>

static const char PORT_PREFIX[] = "device.";
static const char INPUT_SUFFIX[] = ".midi-input";
static const char OUTPUT_SUFFIX[] = ".midi-output";

_Static_assert(
    sizeof(PORT_PREFIX) - 1U + MUSIC_RIG_IDENTIFIER_CAPACITY - 1U +
        sizeof(OUTPUT_SUFFIX) <= MUSIC_RIG_DEVICE_PORT_ID_CAPACITY,
    "device port ID capacity must hold the longest stable slot"
);

static bool bounded_slot(const char *slot, size_t *length)
{
    size_t index;

    if (slot == NULL || slot[0] == '\0') {
        return false;
    }
    for (index = 0U; index < MUSIC_RIG_IDENTIFIER_CAPACITY; ++index) {
        unsigned char value = (unsigned char)slot[index];

        if (value == '\0') {
            *length = index;
            return true;
        }
        if (!((value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
              (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
              value == (unsigned char)'-')) {
            return false;
        }
    }
    return false;
}

static music_rig_result initialize_port(
    music_rig_device_port *port,
    const char *slot,
    size_t slot_length,
    music_rig_device_port_direction direction
)
{
    const char *suffix = direction == MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT
        ? INPUT_SUFFIX
        : OUTPUT_SUFFIX;
    size_t prefix_length = sizeof(PORT_PREFIX) - 1U;
    size_t suffix_length = strlen(suffix);

    if (prefix_length + slot_length + suffix_length + 1U >
        sizeof(port->id)) {
        return MUSIC_RIG_RESULT_BUFFER_TOO_SMALL;
    }

    memcpy(port->slot, slot, slot_length + 1U);
    memcpy(port->id, PORT_PREFIX, prefix_length);
    memcpy(port->id + prefix_length, slot, slot_length);
    memcpy(port->id + prefix_length + slot_length, suffix,
        suffix_length + 1U);
    port->direction = direction;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_device_port_catalogue_build(
    const music_rig_compiled_tables *tables,
    music_rig_device_port_catalogue *catalogue
)
{
    size_t index;
    music_rig_result result;

    if (tables == NULL || catalogue == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    result = music_rig_compiled_tables_validate(
        tables,
        tables->device_profile_count,
        tables->mapping_count,
        tables->target_binding_count,
        tables->ownership_count
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    memset(catalogue, 0, sizeof(*catalogue));

    for (index = 0U; index < tables->device_profile_count; ++index) {
        const char *slot = tables->device_profiles[index].slot;
        size_t slot_length = 0U;
        music_rig_result port_result;

        if (!bounded_slot(slot, &slot_length) ||
            strcmp(slot, tables->input_bindings[index].slot) != 0 ||
            (index != 0U && strcmp(
                tables->device_profiles[index - 1U].slot,
                slot
            ) >= 0)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        port_result = initialize_port(
            &catalogue->ports[index * 2U],
            slot,
            slot_length,
            MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT
        );
        if (port_result != MUSIC_RIG_RESULT_OK) {
            return port_result;
        }
        port_result = initialize_port(
            &catalogue->ports[index * 2U + 1U],
            slot,
            slot_length,
            MUSIC_RIG_DEVICE_PORT_DIRECTION_OUTPUT
        );
        if (port_result != MUSIC_RIG_RESULT_OK) {
            return port_result;
        }
    }
    catalogue->count = (size_t)tables->device_profile_count * 2U;
    return MUSIC_RIG_RESULT_OK;
}

const music_rig_device_port *music_rig_device_port_lookup(
    const music_rig_device_port_catalogue *catalogue,
    const char *slot,
    music_rig_device_port_direction direction
)
{
    size_t index;

    if (catalogue == NULL || slot == NULL ||
        direction < MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT ||
        direction > MUSIC_RIG_DEVICE_PORT_DIRECTION_OUTPUT ||
        catalogue->count > MUSIC_RIG_DEVICE_PORT_CAPACITY) {
        return NULL;
    }
    for (index = 0U; index < catalogue->count; ++index) {
        if (catalogue->ports[index].direction == direction &&
            strcmp(catalogue->ports[index].slot, slot) == 0) {
            return &catalogue->ports[index];
        }
    }
    return NULL;
}

bool music_rig_device_port_catalogues_match(
    const music_rig_device_port_catalogue *left,
    const music_rig_device_port_catalogue *right
)
{
    size_t index;

    if (left == NULL || right == NULL || left->count != right->count ||
        left->count > MUSIC_RIG_DEVICE_PORT_CAPACITY) {
        return false;
    }
    for (index = 0U; index < left->count; ++index) {
        if (left->ports[index].direction != right->ports[index].direction ||
            strcmp(left->ports[index].slot, right->ports[index].slot) != 0 ||
            strcmp(left->ports[index].id, right->ports[index].id) != 0) {
            return false;
        }
    }
    return true;
}
