if(NOT DEFINED CLI)
    message(FATAL_ERROR "CLI is required")
endif()

execute_process(
    COMMAND "${CLI}" status --json
    RESULT_VARIABLE STATUS_RESULT
    OUTPUT_VARIABLE STATUS_OUTPUT
    ERROR_VARIABLE STATUS_ERROR
    TIMEOUT 2
)
if(NOT STATUS_RESULT EQUAL 4)
    message(FATAL_ERROR
        "music-rig status returned ${STATUS_RESULT}, expected adapter failure 4"
    )
endif()
if(NOT STATUS_OUTPUT STREQUAL "")
    message(FATAL_ERROR "music-rig wrote output without a control transport")
endif()
string(FIND "${STATUS_ERROR}" "no request was sent" NO_REQUEST_INDEX)
if(NO_REQUEST_INDEX EQUAL -1)
    message(FATAL_ERROR "music-rig did not report its fail-closed boundary")
endif()

execute_process(
    COMMAND "${CLI}" switch --global full-live-rack
    RESULT_VARIABLE SWITCH_RESULT
    OUTPUT_VARIABLE SWITCH_OUTPUT
    ERROR_VARIABLE SWITCH_ERROR
    TIMEOUT 2
)
if(NOT SWITCH_RESULT EQUAL 4)
    message(FATAL_ERROR
        "music-rig global switch did not fail at the transport boundary: ${SWITCH_RESULT}"
    )
endif()
if(NOT SWITCH_OUTPUT STREQUAL "")
    message(FATAL_ERROR "global switch unexpectedly wrote output")
endif()
string(FIND "${SWITCH_ERROR}" "no request was sent" NO_SWITCH_REQUEST_INDEX)
if(NO_SWITCH_REQUEST_INDEX EQUAL -1)
    message(FATAL_ERROR "global switch did not report its fail-closed boundary")
endif()
