if(NOT DEFINED CHECK_SCRIPT)
    message(FATAL_ERROR "CHECK_SCRIPT is required")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR is required")
endif()

set(FIXTURE_DIR "${WORK_DIR}/portable-core")
file(MAKE_DIRECTORY "${FIXTURE_DIR}")
file(REMOVE
    "${FIXTURE_DIR}/neutral.c"
    "${FIXTURE_DIR}/forbidden.c"
)
file(WRITE "${FIXTURE_DIR}/neutral.c" "int portable_fixture(void) { return 0; }\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DCORE_DIR=${FIXTURE_DIR}"
        -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE NEUTRAL_RESULT
    OUTPUT_VARIABLE NEUTRAL_OUTPUT
    ERROR_VARIABLE NEUTRAL_ERROR
)
if(NOT NEUTRAL_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Portable guard rejected neutral source:\n"
        "${NEUTRAL_OUTPUT}${NEUTRAL_ERROR}"
    )
endif()

function(expect_backend_rejected LABEL CONTENT)
    set(FORBIDDEN_FILE "${FIXTURE_DIR}/forbidden.c")
    file(WRITE "${FORBIDDEN_FILE}" "${CONTENT}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DCORE_DIR=${FIXTURE_DIR}"
            -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE FORBIDDEN_RESULT
        OUTPUT_VARIABLE FORBIDDEN_OUTPUT
        ERROR_VARIABLE FORBIDDEN_ERROR
    )
    file(REMOVE "${FORBIDDEN_FILE}")
    if(FORBIDDEN_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Portable guard accepted ${LABEL} backend leakage"
        )
    endif()
    string(
        FIND
        "${FORBIDDEN_OUTPUT}${FORBIDDEN_ERROR}"
        "contains forbidden token"
        FAILURE_MARKER
    )
    if(FAILURE_MARKER EQUAL -1)
        message(FATAL_ERROR
            "Portable guard rejected ${LABEL} for the wrong reason:\n"
            "${FORBIDDEN_OUTPUT}${FORBIDDEN_ERROR}"
        )
    endif()
endfunction()

expect_backend_rejected("PipeWire" "/* PiPeWiRe graph primitive */")
expect_backend_rejected("Carla" "/* CaRlA host primitive */")

file(REMOVE "${FIXTURE_DIR}/neutral.c")
