# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 JoseFrMunoz
# This file is part of P.Melodies. See LICENSE for the full license text.
#
# The plugin code lives in the INTERFACE library "SharedCode" so that the VST3 target and the
# test executable compile exactly the same sources with the same JUCE configuration.

set(PM_COMPANY_NAME   "JoseFrMunoz")
set(PM_PRODUCT_NAME   "P.Melodies")
set(PM_REPOSITORY_URL "https://github.com/JoseFrMunoz/P.Melodies")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)

juce_add_binary_data(PMBinaryData
    NAMESPACE   BinaryData
    HEADER_NAME BinaryData.h
    SOURCES
        ${PROJECT_SOURCE_DIR}/assets/fonts/Inter-Regular.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/Inter-Medium.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/Inter-SemiBold.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/JetBrainsMono-Regular.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/JetBrainsMono-Medium.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/SpaceGrotesk-Medium.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/SpaceGrotesk-SemiBold.ttf)
set_target_properties(PMBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE PM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_library(SharedCode INTERFACE)
target_sources(SharedCode INTERFACE ${PM_SOURCES})
target_include_directories(SharedCode INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_features(SharedCode INTERFACE cxx_std_20)
target_compile_definitions(SharedCode INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
    DONT_SET_USING_JUCE_NAMESPACE=1)
target_link_libraries(SharedCode INTERFACE
    PMBinaryData
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_plugin(PMelodies
    PRODUCT_NAME                "${PM_PRODUCT_NAME}"
    COMPANY_NAME                "${PM_COMPANY_NAME}"
    COMPANY_WEBSITE             "${PM_REPOSITORY_URL}"
    BUNDLE_ID                   com.josefrmunoz.pmelodies
    DESCRIPTION                 "${PROJECT_DESCRIPTION}"
    # Plugin identity: these two codes identify the plugin inside saved FL Studio projects.
    # NEVER change them.
    PLUGIN_MANUFACTURER_CODE    Jfmz
    PLUGIN_CODE                 Pmel
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ Dynamics Distortion)

target_link_libraries(PMelodies PRIVATE SharedCode)
