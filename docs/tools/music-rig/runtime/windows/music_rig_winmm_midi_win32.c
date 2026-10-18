#include "music_rig/winmm_midi_win32.h"

#include <limits.h>
#include <mmddk.h>
#include <string.h>

static uint64_t qpc_nanoseconds(
    const music_rig_winmm_midi_win32 *native,
    LARGE_INTEGER counter
)
{
    uint64_t ticks;
    uint64_t frequency;
    uint64_t whole;
    uint64_t remainder;
    uint64_t fractional;

    if (counter.QuadPart <= 0 || native->qpc_frequency.QuadPart <= 0) {
        return UINT64_C(0);
    }
    ticks = (uint64_t)counter.QuadPart;
    frequency = (uint64_t)native->qpc_frequency.QuadPart;
    whole = ticks / frequency;
    remainder = ticks % frequency;
    if (whole > UINT64_MAX / UINT64_C(1000000000) ||
        remainder > UINT64_MAX / UINT64_C(1000000000)) {
        return UINT64_MAX;
    }
    fractional = remainder * UINT64_C(1000000000) / frequency;
    whole *= UINT64_C(1000000000);
    return whole > UINT64_MAX - fractional
        ? UINT64_MAX : whole + fractional;
}

static void CALLBACK midi_input_proc(
    HMIDIIN handle,
    UINT message,
    DWORD_PTR instance,
    DWORD_PTR parameter1,
    DWORD_PTR parameter2
)
{
    music_rig_winmm_midi_win32_input *input =
        (music_rig_winmm_midi_win32_input *)instance;
    LARGE_INTEGER counter;
    uint64_t timestamp = UINT64_C(0);

    (void)handle;
    (void)parameter2;
    if (input == NULL || input->callback == NULL ||
        input->callback_argument == NULL) {
        return;
    }
    if (QueryPerformanceCounter(&counter)) {
        music_rig_winmm_midi_win32 *native = NULL;
        music_rig_winmm_midi_callback_context *callback_context =
            input->callback_argument;

        if (callback_context->host != NULL) {
            native = callback_context->host->ops_context;
        }
        if (native != NULL) {
            timestamp = qpc_nanoseconds(native, counter);
        }
    }
    if (message == MIM_DATA || message == MIM_MOREDATA) {
        input->callback(
            input->callback_argument,
            MUSIC_RIG_WINMM_MIDI_INPUT_SHORT,
            (uint32_t)parameter1,
            NULL,
            0U,
            0U,
            timestamp
        );
    } else if (message == MIM_LONGDATA || message == MIM_LONGERROR) {
        MIDIHDR *header = (MIDIHDR *)parameter1;
        size_t token;

        for (token = 0U; token < MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT;
             ++token) {
            if (header == &input->headers[token]) {
                input->callback(
                    input->callback_argument,
                    message == MIM_LONGDATA
                        ? MUSIC_RIG_WINMM_MIDI_INPUT_LONG
                        : MUSIC_RIG_WINMM_MIDI_INPUT_LONG_ERROR,
                    0U,
                    (const uint8_t *)header->lpData,
                    (size_t)header->dwBytesRecorded,
                    (uint32_t)token,
                    timestamp
                );
                return;
            }
        }
    }
}

static music_rig_result device_count(
    void *context,
    music_rig_winmm_midi_direction direction,
    uint32_t *count
)
{
    (void)context;
    if (count == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (direction == MUSIC_RIG_WINMM_MIDI_INPUT) {
        *count = (uint32_t)midiInGetNumDevs();
    } else if (direction == MUSIC_RIG_WINMM_MIDI_OUTPUT) {
        *count = (uint32_t)midiOutGetNumDevs();
    } else {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result device_interface(
    void *context,
    music_rig_winmm_midi_direction direction,
    uint32_t transient_index,
    wchar_t *interface_path,
    size_t interface_capacity
)
{
    music_rig_result result = MUSIC_RIG_RESULT_NOT_FOUND;
    ULONG byte_count = 0U;
    MMRESULT status;
    size_t byte_capacity;
    wchar_t *terminator;

    (void)context;
    if (interface_path == NULL || interface_capacity == 0U ||
        interface_capacity > (size_t)(ULONG_MAX / sizeof(wchar_t))) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    byte_capacity = interface_capacity * sizeof(wchar_t);
    interface_path[0] = L'\0';
    if (direction == MUSIC_RIG_WINMM_MIDI_INPUT) {
        HMIDIIN handle = NULL;

        status = midiInOpen(&handle, (UINT)transient_index, 0U, 0U,
            CALLBACK_NULL);
        if (status != MMSYSERR_NOERROR) {
            return MUSIC_RIG_RESULT_NOT_FOUND;
        }
        status = midiInMessage(handle, DRV_QUERYDEVICEINTERFACESIZE,
            (DWORD_PTR)&byte_count, 0U);
        if (status == MMSYSERR_NOERROR && byte_count != 0U &&
            (size_t)byte_count <= byte_capacity &&
            (byte_count % sizeof(wchar_t)) == 0U) {
            status = midiInMessage(handle, DRV_QUERYDEVICEINTERFACE,
                (DWORD_PTR)interface_path, (DWORD_PTR)byte_count);
            if (status == MMSYSERR_NOERROR) {
                terminator = wmemchr(interface_path, L'\0',
                    (size_t)byte_count / sizeof(wchar_t));
                if (terminator != NULL && terminator != interface_path) {
                    result = MUSIC_RIG_RESULT_OK;
                }
            }
        }
        if (midiInClose(handle) != MMSYSERR_NOERROR &&
            result == MUSIC_RIG_RESULT_OK) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    } else if (direction == MUSIC_RIG_WINMM_MIDI_OUTPUT) {
        HMIDIOUT handle = NULL;

        status = midiOutOpen(&handle, (UINT)transient_index, 0U, 0U,
            CALLBACK_NULL);
        if (status != MMSYSERR_NOERROR) {
            return MUSIC_RIG_RESULT_NOT_FOUND;
        }
        status = midiOutMessage(handle, DRV_QUERYDEVICEINTERFACESIZE,
            (DWORD_PTR)&byte_count, 0U);
        if (status == MMSYSERR_NOERROR && byte_count != 0U &&
            (size_t)byte_count <= byte_capacity &&
            (byte_count % sizeof(wchar_t)) == 0U) {
            status = midiOutMessage(handle, DRV_QUERYDEVICEINTERFACE,
                (DWORD_PTR)interface_path, (DWORD_PTR)byte_count);
            if (status == MMSYSERR_NOERROR) {
                terminator = wmemchr(interface_path, L'\0',
                    (size_t)byte_count / sizeof(wchar_t));
                if (terminator != NULL && terminator != interface_path) {
                    result = MUSIC_RIG_RESULT_OK;
                }
            }
        }
        if (midiOutClose(handle) != MMSYSERR_NOERROR &&
            result == MUSIC_RIG_RESULT_OK) {
            result = MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    } else {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (result != MUSIC_RIG_RESULT_OK) {
        interface_path[0] = L'\0';
    }
    return result;
}

static bool release_input_headers(
    music_rig_winmm_midi_win32_input *input
)
{
    size_t index;
    bool released = true;

    if (input->handle == NULL) {
        return true;
    }
    (void)midiInReset(input->handle);
    for (index = 0U; index < MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT; ++index) {
        if (input->prepared[index]) {
            MMRESULT status = midiInUnprepareHeader(
                input->handle, &input->headers[index],
                (UINT)sizeof(input->headers[index])
            );
            if (status == MMSYSERR_NOERROR) {
                input->prepared[index] = false;
            } else {
                released = false;
            }
        }
    }
    return released;
}

static music_rig_result input_open(
    void *context,
    uint32_t transient_index,
    music_rig_winmm_midi_input_callback_fn callback,
    void *callback_argument,
    void **handle
)
{
    music_rig_winmm_midi_win32 *native = context;
    music_rig_winmm_midi_callback_context *callback_context =
        callback_argument;
    music_rig_winmm_midi_win32_input *input;
    size_t slot_index;
    MMRESULT status;

    if (native == NULL || callback == NULL || callback_context == NULL ||
        callback_context->host == NULL || handle == NULL ||
        callback_context->slot_index >= MUSIC_RIG_DEVICE_PROFILE_CAPACITY) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    *handle = NULL;
    slot_index = callback_context->slot_index;
    input = &native->inputs[slot_index];
    if (input->handle != NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    memset(input, 0, sizeof(*input));
    input->callback = callback;
    input->callback_argument = callback_argument;
    status = midiInOpen(&input->handle, transient_index,
        (DWORD_PTR)midi_input_proc, (DWORD_PTR)input, CALLBACK_FUNCTION);
    if (status != MMSYSERR_NOERROR) {
        input->handle = NULL;
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *handle = input->handle;
    for (slot_index = 0U;
         slot_index < MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT; ++slot_index) {
        MIDIHDR *header = &input->headers[slot_index];

        memset(header, 0, sizeof(*header));
        header->lpData = (LPSTR)input->long_data[slot_index];
        header->dwBufferLength =
            (DWORD)MUSIC_RIG_WINMM_MIDI_LONG_MESSAGE_CAPACITY;
        header->dwUser = (DWORD_PTR)slot_index;
        status = midiInPrepareHeader(
            input->handle, header, (UINT)sizeof(*header)
        );
        if (status != MMSYSERR_NOERROR) {
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
        input->prepared[slot_index] = true;
        status = midiInAddBuffer(input->handle, header, (UINT)sizeof(*header));
        if (status != MMSYSERR_NOERROR) {
            return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    return MUSIC_RIG_RESULT_OK;
}

static music_rig_result output_open(
    void *context,
    uint32_t transient_index,
    void **handle
)
{
    music_rig_winmm_midi_win32 *native = context;
    size_t index;

    if (native == NULL || handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MUSIC_RIG_DEVICE_PROFILE_CAPACITY; ++index) {
        if (native->outputs[index] == NULL) {
            HMIDIOUT output = NULL;
            MMRESULT status = midiOutOpen(&output, transient_index, 0U, 0U,
                CALLBACK_NULL);

            if (status != MMSYSERR_NOERROR) {
                return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            }
            native->outputs[index] = output;
            *handle = output;
            return MUSIC_RIG_RESULT_OK;
        }
    }
    return MUSIC_RIG_RESULT_INVALID_STATE;
}

static music_rig_result input_start(void *context, void *handle)
{
    (void)context;
    return handle != NULL && midiInStart((HMIDIIN)handle) == MMSYSERR_NOERROR
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static music_rig_result input_stop(void *context, void *handle)
{
    MMRESULT stop_status;
    MMRESULT reset_status;

    (void)context;
    if (handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    stop_status = midiInStop((HMIDIIN)handle);
    reset_status = midiInReset((HMIDIIN)handle);
    return stop_status == MMSYSERR_NOERROR && reset_status == MMSYSERR_NOERROR
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static music_rig_result input_requeue_long(
    void *context,
    void *handle,
    uint32_t long_buffer_token
)
{
    music_rig_winmm_midi_win32 *native = context;
    size_t index;

    if (native == NULL || handle == NULL ||
        long_buffer_token >= MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MUSIC_RIG_DEVICE_PROFILE_CAPACITY; ++index) {
        music_rig_winmm_midi_win32_input *input = &native->inputs[index];

        if (input->handle == (HMIDIIN)handle) {
            MIDIHDR *header = &input->headers[long_buffer_token];

            header->dwBytesRecorded = 0U;
            header->dwFlags &= ~MHDR_DONE;
            return midiInAddBuffer(
                    input->handle, header, (UINT)sizeof(*header)
                ) ==
                    MMSYSERR_NOERROR
                ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
        }
    }
    return MUSIC_RIG_RESULT_NOT_FOUND;
}

static music_rig_result input_close(void *context, void *handle)
{
    music_rig_winmm_midi_win32 *native = context;
    size_t index;

    if (native == NULL || handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MUSIC_RIG_DEVICE_PROFILE_CAPACITY; ++index) {
        music_rig_winmm_midi_win32_input *input = &native->inputs[index];

        if (input->handle == (HMIDIIN)handle) {
            MMRESULT status;

            if (!release_input_headers(input)) {
                return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            }
            status = midiInClose(input->handle);
            if (status != MMSYSERR_NOERROR) {
                return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            }
            memset(input, 0, sizeof(*input));
            return MUSIC_RIG_RESULT_OK;
        }
    }
    return MUSIC_RIG_RESULT_NOT_FOUND;
}

static music_rig_result output_send_short(
    void *context,
    void *handle,
    uint32_t packed_short_message
)
{
    (void)context;
    return handle != NULL && midiOutShortMsg((HMIDIOUT)handle,
            (DWORD)packed_short_message) == MMSYSERR_NOERROR
        ? MUSIC_RIG_RESULT_OK : MUSIC_RIG_RESULT_ADAPTER_FAILURE;
}

static music_rig_result output_close(void *context, void *handle)
{
    music_rig_winmm_midi_win32 *native = context;
    size_t index;

    if (native == NULL || handle == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MUSIC_RIG_DEVICE_PROFILE_CAPACITY; ++index) {
        if (native->outputs[index] == (HMIDIOUT)handle) {
            MMRESULT status = midiOutClose(native->outputs[index]);

            if (status != MMSYSERR_NOERROR) {
                return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
            }
            native->outputs[index] = NULL;
            return MUSIC_RIG_RESULT_OK;
        }
    }
    return MUSIC_RIG_RESULT_NOT_FOUND;
}

static DWORD WINAPI worker_entry(void *argument)
{
    music_rig_winmm_midi_win32_worker *worker = argument;

    if (worker != NULL && worker->entry != NULL) {
        worker->entry(worker->argument);
    }
    return 0U;
}

static music_rig_result worker_start(
    void *context,
    music_rig_winmm_midi_worker_kind kind,
    music_rig_winmm_midi_worker_fn worker,
    void *worker_argument,
    void **thread
)
{
    music_rig_winmm_midi_win32 *native = context;
    music_rig_winmm_midi_win32_worker *worker_context;
    HANDLE *thread_handle;

    if (native == NULL || worker == NULL || worker_argument == NULL ||
        thread == NULL || (kind != MUSIC_RIG_WINMM_MIDI_INPUT_WORKER &&
            kind != MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    worker_context = &native->workers[(size_t)kind];
    thread_handle = kind == MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
        ? &native->input_thread : &native->output_thread;
    if (*thread_handle != NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    worker_context->entry = worker;
    worker_context->argument = worker_argument;
    *thread_handle = CreateThread(NULL, 0U, worker_entry, worker_context, 0U,
        NULL);
    if (*thread_handle == NULL) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *thread = *thread_handle;
    return MUSIC_RIG_RESULT_OK;
}

static void worker_signal(
    void *context,
    music_rig_winmm_midi_worker_kind kind
)
{
    music_rig_winmm_midi_win32 *native = context;
    HANDLE event;

    if (native == NULL) {
        return;
    }
    event = kind == MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
        ? native->input_event : native->output_event;
    if (event != NULL) {
        (void)SetEvent(event);
    }
}

static void worker_wait(
    void *context,
    music_rig_winmm_midi_worker_kind kind
)
{
    music_rig_winmm_midi_win32 *native = context;
    HANDLE event;

    if (native == NULL) {
        return;
    }
    event = kind == MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
        ? native->input_event : native->output_event;
    if (event != NULL) {
        (void)WaitForSingleObject(event, INFINITE);
    }
}

static music_rig_result worker_join(
    void *context,
    music_rig_winmm_midi_worker_kind kind,
    void *thread
)
{
    music_rig_winmm_midi_win32 *native = context;
    HANDLE *thread_handle;

    if (native == NULL || thread == NULL ||
        (kind != MUSIC_RIG_WINMM_MIDI_INPUT_WORKER &&
            kind != MUSIC_RIG_WINMM_MIDI_OUTPUT_WORKER)) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    thread_handle = kind == MUSIC_RIG_WINMM_MIDI_INPUT_WORKER
        ? &native->input_thread : &native->output_thread;
    if (*thread_handle != (HANDLE)thread) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    if (WaitForSingleObject(*thread_handle, INFINITE) != WAIT_OBJECT_0) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    if (!CloseHandle(*thread_handle)) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    *thread_handle = NULL;
    return MUSIC_RIG_RESULT_OK;
}

static const music_rig_winmm_midi_ops native_ops = {
    device_count,
    device_interface,
    input_open,
    output_open,
    input_start,
    input_stop,
    input_requeue_long,
    input_close,
    output_send_short,
    output_close,
    worker_start,
    worker_signal,
    worker_wait,
    worker_join
};

music_rig_result music_rig_winmm_midi_win32_init(
    music_rig_winmm_midi_win32 *native
)
{
    if (native == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    memset(native, 0, sizeof(*native));
    if (!QueryPerformanceFrequency(&native->qpc_frequency) ||
        native->qpc_frequency.QuadPart <= 0) {
        memset(native, 0, sizeof(*native));
        return MUSIC_RIG_RESULT_UNSUPPORTED;
    }
    native->input_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    native->output_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (native->input_event == NULL || native->output_event == NULL) {
        (void)music_rig_winmm_midi_win32_dispose(native);
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    return MUSIC_RIG_RESULT_OK;
}

music_rig_result music_rig_winmm_midi_win32_dispose(
    music_rig_winmm_midi_win32 *native
)
{
    size_t index;
    bool failed = false;

    if (native == NULL) {
        return MUSIC_RIG_RESULT_INVALID_ARGUMENT;
    }
    if (native->input_thread != NULL || native->output_thread != NULL) {
        return MUSIC_RIG_RESULT_INVALID_STATE;
    }
    for (index = 0U; index < MUSIC_RIG_DEVICE_PROFILE_CAPACITY; ++index) {
        if (native->inputs[index].handle != NULL ||
            native->outputs[index] != NULL) {
            return MUSIC_RIG_RESULT_INVALID_STATE;
        }
    }
    if (native->input_event != NULL) {
        if (CloseHandle(native->input_event)) {
            native->input_event = NULL;
        } else {
            failed = true;
        }
    }
    if (native->output_event != NULL) {
        if (CloseHandle(native->output_event)) {
            native->output_event = NULL;
        } else {
            failed = true;
        }
    }
    if (failed) {
        return MUSIC_RIG_RESULT_ADAPTER_FAILURE;
    }
    memset(native, 0, sizeof(*native));
    return MUSIC_RIG_RESULT_OK;
}

const music_rig_winmm_midi_ops *music_rig_winmm_midi_win32_ops(void)
{
    return &native_ops;
}
