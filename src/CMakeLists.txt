## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

# --- Filament ANARI device shared library ---

add_library(anari_library_filament SHARED
    Device.h
    Device.cpp
    Library.cpp
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
        filament::filament
        filament::backend
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
elseif(APPLE)
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

target_compile_options(anari_library_filament PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc /external:anglebrackets /external:W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -Wpedantic>
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
