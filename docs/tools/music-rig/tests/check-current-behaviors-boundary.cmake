if(NOT DEFINED BEHAVIOR_DIR)
    message(FATAL_ERROR "BEHAVIOR_DIR is required")
endif()

file(GLOB_RECURSE BEHAVIOR_FILES
    "${BEHAVIOR_DIR}/*.c"
    "${BEHAVIOR_DIR}/*.h"
)
if(NOT BEHAVIOR_FILES)
    message(FATAL_ERROR "No current behavior sources found")
endif()

set(FORBIDDEN_TOKENS
    "windows.h"
    "unistd.h"
    "sys/"
    "jack/"
    "pipewire"
    "carla"
    "systemd"
    "pthread_"
    "jack_"
    "pw_"
    "sd_"
)
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
    call_once
    fopen
    fclose
    fread
    fwrite
    open
    read
    write
    rename
    unlink
    mkdir
    CreateFile
)

foreach(BEHAVIOR_FILE IN LISTS BEHAVIOR_FILES)
    file(READ "${BEHAVIOR_FILE}" CONTENTS)
    string(TOLOWER "${CONTENTS}" NORMALIZED_CONTENTS)
    foreach(FORBIDDEN_TOKEN IN LISTS FORBIDDEN_TOKENS)
        string(FIND "${NORMALIZED_CONTENTS}" "${FORBIDDEN_TOKEN}" TOKEN_INDEX)
        if(NOT TOKEN_INDEX EQUAL -1)
            message(FATAL_ERROR
                "Portable behavior file ${BEHAVIOR_FILE} contains forbidden "
                "token ${FORBIDDEN_TOKEN}"
            )
        endif()
    endforeach()
    foreach(FORBIDDEN_CALL IN LISTS FORBIDDEN_CALLS)
        string(
            REGEX MATCH
            "(^|[^A-Za-z0-9_])${FORBIDDEN_CALL}[ \t\r\n]*\\("
            MATCHED_CALL
            "${CONTENTS}"
        )
        if(MATCHED_CALL)
            message(FATAL_ERROR
                "Portable behavior file ${BEHAVIOR_FILE} contains forbidden "
                "call ${FORBIDDEN_CALL}"
            )
        endif()
    endforeach()
endforeach()
