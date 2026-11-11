// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/AudioEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace pm::param
{

/** Parameter IDs. They are stored in projects and presets: never rename them. */
namespace id
{
inline constexpr const char* inputGain = "inputGain";
inline constexpr const char* outputGain = "outputGain";
inline constexpr const char* mix = "mix";
inline constexpr const char* compare = "compare";
inline constexpr const char* clipperEnabled = "clipperEnabled";
inline constexpr const char* clipperCeiling = "clipperCeiling";
inline constexpr const char* qualityRealtime = "qualityRealtime";
inline constexpr const char* qualityOffline = "qualityOffline";
inline constexpr const char* eqEnabled = "eqEnabled";
inline constexpr const char* compressorEnabled = "compressorEnabled";
inline constexpr const char* saturationEnabled = "saturationEnabled";
inline constexpr const char* distortionEnabled = "distortionEnabled";
inline constexpr const char* bypass = "bypass";
inline constexpr const char* eqCharacter = "eqCharacter";
} // namespace id

/** Fields of an EQ band. The IDs are "eqB<n><Field>" with n from 1 to 8, e.g. "eqB3Freq". */
enum class EqField
{
    active,
    type,
    frequency,
    gain,
    q,
    slope,
    bypass
};

juce::String eqBandId (int band, EqField field);

/** Default frequency of each band slot (only used until the band is created). */
inline constexpr std::array<float, eq::numBands> eqDefaultFrequencies { 50.0f, 120.0f, 300.0f, 700.0f,
                                                                        1500.0f, 3500.0f, 8000.0f, 14000.0f };

juce::StringArray eqTypeNames();
juce::StringArray eqSlopeNames();
juce::StringArray eqCharacterNames();

/** Version hint given to every parameter (JUCE uses it for AU/VST3 parameter ordering). */
inline constexpr int versionHint = 1;

const char* moduleEnabledId (ModuleId moduleId) noexcept;

/** IDs that presets do not touch (decision §12: quality and clipper belong to the project). */
bool isExcludedFromPresets (const juce::String& parameterId);

/** Quality choices → oversampling factor index (0 = 1x … 4 = 16x). */
inline constexpr std::array<int, 3> realtimeQualityFactorIndex { 0, 1, 2 }; // Off, 2x, 4x
inline constexpr std::array<int, 4> offlineQualityFactorIndex { 1, 2, 3, 4 }; // 2x, 4x, 8x, 16x
inline constexpr int defaultRealtimeQualityChoice = 1; // 2x
inline constexpr int defaultOfflineQualityChoice = 2;  // 8x

juce::StringArray realtimeQualityChoices();
juce::StringArray offlineQualityChoices();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Cached raw-value pointers for lock-free reads on the audio thread. */
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);

    EngineParameters read() const noexcept;

private:
    std::atomic<float>* inputGain = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* compare = nullptr;
    std::atomic<float>* clipperEnabled = nullptr;
    std::atomic<float>* clipperCeiling = nullptr;
    std::atomic<float>* qualityRealtime = nullptr;
    std::atomic<float>* qualityOffline = nullptr;
    std::array<std::atomic<float>*, numModules> moduleEnabled {};
    std::atomic<float>* bypass = nullptr;

    struct EqBand
    {
        std::atomic<float>* active = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* bypass = nullptr;
    };
    std::array<EqBand, eq::numBands> eqBands {};
    std::atomic<float>* eqCharacter = nullptr;
};

} // namespace pm::param
