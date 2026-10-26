#ifndef MUSIC_RIG_WIN32_CARLA_HOST_H
#define MUSIC_RIG_WIN32_CARLA_HOST_H

#include "music_rig/arturia_shared_sf2.h"
#include "music_rig/core.h"

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define MUSIC_RIG_WIN32_CARLA_MIDI_QUEUE_CAPACITY ((size_t)64)

#if !defined(_WIN32)
#error "music_rig/win32_carla_host.h is Windows-only"
#endif

typedef struct music_rig_win32_carla_host music_rig_win32_carla_host;

typedef music_rig_result (*music_rig_win32_carla_raw_midi_output_fn)(
    void *context,
    const uint8_t *message,
    size_t message_size
);

typedef struct music_rig_win32_carla_host_options {
    const char *carla_dll_path;
    const char *project_path;
    const char *driver_name;
    const char *client_name;
    const char *midi_client_name;
    const char *midi_port_name;
    /* Required; enqueue to WinMM's configured loopback output, do not block. */
    music_rig_win32_carla_raw_midi_output_fn raw_midi_output;
    void *raw_midi_output_context;
    const char *const *patch_names;
    uint16_t patch_count;
    const uint16_t *patch_indexes;
    uint16_t catalog_patch_count;
    const uint8_t *active_layers;
    size_t active_layer_count;
    uint8_t initial_master_volume;
} music_rig_win32_carla_host_options;

typedef struct music_rig_win32_carla_profile_map {
    uint8_t management_pad_code;
    uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    uint8_t active_layers[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT];
    size_t active_layer_count;
} music_rig_win32_carla_profile_map;

/* Loads the DLL and verifies required exports without starting its engine. */
music_rig_result music_rig_win32_carla_host_probe_library(
    const wchar_t *carla_dll_path,
    char *error_message,
    size_t error_capacity
);

/* All string inputs are UTF-8. Carla calls must be serialized on the opening
 * thread; call idle regularly there. error_message may be NULL or truncated. */
music_rig_result music_rig_win32_carla_host_open(
    music_rig_win32_carla_host **host,
    const music_rig_win32_carla_host_options *options,
    char *error_message,
    size_t error_capacity
);

music_rig_result music_rig_win32_carla_host_idle(
    music_rig_win32_carla_host *host
);

music_rig_result music_rig_win32_carla_host_select_profile(
    music_rig_win32_carla_host *host,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
);

music_rig_result music_rig_win32_carla_host_process_midi(
    music_rig_win32_carla_host *host,
    const uint8_t *message,
    size_t message_size
);

/* The single producer may enqueue MIDI and profile events from the WinMM input
 * worker. Only supported channel-1 notes, CC, and pitch bend are retained;
 * other traffic is ignored. Stop and join that producer before closing Carla. */
music_rig_result music_rig_win32_carla_host_enqueue_midi(
    music_rig_win32_carla_host *host,
    const uint8_t *message,
    size_t message_size
);

/* Enqueue a validated profile event from the same producer as MIDI. Profile
 * events share its FIFO and are applied only by the owner-thread drain. */
music_rig_result music_rig_win32_carla_host_enqueue_profile(
    music_rig_win32_carla_host *host,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
);

/* Drain on the Carla owner thread. Messages are consumed in FIFO order. */
music_rig_result music_rig_win32_carla_host_drain_midi(
    music_rig_win32_carla_host *host,
    size_t *message_count
);

uint64_t music_rig_win32_carla_host_midi_queue_overflows(
    const music_rig_win32_carla_host *host
);

const char *music_rig_win32_carla_host_last_error(
    const music_rig_win32_carla_host *host
);

music_rig_result music_rig_win32_carla_host_close(
    music_rig_win32_carla_host **host
);

#endif
