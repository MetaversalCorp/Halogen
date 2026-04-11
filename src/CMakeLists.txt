## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

# Warnings-as-errors for all targets under src/
add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
)

# -- Material compilation with matc --
#
# matc is a host tool shipped in the Filament desktop SDKs.  Cross-compilation
# SDKs (iOS, Android) do not include host binaries, and the Linux matc may
# require a newer glibc than the CI image provides.  When the tool is not
# available we fall back to a pre-compiled header checked into the repository.

find_program(FILAMENT_MATC matc
    HINTS ${FILAMENT_SDK_DIR}/bin
    NO_DEFAULT_PATH
)

# Verify matc can actually run — on Linux the prebuilt binary may require a
# newer glibc than the build environment provides.
if(FILAMENT_MATC)
    execute_process(
        COMMAND ${FILAMENT_MATC} --help
        RESULT_VARIABLE _matc_result
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT _matc_result EQUAL 0)
        message(STATUS "matc found at ${FILAMENT_MATC} but cannot execute (exit code ${_matc_result})")
        set(FILAMENT_MATC "" CACHE FILEPATH "" FORCE)
    endif()
endif()

set(MATERIAL_NAMES matte physicallyBased)
set(MATERIAL_HEADERS "")

foreach(_mat ${MATERIAL_NAMES})
    set(_mat_src ${CMAKE_CURRENT_SOURCE_DIR}/materials/${_mat}.mat)
    set(_filamat ${CMAKE_CURRENT_BINARY_DIR}/${_mat}.filamat)
    set(_hdr ${CMAKE_CURRENT_BINARY_DIR}/${_mat}_mat.h)
    string(TOUPPER ${_mat} _var)

    if(FILAMENT_MATC)
        add_custom_command(
            OUTPUT ${_filamat}
            COMMAND ${FILAMENT_MATC} -a all -o ${_filamat} ${_mat_src}
            DEPENDS ${_mat_src}
            COMMENT "Compiling ${_mat} material"
        )
    else()
        message(STATUS "matc not found — using pre-compiled ${_mat} material")
        set(_filamat ${CMAKE_CURRENT_SOURCE_DIR}/materials/${_mat}.filamat)
    endif()

    add_custom_command(
        OUTPUT ${_hdr}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT=${_filamat}
            -DOUTPUT=${_hdr}
            -DVAR=${_var}_MAT
            -P ${CMAKE_SOURCE_DIR}/cmake/BinaryToHeader.cmake
        DEPENDS ${_filamat}
        COMMENT "Generating ${_mat} material header"
    )

    list(APPEND MATERIAL_HEADERS ${_hdr})
endforeach()

# --- Filament ANARI device shared library ---

add_library(anari_library_filament SHARED
    Device.h
    Device.cpp
    DeviceState.h
    DeviceState.cpp
    FilamentResource.h
    FilamentResource.cpp
    Object.h
    Camera.h
    Camera.cpp
    Frame.h
    Frame.cpp
    Geometry.h
    Geometry.cpp
    Light.h
    Light.cpp
    Material.h
    Material.cpp
    Renderer.h
    Surface.h
    Surface.cpp
    World.h
    World.cpp
    Library.cpp
    ${MATERIAL_HEADERS}
)

include(GenerateExportHeader)
generate_export_header(anari_library_filament
    EXPORT_MACRO_NAME "FILAMENT_ANARI_EXPORT"
)

target_include_directories(anari_library_filament
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(anari_library_filament
    PUBLIC anari
    PRIVATE
        anari_backend
        helium
        filament::filament
        filament::backend
        filament::geometry
        filament::utils
        filament::filabridge
        filament::filaflat
        filament::smol-v
        filament::zstd
)

if(WIN32)
    target_link_libraries(anari_library_filament PRIVATE
        filament::bluegl
        filament::bluevk
        opengl32
        gdi32
    )
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(anari_library_filament PRIVATE
        filament::bluegl
        filament::bluevk
        "-framework Cocoa"
        "-framework Metal"
        "-framework CoreVideo"
        "-framework OpenGL"
        "-framework QuartzCore"
        "-framework CoreGraphics"
        "-framework IOKit"
        "-framework IOSurface"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(anari_library_filament PRIVATE
        "-framework Foundation"
        "-framework Metal"
        "-framework OpenGLES"
        "-framework CoreVideo"
        "-framework QuartzCore"
        "-framework CoreGraphics"
        "-framework IOKit"
        "-framework IOSurface"
        "-framework UIKit"
    )
elseif(ANDROID)
    target_link_libraries(anari_library_filament PRIVATE
        filament::bluevk
        EGL
        GLESv3
        android
        log
        dl
    )
else()
    target_link_libraries(anari_library_filament PRIVATE
        filament::bluegl
        filament::bluevk
        dl
        pthread
    )
endif()

set_target_properties(anari_library_filament PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
)

# --- Install ---

include(GNUInstallDirs)

install(TARGETS anari_library_filament
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# --- Tests ---

if(BUILD_TESTS)
    add_subdirectory(Test)
endif()
