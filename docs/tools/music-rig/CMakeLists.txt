cmake_minimum_required(VERSION 3.20)

project(music_rig VERSION 0.1.0 LANGUAGES C)

include(CTest)
option(
    MUSIC_RIG_ENABLE_JSON_C_SPIKE
    "Build the optional json-c definition adapter and footprint probe"
    OFF
)

add_library(music_rig_core STATIC
    runtime/core/music_rig_compiled_tables.c
    runtime/core/music_rig_control.c
    runtime/core/music_rig_core.c
    runtime/core/music_rig_definition.c
    runtime/core/music_rig_device_ports.c
    runtime/core/music_rig_diagnostics.c
    runtime/core/music_rig_protocol.c
    runtime/core/music_rig_runtime.c
    runtime/core/music_rig_state.c
)
target_include_directories(music_rig_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/core/include"
)
target_compile_features(music_rig_core PUBLIC c_std_17)
set_target_properties(music_rig_core PROPERTIES
    C_EXTENSIONS OFF
)

add_library(music_rig_current_behaviors STATIC
    runtime/behaviors/music_rig_current_arturia.c
    runtime/behaviors/music_rig_current_smk25.c
)
target_include_directories(music_rig_current_behaviors
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/behaviors/include"
)
target_link_libraries(music_rig_current_behaviors PUBLIC music_rig_core)
target_compile_features(music_rig_current_behaviors PUBLIC c_std_17)
set_target_properties(music_rig_current_behaviors PROPERTIES C_EXTENSIONS OFF)

add_library(music_rig_device_midi_shadow STATIC
    runtime/adapters/music_rig_device_midi_shadow.c
)
target_include_directories(music_rig_device_midi_shadow
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/adapters/include"
)
target_link_libraries(music_rig_device_midi_shadow
    PUBLIC music_rig_core music_rig_current_behaviors
)
target_compile_features(music_rig_device_midi_shadow PUBLIC c_std_17)
set_target_properties(music_rig_device_midi_shadow PROPERTIES C_EXTENSIONS OFF)

add_library(music_rig_smc_mixer_relay STATIC
    runtime/adapters/music_rig_smc_mixer_relay.c
)
target_include_directories(music_rig_smc_mixer_relay
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/adapters/include"
)
target_link_libraries(music_rig_smc_mixer_relay PUBLIC music_rig_core)
target_compile_features(music_rig_smc_mixer_relay PUBLIC c_std_17)
set_target_properties(music_rig_smc_mixer_relay PROPERTIES C_EXTENSIONS OFF)

add_library(music_rig_cli STATIC
    runtime/client/music_rig_cli.c
)
target_include_directories(music_rig_cli
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/client/include"
)
target_link_libraries(music_rig_cli PUBLIC music_rig_core)
target_compile_features(music_rig_cli PUBLIC c_std_17)
set_target_properties(music_rig_cli PROPERTIES C_EXTENSIONS OFF)

if(WIN32)
    set(MUSIC_RIG_FILE_STORAGE_SOURCE
        runtime/windows/music_rig_file_storage.c
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MUSIC_RIG_FILE_STORAGE_SOURCE
        runtime/linux/music_rig_file_storage.c
    )
endif()

if(DEFINED MUSIC_RIG_FILE_STORAGE_SOURCE)
    add_library(music_rig_file_storage STATIC
        ${MUSIC_RIG_FILE_STORAGE_SOURCE}
    )
    target_include_directories(music_rig_file_storage
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
    )
    target_link_libraries(music_rig_file_storage PUBLIC music_rig_core)
    target_compile_features(music_rig_file_storage PUBLIC c_std_17)
    set_target_properties(music_rig_file_storage PROPERTIES
        C_EXTENSIONS OFF
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(music_rig_linux_host STATIC
        runtime/linux/music_rig_linux_control_socket.c
        runtime/linux/music_rig_host_paths.c
        runtime/linux/music_rig_journal_diagnostics.c
        runtime/linux/music_rig_linux_lifecycle.c
    )
    target_include_directories(music_rig_linux_host
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/runtime/linux/include"
    )
    target_link_libraries(music_rig_linux_host PUBLIC music_rig_core)
    target_compile_features(music_rig_linux_host PUBLIC c_std_17)
    set_target_properties(music_rig_linux_host PROPERTIES C_EXTENSIONS OFF)

    find_library(MUSIC_RIG_JACK_LIBRARY NAMES jack libjack.so.0)
    if(MUSIC_RIG_JACK_LIBRARY)
        add_library(music_rig_jack_midi_shadow STATIC
            runtime/linux/music_rig_jack_midi_shadow.c
        )
        target_include_directories(music_rig_jack_midi_shadow
            PUBLIC
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
        )
        target_link_libraries(music_rig_jack_midi_shadow
            PUBLIC music_rig_device_midi_shadow
            PRIVATE "${MUSIC_RIG_JACK_LIBRARY}"
        )
        target_compile_features(music_rig_jack_midi_shadow PUBLIC c_std_17)
        set_target_properties(music_rig_jack_midi_shadow PROPERTIES
            C_EXTENSIONS OFF
        )

        add_library(music_rig_jack_midi_output STATIC
            runtime/linux/music_rig_jack_midi_output.c
        )
        target_include_directories(music_rig_jack_midi_output
            PUBLIC
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
        )
        target_link_libraries(music_rig_jack_midi_output
            PUBLIC music_rig_device_midi_shadow
            PRIVATE "${MUSIC_RIG_JACK_LIBRARY}"
        )
        target_compile_features(music_rig_jack_midi_output PUBLIC c_std_17)
        set_target_properties(music_rig_jack_midi_output PROPERTIES
            C_EXTENSIONS OFF
        )

        add_library(music_rig_jack_smc_mixer_relay STATIC
            runtime/linux/music_rig_jack_smc_mixer_relay.c
        )
        target_include_directories(music_rig_jack_smc_mixer_relay
            PUBLIC
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
        )
        target_link_libraries(music_rig_jack_smc_mixer_relay
            PUBLIC music_rig_smc_mixer_relay
            PRIVATE "${MUSIC_RIG_JACK_LIBRARY}"
        )
        target_compile_features(
            music_rig_jack_smc_mixer_relay PUBLIC c_std_17
        )
        set_target_properties(music_rig_jack_smc_mixer_relay PROPERTIES
            C_EXTENSIONS OFF
        )
    endif()
endif()

if(MUSIC_RIG_ENABLE_JSON_C_SPIKE)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT WIN32)
        message(FATAL_ERROR
            "The json-c adapter supports Linux and Windows"
        )
    endif()
    find_package(json-c CONFIG REQUIRED)
    add_library(music_rig_definition_json STATIC
        runtime/json-c/music_rig_definition_json.c
    )
    target_compile_features(music_rig_definition_json PRIVATE c_std_17)
    set_target_properties(music_rig_definition_json PROPERTIES
        C_EXTENSIONS OFF
    )
    target_link_libraries(music_rig_definition_json
        PUBLIC music_rig_core
        PRIVATE json-c::json-c
    )
endif()

add_executable(music-rig
    music-rig.c
)
target_link_libraries(music-rig PRIVATE music_rig_cli)
if(TARGET music_rig_linux_host)
    target_compile_definitions(music-rig PRIVATE MUSIC_RIG_HAS_LINUX_HOST=1)
    target_link_libraries(music-rig PRIVATE music_rig_linux_host)
endif()

add_executable(music_rig_compiled_table_composition_test
    tests/compiled-table-composition-test.c
)
target_link_libraries(music_rig_compiled_table_composition_test
    PRIVATE music_rig_core
)
add_test(
    NAME music_rig_compiled_table_composition
    COMMAND music_rig_compiled_table_composition_test
)

add_executable(music-rigd
    music-rigd.c
)
target_link_libraries(music-rigd PRIVATE music_rig_core)
if(TARGET music_rig_file_storage)
    target_compile_definitions(music-rigd PRIVATE
        MUSIC_RIG_HAS_FILE_STORAGE=1
    )
    target_link_libraries(music-rigd PRIVATE music_rig_file_storage)
endif()
if(TARGET music_rig_linux_host)
    target_compile_definitions(music-rigd PRIVATE
        MUSIC_RIG_HAS_LINUX_HOST=1
    )
    target_link_libraries(music-rigd PRIVATE music_rig_linux_host)
endif()
if(TARGET music_rig_jack_midi_shadow)
    target_compile_definitions(music-rigd PRIVATE
        MUSIC_RIG_HAS_JACK_MIDI_SHADOW=1
    )
    target_link_libraries(music-rigd PRIVATE music_rig_jack_midi_shadow)
endif()
if(TARGET music_rig_jack_midi_output)
    target_compile_definitions(music-rigd PRIVATE
        MUSIC_RIG_HAS_JACK_MIDI_OUTPUT=1
    )
    target_link_libraries(music-rigd PRIVATE music_rig_jack_midi_output)
endif()
set(MUSIC_RIG_SMC_MIXER_RELAY_COMMAND_ENABLED OFF)
if(TARGET music_rig_jack_smc_mixer_relay AND
    MUSIC_RIG_ENABLE_JSON_C_SPIKE)
    set(MUSIC_RIG_SMC_MIXER_RELAY_COMMAND_ENABLED ON)
    target_compile_definitions(music-rigd PRIVATE
        MUSIC_RIG_HAS_JACK_SMC_MIXER_RELAY=1
    )
    target_link_libraries(music-rigd PRIVATE music_rig_jack_smc_mixer_relay)
endif()
if(MUSIC_RIG_ENABLE_JSON_C_SPIKE)
    target_compile_definitions(music-rigd PRIVATE
        MUSIC_RIG_ENABLE_JSON_DEFINITION=1
    )
    target_link_libraries(music-rigd PRIVATE music_rig_definition_json)
endif()

if(MSVC)
    set(MUSIC_RIG_MSVC_COMPILE_OPTIONS
        /W4
        /WX
        /experimental:c11atomics
    )
    target_compile_options(music_rig_core PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    target_compile_options(music_rig_current_behaviors PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    target_compile_options(music_rig_device_midi_shadow PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    target_compile_options(music_rig_smc_mixer_relay PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    target_compile_options(music_rig_cli PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    if(TARGET music_rig_file_storage)
        target_compile_options(music_rig_file_storage PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
    endif()
    if(TARGET music_rig_definition_json)
        target_compile_options(music_rig_definition_json PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
    endif()
    target_compile_options(music-rig PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
    target_compile_options(music-rigd PRIVATE
        ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
    )
else()
    target_compile_options(music_rig_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    target_compile_options(music_rig_current_behaviors PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    target_compile_options(music_rig_device_midi_shadow PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    target_compile_options(music_rig_smc_mixer_relay PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    target_compile_options(music_rig_cli PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    if(TARGET music_rig_file_storage)
        target_compile_options(music_rig_file_storage PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
    if(TARGET music_rig_linux_host)
        target_compile_options(music_rig_linux_host PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
    if(TARGET music_rig_jack_midi_shadow)
        target_compile_options(music_rig_jack_midi_shadow PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
    if(TARGET music_rig_jack_midi_output)
        target_compile_options(music_rig_jack_midi_output PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
    if(TARGET music_rig_jack_smc_mixer_relay)
        target_compile_options(music_rig_jack_smc_mixer_relay PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
    if(TARGET music_rig_definition_json)
        target_compile_options(music_rig_definition_json PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
    target_compile_options(music-rig PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    target_compile_options(music-rigd PRIVATE
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
    add_executable(music_rig_device_ports_test
        tests/device-ports-test.c
    )
    target_link_libraries(music_rig_device_ports_test PRIVATE music_rig_core)
    add_executable(music_rig_diagnostics_test
        tests/diagnostics-test.c
    )
    target_link_libraries(music_rig_diagnostics_test PRIVATE music_rig_core)
    add_executable(music_rig_current_behaviors_test
        tests/current-behaviors-test.c
    )
    target_link_libraries(music_rig_current_behaviors_test
        PRIVATE music_rig_current_behaviors
    )
    add_executable(music_rig_device_midi_shadow_test
        tests/device-midi-shadow-test.c
    )
    target_link_libraries(music_rig_device_midi_shadow_test
        PRIVATE music_rig_device_midi_shadow
    )
    add_executable(music_rig_smc_mixer_relay_test
        tests/smc-mixer-relay-test.c
    )
    target_link_libraries(music_rig_smc_mixer_relay_test
        PRIVATE music_rig_smc_mixer_relay
    )
    add_executable(music_rig_runtime_test
        tests/runtime-test.c
    )
    target_link_libraries(music_rig_runtime_test PRIVATE music_rig_core)
    add_executable(music_rig_switch_performance_test
        tests/switch-performance-test.c
    )
    target_link_libraries(music_rig_switch_performance_test PRIVATE music_rig_core)
    target_compile_features(music_rig_switch_performance_test PRIVATE c_std_17)
    set_target_properties(music_rig_switch_performance_test PROPERTIES
        C_EXTENSIONS OFF
    )
    add_executable(music_rig_control_test
        tests/control-test.c
    )
    target_link_libraries(music_rig_control_test PRIVATE music_rig_core)
    add_executable(music_rig_cli_test
        tests/cli-test.c
    )
    target_link_libraries(music_rig_cli_test PRIVATE music_rig_cli)
    add_executable(music_rig_state_test
        tests/state-test.c
    )
    target_link_libraries(music_rig_state_test PRIVATE music_rig_core)
    add_executable(music_rig_definition_test
        tests/definition-test.c
    )
    target_link_libraries(music_rig_definition_test PRIVATE music_rig_core)
    if(TARGET music_rig_file_storage)
        add_executable(music_rig_file_storage_test
            tests/file-storage-test.c
        )
        target_link_libraries(music_rig_file_storage_test
            PRIVATE music_rig_file_storage
        )
    endif()
    if(TARGET music_rig_linux_host)
        add_executable(music_rig_linux_host_test
            tests/linux-host-test.c
        )
        target_link_libraries(music_rig_linux_host_test
            PRIVATE music_rig_linux_host
        )
        add_executable(music_rig_linux_lifecycle_process_test
            tests/linux-lifecycle-process-test.c
        )
        target_compile_features(
            music_rig_linux_lifecycle_process_test PRIVATE c_std_17
        )
        set_target_properties(
            music_rig_linux_lifecycle_process_test PROPERTIES C_EXTENSIONS OFF
        )
        add_executable(music_rig_jack_midi_shadow_offline_test
            tests/jack-midi-shadow-offline-test.c
            runtime/linux/music_rig_jack_midi_shadow.c
        )
        target_include_directories(music_rig_jack_midi_shadow_offline_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
        )
        target_link_libraries(music_rig_jack_midi_shadow_offline_test
            PRIVATE music_rig_device_midi_shadow
        )
        target_compile_features(
            music_rig_jack_midi_shadow_offline_test PRIVATE c_std_17
        )
        set_target_properties(
            music_rig_jack_midi_shadow_offline_test PROPERTIES
                C_EXTENSIONS OFF
        )
        add_executable(music_rig_jack_midi_output_offline_test
            tests/jack-midi-output-offline-test.c
            runtime/linux/music_rig_jack_midi_output.c
        )
        target_include_directories(music_rig_jack_midi_output_offline_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
        )
        target_link_libraries(music_rig_jack_midi_output_offline_test
            PRIVATE music_rig_device_midi_shadow
        )
        target_compile_features(
            music_rig_jack_midi_output_offline_test PRIVATE c_std_17
        )
        set_target_properties(music_rig_jack_midi_output_offline_test PROPERTIES
            C_EXTENSIONS OFF
        )
        target_compile_options(music_rig_jack_midi_output_offline_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        add_executable(music_rig_jack_smc_mixer_relay_offline_test
            tests/jack-smc-mixer-relay-offline-test.c
            runtime/linux/music_rig_jack_smc_mixer_relay.c
        )
        target_include_directories(
            music_rig_jack_smc_mixer_relay_offline_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/platform/include"
        )
        target_link_libraries(music_rig_jack_smc_mixer_relay_offline_test
            PRIVATE music_rig_smc_mixer_relay
        )
        target_compile_features(
            music_rig_jack_smc_mixer_relay_offline_test PRIVATE c_std_17
        )
        set_target_properties(
            music_rig_jack_smc_mixer_relay_offline_test PROPERTIES
                C_EXTENSIONS OFF
        )
    endif()

    if(MSVC)
        target_compile_options(music_rig_core_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_protocol_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_device_ports_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_diagnostics_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_current_behaviors_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_device_midi_shadow_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_smc_mixer_relay_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_runtime_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_switch_performance_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_control_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_cli_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_state_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        target_compile_options(music_rig_definition_test PRIVATE
            ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
        )
        if(TARGET music_rig_file_storage_test)
            target_compile_options(music_rig_file_storage_test PRIVATE
                ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
            )
        endif()
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
        target_compile_options(music_rig_device_ports_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_diagnostics_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_current_behaviors_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_device_midi_shadow_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_smc_mixer_relay_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_runtime_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_switch_performance_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_control_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_cli_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_state_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        target_compile_options(music_rig_definition_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
        if(TARGET music_rig_file_storage_test)
            target_compile_options(music_rig_file_storage_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
        endif()
        if(TARGET music_rig_linux_host_test)
            target_compile_options(music_rig_linux_host_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            target_compile_options(
                music_rig_linux_lifecycle_process_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            target_compile_options(
                music_rig_jack_midi_shadow_offline_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            target_compile_options(
                music_rig_jack_smc_mixer_relay_offline_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
        endif()
    endif()

    add_test(NAME music_rig_core_test COMMAND music_rig_core_test)
    add_test(NAME music_rig_protocol_test COMMAND music_rig_protocol_test)
    add_test(NAME music_rig_device_ports_test COMMAND music_rig_device_ports_test)
    add_test(NAME music_rig_diagnostics_test COMMAND music_rig_diagnostics_test)
    add_test(
        NAME music_rig_current_behaviors_test
        COMMAND music_rig_current_behaviors_test
    )
    add_test(
        NAME music_rig_device_midi_shadow_test
        COMMAND music_rig_device_midi_shadow_test
    )
    add_test(
        NAME music_rig_smc_mixer_relay_test
        COMMAND music_rig_smc_mixer_relay_test
    )
    add_test(NAME music_rig_runtime_test COMMAND music_rig_runtime_test)
    add_test(
        NAME music_rig_switch_performance
        COMMAND music_rig_switch_performance_test
    )
    add_test(NAME music_rig_control_test COMMAND music_rig_control_test)
    add_test(NAME music_rig_cli_test COMMAND music_rig_cli_test)
    add_test(NAME music_rig_state_test COMMAND music_rig_state_test)
    add_test(NAME music_rig_definition_test COMMAND music_rig_definition_test)
    if(TARGET music_rig_file_storage_test)
        add_test(
            NAME music_rig_file_storage_test
            COMMAND music_rig_file_storage_test
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/compiled-current-envelope.json"
                "${CMAKE_CURRENT_BINARY_DIR}/file-storage-state.bin"
                "${CMAKE_CURRENT_BINARY_DIR}/missing-storage-parent/state.bin"
        )
    endif()
    if(TARGET music_rig_linux_host_test)
        add_test(
            NAME music_rig_linux_host_test
            COMMAND music_rig_linux_host_test
        )
        add_test(
            NAME music_rig_linux_lifecycle_process_test
            COMMAND music_rig_linux_lifecycle_process_test
                $<TARGET_FILE:music-rigd>
        )
        set_tests_properties(
            music_rig_linux_lifecycle_process_test
            PROPERTIES TIMEOUT 5
        )
        add_test(
            NAME music_rig_jack_midi_shadow_offline_test
            COMMAND music_rig_jack_midi_shadow_offline_test
        )
        add_test(
            NAME music_rig_jack_midi_output_offline_test
            COMMAND music_rig_jack_midi_output_offline_test
        )
        add_test(
            NAME music_rig_jack_smc_mixer_relay_offline_test
            COMMAND music_rig_jack_smc_mixer_relay_offline_test
        )
        add_test(
            NAME music_rigd_linux_host_paths
            COMMAND "${CMAKE_COMMAND}"
                "-DDAEMON=$<TARGET_FILE:music-rigd>"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/host-path-check"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-music-rigd-host-paths.cmake"
        )
        add_test(
            NAME music_rig_systemd_shadow_unit
            COMMAND "${CMAKE_COMMAND}"
                "-DUNIT=${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/music-rigd.service"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-systemd-shadow-unit.cmake"
        )
    endif()
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
    add_test(
        NAME music_rig_cli_no_transport
        COMMAND "${CMAKE_COMMAND}"
            "-DCLI=$<TARGET_FILE:music-rig>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-music-rig-no-transport.cmake"
    )
    set_tests_properties(music_rig_cli_no_transport PROPERTIES TIMEOUT 5)
    add_test(
        NAME music_rigd_version
        COMMAND $<TARGET_FILE:music-rigd> --version
    )
    add_test(
        NAME music_rigd_no_default_activation
        COMMAND "${CMAKE_COMMAND}"
            "-DDAEMON=$<TARGET_FILE:music-rigd>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-music-rigd-no-default.cmake"
    )
    set_tests_properties(
        music_rigd_no_default_activation
        PROPERTIES TIMEOUT 5
    )
    add_test(
        NAME music_rigd_smc_mixer_activation_boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DDAEMON=$<TARGET_FILE:music-rigd>"
            "-DEXPECT_RELAY=${MUSIC_RIG_SMC_MIXER_RELAY_COMMAND_ENABLED}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-music-rigd-smc-mixer-boundary.cmake"
    )
    set_tests_properties(
        music_rigd_smc_mixer_activation_boundary
        PROPERTIES TIMEOUT 5
    )
    add_test(
        NAME music_rig_runtime_boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DRUNTIME_DIR=${CMAKE_CURRENT_SOURCE_DIR}/runtime/core"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-runtime-boundary.cmake"
    )
    add_test(
        NAME music_rig_current_behaviors_boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DBEHAVIOR_DIR=${CMAKE_CURRENT_SOURCE_DIR}/runtime/behaviors"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-current-behaviors-boundary.cmake"
    )
    add_test(
        NAME music_rig_device_midi_shadow_boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DSHADOW_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/runtime/adapters/music_rig_device_midi_shadow.c"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-device-midi-shadow-boundary.cmake"
    )
    add_test(
        NAME music_rig_smc_mixer_relay_boundary
        COMMAND "${CMAKE_COMMAND}"
            "-DRELAY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/runtime/adapters/music_rig_smc_mixer_relay.c"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-smc-mixer-relay-boundary.cmake"
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(
            NAME music_rig_jack_midi_shadow_boundary
            COMMAND "${CMAKE_COMMAND}"
                "-DJACK_SHADOW_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/runtime/linux/music_rig_jack_midi_shadow.c"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-jack-midi-shadow-boundary.cmake"
        )
        add_test(
            NAME music_rig_jack_smc_mixer_relay_boundary
            COMMAND "${CMAKE_COMMAND}"
                "-DJACK_RELAY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/runtime/linux/music_rig_jack_smc_mixer_relay.c"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-jack-smc-mixer-relay-boundary.cmake"
        )
        add_test(
            NAME music_rig_smc_mixer_links_transaction
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-smc-mixer-links"
        )
        set_tests_properties(
            music_rig_smc_mixer_links_transaction PROPERTIES TIMEOUT 60
        )
        add_test(
            NAME music_rig_pipewire_patchbay_identity
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-pipewire-patchbay-identity"
        )
        set_tests_properties(
            music_rig_pipewire_patchbay_identity PROPERTIES TIMEOUT 30
        )
    endif()

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

    add_test(
        NAME music_rig_materializer_self_test
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-materialize-compiled-rig"
    )
    set_tests_properties(
        music_rig_materializer_self_test
        PROPERTIES TIMEOUT 30
    )

    add_test(
        NAME music_rig_materialized_parity_self_test
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-materialized-rig-parity"
    )
    set_tests_properties(
        music_rig_materialized_parity_self_test
        PROPERTIES TIMEOUT 30
    )

    add_test(
        NAME music_rig_protected_single_rig_regression
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-protected-single-rig-regression"
    )
    set_tests_properties(
        music_rig_protected_single_rig_regression
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

    set(MUSIC_RIG_M3_SHADOW_EVIDENCE_VALIDATOR
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/validate-m3-shadow-evidence"
    )
    add_test(
        NAME music_rig_m3_shadow_evidence
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_M3_SHADOW_EVIDENCE_VALIDATOR}"
            --validate
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/m3-shadow-evidence-2026-08-12.json"
    )
    add_test(
        NAME music_rig_m3_shadow_evidence_validator_self_test
        COMMAND "${Python3_EXECUTABLE}"
            "${MUSIC_RIG_M3_SHADOW_EVIDENCE_VALIDATOR}"
            --self-test
    )
    set_tests_properties(
        music_rig_m3_shadow_evidence
        music_rig_m3_shadow_evidence_validator_self_test
        PROPERTIES TIMEOUT 10
    )

    if(MUSIC_RIG_ENABLE_JSON_C_SPIKE)
        set(MUSIC_RIG_JSON_DEFINITION_FIXTURE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/compiled-current-envelope.json"
        )
        set(MUSIC_RIG_JSON_DEFINITION_FINGERPRINT
            "sha256:b5988448d8346ffdca61bbcebd3d0b8bd4c62ae214923fe55bf1ffdbffbb22c3"
        )
        set(MUSIC_RIG_MIXED_PADS_DEFINITION
            "${CMAKE_CURRENT_BINARY_DIR}/compiled-mixed-pads.json"
        )
        execute_process(
            COMMAND "${Python3_EXECUTABLE}"
                "${MUSIC_RIG_COMPILER}"
                --rig-root
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/performance-rigs/pedro-performance-rig"
                --platform-binding airstar-current
                --rig-profile multilevel-volume-mixed-pads
                --output "${MUSIC_RIG_MIXED_PADS_DEFINITION}"
            RESULT_VARIABLE mixed_pads_result
            OUTPUT_VARIABLE mixed_pads_output
            ERROR_VARIABLE mixed_pads_error
        )
        if(NOT mixed_pads_result EQUAL 0)
            message(FATAL_ERROR
                "could not compile mixed-pad definition: ${mixed_pads_error}"
            )
        endif()
        string(REGEX MATCH
            "definition=sha256:[0-9a-f]+"
            mixed_pads_fingerprint_match
            "${mixed_pads_output}"
        )
        string(REPLACE "definition=" "" MUSIC_RIG_MIXED_PADS_FINGERPRINT
            "${mixed_pads_fingerprint_match}"
        )
        if(MUSIC_RIG_MIXED_PADS_FINGERPRINT STREQUAL "")
            message(FATAL_ERROR "mixed-pad definition fingerprint was not reported")
        endif()
        add_executable(music_rig_json_definition_test
            tests/json-definition-test.c
        )
        target_link_libraries(music_rig_json_definition_test
            PRIVATE music_rig_definition_json music_rig_device_midi_shadow
        )
        add_executable(music_rig_shadow_idle_workload
            tests/shadow-idle-workload.c
        )
        target_link_libraries(music_rig_shadow_idle_workload
            PRIVATE
                music_rig_definition_json
                music_rig_device_midi_shadow
                music_rig_file_storage
        )
        target_compile_features(
            music_rig_shadow_idle_workload PRIVATE c_std_17
        )
        set_target_properties(music_rig_shadow_idle_workload PROPERTIES
            C_EXTENSIONS OFF
        )
        if(TARGET music_rig_jack_midi_shadow)
            add_library(music_rig_fake_jack_midi_shadow SHARED
                tests/fake-jack-midi-shadow.c
            )
            target_compile_features(
                music_rig_fake_jack_midi_shadow PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_fake_jack_midi_shadow PROPERTIES C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_fake_jack_midi_shadow PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
                -fno-sanitize=all
            )
            target_link_options(music_rig_fake_jack_midi_shadow PRIVATE
                -fno-sanitize=all
            )
            add_executable(music_rig_midi_shadow_process_test
                tests/midi-shadow-process-test.c
            )
            target_compile_features(
                music_rig_midi_shadow_process_test PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_midi_shadow_process_test PROPERTIES C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_midi_shadow_process_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
        endif()
        if(TARGET music_rig_jack_midi_output)
            find_package(Threads REQUIRED)
            add_library(music_rig_fake_jack_midi_output SHARED
                tests/fake-jack-midi-output.c
            )
            target_compile_features(
                music_rig_fake_jack_midi_output PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_fake_jack_midi_output PROPERTIES C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_fake_jack_midi_output PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
                -fno-sanitize=all
            )
            target_link_options(music_rig_fake_jack_midi_output PRIVATE
                -fno-sanitize=all
            )
            target_link_libraries(music_rig_fake_jack_midi_output PRIVATE
                Threads::Threads
            )
            add_executable(music_rig_midi_output_process_test
                tests/midi-output-process-test.c
            )
            target_compile_features(
                music_rig_midi_output_process_test PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_midi_output_process_test PROPERTIES C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_midi_output_process_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
        endif()
        if(TARGET music_rig_jack_smc_mixer_relay)
            add_library(music_rig_fake_jack_smc_mixer_relay SHARED
                tests/fake-jack-smc-mixer-relay.c
            )
            target_compile_features(
                music_rig_fake_jack_smc_mixer_relay PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_fake_jack_smc_mixer_relay PROPERTIES
                    C_EXTENSIONS OFF
            )
            target_compile_options(
                music_rig_fake_jack_smc_mixer_relay PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
                -fno-sanitize=all
            )
            target_link_options(music_rig_fake_jack_smc_mixer_relay PRIVATE
                -fno-sanitize=all
            )
            add_executable(music_rig_smc_mixer_relay_process_test
                tests/smc-mixer-relay-process-test.c
            )
            target_compile_features(
                music_rig_smc_mixer_relay_process_test PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_smc_mixer_relay_process_test PROPERTIES
                    C_EXTENSIONS OFF
            )
            target_compile_options(
                music_rig_smc_mixer_relay_process_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
        endif()
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
            target_compile_options(music_rig_json_definition_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            target_compile_options(music_rig_shadow_idle_workload PRIVATE
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
            target_compile_options(music_rig_json_definition_test PRIVATE
                ${MUSIC_RIG_MSVC_COMPILE_OPTIONS}
            )
            target_compile_options(music_rig_shadow_idle_workload PRIVATE
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
        add_test(
            NAME music_rig_json_definition
            COMMAND music_rig_json_definition_test
                "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
        )
        add_test(
            NAME music_rig_shadow_idle_workload
            COMMAND music_rig_shadow_idle_workload
                --definition "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                --expected-fingerprint
                "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                --duration-ms 50
        )
        set_tests_properties(
            music_rig_shadow_idle_workload PROPERTIES TIMEOUT 5
        )
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_executable(music_rig_daemon_ipc_process_test
                tests/daemon-ipc-process-test.c
            )
            target_compile_features(music_rig_daemon_ipc_process_test PRIVATE c_std_17)
            set_target_properties(music_rig_daemon_ipc_process_test PROPERTIES
                C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_daemon_ipc_process_test PRIVATE
                -Wall -Wextra -Wpedantic -Werror
            )
            add_test(
                NAME music_rig_daemon_ipc_process
                COMMAND music_rig_daemon_ipc_process_test
                    $<TARGET_FILE:music-rigd>
                    $<TARGET_FILE:music-rig>
                    "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    "${MUSIC_RIG_MIXED_PADS_DEFINITION}"
                    "${MUSIC_RIG_MIXED_PADS_FINGERPRINT}"
            )
            add_test(
                NAME music_rig_shadow_idle_resource_mechanics
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/measure-shadow-idle-linux"
                    --workload $<TARGET_FILE:music_rig_shadow_idle_workload>
                    --definition "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    --expected-fingerprint
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    --source-commit self-test
                    --duration-ms 250
                    --self-test
            )
        else()
            add_test(
                NAME music_rig_shadow_idle_resource_mechanics
                COMMAND powershell
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/measure-shadow-idle-windows.ps1"
                    -WorkloadPath
                    $<TARGET_FILE:music_rig_shadow_idle_workload>
                    -DefinitionPath
                    "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    -ExpectedFingerprint
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    -SourceCommit self-test
                    -DurationMs 250
                    -SelfTest
            )
        endif()
        set_tests_properties(
            music_rig_shadow_idle_resource_mechanics PROPERTIES TIMEOUT 10
        )
        if(TARGET music_rig_midi_shadow_process_test)
            add_test(
                NAME music_rig_midi_shadow_process
                COMMAND music_rig_midi_shadow_process_test
                    $<TARGET_FILE:music-rigd>
                    "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    $<TARGET_FILE:music_rig_fake_jack_midi_shadow>
            )
            set_tests_properties(
                music_rig_midi_shadow_process PROPERTIES TIMEOUT 5
            )
        endif()
        if(TARGET music_rig_midi_output_process_test)
            add_test(
                NAME music_rig_midi_output_process
                COMMAND music_rig_midi_output_process_test
                    $<TARGET_FILE:music-rigd>
                    "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    $<TARGET_FILE:music_rig_fake_jack_midi_output>
            )
            set_tests_properties(
                music_rig_midi_output_process PROPERTIES TIMEOUT 5
            )
            add_executable(music_rig_output_runtime_process_test
                tests/output-runtime-process-test.c
            )
            target_compile_features(
                music_rig_output_runtime_process_test PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_output_runtime_process_test PROPERTIES
                    C_EXTENSIONS OFF
            )
            target_compile_options(music_rig_output_runtime_process_test PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
            )
            add_test(
                NAME music_rig_output_runtime_process
                COMMAND music_rig_output_runtime_process_test
                    $<TARGET_FILE:music-rigd>
                    $<TARGET_FILE:music-rig>
                    "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    "${MUSIC_RIG_MIXED_PADS_DEFINITION}"
                    "${MUSIC_RIG_MIXED_PADS_FINGERPRINT}"
                    $<TARGET_FILE:music_rig_fake_jack_midi_output>
            )
            set_tests_properties(
                music_rig_output_runtime_process PROPERTIES TIMEOUT 5
            )
        endif()
        if(TARGET music_rig_smc_mixer_relay_process_test)
            add_test(
                NAME music_rig_smc_mixer_relay_process
                COMMAND music_rig_smc_mixer_relay_process_test
                    $<TARGET_FILE:music-rigd>
                    "${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                    "${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                    $<TARGET_FILE:music_rig_fake_jack_smc_mixer_relay>
            )
            set_tests_properties(
                music_rig_smc_mixer_relay_process PROPERTIES TIMEOUT 5
            )
        endif()
        set_tests_properties(
            music_rig_json_definition
            PROPERTIES TIMEOUT 10
        )
        add_test(
            NAME music_rigd_definition_validation
            COMMAND "${CMAKE_COMMAND}"
                "-DDAEMON=$<TARGET_FILE:music-rigd>"
                "-DDEFINITION=${MUSIC_RIG_JSON_DEFINITION_FIXTURE}"
                "-DEXPECTED_FINGERPRINT=${MUSIC_RIG_JSON_DEFINITION_FINGERPRINT}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-music-rigd-definition.cmake"
        )
        set_tests_properties(
            music_rigd_definition_validation
            PROPERTIES TIMEOUT 10
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
            target_link_libraries(
                music_rig_arturia_helper_offline_test
                PRIVATE music_rig_current_behaviors
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
            add_executable(music_rig_smk25_behavior_parity_test
                tests/smk25-current-behavior-parity-test.c
            )
            target_link_libraries(
                music_rig_smk25_behavior_parity_test
                PRIVATE
                    music_rig_current_behaviors
                    music_rig_forbidden_jack
            )
            target_compile_features(
                music_rig_smk25_behavior_parity_test PRIVATE c_std_17
            )
            set_target_properties(
                music_rig_smk25_behavior_parity_test
                PROPERTIES C_EXTENSIONS OFF
            )
            target_compile_options(
                music_rig_smk25_behavior_parity_test PRIVATE
                    -Wall
                    -Wextra
                    -Wpedantic
                    -Werror
            )
            add_test(
                NAME music_rig_smk25_helper_self_test
                COMMAND music_rig_smk25_helper --self-test
            )
            add_test(
                NAME music_rig_smk25_behavior_parity
                COMMAND music_rig_smk25_behavior_parity_test
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
                music_rig_smk25_behavior_parity
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
