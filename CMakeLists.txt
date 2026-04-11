## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)

project(filament-anari VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Match Filament's static CRT on Windows (/MT)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(BUILD_TESTS "Build tests" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

## Filament — download precompiled SDK if not present

set(FILAMENT_VERSION "1.71.0" CACHE STRING "Filament version to download")
set(FILAMENT_SDK_DIR "${CMAKE_SOURCE_DIR}/external/filament" CACHE PATH "Path to Filament SDK")

include(DownloadFilament)

## ANARI SDK — as subproject, minimal build
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_CTS OFF CACHE BOOL "" FORCE)
set(BUILD_CAT OFF CACHE BOOL "" FORCE)
set(BUILD_VIEWER OFF CACHE BOOL "" FORCE)
set(BUILD_HELIDE_DEVICE OFF CACHE BOOL "" FORCE)
set(BUILD_SINK_DEVICE OFF CACHE BOOL "" FORCE)
set(BUILD_DEBUG_DEVICE OFF CACHE BOOL "" FORCE)
set(INSTALL_VIEWER_LIBRARY OFF CACHE BOOL "" FORCE)
set(INSTALL_CODE_GEN_SCRIPTS OFF CACHE BOOL "" FORCE)
set(ANARI_CODE_GEN_ROOT "${CMAKE_SOURCE_DIR}/external/anari-sdk/code_gen" CACHE PATH "" FORCE)

# ANARI SDK references ${CMAKE_SOURCE_DIR}/code_gen internally. Create a
# symlink/junction so it resolves correctly when used as a subproject.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/code_gen")
    if(WIN32)
        file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/code_gen" _CODEGEN_LINK)
        file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/external/anari-sdk/code_gen" _CODEGEN_TARGET)
        execute_process(COMMAND cmd /c mklink /J "${_CODEGEN_LINK}" "${_CODEGEN_TARGET}")
    else()
        file(CREATE_LINK
            "${CMAKE_SOURCE_DIR}/external/anari-sdk/code_gen"
            "${CMAKE_SOURCE_DIR}/code_gen"
            SYMBOLIC)
    endif()
endif()

add_subdirectory(external/anari-sdk EXCLUDE_FROM_ALL)

## Corrade — for testing only
set(CORRADE_WITH_INTERCONNECT OFF CACHE BOOL "" FORCE)
set(CORRADE_WITH_PLUGINMANAGER OFF CACHE BOOL "" FORCE)
set(CORRADE_WITH_TESTSUITE ON CACHE BOOL "" FORCE)
set(CORRADE_WITH_UTILITY ON CACHE BOOL "" FORCE)
set(CORRADE_BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
if(BUILD_TESTS)
    add_subdirectory(external/corrade EXCLUDE_FROM_ALL)
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/external/corrade/modules/" ${CMAKE_MODULE_PATH})
    enable_testing()
endif()

add_subdirectory(src)
