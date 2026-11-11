# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 JoseFrMunoz
# This file is part of P.Melodies. See LICENSE for the full license text.

file(GLOB_RECURSE PM_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h)

add_executable(Tests ${PM_TEST_SOURCES})
target_include_directories(Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(Tests PRIVATE SharedCode Catch2::Catch2)

include(Catch)
catch_discover_tests(Tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 600)

# Tests compare floating-point samples for exact (bit-identical) equality on purpose. Set per source
# so it comes after JUCE's recommended warning flags on the command line.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(${PM_TEST_SOURCES} PROPERTIES COMPILE_OPTIONS -Wno-float-equal)
endif()
