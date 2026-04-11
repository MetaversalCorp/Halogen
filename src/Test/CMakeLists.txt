## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

find_package(Corrade REQUIRED TestSuite)

corrade_add_test(DeviceTest DeviceTest.cpp)
target_link_libraries(DeviceTest PRIVATE anari)
target_compile_options(DeviceTest PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc /external:anglebrackets /external:W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -Wpedantic>
)
# Ensure the filament library is built before tests run
add_dependencies(DeviceTest anari_library_filament)

# Set library search paths so tests find shared libraries at runtime
if(WIN32)
    set_tests_properties(DeviceTest PROPERTIES
        ENVIRONMENT "PATH=$<TARGET_FILE_DIR:anari>\;$<TARGET_FILE_DIR:anari_library_filament>\;$ENV{PATH}"
    )
elseif(APPLE)
    set_tests_properties(DeviceTest PROPERTIES
        ENVIRONMENT "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:anari>:$<TARGET_FILE_DIR:anari_library_filament>"
    )
else()
    set_tests_properties(DeviceTest PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:anari>:$<TARGET_FILE_DIR:anari_library_filament>"
    )
endif()
