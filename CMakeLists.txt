# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 JoseFrMunoz
# This file is part of P.Melodies. See LICENSE for the full license text.

cmake_minimum_required(VERSION 3.25)

project(PMelodies
    VERSION 0.1.0
    DESCRIPTION "EQ, compressor, multiband saturation and distortion in a reorderable chain"
    HOMEPAGE_URL "https://github.com/JoseFrMunoz/P.Melodies"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime: the .vst3 must not depend on the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PM_BUILD_TESTS "Build the unit test executable" ON)

include(cmake/Dependencies.cmake)
include(cmake/Plugin.cmake)

if(PM_BUILD_TESTS)
    enable_testing()
    include(cmake/Tests.cmake)
endif()
