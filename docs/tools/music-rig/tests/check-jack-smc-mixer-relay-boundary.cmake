if(NOT DEFINED JACK_RELAY_SOURCE)
    message(FATAL_ERROR "JACK_RELAY_SOURCE is required")
endif()

file(READ "${JACK_RELAY_SOURCE}" CONTENTS)
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
    jack_get_ports
    jack_port_by_name
    jack_port_get_all_connections
    jack_port_get_connections
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
            "JACK SMC-Mixer relay contains forbidden call ${FORBIDDEN_CALL}"
        )
    endif()
endforeach()

string(TOLOWER "${CONTENTS}" NORMALIZED)
foreach(FORBIDDEN_TOKEN IN ITEMS
    "jack_default_audio_type" "pipewire" "pthread_" "system(" "popen("
)
    string(FIND "${NORMALIZED}" "${FORBIDDEN_TOKEN}" INDEX)
    if(NOT INDEX EQUAL -1)
        message(FATAL_ERROR
            "JACK SMC-Mixer relay contains forbidden token ${FORBIDDEN_TOKEN}"
        )
    endif()
endforeach()

foreach(REQUIRED_TOKEN IN ITEMS
    "jack_no_start_server" "jack_port_is_input" "jack_port_is_output"
    "jack_midi_clear_buffer" "jack_midi_event_write"
)
    string(FIND "${NORMALIZED}" "${REQUIRED_TOKEN}" INDEX)
    if(INDEX EQUAL -1)
        message(FATAL_ERROR
            "JACK SMC-Mixer relay omits required token ${REQUIRED_TOKEN}"
        )
    endif()
endforeach()
