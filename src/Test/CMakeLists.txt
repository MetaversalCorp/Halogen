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

corrade_add_test(ParameterTest ParameterTest.cpp)
target_link_libraries(ParameterTest PRIVATE anari)
add_dependencies(ParameterTest anari_library_filament)

corrade_add_test(TutorialTest TutorialTest.cpp)
target_link_libraries(TutorialTest PRIVATE anari)
add_dependencies(TutorialTest anari_library_filament)

corrade_add_test(Milestone2Test Milestone2Test.cpp)
target_link_libraries(Milestone2Test PRIVATE anari)
add_dependencies(Milestone2Test anari_library_filament)
