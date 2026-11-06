# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 JoseFrMunoz
# This file is part of P.Melodies. See LICENSE for the full license text.
#
# Third-party dependencies, pinned to exact tags.
# For offline/local work, point FETCHCONTENT_SOURCE_DIR_JUCE / _CATCH2 at existing checkouts.

include(FetchContent)

set(PM_JUCE_TAG "8.0.17-security" CACHE STRING "JUCE git tag")
set(PM_CATCH2_TAG "v3.8.1" CACHE STRING "Catch2 git tag")

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${PM_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE
    SYSTEM)
FetchContent_MakeAvailable(JUCE)

if(PM_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${PM_CATCH2_TAG}
        GIT_SHALLOW    TRUE
        SYSTEM)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
