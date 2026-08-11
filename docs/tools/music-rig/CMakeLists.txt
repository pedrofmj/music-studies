cmake_minimum_required(VERSION 3.20)

project(music_rig VERSION 0.1.0 LANGUAGES C)

include(CTest)
option(
    MUSIC_RIG_ENABLE_JSON_C_SPIKE
    "Build the optional json-c dependency and footprint probe"
    OFF
)

add_library(music_rig_core STATIC
    runtime/core/music_rig_core.c
    runtime/core/music_rig_protocol.c
)
target_include_directories(music_rig_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/core/include"
)
target_compile_features(music_rig_core PUBLIC c_std_17)
set_target_properties(music_rig_core PROPERTIES
    C_EXTENSIONS OFF
)

add_executable(music-rig
    music-rig.c
)
target_link_libraries(music-rig PRIVATE music_rig_core)

if(MSVC)
    set(MUSIC_RIG_MSVC_COMPILE_OPTIONS
        /W4
        /WX
        /experimental:c11atomics
    )
    target_compile_options(music_rig_core PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    target_compile_options(music-rig PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
else()
    target_compile_options(music_rig_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    target_compile_options(music-rig PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

if(BUILD_TESTING)
    add_executable(music_rig_core_test
        tests/core-test.c
    )
    target_link_libraries(music_rig_core_test PRIVATE music_rig_core)
    add_executable(music_rig_protocol_test
        tests/protocol-test.c
    )
    target_link_libraries(music_rig_protocol_test PRIVATE music_rig_core)

    if(MSVC)
        target_compile_options(music_rig_core_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_protocol_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
    else()
        target_compile_options(music_rig_core_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_protocol_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()

    add_test(NAME music_rig_core_test COMMAND music_rig_core_test)
    add_test(NAME music_rig_protocol_test COMMAND music_rig_protocol_test)
    add_test(
        NAME music_rig_core_portability
        COMMAND "${CMAKE_COMMAND}"
            "-DCORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/runtime/core"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-portable-core.cmake"
    )
    add_test(
        NAME music_rig_core_portability_guard_self_test
        COMMAND "${CMAKE_COMMAND}"
            "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-portable-core.cmake"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/portable-core-guard-self-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-portable-core-guard.cmake"
    )
    set_tests_properties(
        music_rig_core_portability_guard_self_test
        PROPERTIES TIMEOUT 10
    )
    add_test(
        NAME music_rig_cli_version
        COMMAND $<TARGET_FILE:music-rig> --version
    )

    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    set(MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR
        "${CMAKE_CURRENT_SOURCE_DIR}/validate-performance-rig.py"
    )
    add_test(
        NAME music_rig_profile_schema_self_test
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR}"
            --self-test
    )
    add_test(
        NAME music_rig_current_rig_schema
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR}"
            --validate
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig/rig.json"
    )
    add_test(
        NAME music_rig_current_rig_profile_schema
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR}"
            --validate
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig/rig-profiles/full-live-rack.json"
    )
    add_test(
        NAME music_rig_current_platform_binding_schema
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR}"
            --validate
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig/platform-bindings/linux/airstar-current.json"
    )
    add_test(
        NAME music_rig_current_switch_triggers_schema
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR}"
            --validate
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig/switch-triggers.json"
    )
    add_test(
        NAME music_rig_current_authored_catalogue
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_PROFILE_SCHEMA_VALIDATOR}"
            --validate-root
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig"
            --authority-setup
            "${CMAKE_CURRENT_SOURCE_DIR}/../airstar-live-setup/setup.json"
            --authority-pad-capture
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/hardware-preset-airstar-2026-08-11.json"
    )
    set_tests_properties(
        music_rig_profile_schema_self_test
        music_rig_current_rig_schema
        music_rig_current_rig_profile_schema
        music_rig_current_platform_binding_schema
        music_rig_current_switch_triggers_schema
        music_rig_current_authored_catalogue
        PROPERTIES TIMEOUT 10
    )

    set(MUSIC_RIG_COMPILER
        "${CMAKE_CURRENT_SOURCE_DIR}/compile-performance-rig.py"
    )
    add_test(
        NAME music_rig_compiler_self_test
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-compile-performance-rig"
    )
    add_test(
        NAME music_rig_current_compile_check
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_COMPILER}"
            --rig-root
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig"
            --platform-binding airstar-current
            --check-only
    )
    set_tests_properties(
        music_rig_compiler_self_test
        music_rig_current_compile_check
        PROPERTIES TIMEOUT 30
    )

    set(MUSIC_RIG_SWITCH_BENCHMARK_VALIDATOR
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/validate-switch-benchmark"
    )
    add_test(
        NAME music_rig_switch_benchmark_contract
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_SWITCH_BENCHMARK_VALIDATOR}"
            --check-contract
    )
    add_test(
        NAME music_rig_switch_benchmark_fixture
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_SWITCH_BENCHMARK_VALIDATOR}"
            --validate
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/tests/fixtures/switch-benchmark-result-valid.json"
    )
    add_test(
        NAME music_rig_switch_benchmark_validator_self_test
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_SWITCH_BENCHMARK_VALIDATOR}"
            --self-test
    )
    set_tests_properties(
        music_rig_switch_benchmark_contract
        music_rig_switch_benchmark_fixture
        music_rig_switch_benchmark_validator_self_test
        PROPERTIES TIMEOUT 10
    )

    if(MUSIC_RIG_ENABLE_JSON_C_SPIKE)
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT WIN32)
            message(FATAL_ERROR
                "The json-c footprint harness supports Linux and Windows"
            )
        endif()
        find_package(json-c CONFIG REQUIRED)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(MUSIC_RIG_JSON_C_SPIKE_SOURCE tests/linux-json-c-spike.c)
        else()
            set(MUSIC_RIG_JSON_C_SPIKE_SOURCE tests/windows-json-c-spike.c)
        endif()
        add_executable(
            music_rig_json_c_spike
            ${MUSIC_RIG_JSON_C_SPIKE_SOURCE}
        )
        target_compile_features(music_rig_json_c_spike PRIVATE c_std_17)
        set_target_properties(music_rig_json_c_spike PROPERTIES
            C_EXTENSIONS OFF
        )
        target_link_libraries(music_rig_json_c_spike PRIVATE json-c::json-c)
        target_compile_definitions(music_rig_json_c_spike PRIVATE
            MUSIC_RIG_COMPILER_ID="${CMAKE_C_COMPILER_ID}"
            MUSIC_RIG_COMPILER_VERSION="${CMAKE_C_COMPILER_VERSION}"
        )
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_compile_definitions(music_rig_json_c_spike PRIVATE
                MUSIC_RIG_JSON_C_LIBRARY_PATH="$<TARGET_FILE:json-c::json-c>"
            )
            target_compile_options(music_rig_json_c_spike PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            add_test(
                NAME music_rig_json_c_spike
                COMMAND music_rig_json_c_spike
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/compiled-runtime-minimal.json"
            )
        else()
            target_compile_definitions(music_rig_json_c_spike PRIVATE
                MUSIC_RIG_JSON_C_LINKAGE="static-vcpkg"
            )
            target_compile_options(music_rig_json_c_spike PRIVATE
                ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
            )
            target_link_libraries(music_rig_json_c_spike PRIVATE Psapi)
            add_test(
                NAME music_rig_json_c_spike
                COMMAND music_rig_json_c_spike
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/compiled-runtime-minimal.json"
                    $<TARGET_FILE:music-rig>
            )
        endif()
        set_tests_properties(
            music_rig_json_c_spike
            PROPERTIES TIMEOUT 30
        )
    endif()

    if(UNIX)
        find_package(Threads REQUIRED)
        add_executable(music_rig_generation_adoption_test
            tests/generation-adoption-test.c
        )
        target_link_libraries(music_rig_generation_adoption_test
            PRIVATE
                music_rig_core
                Threads::Threads
        )
        target_compile_options(music_rig_generation_adoption_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        add_test(
            NAME music_rig_generation_adoption
            COMMAND music_rig_generation_adoption_test
        )

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_executable(music_rig_linux_ipc_roundtrip_test
                tests/linux-ipc-roundtrip-test.c
            )
            target_link_libraries(music_rig_linux_ipc_roundtrip_test
                PRIVATE
                    music_rig_core
                    Threads::Threads
            )
            target_compile_options(music_rig_linux_ipc_roundtrip_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            add_test(
                NAME music_rig_linux_ipc_roundtrip
                COMMAND music_rig_linux_ipc_roundtrip_test
            )
            set_tests_properties(
                music_rig_linux_ipc_roundtrip
                PROPERTIES TIMEOUT 10
            )

            add_executable(music_rig_arturia_helper_offline_test
                tests/arturia-main-volume-encoder-offline-test.c
            )
            target_compile_features(
                music_rig_arturia_helper_offline_test
                PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_arturia_helper_offline_test
                PROPERTIES C_EXTENSIONS OFF
            )
            target_compile_options(
                music_rig_arturia_helper_offline_test
                PRIVATE
                    -Wall
                    -Wextra
                    -Wpedantic
                    -Werror
            )
            add_test(
                NAME music_rig_arturia_helper_offline
                COMMAND music_rig_arturia_helper_offline_test
            )

            add_library(music_rig_forbidden_jack STATIC
                tests/forbid-jack-client.c
            )
            target_compile_features(
                music_rig_forbidden_jack
                PRIVATE c_std_17
            )
            set_target_properties(music_rig_forbidden_jack PROPERTIES
                C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_forbidden_jack PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )

            add_executable(music_rig_smk25_helper
                ../smk25-pad-layers/smk25-pad-layers.c
            )
            target_compile_features(
                music_rig_smk25_helper
                PRIVATE c_std_17
            )
            set_target_properties(music_rig_smk25_helper PROPERTIES
                C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_smk25_helper PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            target_link_libraries(
                music_rig_smk25_helper
                PRIVATE music_rig_forbidden_jack
            )
            add_test(
                NAME music_rig_smk25_helper_self_test
                COMMAND music_rig_smk25_helper --self-test
            )
            add_test(
                NAME music_rig_smk25_helper_config
                COMMAND music_rig_smk25_helper --check-config
                    "${CMAKE_CURRENT_SOURCE_DIR}/../smk25-pad-layers/smk25-pad-layers.conf"
            )
            add_test(
                NAME music_rig_smk25_helper_jack_guard
                COMMAND "${CMAKE_COMMAND}"
                    "-DHELPER=$<TARGET_FILE:music_rig_smk25_helper>"
                    "-DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../smk25-pad-layers/smk25-pad-layers.conf"
                    "-DSTATE=${CMAKE_CURRENT_BINARY_DIR}/smk25-jack-guard-state"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-smk25-jack-guard.cmake"
            )
            set_tests_properties(
                music_rig_arturia_helper_offline
                music_rig_smk25_helper_self_test
                music_rig_smk25_helper_config
                music_rig_smk25_helper_jack_guard
                PROPERTIES TIMEOUT 10
            )

            add_test(
                NAME music_rig_current_rack_startup_contract
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/generate-current-rack-startup-transcript"
                    --check-document
                    "${CMAKE_CURRENT_SOURCE_DIR}/CURRENT-RACK-STARTUP.md"
            )
            set_tests_properties(
                music_rig_current_rack_startup_contract
                PROPERTIES TIMEOUT 10
            )

        endif()
        add_test(
            NAME music_rig_airstar_baseline_fixture
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/tests/test-capture-airstar-baseline"
        )
        add_test(
            NAME music_rig_airstar_observer_safety
            COMMAND "${CMAKE_COMMAND}"
                "-DOBSERVER=${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/airstar-read-only-observer"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/tests/check-observer-safety.cmake"
        )
    elseif(WIN32)
        add_executable(music_rig_windows_generation_adoption_test
            tests/windows-generation-adoption-test.c
        )
        target_link_libraries(music_rig_windows_generation_adoption_test
            PRIVATE music_rig_core
        )
        target_compile_options(
            music_rig_windows_generation_adoption_test
            PRIVATE ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        add_test(
            NAME music_rig_windows_generation_adoption
            COMMAND music_rig_windows_generation_adoption_test
        )
        set_tests_properties(
            music_rig_windows_generation_adoption
            PROPERTIES TIMEOUT 10
        )

        add_executable(music_rig_windows_ipc_roundtrip_test
            tests/windows-ipc-roundtrip-test.c
        )
        target_link_libraries(music_rig_windows_ipc_roundtrip_test
            PRIVATE
                music_rig_core
                Advapi32
        )
        target_compile_options(
            music_rig_windows_ipc_roundtrip_test
            PRIVATE ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        add_test(
            NAME music_rig_windows_ipc_roundtrip
            COMMAND music_rig_windows_ipc_roundtrip_test
        )
        set_tests_properties(
            music_rig_windows_ipc_roundtrip
            PROPERTIES TIMEOUT 10
        )

        add_executable(music_rig_windows_resource_measurement_test
            tests/windows-resource-measurement-test.c
        )
        target_link_libraries(
            music_rig_windows_resource_measurement_test
            PRIVATE Psapi
        )
        target_compile_options(
            music_rig_windows_resource_measurement_test
            PRIVATE ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        add_test(
            NAME music_rig_windows_resource_measurement
            COMMAND music_rig_windows_resource_measurement_test
                --self-test
                $<TARGET_FILE:music-rig>
        )
        set_tests_properties(
            music_rig_windows_resource_measurement
            PROPERTIES TIMEOUT 10
        )
    endif()

endif()
