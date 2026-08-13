if(NOT DEFINED RELAY_SOURCE)
    message(FATAL_ERROR "RELAY_SOURCE is required")
endif()

file(READ "${RELAY_SOURCE}" CONTENTS)
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
    open
    read
    write
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
            "SMC-Mixer relay contains forbidden call ${FORBIDDEN_CALL}"
        )
    endif()
endforeach()

string(TOLOWER "${CONTENTS}" NORMALIZED)
foreach(FORBIDDEN_TOKEN IN ITEMS
    "windows.h" "unistd.h" "sys/" "jack/" "pipewire" "pthread_"
    "jack_" "pw_" "createfile" "criticalsection" "json"
)
    string(FIND "${NORMALIZED}" "${FORBIDDEN_TOKEN}" INDEX)
    if(NOT INDEX EQUAL -1)
        message(FATAL_ERROR
            "SMC-Mixer relay contains forbidden token ${FORBIDDEN_TOKEN}"
        )
    endif()
endforeach()

string(FIND "${CONTENTS}" "music_rig_smc_mixer_relay_begin_cycle(" PROCESS_START)
string(FIND "${CONTENTS}" "music_rig_smc_mixer_relay_input_port_id(" PROCESS_END)
if(PROCESS_START EQUAL -1 OR PROCESS_END EQUAL -1 OR
    PROCESS_END LESS_EQUAL PROCESS_START)
    message(FATAL_ERROR "SMC-Mixer relay event boundary was not found")
endif()
math(EXPR PROCESS_LENGTH "${PROCESS_END} - ${PROCESS_START}")
string(SUBSTRING "${CONTENTS}" ${PROCESS_START} ${PROCESS_LENGTH} PROCESS_BODY)
foreach(FORBIDDEN_REALTIME_CALL IN ITEMS strcmp strncmp snprintf memcpy memmove)
    string(
        REGEX MATCH
        "(^|[^A-Za-z0-9_])${FORBIDDEN_REALTIME_CALL}[ \t\r\n]*\\("
        MATCHED_REALTIME_CALL
        "${PROCESS_BODY}"
    )
    if(MATCHED_REALTIME_CALL)
        message(FATAL_ERROR
            "SMC-Mixer relay event path contains forbidden call "
            "${FORBIDDEN_REALTIME_CALL}"
        )
    endif()
endforeach()
