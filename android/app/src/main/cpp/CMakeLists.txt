cmake_minimum_required(VERSION 3.22.1)
project(music_rig_fluidsynth LANGUAGES C CXX)

if(NOT DEFINED FLUIDSYNTH_ROOT)
    message(FATAL_ERROR "Set FLUIDSYNTH_ROOT to an Android FluidSynth bundle")
endif()

set(MUSIC_RIG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../")
set(FLUIDSYNTH_INCLUDE "${FLUIDSYNTH_ROOT}/include")
set(FLUIDSYNTH_LIBRARY "${FLUIDSYNTH_ROOT}/lib/${ANDROID_ABI}/libfluidsynth.so")

if(NOT EXISTS "${FLUIDSYNTH_INCLUDE}/fluidsynth.h")
    message(FATAL_ERROR "Missing ${FLUIDSYNTH_INCLUDE}/fluidsynth.h")
endif()
if(NOT EXISTS "${FLUIDSYNTH_LIBRARY}")
    message(FATAL_ERROR "Missing Android FluidSynth library: ${FLUIDSYNTH_LIBRARY}")
endif()

add_library(fluid_synth SHARED IMPORTED)
set_target_properties(fluid_synth PROPERTIES IMPORTED_LOCATION "${FLUIDSYNTH_LIBRARY}")

add_library(music_rig_portable STATIC
    "${MUSIC_RIG_ROOT}/docs/tools/music-rig/runtime/behaviors/music_rig_arturia_control_state.c"
    "${MUSIC_RIG_ROOT}/docs/tools/music-rig/runtime/core/music_rig_arturia_shared_sf2.c"
    music_rig_android_portable.c)
target_include_directories(music_rig_portable PUBLIC
    "${MUSIC_RIG_ROOT}/docs/tools/music-rig/runtime/behaviors/include"
    "${MUSIC_RIG_ROOT}/docs/tools/music-rig/runtime/core/include")
target_compile_features(music_rig_portable PUBLIC c_std_11)

add_library(music_rig_fluidsynth SHARED fluid_synth_jni.cpp)
target_include_directories(music_rig_fluidsynth PRIVATE "${FLUIDSYNTH_INCLUDE}")
target_compile_features(music_rig_fluidsynth PRIVATE cxx_std_17)
target_link_libraries(music_rig_fluidsynth PRIVATE
    music_rig_portable fluid_synth android log)
