if(NOT DEFINED DAEMON)
    message(FATAL_ERROR "DAEMON is required")
endif()

execute_process(
    COMMAND "${DAEMON}" run-win32-carla --help
    RESULT_VARIABLE HELP_RESULT
    OUTPUT_VARIABLE HELP_OUTPUT
    ERROR_VARIABLE HELP_ERROR
    TIMEOUT 5
)
if(NOT HELP_RESULT EQUAL 0)
    message(FATAL_ERROR
        "run-win32-carla --help returned ${HELP_RESULT}: ${HELP_ERROR}")
endif()
foreach(REQUIRED_TEXT
        "run-win32-carla"
        "--input-interface PHYSICAL_WINMM_PATH"
        "--loopback-output-interface LOOPBACK_WINMM_PATH"
        "--patch-indexes 9_CSV"
        "--pad-profile CODE:PATCH_INDEXES:ACTIVE_LAYERS"
        "Code 2 is unsupported"
        "Basic Loopback input")
    string(FIND "${HELP_OUTPUT}" "${REQUIRED_TEXT}" TEXT_POSITION)
    if(TEXT_POSITION EQUAL -1)
        message(FATAL_ERROR "run-win32-carla help omitted: ${REQUIRED_TEXT}")
    endif()
endforeach()

set(COMMON_ARGUMENTS
    --carla-dll fake-carla.dll
    --project fake-project.carxp
    --driver fake-driver
    --client fake-client
    --midi-client "Basic Loopback"
    --midi-port "MIDI 1"
    --input-interface fake-winmm-input
    --loopback-output-interface fake-winmm-output
    --patch-names A,B,C,D,E,F,G,H,I
    --patch-indexes 0,1,2,3,4,5,6,7,8
    --active-layers 1,3,9
)

function(expect_rejected label)
    execute_process(
        COMMAND "${DAEMON}" run-win32-carla ${ARGN}
        RESULT_VARIABLE COMMAND_RESULT
        OUTPUT_VARIABLE COMMAND_OUTPUT
        ERROR_VARIABLE COMMAND_ERROR
        TIMEOUT 5
    )
    if(COMMAND_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Malformed ${label} invocation unexpectedly succeeded")
    endif()
    if(COMMAND_OUTPUT MATCHES "run-win32-carla ready" OR
        COMMAND_ERROR MATCHES "runtime open failed" OR
        COMMAND_ERROR MATCHES "WinMM native initialization failed" OR
        NOT COMMAND_ERROR MATCHES "invalid run-win32-carla options or values")
        message(FATAL_ERROR
            "Malformed ${label} invocation did not fail in argument validation")
    endif()
endfunction()

expect_rejected(duplicate-option ${COMMON_ARGUMENTS}
    --master-volume 100 --driver second-driver)
expect_rejected(missing-required-option --carla-dll fake-carla.dll)
expect_rejected(master-volume-overflow
    ${COMMON_ARGUMENTS} --master-volume 999999999999999999999999999999)
expect_rejected(patch-index-out-of-range
    --carla-dll fake-carla.dll
    --project fake-project.carxp
    --driver fake-driver
    --client fake-client
    --midi-client "Basic Loopback"
    --midi-port "MIDI 1"
    --input-interface fake-winmm-input
    --loopback-output-interface fake-winmm-output
    --patch-names A,B,C,D,E,F,G,H,I
    --patch-indexes 0,1,2,3,4,5,6,7,9
    --active-layers 1
    --master-volume 100)
expect_rejected(index-count-not-nine
    --carla-dll fake-carla.dll
    --project fake-project.carxp
    --driver fake-driver
    --client fake-client
    --midi-client "Basic Loopback"
    --midi-port "MIDI 1"
    --input-interface fake-winmm-input
    --loopback-output-interface fake-winmm-output
    --patch-names A,B,C,D,E,F,G,H,I
    --patch-indexes 0,1,2,3,4,5,6,7
    --active-layers 1
    --master-volume 100)
expect_rejected(empty-patch-name
    --carla-dll fake-carla.dll
    --project fake-project.carxp
    --driver fake-driver
    --client fake-client
    --midi-client "Basic Loopback"
    --midi-port "MIDI 1"
    --input-interface fake-winmm-input
    --loopback-output-interface fake-winmm-output
    --patch-names A,,B
    --patch-indexes 0,0,0,0,0,0,0,0,0
    --active-layers 1
    --master-volume 100)
expect_rejected(duplicate-active-layer
    --carla-dll fake-carla.dll
    --project fake-project.carxp
    --driver fake-driver
    --client fake-client
    --midi-client "Basic Loopback"
    --midi-port "MIDI 1"
    --input-interface fake-winmm-input
    --loopback-output-interface fake-winmm-output
    --patch-names A,B,C,D,E,F,G,H,I
    --patch-indexes 0,1,2,3,4,5,6,7,8
    --active-layers 1,1
    --master-volume 100)
expect_rejected(active-layer-out-of-range
    --carla-dll fake-carla.dll
    --project fake-project.carxp
    --driver fake-driver
    --client fake-client
    --midi-client "Basic Loopback"
    --midi-port "MIDI 1"
    --input-interface fake-winmm-input
    --loopback-output-interface fake-winmm-output
    --patch-names A,B,C,D,E,F,G,H,I
    --patch-indexes 0,1,2,3,4,5,6,7,8
    --active-layers 10
    --master-volume 100)
expect_rejected(pad-code-out-of-range
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 17:0,1,2,3,4,5,6,7,8:1)
expect_rejected(pad-code-unsupported
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 2:0,1,2,3,4,5,6,7,8:1)
expect_rejected(duplicate-pad-code
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 3:0,1,2,3,4,5,6,7,8:1
    --pad-profile 3:0,1,2,3,4,5,6,7,8:2)
expect_rejected(pad-profile-bad-index-csv
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 3:0,1,2,3,4,5,6,8:1)
expect_rejected(pad-profile-index-out-of-range
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 3:0,1,2,3,4,5,6,7,9:1)
expect_rejected(pad-profile-bad-layer-csv
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 3:0,1,2,3,4,5,6,7,8:1,1)
expect_rejected(pad-profile-layer-out-of-range
    ${COMMON_ARGUMENTS} --master-volume 100
    --pad-profile 3:0,1,2,3,4,5,6,7,8:10)

message(STATUS
    "Win32 Carla CLI help and malformed-option checks passed without starting host APIs")
