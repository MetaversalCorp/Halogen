## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

find_package(Corrade REQUIRED TestSuite)

corrade_add_test(DeviceTest DeviceTest.cpp)
target_link_libraries(DeviceTest PRIVATE anari)
add_dependencies(DeviceTest anari_library_filament)

corrade_add_test(ParameterTest ParameterTest.cpp)
target_link_libraries(ParameterTest PRIVATE anari)
add_dependencies(ParameterTest anari_library_filament)

corrade_add_test(TutorialTest TutorialTest.cpp)
target_link_libraries(TutorialTest PRIVATE anari)
add_dependencies(TutorialTest anari_library_filament)
