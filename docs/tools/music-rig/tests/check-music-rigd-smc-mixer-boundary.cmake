if(NOT DEFINED DAEMON)
    message(FATAL_ERROR "DAEMON is required")
endif()
if(NOT DEFINED EXPECT_RELAY)
    message(FATAL_ERROR "EXPECT_RELAY is required")
endif()

execute_process(
    COMMAND "${DAEMON}" --help
    RESULT_VARIABLE DAEMON_RESULT
    OUTPUT_VARIABLE DAEMON_OUTPUT
    ERROR_VARIABLE DAEMON_ERROR
    TIMEOUT 2
)

if(NOT DAEMON_RESULT EQUAL 0)
    message(FATAL_ERROR "music-rigd --help returned ${DAEMON_RESULT}")
endif()
set(DAEMON_HELP "${DAEMON_OUTPUT}${DAEMON_ERROR}")
string(FIND "${DAEMON_HELP}" "run-smc-mixer-relay" RELAY_INDEX)
if(EXPECT_RELAY AND RELAY_INDEX EQUAL -1)
    message(FATAL_ERROR
        "the explicit SMC-Mixer relay command is missing from this build"
    )
endif()
if(NOT EXPECT_RELAY AND NOT RELAY_INDEX EQUAL -1)
    message(FATAL_ERROR
        "an output-capable SMC-Mixer command leaked into a suppressed build"
    )
endif()
