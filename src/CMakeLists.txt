## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

# Warnings-as-errors for all targets under src/
add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
    $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
    $<$<CXX_COMPILER_ID:MSVC>:/external:W0>
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

set(MATTE_MAT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/materials/matte.mat)
set(MATTE_FILAMAT ${CMAKE_CURRENT_BINARY_DIR}/matte.filamat)
set(MATTE_MAT_HDR ${CMAKE_CURRENT_BINARY_DIR}/matte_mat.h)

if(FILAMENT_MATC)
    add_custom_command(
        OUTPUT ${MATTE_FILAMAT}
        COMMAND ${FILAMENT_MATC} -a all -o ${MATTE_FILAMAT} ${MATTE_MAT_SRC}
        DEPENDS ${MATTE_MAT_SRC}
        COMMENT "Compiling matte material"
    )
else()
    message(STATUS "matc not found — using pre-compiled material")
    set(MATTE_FILAMAT ${CMAKE_CURRENT_SOURCE_DIR}/materials/matte.filamat)
endif()

add_custom_command(
    OUTPUT ${MATTE_MAT_HDR}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${MATTE_FILAMAT}
        -DOUTPUT=${MATTE_MAT_HDR}
        -DVAR=MATTE_MAT
        -P ${CMAKE_SOURCE_DIR}/cmake/BinaryToHeader.cmake
    DEPENDS ${MATTE_FILAMAT}
    COMMENT "Generating matte material header"
)

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
    ${MATTE_MAT_HDR}
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
