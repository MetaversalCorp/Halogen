## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

# DownloadFilament.cmake — Download Filament precompiled SDK if not present.

if(EXISTS "${FILAMENT_SDK_DIR}/README.md")
    message(STATUS "Filament SDK found at: ${FILAMENT_SDK_DIR}")
else()
    # Determine platform suffix
    if(WIN32)
        set(_FILAMENT_PLATFORM "windows")
    elseif(APPLE)
        if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(_FILAMENT_PLATFORM "ios")
        else()
            set(_FILAMENT_PLATFORM "mac")
        endif()
    elseif(ANDROID)
        set(_FILAMENT_PLATFORM "android-native")
    elseif(UNIX)
        # Check for ARM
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
            set(_FILAMENT_PLATFORM "arm-linux")
        else()
            set(_FILAMENT_PLATFORM "linux")
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform for Filament precompiled download")
    endif()

    set(_FILAMENT_ARCHIVE "filament-v${FILAMENT_VERSION}-${_FILAMENT_PLATFORM}.tgz")
    set(_FILAMENT_URL "https://github.com/google/filament/releases/download/v${FILAMENT_VERSION}/${_FILAMENT_ARCHIVE}")
    set(_FILAMENT_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/external")

    message(STATUS "Downloading Filament ${FILAMENT_VERSION} for ${_FILAMENT_PLATFORM}...")
    message(STATUS "  URL: ${_FILAMENT_URL}")

    file(DOWNLOAD
        "${_FILAMENT_URL}"
        "${_FILAMENT_DOWNLOAD_DIR}/${_FILAMENT_ARCHIVE}"
        SHOW_PROGRESS
        STATUS _FILAMENT_DL_STATUS
    )

    list(GET _FILAMENT_DL_STATUS 0 _FILAMENT_DL_CODE)
    if(NOT _FILAMENT_DL_CODE EQUAL 0)
        list(GET _FILAMENT_DL_STATUS 1 _FILAMENT_DL_MSG)
        message(FATAL_ERROR "Failed to download Filament: ${_FILAMENT_DL_MSG}")
    endif()

    message(STATUS "Extracting Filament SDK...")
    file(ARCHIVE_EXTRACT
        INPUT "${_FILAMENT_DOWNLOAD_DIR}/${_FILAMENT_ARCHIVE}"
        DESTINATION "${FILAMENT_SDK_DIR}"
    )

    # Some Filament archives contain a single top-level directory — flatten it
    file(GLOB _FILAMENT_EXTRACTED "${FILAMENT_SDK_DIR}/*")
    list(LENGTH _FILAMENT_EXTRACTED _FILAMENT_EXTRACTED_COUNT)
    if(_FILAMENT_EXTRACTED_COUNT EQUAL 1 AND IS_DIRECTORY "${_FILAMENT_EXTRACTED}")
        file(GLOB _INNER "${_FILAMENT_EXTRACTED}/*")
        foreach(_ITEM IN LISTS _INNER)
            get_filename_component(_NAME "${_ITEM}" NAME)
            file(RENAME "${_ITEM}" "${FILAMENT_SDK_DIR}/${_NAME}")
        endforeach()
        file(REMOVE_RECURSE "${_FILAMENT_EXTRACTED}")
    endif()

    # Clean up archive
    file(REMOVE "${_FILAMENT_DOWNLOAD_DIR}/${_FILAMENT_ARCHIVE}")

    if(NOT EXISTS "${FILAMENT_SDK_DIR}")
        message(FATAL_ERROR "Filament SDK extraction failed — expected directory: ${FILAMENT_SDK_DIR}")
    endif()

    message(STATUS "Filament SDK ready at: ${FILAMENT_SDK_DIR}")
endif()

# Set up Filament include and lib paths
set(FILAMENT_INCLUDE_DIR "${FILAMENT_SDK_DIR}/include")

# Filament uses different lib directory names per platform
if(WIN32)
    # Windows: lib/x86_64/mt or lib/x86_64/md depending on runtime
    set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/x86_64/mt")
    if(NOT EXISTS "${FILAMENT_LIB_DIR}")
        set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/x86_64/md")
    endif()
elseif(APPLE)
    # On macOS, respect CMAKE_OSX_ARCHITECTURES for cross-compilation
    if(CMAKE_OSX_ARCHITECTURES)
        set(_FILAMENT_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(_FILAMENT_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/${_FILAMENT_ARCH}")
    if(NOT EXISTS "${FILAMENT_LIB_DIR}")
        set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/universal")
    endif()
else()
    set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/${CMAKE_SYSTEM_PROCESSOR}")
    if(NOT EXISTS "${FILAMENT_LIB_DIR}")
        set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/aarch64")
        if(NOT EXISTS "${FILAMENT_LIB_DIR}")
            set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/arm64")
            if(NOT EXISTS "${FILAMENT_LIB_DIR}")
                set(FILAMENT_LIB_DIR "${FILAMENT_SDK_DIR}/lib/x86_64")
            endif()
        endif()
    endif()
endif()

if(NOT EXISTS "${FILAMENT_INCLUDE_DIR}")
    message(FATAL_ERROR "Filament include directory not found: ${FILAMENT_INCLUDE_DIR}")
endif()

message(STATUS "Filament include: ${FILAMENT_INCLUDE_DIR}")
message(STATUS "Filament libs:    ${FILAMENT_LIB_DIR}")

# Create imported targets for Filament libraries we need
function(filament_add_library name)
    add_library(filament::${name} STATIC IMPORTED GLOBAL)
    if(WIN32)
        set_target_properties(filament::${name} PROPERTIES
            IMPORTED_LOCATION "${FILAMENT_LIB_DIR}/${name}.lib"
        )
    else()
        set_target_properties(filament::${name} PROPERTIES
            IMPORTED_LOCATION "${FILAMENT_LIB_DIR}/lib${name}.a"
        )
    endif()
    target_include_directories(filament::${name} INTERFACE "${FILAMENT_INCLUDE_DIR}")
endfunction()

filament_add_library(filament)
filament_add_library(backend)
filament_add_library(utils)
filament_add_library(filabridge)
filament_add_library(filaflat)
filament_add_library(ibl)
filament_add_library(geometry)
filament_add_library(smol-v)
filament_add_library(zstd)

filament_add_library(bluegl)
filament_add_library(bluevk)
