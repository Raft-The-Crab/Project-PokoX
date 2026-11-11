# Four configurations required by systemplan/02_REPOSITORY_AND_BUILD_FOUNDATION.md:
# Debug, Development, Release and Server. Development and Release deliberately
# carry different flags and definitions; Server is headless.

set(POKO_CONFIGURATIONS Debug Development Release Server)

get_property(_poko_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_poko_is_multi_config)
    set(CMAKE_CONFIGURATION_TYPES "${POKO_CONFIGURATIONS}"
        CACHE STRING "PokoX build configurations" FORCE)
endif()

# CMake ships defaults for Debug/Release but not for custom configurations.
# Define them (and their linker flags) so multi-config generators accept them.
if(MSVC)
    set(_poko_development_flags "/Od /Zi /RTC1")
    set(_poko_server_flags "/O2 /DNDEBUG")
else()
    set(_poko_development_flags "-O0 -g")
    set(_poko_server_flags "-O2 -DNDEBUG")
endif()

foreach(_poko_cfg IN LISTS POKO_CONFIGURATIONS)
    string(TOUPPER "${_poko_cfg}" _poko_upper)
    if(NOT DEFINED CMAKE_CXX_FLAGS_${_poko_upper})
        set(CMAKE_CXX_FLAGS_${_poko_upper} "" CACHE STRING "C++ flags (${_poko_cfg})")
    endif()
    if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_${_poko_upper})
        set(CMAKE_EXE_LINKER_FLAGS_${_poko_upper} "" CACHE STRING "Link flags (${_poko_cfg})")
    endif()
    mark_as_advanced(CMAKE_CXX_FLAGS_${_poko_upper} CMAKE_EXE_LINKER_FLAGS_${_poko_upper})
endforeach()

set(CMAKE_CXX_FLAGS_DEVELOPMENT "${_poko_development_flags}"
    CACHE STRING "C++ flags (Development)" FORCE)
set(CMAKE_CXX_FLAGS_SERVER "${_poko_server_flags}"
    CACHE STRING "C++ flags (Server)" FORCE)

# Active configuration for single-config generators; "Multi" otherwise.
if(_poko_is_multi_config)
    set(POKO_ACTIVE_CONFIG "Multi")
    set(POKO_SERVER_CONFIG OFF)
else()
    set(POKO_ACTIVE_CONFIG "${CMAKE_BUILD_TYPE}")
    if(CMAKE_BUILD_TYPE STREQUAL "Server")
        set(POKO_SERVER_CONFIG ON)
    else()
        set(POKO_SERVER_CONFIG OFF)
    endif()
endif()

if(NOT POKO_ACTIVE_CONFIG IN_LIST POKO_CONFIGURATIONS AND NOT _poko_is_multi_config)
    message(FATAL_ERROR
        "Unknown configuration '${POKO_ACTIVE_CONFIG}'. "
        "Use one of: ${POKO_CONFIGURATIONS} (see CMakePresets.json).")
endif()

message(STATUS "Configurations: ${POKO_CONFIGURATIONS}")
