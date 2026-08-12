if(NOT DEFINED RUNTIME_DIR)
    message(FATAL_ERROR "RUNTIME_DIR is required")
endif()

set(RUNTIME_SOURCES
    "${RUNTIME_DIR}/music_rig_compiled_tables.c"
    "${RUNTIME_DIR}/music_rig_control.c"
    "${RUNTIME_DIR}/music_rig_definition.c"
    "${RUNTIME_DIR}/music_rig_device_ports.c"
    "${RUNTIME_DIR}/music_rig_diagnostics.c"
    "${RUNTIME_DIR}/music_rig_runtime.c"
    "${RUNTIME_DIR}/music_rig_state.c"
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
)

foreach(RUNTIME_SOURCE IN LISTS RUNTIME_SOURCES)
    file(READ "${RUNTIME_SOURCE}" CONTENTS)
    foreach(FORBIDDEN_CALL IN LISTS FORBIDDEN_CALLS)
        string(
            REGEX MATCH
            "(^|[^A-Za-z0-9_])${FORBIDDEN_CALL}[ \t\r\n]*\\("
            MATCHED_CALL
            "${CONTENTS}"
        )
        if(MATCHED_CALL)
            message(FATAL_ERROR
                "Portable runtime source ${RUNTIME_SOURCE} contains forbidden "
                "call ${FORBIDDEN_CALL}"
            )
        endif()
    endforeach()
endforeach()
