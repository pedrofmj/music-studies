if(NOT DEFINED RUNTIME_SOURCE)
    message(FATAL_ERROR "RUNTIME_SOURCE is required")
endif()

file(READ "${RUNTIME_SOURCE}" CONTENTS)
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
            "Portable runtime contains forbidden call ${FORBIDDEN_CALL}"
        )
    endif()
endforeach()
