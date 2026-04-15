## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)

project(filament-anari VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Place all build outputs (executables, shared libraries) in a common directory
# so that shared libraries are found at runtime without PATH manipulation.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin)

# Default to the shared CRT on Windows (/MD), but allow callers to override it.
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Disable tests for mobile cross-compilation targets
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ANDROID)
    option(BUILD_TESTS "Build tests" OFF)
else()
    option(BUILD_TESTS "Build tests" ON)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

## Filament — find existing SDK or download precompiled
set(FILAMENT_ROOT "" CACHE PATH
    "Path to an existing Filament SDK (used as hint for find_package)")
set(FILAMENT_VERSION "1.71.0" CACHE STRING "Filament version to download")
set(FILAMENT_SDK_DIR "${CMAKE_SOURCE_DIR}/external/filament" CACHE PATH
    "Path to auto-downloaded Filament SDK")

if(FILAMENT_ROOT)
    set(FILAMENT_SDK_DIR "${FILAMENT_ROOT}" CACHE PATH "" FORCE)
endif()

# Try to find an existing Filament SDK first
find_package(Filament QUIET)

if(NOT Filament_FOUND)
    # No SDK found — download it, then find again
    include(DownloadFilament)
    unset(FILAMENT_INCLUDE_DIR CACHE)
    find_package(Filament REQUIRED)
endif()

## ANARI SDK — use existing installation or build as subproject
set(ANARI_ROOT "" CACHE PATH "Path to an existing ANARI SDK installation (skips subproject build)")

if(ANARI_ROOT)
    find_package(anari REQUIRED HINTS "${ANARI_ROOT}" "${ANARI_ROOT}/lib/cmake/anari")
else()
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ANDROID)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    else()
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    endif()
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

    # Mark ANARI SDK include directories as SYSTEM so that warnings in external
    # headers do not trigger -Werror in our targets.
    foreach(_tgt anari anari_backend helium)
        if(TARGET ${_tgt})
            get_target_property(_dirs ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
            if(_dirs)
                set_property(TARGET ${_tgt} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
                target_include_directories(${_tgt} SYSTEM INTERFACE ${_dirs})
            endif()
        endif()
    endforeach()

    # Force ANARI shared library into the common output directory so that tests
    # and our device library can find it at runtime without PATH manipulation.
    if(TARGET anari AND BUILD_SHARED_LIBS)
        set_target_properties(anari PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
        )
    endif()
endif()

## Corrade — always static to avoid extra DLL dependencies
set(CORRADE_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(CORRADE_BUILD_STATIC_PIC ON CACHE BOOL "" FORCE)
set(CORRADE_WITH_INTERCONNECT OFF CACHE BOOL "" FORCE)
set(CORRADE_WITH_PLUGINMANAGER OFF CACHE BOOL "" FORCE)
if(BUILD_TESTS)
    set(CORRADE_WITH_TESTSUITE ON CACHE BOOL "" FORCE)
else()
    set(CORRADE_WITH_TESTSUITE OFF CACHE BOOL "" FORCE)
endif()
set(CORRADE_WITH_UTILITY ON CACHE BOOL "" FORCE)
set(CORRADE_BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
if(CMAKE_CROSSCOMPILING)
    # We never use Corrade resources, so provide a dummy corrade-rc to
    # satisfy the cross-compilation check.
    set(CORRADE_RC_EXECUTABLE ${CMAKE_COMMAND} CACHE FILEPATH "" FORCE)
endif()
add_subdirectory(external/corrade EXCLUDE_FROM_ALL)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/external/corrade/modules/" ${CMAKE_MODULE_PATH})

# Mark Corrade includes as SYSTEM (same treatment as ANARI SDK above)
foreach(_tgt CorradeUtility CorradeTestSuite)
    if(TARGET ${_tgt})
        get_target_property(_dirs ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
        if(_dirs)
            set_property(TARGET ${_tgt} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
            target_include_directories(${_tgt} SYSTEM INTERFACE ${_dirs})
        endif()
    endif()
endforeach()

if(BUILD_TESTS)
    enable_testing()

    # GLFW — for windowed native surface tests (Mac and Windows only)
    if(NOT CMAKE_CROSSCOMPILING AND (WIN32 OR APPLE))
        include(FetchContent)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW ON
        )
        FetchContent_MakeAvailable(glfw)

        # Mark GLFW includes as SYSTEM to suppress warnings
        if(TARGET glfw)
            get_target_property(_dirs glfw INTERFACE_INCLUDE_DIRECTORIES)
            if(_dirs)
                set_property(TARGET glfw PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
                target_include_directories(glfw SYSTEM INTERFACE ${_dirs})
            endif()
        endif()
    endif()
endif()

set(ANARI_TARGET anari)
set(ANARI_BACKEND_TARGET anari_backend)
set(HELIUM_TARGET helium)

if(TARGET anari::anari)
    set(ANARI_TARGET anari::anari)
endif()

if(TARGET anari::anari_backend)
    set(ANARI_BACKEND_TARGET anari::anari_backend)
endif()

if(TARGET anari::helium)
    set(HELIUM_TARGET anari::helium)
endif()

add_subdirectory(src)

if(NOT CMAKE_CROSSCOMPILING AND BUILD_TESTS)
    add_subdirectory(examples)
endif()
