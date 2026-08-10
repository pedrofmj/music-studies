cmake_minimum_required(VERSION 3.20)

foreach(required_variable IN ITEMS HELPER CONFIG STATE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(EXISTS "${STATE}")
    message(FATAL_ERROR "Guard state path already exists: ${STATE}")
endif()

execute_process(
    COMMAND "${HELPER}" "${CONFIG}" "${STATE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE standard_output
    ERROR_VARIABLE standard_error
)

if(NOT result EQUAL 99)
    message(FATAL_ERROR
        "Expected forbidden JACK exit 99, got ${result}\n"
        "stdout: ${standard_output}\n"
        "stderr: ${standard_error}"
    )
endif()

if(NOT standard_error MATCHES
    "Offline helper test attempted forbidden JACK call: jack_client_open")
    message(FATAL_ERROR "Forbidden JACK diagnostic was not emitted")
endif()

if(EXISTS "${STATE}")
    message(FATAL_ERROR "Guard invocation created production-like state")
endif()

message(STATUS "SMK-25 JACK isolation guard: OK")
