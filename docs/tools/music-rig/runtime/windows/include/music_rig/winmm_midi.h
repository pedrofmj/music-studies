#ifndef MUSIC_RIG_WINMM_MIDI_H
#define MUSIC_RIG_WINMM_MIDI_H

#include "music_rig/arturia_control_state.h"
#include "music_rig/arturia_shared_sf2.h"
#include "music_rig/device_midi_shadow.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define MUSIC_RIG_WINMM_MIDI_ABI_VERSION UINT32_C(1)
#define MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY ((size_t)512)
#define MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY ((size_t)64)
#define MUSIC_RIG_WINMM_MIDI_LONG_MESSAGE_CAPACITY ((size_t)256)
#define MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT ((size_t)4)

typedef enum music_rig_winmm_midi_direction {
    MUSIC_RIG_WINMM_MIDI_INPUT = 0,
    MUSIC_RIG_WINMM_MIDI_OUTPUT = 1
} music_rig_winmm_midi_direction;

typedef enum music_rig_winmm_midi_input_kind {
    MUSIC_RIG_WINMM_MIDI_INPUT_SHORT = 0,
    MUSIC_RIG_WINMM_MIDI_INPUT_LONG = 1,
    MUSIC_RIG_WINMM_MIDI_INPUT_LONG_ERROR = 2
} music_rig_winmm_midi_input_kind;

typedef enum music_rig_winmm_midi_worker_kind {
    MUSIC_RIG_WINMM_MIDI_INPUT_WORKER = 0,
    MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER = 1
} music_rig_winmm_midi_worker_kind;

typedef struct music_rig_winmm_midi_endpoint {
    char slot[MUSIC_RIG_IDENTIFIER_CAPACITY];
    wchar_t input_interface[MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY];
    wchar_t output_interface[MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY];
} music_rig_winmm_midi_endpoint;

typedef void (*music_rig_winmm_midi_worker_fn)(void *argument);

typedef void (*music_rig_winmm_midi_input_callback_fn)(
    void *argument,
    music_rig_winmm_midi_input_kind kind,
    uint32_t packed_short_message,
    const uint8_t *long_message,
    size_t long_message_size,
    uint32_t long_buffer_token,
    uint64_t qpc_ns
);

/* All operations except the MIDI callback run on control or worker threads. */
typedef struct music_rig_winmm_midi_ops {
    music_rig_result (*device_count)(
        void *context,
        music_rig_winmm_midi_direction direction,
        uint32_t *count
    );
    music_rig_result (*device_interface)(
        void *context,
        music_rig_winmm_midi_direction direction,
        uint32_t transient_index,
        wchar_t *interface_path,
        size_t interface_capacity
    );
    music_rig_result (*input_open)(
        void *context,
        uint32_t transient_index,
        music_rig_winmm_midi_input_callback_fn callback,
        void *callback_argument,
        void **handle
    );
    music_rig_result (*output_open)(
        void *context,
        uint32_t transient_index,
        void **handle
    );
    music_rig_result (*input_start)(void *context, void *handle);
    music_rig_result (*input_stop)(void *context, void *handle);
    music_rig_result (*input_requeue_long)(
        void *context,
        void *handle,
        uint32_t long_buffer_token
    );
    music_rig_result (*input_close)(void *context, void *handle);
    music_rig_result (*output_send_short)(
        void *context,
        void *handle,
        uint32_t packed_short_message
    );
    music_rig_result (*output_close)(void *context, void *handle);
    music_rig_result (*worker_start)(
        void *context,
        music_rig_winmm_midi_worker_kind kind,
        music_rig_winmm_midi_worker_fn worker,
        void *worker_argument,
        void **thread
    );
    void (*worker_signal)(
        void *context,
        music_rig_winmm_midi_worker_kind kind
    );
    void (*worker_wait)(
        void *context,
        music_rig_winmm_midi_worker_kind kind
    );
    music_rig_result (*worker_join)(
        void *context,
        music_rig_winmm_midi_worker_kind kind,
        void *thread
    );
} music_rig_winmm_midi_ops;

typedef struct music_rig_winmm_midi_event {
    uint64_t qpc_ns;
    uint32_t packed_short_message;
    uint32_t long_buffer_token;
    uint16_t message_size;
    uint8_t kind;
    uint8_t message[MUSIC_RIG_WINMM_MIDI_LONG_MESSAGE_CAPACITY];
} music_rig_winmm_midi_event;

typedef struct music_rig_winmm_midi_input_ring {
    _Atomic size_t head;
    _Atomic size_t tail;
    _Atomic uint32_t pending_long_buffers;
    music_rig_winmm_midi_event events[MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY];
} music_rig_winmm_midi_input_ring;

typedef struct music_rig_winmm_midi_output_event {
    size_t slot_index;
    uint8_t message[3];
} music_rig_winmm_midi_output_event;

typedef struct music_rig_winmm_midi_output_ring {
    _Atomic size_t head;
    _Atomic size_t tail;
    music_rig_winmm_midi_output_event events[
        MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY
    ];
} music_rig_winmm_midi_output_ring;

typedef struct music_rig_winmm_midi music_rig_winmm_midi;

typedef struct music_rig_winmm_midi_callback_context {
    music_rig_winmm_midi *host;
    size_t slot_index;
} music_rig_winmm_midi_callback_context;

typedef struct music_rig_winmm_midi_metrics {
    uint64_t input_events;
    uint64_t input_overflows;
    uint64_t malformed_input;
    uint64_t callback_errors;
    uint64_t output_events;
    uint64_t output_overflows;
    uint64_t output_failures;
} music_rig_winmm_midi_metrics;

struct music_rig_winmm_midi {
    uint32_t abi_version;
    const music_rig_winmm_midi_ops *ops;
    void *ops_context;
    music_rig_device_midi_shadow *shadow;
    music_rig_winmm_midi_endpoint endpoints[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    music_rig_winmm_midi_callback_context callback_contexts[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    void *input_handles[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    void *output_handles[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    bool input_started[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
    music_rig_winmm_midi_input_ring input_rings[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    music_rig_winmm_midi_output_ring generated_output;
    music_rig_winmm_midi_output_ring api_output;
    _Atomic uint8_t arturia_control_values[MUSIC_RIG_ARTURIA_CONTROL_COUNT];
    void *input_thread;
    void *output_thread;
    size_t endpoint_count;
    _Atomic bool active;
    _Atomic bool input_stopping;
    _Atomic bool output_stopping;
    _Atomic bool callbacks_enabled;
    _Atomic uint32_t callbacks_inflight;
    _Atomic uint64_t input_events;
    _Atomic uint64_t input_overflows;
    _Atomic uint64_t malformed_input;
    _Atomic uint64_t callback_errors;
    _Atomic uint64_t output_events;
    _Atomic uint64_t output_overflows;
    _Atomic uint64_t output_failures;
    bool input_worker_started;
    bool output_worker_started;
};

music_rig_result music_rig_winmm_midi_init(
    music_rig_winmm_midi *host,
    const music_rig_winmm_midi_ops *ops,
    void *ops_context,
    const music_rig_winmm_midi_endpoint *endpoints,
    size_t endpoint_count
);

music_rig_result music_rig_winmm_midi_observer_init(
    music_rig_winmm_midi *host,
    music_rig_device_midi_shadow_observer *observer
);

music_rig_result music_rig_winmm_midi_attach_shadow(
    music_rig_winmm_midi *host,
    music_rig_device_midi_shadow *shadow
);

music_rig_result music_rig_winmm_midi_start(music_rig_winmm_midi *host);
music_rig_result music_rig_winmm_midi_stop(music_rig_winmm_midi *host);

/* Serialize API production and host start/stop with the caller's control loop. */
/* These calls only enqueue channel-voice short MIDI; the output worker sends. */
music_rig_result music_rig_winmm_midi_send(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const uint8_t *message,
    size_t message_size
);

music_rig_result music_rig_winmm_midi_send_arturia_state(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const music_rig_arturia_control_state *state
);

music_rig_result music_rig_winmm_midi_arturia_state_read(
    const music_rig_winmm_midi *host,
    music_rig_arturia_control_state *state
);

music_rig_result music_rig_winmm_midi_send_shared_sf2_plan(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const music_rig_arturia_shared_sf2_plan *plan,
    const uint8_t fader_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    const uint8_t knob_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT]
);

/* Deterministic worker steps are also used by the no-device seam tests. */
size_t music_rig_winmm_midi_drain_input(music_rig_winmm_midi *host);
size_t music_rig_winmm_midi_drain_output(music_rig_winmm_midi *host);
void music_rig_winmm_midi_input_callback(
    void *argument,
    music_rig_winmm_midi_input_kind kind,
    uint32_t packed_short_message,
    const uint8_t *long_message,
    size_t long_message_size,
    uint32_t long_buffer_token,
    uint64_t qpc_ns
);
music_rig_winmm_midi_metrics music_rig_winmm_midi_metrics_read(
    const music_rig_winmm_midi *host
);

#endif
