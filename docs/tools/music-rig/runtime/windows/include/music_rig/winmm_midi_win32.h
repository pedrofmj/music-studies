#ifndef MUSIC_RIG_WINMM_MIDI_WIN32_H
#define MUSIC_RIG_WINMM_MIDI_WIN32_H

#include "music_rig/winmm_midi.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

typedef struct music_rig_winmm_midi_win32_input {
    HMIDIIN handle;
    music_rig_winmm_midi_input_callback_fn callback;
    void *callback_argument;
    MIDIHDR headers[MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT];
    uint8_t long_data[MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT]
        [MUSIC_RIG_WINMM_MIDI_LONG_MESSAGE_CAPACITY];
    bool prepared[MUSIC_RIG_WINMM_MIDI_LONG_BUFFER_COUNT];
} music_rig_winmm_midi_win32_input;

typedef struct music_rig_winmm_midi_win32_worker {
    music_rig_winmm_midi_worker_fn entry;
    void *argument;
} music_rig_winmm_midi_win32_worker;

typedef struct music_rig_winmm_midi_win32 {
    LARGE_INTEGER qpc_frequency;
    HANDLE input_event;
    HANDLE output_event;
    HANDLE input_thread;
    HANDLE output_thread;
    music_rig_winmm_midi_win32_worker workers[2];
    music_rig_winmm_midi_win32_input inputs[
        MUSIC_RIG_DEVICE_PROFILE_CAPACITY
    ];
    HMIDIOUT outputs[MUSIC_RIG_DEVICE_PROFILE_CAPACITY];
} music_rig_winmm_midi_win32;

music_rig_result music_rig_winmm_midi_win32_init(
    music_rig_winmm_midi_win32 *native
);
music_rig_result music_rig_winmm_midi_win32_dispose(
    music_rig_winmm_midi_win32 *native
);
const music_rig_winmm_midi_ops *music_rig_winmm_midi_win32_ops(void);

#endif
