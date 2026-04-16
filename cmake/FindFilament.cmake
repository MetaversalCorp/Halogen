## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

# FindFilament.cmake — Locate a Filament SDK installation.
#
# This module defines:
#   Filament_FOUND          — TRUE if the SDK was found
#   FILAMENT_INCLUDE_DIR    — Path to the include directory
#   FILAMENT_LIB_DIR        — Path to the library directory
#   filament::filament, filament::backend, etc. — Imported targets
#
# Hints:
#   FILAMENT_ROOT — Path to a Filament SDK root (set via -DFILAMENT_ROOT=...)
#   FILAMENT_SDK_DIR — Alternative hint (used by DownloadFilament)

# Build the list of candidate directories
set(_FILAMENT_HINTS "")
if(FILAMENT_ROOT)
    list(APPEND _FILAMENT_HINTS "${FILAMENT_ROOT}")
endif()
if(FILAMENT_SDK_DIR)
    list(APPEND _FILAMENT_HINTS "${FILAMENT_SDK_DIR}")
endif()
# Default: external/filament in the source tree (auto-downloaded)
list(APPEND _FILAMENT_HINTS "${CMAKE_SOURCE_DIR}/external/filament")

# Find the include directory
# NO_CMAKE_FIND_ROOT_PATH is required so that cross-compilation toolchains
# (Android NDK, iOS) do not re-root the HINTS under their sysroot.
find_path(FILAMENT_INCLUDE_DIR
    NAMES filament/Engine.h
    HINTS ${_FILAMENT_HINTS}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT FILAMENT_INCLUDE_DIR)
    if(Filament_FIND_REQUIRED)
        message(FATAL_ERROR
            "Filament SDK not found. Set FILAMENT_ROOT to the SDK path, or let "
            "DownloadFilament.cmake fetch it automatically.")
    endif()
    set(Filament_FOUND FALSE)
    return()
endif()

# Derive SDK root from include dir
get_filename_component(_FILAMENT_SDK_ROOT "${FILAMENT_INCLUDE_DIR}" DIRECTORY)

# Determine platform-appropriate lib directory
if(WIN32)
    set(_FILAMENT_USES_SHARED_CRT OFF)
    if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        set(_FILAMENT_USES_SHARED_CRT ON)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(_FILAMENT_USES_SHARED_CRT)
            set(_FILAMENT_LIB_CANDIDATES
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/mdd"
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/md"
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/mtd"
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/mt")
        else()
            set(_FILAMENT_LIB_CANDIDATES
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/mtd"
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/mt"
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/mdd"
                "${_FILAMENT_SDK_ROOT}/lib/x86_64/md")
        endif()
    elseif(_FILAMENT_USES_SHARED_CRT)
        set(_FILAMENT_LIB_CANDIDATES
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/md"
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/mt"
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/mdd"
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/mtd")
    else()
        set(_FILAMENT_LIB_CANDIDATES
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/mt"
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/md"
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/mtd"
            "${_FILAMENT_SDK_ROOT}/lib/x86_64/mdd")
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_FILAMENT_LIB_CANDIDATES
            "${_FILAMENT_SDK_ROOT}/lib/arm64"
            "${_FILAMENT_SDK_ROOT}/lib/universal")
    elseif(CMAKE_OSX_ARCHITECTURES)
        set(_FILAMENT_LIB_CANDIDATES
            "${_FILAMENT_SDK_ROOT}/lib/${CMAKE_OSX_ARCHITECTURES}"
            "${_FILAMENT_SDK_ROOT}/lib/universal")
    else()
        set(_FILAMENT_LIB_CANDIDATES
            "${_FILAMENT_SDK_ROOT}/lib/${CMAKE_SYSTEM_PROCESSOR}"
            "${_FILAMENT_SDK_ROOT}/lib/universal")
    endif()
elseif(ANDROID)
    set(_FILAMENT_LIB_CANDIDATES
        "${_FILAMENT_SDK_ROOT}/lib/${CMAKE_ANDROID_ARCH_ABI}"
        "${_FILAMENT_SDK_ROOT}/lib/arm64-v8a")
else()
    set(_FILAMENT_LIB_CANDIDATES
        "${_FILAMENT_SDK_ROOT}/lib/${CMAKE_SYSTEM_PROCESSOR}"
        "${_FILAMENT_SDK_ROOT}/lib/aarch64"
        "${_FILAMENT_SDK_ROOT}/lib/arm64"
        "${_FILAMENT_SDK_ROOT}/lib/x86_64")
endif()

set(FILAMENT_LIB_DIR "")
foreach(_dir IN LISTS _FILAMENT_LIB_CANDIDATES)
    if(EXISTS "${_dir}")
        set(FILAMENT_LIB_DIR "${_dir}")
        break()
    endif()
endforeach()

if(NOT FILAMENT_LIB_DIR)
    if(Filament_FIND_REQUIRED)
        message(FATAL_ERROR
            "Filament libraries not found. Tried: ${_FILAMENT_LIB_CANDIDATES}")
    endif()
    set(Filament_FOUND FALSE)
    return()
endif()

message(STATUS "Filament SDK found:")
message(STATUS "  Include: ${FILAMENT_INCLUDE_DIR}")
message(STATUS "  Libs:    ${FILAMENT_LIB_DIR}")

# Create imported targets
function(_filament_add_library name)
    if(TARGET filament::${name})
        return()
    endif()
    add_library(filament::${name} STATIC IMPORTED GLOBAL)
    if(WIN32)
        set_target_properties(filament::${name} PROPERTIES
            IMPORTED_LOCATION "${FILAMENT_LIB_DIR}/${name}.lib")
    else()
        set_target_properties(filament::${name} PROPERTIES
            IMPORTED_LOCATION "${FILAMENT_LIB_DIR}/lib${name}.a")
    endif()
    target_include_directories(filament::${name} SYSTEM INTERFACE
        "${FILAMENT_INCLUDE_DIR}")
endfunction()

_filament_add_library(filament)
_filament_add_library(backend)
_filament_add_library(utils)
_filament_add_library(filabridge)
_filament_add_library(filaflat)
_filament_add_library(ibl)
_filament_add_library(geometry)
_filament_add_library(smol-v)
_filament_add_library(zstd)
_filament_add_library(bluegl)
_filament_add_library(bluevk)

set(Filament_FOUND TRUE)

# Clean up
unset(_FILAMENT_HINTS)
unset(_FILAMENT_SDK_ROOT)
unset(_FILAMENT_LIB_CANDIDATES)
