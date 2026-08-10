if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR is required")
endif()

file(GLOB_RECURSE CORE_FILES
    "${CORE_DIR}/*.c"
    "${CORE_DIR}/*.h"
)

if(NOT CORE_FILES)
    message(FATAL_ERROR "No portable core sources found in ${CORE_DIR}")
endif()

set(FORBIDDEN_TOKENS
    "windows.h"
    "unistd.h"
    "sys/socket.h"
    "sys/un.h"
    "jack/"
    "pipewire/"
    "systemd/"
    "pthread_"
    "jack_"
    "pw_"
    "sd_"
)

foreach(CORE_FILE IN LISTS CORE_FILES)
    file(READ "${CORE_FILE}" CONTENTS)
    foreach(FORBIDDEN_TOKEN IN LISTS FORBIDDEN_TOKENS)
        string(FIND "${CONTENTS}" "${FORBIDDEN_TOKEN}" TOKEN_INDEX)
        if(NOT TOKEN_INDEX EQUAL -1)
            message(FATAL_ERROR
                "Portable core file ${CORE_FILE} contains forbidden token "
                "${FORBIDDEN_TOKEN}"
            )
        endif()
    endforeach()
endforeach()
