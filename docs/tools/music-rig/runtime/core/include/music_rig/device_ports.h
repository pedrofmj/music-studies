#ifndef MUSIC_RIG_DEVICE_PORTS_H
#define MUSIC_RIG_DEVICE_PORTS_H

#include "music_rig/compiled_tables.h"

#include <stdbool.h>
#include <stddef.h>

#define MUSIC_RIG_DEVICE_PORT_ID_CAPACITY ((size_t)96)
#define MUSIC_RIG_DEVICE_PORT_CAPACITY \
    (MUSIC_RIG_DEVICE_PROFILE_CAPACITY * (size_t)2)

typedef enum music_rig_device_port_direction {
    MUSIC_RIG_DEVICE_PORT_DIRECTION_INVALID = 0,
    MUSIC_RIG_DEVICE_PORT_DIRECTION_INPUT = 1,
    MUSIC_RIG_DEVICE_PORT_DIRECTION_OUTPUT = 2
} music_rig_device_port_direction;

typedef struct music_rig_device_port {
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    char id[MUSIC_RIG_DEVICE_PORT_ID_CAPACITY];
    music_rig_device_port_direction direction;
} music_rig_device_port;

typedef struct music_rig_device_port_catalogue {
    size_t count;
    music_rig_device_port ports[MUSIC_RIG_DEVICE_PORT_CAPACITY];
} music_rig_device_port_catalogue;

music_rig_result music_rig_device_port_catalogue_build(
    const music_rig_compiled_tables *tables,
    music_rig_device_port_catalogue *catalogue
);

const music_rig_device_port *music_rig_device_port_lookup(
    const music_rig_device_port_catalogue *catalogue,
    const char *slot,
    music_rig_device_port_direction direction
);

bool music_rig_device_port_catalogues_match(
    const music_rig_device_port_catalogue *left,
    const music_rig_device_port_catalogue *right
);

#endif
