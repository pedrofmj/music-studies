#ifndef MUSIC_RIG_WIN32_CARLA_RUNTIME_H
#define MUSIC_RIG_WIN32_CARLA_RUNTIME_H

#include "music_rig/win32_carla_host.h"
#include "music_rig/winmm_midi.h"

#include <stddef.h>
#include <stdint.h>

#if !defined(_WIN32)
#error "music_rig/win32_carla_runtime.h is Windows-only"
#endif

typedef struct music_rig_win32_carla_runtime music_rig_win32_carla_runtime;

#define MUSIC_RIG_WIN32_CARLA_RUNTIME_PROFILE_CAPACITY ((size_t)16)

/* Management codes match Android ArturiaPadModes: 1..16 are Bank A pads 1..8,
 * then Bank B pads 1..8. Code 2 (Bank A Pad 2) is unsupported. Each profile
 * plan uses the Carla options' catalog_patch_count. Profile maps are copied
 * during open and validated before WinMM is initialized.
 *
 * Endpoint input paths must name the physical Arturia input and the selected
 * output path must name the dedicated Windows MIDI Services Basic Loopback
 * output. Carla's MIDI client/port names must identify that loopback input.
 * If startup fails and WinMM cleanup remains pending, runtime is non-NULL and
 * must be passed to close for cleanup retry. */
music_rig_result music_rig_win32_carla_runtime_open(
    music_rig_win32_carla_runtime **runtime,
    const music_rig_winmm_midi_ops *winmm_ops,
    void *winmm_ops_context,
    const music_rig_winmm_midi_endpoint *endpoints,
    size_t endpoint_count,
    const music_rig_win32_carla_profile_map *pad_profiles,
    size_t pad_profile_count,
    const music_rig_win32_carla_host_options *carla_options,
    const char *loopback_output_slot,
    char *error_message,
    size_t error_capacity
);

/* Pump and profile selection are restricted to the thread that opened Carla.
 * Pump idles Carla, then drains queued MIDI and profile events in FIFO order. */
music_rig_result music_rig_win32_carla_runtime_idle(
    music_rig_win32_carla_runtime *runtime
);
music_rig_result music_rig_win32_carla_runtime_pump(
    music_rig_win32_carla_runtime *runtime,
    size_t *drained_message_count
);
music_rig_result music_rig_win32_carla_runtime_select_profile(
    music_rig_win32_carla_runtime *runtime,
    const uint16_t patch_indexes[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    uint16_t catalog_patch_count,
    const uint8_t *active_layers,
    size_t active_layer_count
);

/* Stops and joins WinMM before closing Carla. If WinMM resources cannot be
 * released, the runtime remains in closing state and close may be retried. */
music_rig_result music_rig_win32_carla_runtime_close(
    music_rig_win32_carla_runtime **runtime
);

#endif
