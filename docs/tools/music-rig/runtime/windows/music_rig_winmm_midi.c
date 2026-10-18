#include "music_rig/winmm_midi.h"

#include <string.h>

#define WINMM_EVENT_MASK (MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY - 1U)

_Static_assert(
    (MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY & WINMM_EVENT_MASK) == 0U,
    "WinMM event ring capacity must be a power of two"
);

static bool valid_host(const music_rig_winmm_midi *host)
{
    return host != NULL &&
        host->abi_version == MUSIC_RIG_WINMM_MIDI_ABI_VERSION;
}

static bool string_terminated(const char *text, size_t capacity)
{
    size_t index;

    for (index = 0U; index < capacity; ++index) {
        if (text[index] == '\0') {
            return index != 0U;
        }
    }
    return false;
}

static bool wide_string_terminated(const wchar_t *text, size_t capacity)
{
    size_t index;

    for (index = 0U; index < capacity; ++index) {
        if (text[index] == L'\0') {
            return index != 0U;
        }
    }
    return false;
}

static bool valid_ops(const music_rig_winmm_midi_ops *ops)
{
    return ops != NULL && ops->device_count != NULL &&
        ops->device_interface != NULL && ops->input_open != NULL &&
        ops->output_open != NULL && ops->input_start != NULL &&
        ops->input_stop != NULL && ops->input_requeue_long != NULL &&
        ops->input_close != NULL && ops->output_send_short != NULL &&
        ops->output_close != NULL && ops->worker_start != NULL &&
        ops->worker_signal != NULL && ops->worker_wait != NULL &&
        ops->worker_join != NULL;
}

static void increment(_Atomic uint64_t *value)
{
    (void)atomic_fetch_add_explicit(value, UINT64_C(1), memory_order_relaxed);
}

static void mark_long_buffer_pending(
    music_rig_winmm_midi *host,
    music_rig_winmm_midi_input_ring *ring,
    uint32_t token
)
{
    if (token >= 32U) {
        increment(&host->callback_errors);
    } else {
        (void)atomic_fetch_or_explicit(
            &ring->pending_long_buffers, UINT32_C(1) << token,
            memory_order_release
        );
    }
}

static bool normalize_short(
    uint32_t packed,
    uint8_t message[3],
    size_t *message_size
)
{
    uint8_t status = (uint8_t)(packed & UINT32_C(0xff));
    uint8_t first = (uint8_t)((packed >> 8U) & UINT32_C(0xff));
    uint8_t second = (uint8_t)((packed >> 16U) & UINT32_C(0xff));
    uint8_t length;

    if (status < UINT8_C(0x80)) {
        return false;
    }
    if (status < UINT8_C(0xf0)) {
        uint8_t type = status & UINT8_C(0xf0);

        length = type == UINT8_C(0xc0) || type == UINT8_C(0xd0)
            ? UINT8_C(2) : UINT8_C(3);
    } else if (status == UINT8_C(0xf1) || status == UINT8_C(0xf3)) {
        length = UINT8_C(2);
    } else if (status == UINT8_C(0xf2)) {
        length = UINT8_C(3);
    } else {
        length = UINT8_C(1);
    }
    if ((length > UINT8_C(1) && first > UINT8_C(127)) ||
        (length > UINT8_C(2) && second > UINT8_C(127))) {
        return false;
    }
    message[0] = status;
    message[1] = first;
    message[2] = second;
    *message_size = (size_t)length;
    return true;
}

static bool input_ring_push(
    music_rig_winmm_midi_input_ring *ring,
    const music_rig_winmm_midi_event *event
)
{
    size_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);

    if (head - tail >= MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY) {
        return false;
    }
    ring->events[head & WINMM_EVENT_MASK] = *event;
    atomic_store_explicit(&ring->head, head + 1U, memory_order_release);
    return true;
}

static bool output_ring_push_batch(
    music_rig_winmm_midi_output_ring *ring,
    const music_rig_winmm_midi_output_event *events,
    size_t count
)
{
    size_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    size_t index;

    if (count > MUSIC_RIG_WINMM_MIDI_EVENT_CAPACITY - (head - tail)) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        ring->events[(head + index) & WINMM_EVENT_MASK] = events[index];
    }
    atomic_store_explicit(&ring->head, head + count, memory_order_release);
    return true;
}

static bool output_ring_pop(
    music_rig_winmm_midi_output_ring *ring,
    music_rig_winmm_midi_output_event *event
)
{
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&ring->head, memory_order_acquire);

    if (tail == head) {
        return false;
    }
    *event = ring->events[tail & WINMM_EVENT_MASK];
    atomic_store_explicit(&ring->tail, tail + 1U, memory_order_release);
    return true;
}

static bool input_has_events(const music_rig_winmm_midi_input_ring *ring)
{
    return atomic_load_explicit(&ring->tail, memory_order_relaxed) !=
        atomic_load_explicit(&ring->head, memory_order_acquire);
}

static music_rig_result find_interface(
    music_rig_winmm_midi *host,
    music_rig_winmm_midi_direction direction,
    const wchar_t *expected,
    uint32_t *matched_index
)
{
    uint32_t count = 0U;
    uint32_t index;
    bool found = false;
    music_rig_result result = host->ops->device_count(
        host->ops_context, direction, &count
    );

    if (result != MUSIC_RIG_RESULT_OK) {
        return result;
    }
    for (index = 0U; index < count; ++index) {
        wchar_t path[MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY] = {L'\0'};

        result = host->ops->device_interface(
            host->ops_context, direction, index, path,
            MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY
        );
        if (result == MUSIC_RIG_RESULT_NOT_FOUND) {
            continue;
        }
        if (result != MUSIC_RIG_RESULT_OK) {
            return result;
        }
        if (!wide_string_terminated(
                path, MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        if (wcscmp(path, expected) == 0) {
            if (found) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
            found = true;
            *matched_index = index;
        }
    }
    return found ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_NOT_FOUND;
}

static void generated_output(
    void *context,
    size_t slot_index,
    uint32_t frame,
    const uint8_t *message,
    size_t message_size
)
{
    music_rig_winmm_midi *host = context;
    music_rig_winmm_midi_output_event event;

    (void)frame;
    if (!valid_host(host) || slot_index >= host->endpoint_count ||
        message == NULL || message_size == 0U || message_size > 3U) {
        if (valid_host(host)) {
            increment(&host->output_failures);
        }
        return;
    }
    memset(&event, 0, sizeof(event));
    event.slot_index = slot_index;
    memcpy(event.message, message, message_size);
    if (!output_ring_push_batch(&host->generated_output, &event, 1U)) {
        increment(&host->output_overflows);
        return;
    }
    host->ops->worker_signal(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER
    );
}

void music_rig_winmm_midi_input_callback(
    void *argument,
    music_rig_winmm_midi_input_kind kind,
    uint32_t packed_short_message,
    const uint8_t *long_message,
    size_t long_message_size,
    uint32_t long_buffer_token,
    uint64_t qpc_ns
)
{
    music_rig_winmm_midi_callback_context *callback = argument;
    music_rig_winmm_midi *host;
    music_rig_winmm_midi_input_ring *ring;
    music_rig_winmm_midi_event event;
    uint8_t short_message[3];
    size_t short_size = 0U;

    if (callback == NULL || !valid_host(callback->host)) {
        return;
    }
    host = callback->host;
    if (callback->slot_index >= host->endpoint_count ||
        !atomic_load_explicit(
            &host->callbacks_enabled, memory_order_acquire)) {
        return;
    }
    (void)atomic_fetch_add_explicit(
        &host->callbacks_inflight, UINT32_C(1), memory_order_acq_rel
    );
    if (!atomic_load_explicit(
            &host->callbacks_enabled, memory_order_acquire)) {
        (void)atomic_fetch_sub_explicit(
            &host->callbacks_inflight, UINT32_C(1), memory_order_release
        );
        return;
    }
    ring = &host->input_rings[callback->slot_index];
    if (kind == MUSIC_RIG_WINMM_MIDI_INPUT_LONG_ERROR) {
        mark_long_buffer_pending(host, ring, long_buffer_token);
        increment(&host->callback_errors);
        goto callback_done;
    }
    memset(&event, 0, sizeof(event));
    event.qpc_ns = qpc_ns;
    event.kind = (uint8_t)kind;
    event.long_buffer_token = long_buffer_token;
    if (kind == MUSIC_RIG_WINMM_MIDI_INPUT_SHORT) {
        if (!normalize_short(
                packed_short_message, short_message, &short_size)) {
            increment(&host->malformed_input);
            goto callback_done;
        }
        event.packed_short_message = packed_short_message;
        event.message_size = (uint16_t)short_size;
        memcpy(event.message, short_message, short_size);
        if (short_message[0] == UINT8_C(0xb0) &&
            strcmp(host->endpoints[callback->slot_index].slot, "arturia-main") == 0) {
            int control_index = music_rig_arturia_control_state_index(
                short_message[1]
            );
            if (control_index >= 0) {
                atomic_store_explicit(
                    &host->arturia_control_values[(size_t)control_index],
                    short_message[2], memory_order_relaxed
                );
            }
        }
    } else if (kind == MUSIC_RIG_WINMM_MIDI_INPUT_LONG) {
        if ((long_message == NULL && long_message_size != 0U) ||
            long_message_size > MUSIC_RIG_WINMM_MIDI_LONG_MESSAGE_CAPACITY) {
            increment(&host->malformed_input);
            mark_long_buffer_pending(host, ring, long_buffer_token);
            goto callback_done;
        }
        if (long_message_size != 0U) {
            event.message_size = (uint16_t)long_message_size;
            memcpy(event.message, long_message, long_message_size);
        }
    } else {
        increment(&host->callback_errors);
        goto callback_done;
    }
    if (!input_ring_push(ring, &event)) {
        increment(&host->input_overflows);
    } else {
        increment(&host->input_events);
    }
    if (kind == MUSIC_RIG_WINMM_MIDI_INPUT_LONG) {
        mark_long_buffer_pending(host, ring, long_buffer_token);
    }
callback_done:
    (void)atomic_fetch_sub_explicit(
        &host->callbacks_inflight, UINT32_C(1), memory_order_release
    );
    host->ops->worker_signal(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
    );
}

size_t music_rig_winmm_midi_drain_input(music_rig_winmm_midi *host)
{
    size_t event_count = 0U;
    size_t slot_index;
    bool has_events = false;
    bool cycle_ready = true;

    if (!valid_host(host) || host->shadow == NULL) {
        return 0U;
    }
    for (slot_index = 0U; slot_index < host->endpoint_count; ++slot_index) {
        if (input_has_events(&host->input_rings[slot_index])) {
            has_events = true;
            break;
        }
    }
    if (has_events && music_rig_device_midi_shadow_begin_cycle(host->shadow) !=
            MUSIC_RIG_RESULT_OK) {
        increment(&host->callback_errors);
        cycle_ready = false;
    }
    for (slot_index = 0U; slot_index < host->endpoint_count; ++slot_index) {
        music_rig_winmm_midi_input_ring *ring = &host->input_rings[slot_index];
        size_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
        size_t head = atomic_load_explicit(&ring->head, memory_order_acquire);

        while (tail != head) {
            music_rig_winmm_midi_event *event =
                &ring->events[tail & WINMM_EVENT_MASK];
            music_rig_result result = cycle_ready
                ? music_rig_device_midi_shadow_process(
                    host->shadow, slot_index, 0U, event->message,
                    (size_t)event->message_size
                )
                : MUSIC_RIG_RESULT_ADAPTER_FAILURE;

            if (result == MUSIC_RIG_RESULT_INVALID_DATA) {
                increment(&host->malformed_input);
            } else if (result != MUSIC_RIG_RESULT_OK) {
                increment(&host->callback_errors);
            }
            ++tail;
            ++event_count;
            atomic_store_explicit(&ring->tail, tail, memory_order_release);
        }
        {
            uint32_t pending = atomic_exchange_explicit(
                &ring->pending_long_buffers, UINT32_C(0),
                memory_order_acq_rel
            );
            uint32_t token;

            for (token = 0U; token < 32U; ++token) {
                if ((pending & (UINT32_C(1) << token)) != 0U &&
                    host->input_handles[slot_index] != NULL &&
                    host->ops->input_requeue_long(
                        host->ops_context, host->input_handles[slot_index],
                        token
                    ) != MUSIC_RIG_RESULT_OK) {
                    increment(&host->callback_errors);
                }
            }
        }
    }
    return event_count;
}

size_t music_rig_winmm_midi_drain_output(music_rig_winmm_midi *host)
{
    size_t event_count = 0U;
    music_rig_winmm_midi_output_event event;

    if (!valid_host(host)) {
        return 0U;
    }
    while (output_ring_pop(&host->generated_output, &event)) {
        uint32_t packed;

        if (event.slot_index >= host->endpoint_count ||
            host->output_handles[event.slot_index] == NULL) {
            increment(&host->output_failures);
        } else {
            packed = (uint32_t)event.message[0] |
                ((uint32_t)event.message[1] << 8U) |
                ((uint32_t)event.message[2] << 16U);
            if (host->ops->output_send_short(
                    host->ops_context, host->output_handles[event.slot_index],
                    packed
                ) != MUSIC_RIG_RESULT_OK) {
                increment(&host->output_failures);
            } else {
                increment(&host->output_events);
            }
        }
        ++event_count;
    }
    while (output_ring_pop(&host->api_output, &event)) {
        uint32_t packed;

        if (event.slot_index >= host->endpoint_count ||
            host->output_handles[event.slot_index] == NULL) {
            increment(&host->output_failures);
        } else {
            packed = (uint32_t)event.message[0] |
                ((uint32_t)event.message[1] << 8U) |
                ((uint32_t)event.message[2] << 16U);
            if (host->ops->output_send_short(
                    host->ops_context, host->output_handles[event.slot_index],
                    packed
                ) != MUSIC_RIG_RESULT_OK) {
                increment(&host->output_failures);
            } else {
                increment(&host->output_events);
            }
        }
        ++event_count;
    }
    return event_count;
}

static bool input_work_pending(const music_rig_winmm_midi *host)
{
    size_t index;

    if (atomic_load_explicit(
            &host->callbacks_inflight, memory_order_acquire) != 0U) {
        return true;
    }
    for (index = 0U; index < host->endpoint_count; ++index) {
        const music_rig_winmm_midi_input_ring *ring = &host->input_rings[index];

        if (input_has_events(ring) || atomic_load_explicit(
                &ring->pending_long_buffers, memory_order_acquire) != 0U) {
            return true;
        }
    }
    return false;
}

static bool output_work_pending(const music_rig_winmm_midi *host)
{
    return atomic_load_explicit(&host->generated_output.tail,
               memory_order_relaxed) != atomic_load_explicit(
                   &host->generated_output.head, memory_order_acquire) ||
        atomic_load_explicit(&host->api_output.tail, memory_order_relaxed) !=
            atomic_load_explicit(&host->api_output.head, memory_order_acquire);
}

static void input_worker(void *argument)
{
    music_rig_winmm_midi *host = argument;

    for (;;) {
        size_t drained = music_rig_winmm_midi_drain_input(host);
        bool stopping = atomic_load_explicit(
            &host->input_stopping, memory_order_acquire
        );

        if (stopping && !input_work_pending(host)) {
            break;
        }
        if (drained == 0U) {
            host->ops->worker_wait(
                host->ops_context, MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
            );
        }
    }
}

static void output_worker(void *argument)
{
    music_rig_winmm_midi *host = argument;

    for (;;) {
        size_t drained = music_rig_winmm_midi_drain_output(host);
        bool stopping = atomic_load_explicit(
            &host->output_stopping, memory_order_acquire
        );

        if (stopping && !output_work_pending(host)) {
            break;
        }
        if (drained == 0U) {
            host->ops->worker_wait(
                host->ops_context, MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER
            );
        }
    }
}

static music_rig_result stop_workers_and_ports(music_rig_winmm_midi *host)
{
    music_rig_result result = MUSIC_RIG_RESULT_OK;
    bool input_worker_joined = true;
    bool output_worker_joined = true;
    size_t index;

    atomic_store_explicit(&host->active, false, memory_order_release);
    for (index = 0U; index < host->endpoint_count; ++index) {
        if (host->input_started[index]) {
            if (host->ops->input_stop(
                    host->ops_context, host->input_handles[index]
                ) != MUSIC_RIG_RESULT_OK) {
                result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            } else {
                host->input_started[index] = false;
            }
        }
    }
    atomic_store_explicit(
        &host->callbacks_enabled, false, memory_order_release
    );
    atomic_store_explicit(&host->input_stopping, true, memory_order_release);
    host->ops->worker_signal(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
    );
    if (host->input_worker_started) {
        if (host->ops->worker_join(
                host->ops_context, MUSIC_RIG_WINMM_MIDI_INPUT_WORKER,
                host->input_thread
            ) != MUSIC_RIG_RESULT_OK) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            input_worker_joined = false;
        } else {
            host->input_worker_started = false;
            host->input_thread = NULL;
        }
    }
    atomic_store_explicit(&host->output_stopping, true, memory_order_release);
    host->ops->worker_signal(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER
    );
    if (host->output_worker_started) {
        if (host->ops->worker_join(
                host->ops_context, MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER,
                host->output_thread
            ) != MUSIC_RIG_RESULT_OK) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            output_worker_joined = false;
        } else {
            host->output_worker_started = false;
            host->output_thread = NULL;
        }
    }
    for (index = 0U; index < host->endpoint_count; ++index) {
        if (input_worker_joined && host->input_handles[index] != NULL) {
            if (host->ops->input_close(
                    host->ops_context, host->input_handles[index]
                ) != MUSIC_RIG_RESULT_OK) {
                result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            } else {
                host->input_handles[index] = NULL;
            }
        }
        if (output_worker_joined && host->output_handles[index] != NULL) {
            if (host->ops->output_close(
                    host->ops_context, host->output_handles[index]
                ) != MUSIC_RIG_RESULT_OK) {
                result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            } else {
                host->output_handles[index] = NULL;
            }
        }
    }
    atomic_store_explicit(&host->active, false, memory_order_release);
    return result;
}

music_rig_result music_rig_winmm_midi_init(
    music_rig_winmm_midi *host,
    const music_rig_winmm_midi_ops *ops,
    void *ops_context,
    const music_rig_winmm_midi_endpoint *endpoints,
    size_t endpoint_count
)
{
    size_t index;

    if (host == NULL || !valid_ops(ops) || endpoints == NULL ||
        endpoint_count == 0U ||
        endpoint_count > MUSIC_RIG_DEVICE_PROFILE_CAPACITY) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < endpoint_count; ++index) {
        size_t earlier;

        if (!string_terminated(
                endpoints[index].slot, sizeof(endpoints[index].slot)) ||
            !wide_string_terminated(
                endpoints[index].input_interface,
                MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY) ||
            !wide_string_terminated(
                endpoints[index].output_interface,
                MUSIC_RIG_WINMM_MIDI_INTERFACE_CAPACITY)) {
            return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
        }
        for (earlier = 0U; earlier < index; ++earlier) {
            if (strcmp(endpoints[index].slot, endpoints[earlier].slot) == 0 ||
                wcscmp(endpoints[index].input_interface,
                    endpoints[earlier].input_interface) == 0 ||
                wcscmp(endpoints[index].output_interface,
                    endpoints[earlier].output_interface) == 0) {
                return MUSIC_RIG_RESULT_INVALID_DATA;
            }
        }
    }
    memset(host, 0, sizeof(*host));
    host->abi_version = MUSIC_RIG_WINMM_MIDI_ABI_VERSION;
    host->ops = ops;
    host->ops_context = ops_context;
    host->endpoint_count = endpoint_count;
    memcpy(host->endpoints, endpoints, endpoint_count * sizeof(*endpoints));
    atomic_init(&host->active, false);
    atomic_init(&host->input_stopping, false);
    atomic_init(&host->output_stopping, false);
    atomic_init(&host->callbacks_enabled, false);
    atomic_init(&host->callbacks_inflight, UINT32_C(0));
    atomic_init(&host->input_events, UINT64_C(0));
    atomic_init(&host->input_overflows, UINT64_C(0));
    atomic_init(&host->malformed_input, UINT64_C(0));
    atomic_init(&host->callback_errors, UINT64_C(0));
    atomic_init(&host->output_events, UINT64_C(0));
    atomic_init(&host->output_overflows, UINT64_C(0));
    atomic_init(&host->output_failures, UINT64_C(0));
    atomic_init(&host->generated_output.head, 0U);
    atomic_init(&host->generated_output.tail, 0U);
    atomic_init(&host->api_output.head, 0U);
    atomic_init(&host->api_output.tail, 0U);
    {
        music_rig_arturia_control_state defaults;

        music_rig_arturia_control_state_init(&defaults);
        for (index = 0U; index < MUSIC_RIG_ARTURIA_CONTROL_COUNT; ++index) {
            atomic_init(&host->arturia_control_values[index],
                defaults.values[index]);
        }
    }
    for (index = 0U; index < endpoint_count; ++index) {
        atomic_init(&host->input_rings[index].head, 0U);
        atomic_init(&host->input_rings[index].tail, 0U);
        atomic_init(&host->input_rings[index].pending_long_buffers, 0U);
        host->callback_contexts[index].host = host;
        host->callback_contexts[index].slot_index = index;
    }
    if (!atomic_is_lock_free(&host->active) ||
        !atomic_is_lock_free(&host->input_stopping) ||
        !atomic_is_lock_free(&host->output_stopping) ||
        !atomic_is_lock_free(&host->callbacks_enabled) ||
        !atomic_is_lock_free(&host->callbacks_inflight) ||
        !atomic_is_lock_free(&host->input_rings[0].head) ||
        !atomic_is_lock_free(&host->input_rings[0].tail) ||
        !atomic_is_lock_free(&host->input_rings[0].pending_long_buffers) ||
        !atomic_is_lock_free(&host->generated_output.head) ||
        !atomic_is_lock_free(&host->api_output.head) ||
        !atomic_is_lock_free(&host->arturia_control_values[0]) ||
        !atomic_is_lock_free(&host->input_events) ||
        !atomic_is_lock_free(&host->input_overflows) ||
        !atomic_is_lock_free(&host->malformed_input) ||
        !atomic_is_lock_free(&host->callback_errors) ||
        !atomic_is_lock_free(&host->output_events) ||
        !atomic_is_lock_free(&host->output_overflows) ||
        !atomic_is_lock_free(&host->output_failures)) {
        memset(host, 0, sizeof(*host));
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_observer_init(
    music_rig_winmm_midi *host,
    music_rig_device_midi_shadow_observer *observer
)
{
    if (!valid_host(host) || observer == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(observer, 0, sizeof(*observer));
    observer->abi_version = MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION;
    observer->context = host;
    observer->output_midi = generated_output;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_attach_shadow(
    music_rig_winmm_midi *host,
    music_rig_device_midi_shadow *shadow
)
{
    size_t index;

    if (!valid_host(host) || shadow == NULL ||
        atomic_load_explicit(&host->active, memory_order_acquire) ||
        music_rig_device_midi_shadow_slot_count(shadow) !=
            host->endpoint_count || shadow->observer.output_midi !=
                generated_output || shadow->observer.context != host ||
        shadow->observer.abi_version !=
            MUSIC_RIG_DEVICE_MIDI_SHADOW_OBSERVER_ABI_VERSION) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < host->endpoint_count; ++index) {
        const char *slot = music_rig_device_midi_shadow_slot_name(
            shadow, index
        );

        if (slot == NULL || strcmp(slot, host->endpoints[index].slot) != 0) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    host->shadow = shadow;
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_start(music_rig_winmm_midi *host)
{
    music_rig_result result;
    size_t index;

    if (!valid_host(host) || host->shadow == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (atomic_load_explicit(&host->active, memory_order_acquire) ||
        host->input_worker_started || host->output_worker_started) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    for (index = 0U; index < host->endpoint_count; ++index) {
        if (host->input_started[index] || host->input_handles[index] != NULL ||
            host->output_handles[index] != NULL) {
            return MUSIC_RIG_RESULT_INVALID_STATE;
        }
    }
    atomic_store_explicit(&host->input_stopping, false, memory_order_release);
    atomic_store_explicit(&host->output_stopping, false, memory_order_release);
    for (index = 0U; index < host->endpoint_count; ++index) {
        uint32_t input_index = 0U;
        uint32_t output_index = 0U;

        result = find_interface(host, MUSIC_RIG_WINMM_MIDI_INPUT,
            host->endpoints[index].input_interface, &input_index);
        if (result == MUSIC_RIG_RESULT_OK) {
            result = find_interface(host, MUSIC_RIG_WINMM_MIDI_OUTPUT,
                host->endpoints[index].output_interface, &output_index);
        }
        if (result != MUSIC_RIG_RESULT_OK) {
            (void)stop_workers_and_ports(host);
            return result;
        }
        result = host->ops->input_open(
            host->ops_context, input_index, music_rig_winmm_midi_input_callback,
            &host->callback_contexts[index], &host->input_handles[index]
        );
        if (result == MUSIC_RIG_RESULT_OK) {
            result = host->ops->output_open(
                host->ops_context, output_index, &host->output_handles[index]
            );
        }
        if (result != MUSIC_RIG_RESULT_OK) {
            (void)stop_workers_and_ports(host);
            return result;
        }
    }
    result = host->ops->worker_start(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_INPUT_WORKER, input_worker,
        host, &host->input_thread
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)stop_workers_and_ports(host);
        return result;
    }
    host->input_worker_started = true;
    result = host->ops->worker_start(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER, output_worker,
        host, &host->output_thread
    );
    if (result != MUSIC_RIG_RESULT_OK) {
        (void)stop_workers_and_ports(host);
        return result;
    }
    host->output_worker_started = true;
    atomic_store_explicit(
        &host->callbacks_enabled, true, memory_order_release
    );
    for (index = 0U; index < host->endpoint_count; ++index) {
        result = host->ops->input_start(
            host->ops_context, host->input_handles[index]
        );
        if (result != MUSIC_RIG_RESULT_OK) {
            (void)stop_workers_and_ports(host);
            return result;
        }
        host->input_started[index] = true;
    }
    atomic_store_explicit(&host->active, true, memory_order_release);
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_stop(music_rig_winmm_midi *host)
{
    size_t index;
    bool has_resources = false;

    if (!valid_host(host)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < host->endpoint_count; ++index) {
        if (host->input_started[index] || host->input_handles[index] != NULL ||
            host->output_handles[index] != NULL) {
            has_resources = true;
            break;
        }
    }
    if (!atomic_load_explicit(&host->active, memory_order_acquire) &&
        !host->input_worker_started && !host->output_worker_started &&
        !has_resources) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    return stop_workers_and_ports(host);
}

static music_rig_result send_batch(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const uint8_t *messages,
    size_t message_count
)
{
    music_rig_winmm_midi_output_event events[
        MUSIC_RIG_ARTURIA_CONTROL_COUNT
    ];
    size_t index;

    if (!valid_host(host) || !atomic_load_explicit(
            &host->active, memory_order_acquire) ||
        slot_index >= host->endpoint_count || messages == NULL ||
        message_count == 0U ||
        message_count > MUSIC_RIG_ARTURIA_CONTROL_COUNT) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(events, 0, message_count * sizeof(events[0]));
    for (index = 0U; index < message_count; ++index) {
        const uint8_t *message = &messages[index * 3U];

        if (message[0] < UINT8_C(0x80) || message[0] >= UINT8_C(0xf0) ||
            message[1] > UINT8_C(127) || message[2] > UINT8_C(127)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        events[index].slot_index = slot_index;
        memcpy(events[index].message, message, 3U);
    }
    if (!output_ring_push_batch(&host->api_output, events, message_count)) {
        increment(&host->output_overflows);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    host->ops->worker_signal(
        host->ops_context, MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER
    );
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_send(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const uint8_t *message,
    size_t message_size
)
{
    uint8_t batch[1][3] = {{0U, 0U, 0U}};

    if (message == NULL || message_size == 0U || message_size > 3U) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memcpy(batch[0], message, message_size);
    if (message_size < 3U) {
        batch[0][2] = 0U;
    }
    return send_batch(host, slot_index, &batch[0][0], 1U);
}

music_rig_result music_rig_winmm_midi_send_arturia_state(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const music_rig_arturia_control_state *state
)
{
    uint8_t messages[MUSIC_RIG_ARTURIA_CONTROL_COUNT][3];
    size_t index;

    if (state == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MUSIC_RIG_ARTURIA_CONTROL_COUNT; ++index) {
        if (!music_rig_arturia_control_state_message(
                state, index, messages[index])) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
    }
    return send_batch(host, slot_index, &messages[0][0],
        MUSIC_RIG_ARTURIA_CONTROL_COUNT);
}

music_rig_result music_rig_winmm_midi_arturia_state_read(
    const music_rig_winmm_midi *host,
    music_rig_arturia_control_state *state
)
{
    size_t index;

    if (!valid_host(host) || state == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    music_rig_arturia_control_state_init(state);
    for (index = 0U; index < MUSIC_RIG_ARTURIA_CONTROL_COUNT; ++index) {
        state->values[index] = atomic_load_explicit(
            &host->arturia_control_values[index], memory_order_relaxed
        );
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_send_shared_sf2_plan(
    music_rig_winmm_midi *host,
    size_t slot_index,
    const music_rig_arturia_shared_sf2_plan *plan,
    const uint8_t fader_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT],
    const uint8_t knob_values[MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT]
)
{
    uint8_t messages[MUSIC_RIG_ARTURIA_CONTROL_COUNT][3];
    size_t route_index;

    if (plan == NULL || fader_values == NULL || knob_values == NULL ||
        plan->route_count == 0U ||
        plan->route_count > MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (route_index = 0U; route_index < plan->route_count; ++route_index) {
        const music_rig_arturia_shared_sf2_route *route =
            &plan->routes[route_index];
        size_t value_index;

        if (route->layer == 0U ||
            route->layer > MUSIC_RIG_ARTURIA_SHARED_SF2_LAYER_COUNT ||
            route->fader_cc > UINT8_C(127) ||
            route->knob_cc > UINT8_C(127)) {
            return MUSIC_RIG_RESULT_INVALID_DATA;
        }
        value_index = (size_t)route->layer - 1U;
        messages[route_index * 2U][0] = UINT8_C(0xb0);
        messages[route_index * 2U][1] = route->fader_cc;
        messages[route_index * 2U][2] = fader_values[value_index];
        messages[route_index * 2U + 1U][0] = UINT8_C(0xb0);
        messages[route_index * 2U + 1U][1] = route->knob_cc;
        messages[route_index * 2U + 1U][2] = knob_values[value_index];
    }
    return send_batch(host, slot_index, &messages[0][0],
        plan->route_count * 2U);
}

music_rig_winmm_midi_metrics music_rig_winmm_midi_metrics_read(
    const music_rig_winmm_midi *host
)
{
    music_rig_winmm_midi_metrics metrics = {0U, 0U, 0U, 0U, 0U, 0U, 0U};

    if (!valid_host(host)) {
        return metrics;
    }
    metrics.input_events = atomic_load_explicit(
        &host->input_events, memory_order_relaxed
    );
    metrics.input_overflows = atomic_load_explicit(
        &host->input_overflows, memory_order_relaxed
    );
    metrics.malformed_input = atomic_load_explicit(
        &host->malformed_input, memory_order_relaxed
    );
    metrics.callback_errors = atomic_load_explicit(
        &host->callback_errors, memory_order_relaxed
    );
    metrics.output_events = atomic_load_explicit(
        &host->output_events, memory_order_relaxed
    );
    metrics.output_overflows = atomic_load_explicit(
        &host->output_overflows, memory_order_relaxed
    );
    metrics.output_failures = atomic_load_explicit(
        &host->output_failures, memory_order_relaxed
    );
    return metrics;
}
