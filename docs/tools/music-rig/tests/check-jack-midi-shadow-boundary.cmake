if(NOT DEFINED JACK_SHADOW_SOURCE)
    message(FATAL_ERROR "JACK_SHADOW_SOURCE is required")
endif()

file(READ "${JACK_SHADOW_SOURCE}" CONTENTS)
set(FORBIDDEN_CALLS
    malloc
    calloc
    realloc
    free
    aligned_alloc
    mtx_lock
    mtx_trylock
    mtx_timedlock
    cnd_wait
    cnd_timedwait
    fopen
    fclose
    fread
    fwrite
    jack_connect
    jack_disconnect
    jack_port_disconnect
    jack_midi_event_reserve
    jack_midi_event_write
    jack_midi_clear_buffer
)

foreach(FORBIDDEN_CALL IN LISTS FORBIDDEN_CALLS)
    string(
        REGEX MATCH
        "(^|[^A-Za-z0-9_])${FORBIDDEN_CALL}[ \t\r\n]*\\("
        MATCHED_CALL
        "${CONTENTS}"
    )
    if(MATCHED_CALL)
        message(FATAL_ERROR
            "JACK MIDI shadow contains forbidden call ${FORBIDDEN_CALL}"
        )
    endif()
endforeach()

string(TOLOWER "${CONTENTS}" NORMALIZED)
foreach(FORBIDDEN_TOKEN IN ITEMS
    "jack_port_is_output" "jack_default_audio_type" "pipewire" "pthread_"
)
    string(FIND "${NORMALIZED}" "${FORBIDDEN_TOKEN}" INDEX)
    if(NOT INDEX EQUAL -1)
        message(FATAL_ERROR
            "JACK MIDI shadow contains forbidden token ${FORBIDDEN_TOKEN}"
        )
    endif()
endforeach()

string(FIND "${NORMALIZED}" "jack_no_start_server" NO_START_INDEX)
if(NO_START_INDEX EQUAL -1)
    message(FATAL_ERROR "JACK MIDI shadow may start a JACK server")
endif()
