if(NOT DEFINED SHADOW_SOURCE)
    message(FATAL_ERROR "SHADOW_SOURCE is required")
endif()

file(READ "${SHADOW_SOURCE}" CONTENTS)
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
    strcmp
    strncmp
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
            "Shadow event engine contains forbidden call ${FORBIDDEN_CALL}"
        )
    endif()
endforeach()

string(TOLOWER "${CONTENTS}" NORMALIZED)
foreach(FORBIDDEN_TOKEN IN ITEMS
    "windows.h" "unistd.h" "sys/" "jack/" "pipewire" "pthread_"
    "jack_" "pw_" "createfile" "criticalsection"
)
    string(FIND "${NORMALIZED}" "${FORBIDDEN_TOKEN}" INDEX)
    if(NOT INDEX EQUAL -1)
        message(FATAL_ERROR
            "Shadow event engine contains forbidden token ${FORBIDDEN_TOKEN}"
        )
    endif()
endforeach()
