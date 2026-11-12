cmake_minimum_required(VERSION 3.25)

project(PokoX
    VERSION 0.1.0
    DESCRIPTION "PokoX engine, runtime and platform"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global language settings. C++20 is required everywhere (systemplan/02).
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Development Release Server)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/config/cmake")
include(PokoConfigurations)
include(PokoWarnings)
include(PokoTargets)
include(PokoDependencies)

option(POKO_BUILD_TESTS "Build unit tests" ON)
option(POKO_BUILD_TOOLS "Build CLI tools" ON)
option(POKO_BUILD_DEMOS "Build windowed and headless demos" ON)
option(POKO_BUILD_SERVER "Build the dedicated server runtime" ON)

enable_testing()

message(STATUS "PokoX ${PROJECT_VERSION} | config=${POKO_ACTIVE_CONFIG} | tests=${POKO_BUILD_TESTS} tools=${POKO_BUILD_TOOLS} demos=${POKO_BUILD_DEMOS} server=${POKO_BUILD_SERVER}")

# ---------------------------------------------------------------------------
# Targets. Each subsystem owns its own CMakeLists.txt.
# ---------------------------------------------------------------------------
add_subdirectory(engine/core)
add_subdirectory(engine/platform_abstraction)
add_subdirectory(engine/physics)
add_subdirectory(engine/assets)
add_subdirectory(engine/scene)
add_subdirectory(engine/ecs)
add_subdirectory(engine/animation)
add_subdirectory(engine/input)
add_subdirectory(engine/audio)

# A headless Server build contains no renderer (systemplan/18).
if(NOT POKO_SERVER_CONFIG)
    add_subdirectory(engine/renderer)
endif()

if(POKO_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

add_subdirectory(runtime/game)

if(POKO_BUILD_SERVER)
    add_subdirectory(server/runtime)
endif()

# A headless Server build must not contain windowing/graphics targets.
if(POKO_BUILD_DEMOS AND NOT POKO_SERVER_CONFIG)
    add_subdirectory(examples/hello_world)
endif()

if(POKO_BUILD_TESTS)
    add_subdirectory(tests/unit)
endif()

poko_print_target_summary()
