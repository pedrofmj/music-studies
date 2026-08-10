cmake_minimum_required(VERSION 3.20)

project(music_rig VERSION 0.1.0 LANGUAGES C)

include(CTest)

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
    target_compile_options(music_rig_core PRIVATE /W4 /WX)
    target_compile_options(music-rig PRIVATE /W4 /WX)
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
        target_compile_options(music_rig_core_test PRIVATE /W4 /WX)
        target_compile_options(music_rig_protocol_test PRIVATE /W4 /WX)
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
        NAME music_rig_cli_version
        COMMAND $<TARGET_FILE:music-rig> --version
    )
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
    endif()

endif()
