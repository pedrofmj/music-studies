if(NOT DEFINED DAEMON OR NOT DEFINED DEFINITION OR
    NOT DEFINED EXPECTED_FINGERPRINT)
    message(FATAL_ERROR
        "DAEMON, DEFINITION, and EXPECTED_FINGERPRINT are required"
    )
endif()

execute_process(
    COMMAND "${DAEMON}"
        validate-definition
        --definition "${DEFINITION}"
        --expected-fingerprint "${EXPECTED_FINGERPRINT}"
    RESULT_VARIABLE VALID_RESULT
    OUTPUT_VARIABLE VALID_OUTPUT
    ERROR_VARIABLE VALID_ERROR
    TIMEOUT 5
)
if(NOT VALID_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Valid definition returned ${VALID_RESULT}: ${VALID_ERROR}"
    )
endif()
if(NOT VALID_ERROR STREQUAL "")
    message(FATAL_ERROR "Valid definition wrote stderr: ${VALID_ERROR}")
endif()

set(EXPECTED_LINES
    "definition valid"
    "generation 1"
    "rig pedro-performance-rig"
    "rig-profile full-live-rack"
    "platform-binding airstar-current"
    "platform linux"
    "device-profiles 5"
    "mappings 72"
    "target-bindings 71"
    "ownership 57"
    "output-mode suppressed"
)
foreach(EXPECTED_LINE IN LISTS EXPECTED_LINES)
    string(FIND "${VALID_OUTPUT}" "${EXPECTED_LINE}" LINE_INDEX)
    if(LINE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Definition output omitted: ${EXPECTED_LINE}\n${VALID_OUTPUT}"
        )
    endif()
endforeach()

execute_process(
    COMMAND "${DAEMON}"
        validate-definition
        --definition "${DEFINITION}"
        --expected-fingerprint
        "sha256:0000000000000000000000000000000000000000000000000000000000000000"
    RESULT_VARIABLE MISMATCH_RESULT
    OUTPUT_VARIABLE MISMATCH_OUTPUT
    ERROR_VARIABLE MISMATCH_ERROR
    TIMEOUT 5
)
if(NOT MISMATCH_RESULT EQUAL 7)
    message(FATAL_ERROR
        "Fingerprint mismatch returned ${MISMATCH_RESULT}, expected 7"
    )
endif()
if(NOT MISMATCH_OUTPUT STREQUAL "")
    message(FATAL_ERROR "Fingerprint mismatch wrote stdout")
endif()
string(FIND "${MISMATCH_ERROR}" "result 7" MISMATCH_INDEX)
if(MISMATCH_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Fingerprint mismatch omitted its explicit result: ${MISMATCH_ERROR}"
    )
endif()
