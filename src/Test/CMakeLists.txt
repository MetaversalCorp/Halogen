## Copyright 2026 Jonathan Hale
## SPDX-License-Identifier: MIT

find_package(Corrade REQUIRED TestSuite)

# --- DeviceTest ---

corrade_add_test(DeviceTest DeviceTest.cpp)
target_link_libraries(DeviceTest PRIVATE anari)
target_compile_options(DeviceTest PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc /external:anglebrackets /external:W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -Wpedantic>
)
add_dependencies(DeviceTest anari_library_filament)

# --- TutorialTest ---

corrade_add_test(TutorialTest TutorialTest.cpp)
target_link_libraries(TutorialTest PRIVATE anari)
target_compile_options(TutorialTest PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc /external:anglebrackets /external:W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -Wpedantic>
)
add_dependencies(TutorialTest anari_library_filament)

# Set library search paths so tests find shared libraries at runtime
foreach(TEST_TARGET DeviceTest TutorialTest)
    if(WIN32)
        set_tests_properties(${TEST_TARGET} PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:anari>\;$<TARGET_FILE_DIR:anari_library_filament>\;$ENV{PATH}"
        )
    elseif(APPLE)
        set_tests_properties(${TEST_TARGET} PROPERTIES
            ENVIRONMENT "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:anari>:$<TARGET_FILE_DIR:anari_library_filament>"
        )
    else()
        set_tests_properties(${TEST_TARGET} PROPERTIES
            ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:anari>:$<TARGET_FILE_DIR:anari_library_filament>"
        )
    endif()
endforeach()
