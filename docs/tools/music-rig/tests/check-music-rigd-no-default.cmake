if(NOT DEFINED DAEMON)
    message(FATAL_ERROR "DAEMON is required")
endif()

execute_process(
    COMMAND "${DAEMON}"
    RESULT_VARIABLE DAEMON_RESULT
    OUTPUT_VARIABLE DAEMON_OUTPUT
    ERROR_VARIABLE DAEMON_ERROR
    TIMEOUT 2
)

if(NOT DAEMON_RESULT EQUAL 2)
    message(FATAL_ERROR
        "music-rigd without a command returned ${DAEMON_RESULT}, expected 2"
    )
endif()
if(NOT DAEMON_OUTPUT STREQUAL "")
    message(FATAL_ERROR "music-rigd unexpectedly wrote default output")
endif()
string(FIND "${DAEMON_ERROR}" "Usage:" USAGE_INDEX)
if(USAGE_INDEX EQUAL -1)
    message(FATAL_ERROR "music-rigd did not report its inert command boundary")
endif()
