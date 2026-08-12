if(NOT DEFINED DAEMON OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "DAEMON and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
set(HOME_DIR "${WORK_DIR}/home")
set(CONFIG_DIR "${WORK_DIR}/config")
set(CACHE_DIR "${WORK_DIR}/cache")
set(STATE_DIR "${WORK_DIR}/state")
set(RUNTIME_DIR "${WORK_DIR}/runtime")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "HOME=${HOME_DIR}"
        "XDG_CONFIG_HOME=${CONFIG_DIR}"
        "XDG_CACHE_HOME=${CACHE_DIR}"
        "XDG_STATE_HOME=${STATE_DIR}"
        "XDG_RUNTIME_DIR=${RUNTIME_DIR}"
        "${DAEMON}" resolve-paths --check-only
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR_OUTPUT
    TIMEOUT 5
)
if(NOT RESULT EQUAL 0 OR NOT ERROR_OUTPUT STREQUAL "")
    message(FATAL_ERROR
        "Host path preflight failed (${RESULT}): ${ERROR_OUTPUT}"
    )
endif()

set(EXPECTED_LINES
    "config-directory ${CONFIG_DIR}/music-rig"
    "config-file ${CONFIG_DIR}/music-rig/config.json"
    "cache-directory ${CACHE_DIR}/music-rig"
    "compiled-cache-directory ${CACHE_DIR}/music-rig/compiled"
    "state-directory ${STATE_DIR}/music-rig"
    "active-state-file ${STATE_DIR}/music-rig/active.state"
    "device-state-directory ${STATE_DIR}/music-rig/device-state"
    "runtime-directory ${RUNTIME_DIR}/music-rig"
    "control-socket ${RUNTIME_DIR}/music-rig/control.sock"
    "output-mode suppressed"
    "filesystem-writes 0"
)
foreach(EXPECTED_LINE IN LISTS EXPECTED_LINES)
    string(FIND "${OUTPUT}" "${EXPECTED_LINE}\n" LINE_INDEX)
    if(LINE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Host path preflight omitted: ${EXPECTED_LINE}\n${OUTPUT}"
        )
    endif()
endforeach()
if(EXISTS "${WORK_DIR}")
    message(FATAL_ERROR "Host path preflight created ${WORK_DIR}")
endif()
