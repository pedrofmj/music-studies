#ifndef MUSIC_RIG_JACK_MIDI_SHADOW_H
#define MUSIC_RIG_JACK_MIDI_SHADOW_H

#include "music_rig/device_midi_shadow.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>

#define MUSIC_RIG_JACK_MIDI_SHADOW_ABI_VERSION UINT32_C(1)

/* Caller-owned Linux JACK host. Read status only after stop. */
typedef struct music_rig_jack_midi_shadow {
    uint32_t abi_version;
    music_rig_device_midi_shadow *shadow;
    void *client;
    void *input_ports[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    size_t port_count;
    _Atomic music_rig_result last_process_result;
    _Atomic bool active;
    _Atomic bool backend_shutdown;
} music_rig_jack_midi_shadow;

music_rig_result music_rig_jack_midi_shadow_init(
    music_rig_jack_midi_shadow *host,
    music_rig_device_midi_shadow *shadow
);

music_rig_result music_rig_jack_midi_shadow_start(
    music_rig_jack_midi_shadow *host
);

music_rig_result music_rig_jack_midi_shadow_stop(
    music_rig_jack_midi_shadow *host
);

#endif
