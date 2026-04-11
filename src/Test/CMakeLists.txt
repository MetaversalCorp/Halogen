## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

find_package(Corrade REQUIRED TestSuite)

corrade_add_test(DeviceTest DeviceTest.cpp)
target_link_libraries(DeviceTest PRIVATE anari)
# Ensure the filament library DLL is built before tests run
add_dependencies(DeviceTest anari_library_filament)

# Set PATH so tests find anari.dll and anari_library_filament.dll
set_tests_properties(DeviceTest PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:anari>\;$<TARGET_FILE_DIR:anari_library_filament>\;$ENV{PATH}"
)
