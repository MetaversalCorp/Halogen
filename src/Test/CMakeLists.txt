## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

find_package(Corrade REQUIRED TestSuite)

corrade_add_test(DeviceTest DeviceTest.cpp)
target_link_libraries(DeviceTest PRIVATE anari)
add_dependencies(DeviceTest anari_library_filament)

corrade_add_test(AabbTest AabbTest.cpp)
target_include_directories(AabbTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(AabbTest SYSTEM PRIVATE
    ${FILAMENT_INCLUDE_DIR})

corrade_add_test(ColorConversionTest ColorConversionTest.cpp)
target_link_libraries(ColorConversionTest PRIVATE anari)
target_include_directories(ColorConversionTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(ColorConversionTest SYSTEM PRIVATE
    ${FILAMENT_INCLUDE_DIR})

corrade_add_test(GeometryTest GeometryTest.cpp)
target_link_libraries(GeometryTest PRIVATE anari)
add_dependencies(GeometryTest anari_library_filament)

corrade_add_test(SamplerTest SamplerTest.cpp)
target_link_libraries(SamplerTest PRIVATE anari)
add_dependencies(SamplerTest anari_library_filament)

corrade_add_test(MaterialTest MaterialTest.cpp)
target_link_libraries(MaterialTest PRIVATE anari)
add_dependencies(MaterialTest anari_library_filament)

corrade_add_test(LightTest LightTest.cpp)
target_link_libraries(LightTest PRIVATE anari)
add_dependencies(LightTest anari_library_filament)

corrade_add_test(RenderTest RenderTest.cpp)
target_link_libraries(RenderTest PRIVATE anari)
add_dependencies(RenderTest anari_library_filament)

corrade_add_test(ScreenshotTest ScreenshotTest.cpp)
target_link_libraries(ScreenshotTest PRIVATE anari stb_image)
target_compile_definitions(ScreenshotTest PRIVATE
    SCREENSHOT_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
add_dependencies(ScreenshotTest anari_library_filament)

corrade_add_test(NativeSurfaceTest NativeSurfaceTest.cpp)
target_link_libraries(NativeSurfaceTest PRIVATE anari)
add_dependencies(NativeSurfaceTest anari_library_filament)
