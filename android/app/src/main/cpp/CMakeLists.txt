cmake_minimum_required(VERSION 3.22.1)
project("postvoid_runner" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Gamepad Hook Library (Includes xdelta3 on-the-fly patcher) ────────────────
# Standalone .so mirroring PortMaster's gamepad.cpp approach:
# intercepts libyoyo.so's Function_Add and replaces all gamepad_* GML built-ins
# with our own state-backed implementations driven from Java controller events,
# plus native xdelta3 patching for BYOD data.win decoding.
add_library(gamepad_hook SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/gamepad_hook/gamepad_hook.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xdelta3/xdelta3.c
)

target_include_directories(gamepad_hook PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/xdelta3
)

target_compile_definitions(gamepad_hook PRIVATE
    XD3_POSIX=1
    XD3_MAIN=1
    XD3_INCLUDE_MAIN=1
    SECONDARY_DJW=1
    SECONDARY_FGK=1
    SIZEOF_SIZE_T=${CMAKE_SIZEOF_VOID_P}
    SIZEOF_UNSIGNED_LONG_LONG=8
)

target_link_libraries(gamepad_hook
    PRIVATE
    log
    dl
    m
)

# ── Legacy postvoid_runner (unused stub, kept for build compatibility) ─────────
find_package(oboe REQUIRED CONFIG)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/input
    ${CMAKE_CURRENT_SOURCE_DIR}/game
)

set(RUNNER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gms_datawin_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gms_runtime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/gms_vm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/renderer_gles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/shader_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/vertex_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/oboe_audio_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/input/touch_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/game/game_objects.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/game/game_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/game/menu_objects.cpp
)

add_library(postvoid_runner SHARED
    ${RUNNER_SOURCES}
)

target_link_libraries(postvoid_runner
    PRIVATE
    oboe::oboe
    GLESv3
    EGL
    log
    android
    m
)

