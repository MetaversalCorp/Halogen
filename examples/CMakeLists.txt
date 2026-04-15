## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

add_executable(render_scene render_scene.cpp)
target_link_libraries(render_scene PRIVATE anari)
target_include_directories(render_scene SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/external/anari-sdk/external/stb_image)
add_dependencies(render_scene anari_library_halogen)
