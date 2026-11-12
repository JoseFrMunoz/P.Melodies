// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/ModuleBase.h"
#include "dsp/DelayLine.h"
#include "dsp/comp/CompressorDynamics.h"
#include "dsp/comp/CompressorMeter.h"
#include "dsp/eq/SvfCascade.h"
#include "modules/comp/CompParameters.h"

#include <vector>

namespace pm::comp
{

/**
    The compressor module (§5, docs/dsp/compresor.md).

        sidechain (input or external) ─ sidechain EQ ─ delay (20 ms − lookahead) ─ detector ─ gain
        input ─ delay (20 ms) ─ × gain × makeup ─ mix with the delayed input ─ [listen] ─ output

    - Constant latency: the audio is always delayed by the maximum lookahead (20 ms); the
      detector sees the signal `lookahead` ms earlier. Plus the oversampling latency, from
      ModuleBase.
    - Runs at the oversampled rate (its Off / 2x / 4x selector or the global quality, whichever is
      higher). The external sidechain is upsampled by ModuleBase with the same filters.
    - Stereo linked: one gain for every channel.
    - Listen plays the filtered sidechain instead of the output, with a 20 ms crossfade.
    - Makeup, auto-gain, mix and listen glide over 20 ms; the curve settings glide inside the
      detector.
*/
class CompressorModule final : public ModuleBase
{
public:
    CompressorModule() noexcept;

    void setParameters (const ModuleParameters& parameters) noexcept override;

    /** Where the heard compressor sends its meter data (the engine's). Set before playing. */
    void setMeter (dsp::comp::CompressorMeter* destination) noexcept { meter = destination; }

    static constexpr double glideSeconds = 0.020;
    static constexpr double silentSidechainSeconds = 0.3;
    static constexpr float silenceLevel = 1.0e-5f; // −100 dBFS

protected:
    void prepareCore (double baseRate, int blockSize, int numChannels) override;
    void setCoreSampleRate (double sampleRate) noexcept override;
    void resetCore() noexcept override;
    void processCore (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept override;
    bool isCoreNeutral() const noexcept override;
    int getRequestedOversamplingIndex() const noexcept override { return pending.oversamplingIndex; }
    int getCoreLatencySamples() const noexcept override { return maxLookaheadSamples; }
    int getCoreSettleSamples() const noexcept override { return settleSamples; }
    void copyCoreStateFrom (const ModuleBase& other) noexcept override;
    void observe (juce::dsp::AudioBlock<const float> alignedInput, juce::dsp::AudioBlock<const float> output,
                  const SidechainBlock* sidechain, bool wetComputed) noexcept override;

private:
    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    struct FilterState
    {
        dsp::eq::SvfCascade cascade;
        SidechainFilter applied;
        bool active = false;
        bool designed = false;
    };

    void applySettings (bool jump) noexcept;
    void updateFilter (FilterState& filter, const SidechainFilter& wanted, dsp::eq::FilterType type, bool jump) noexcept;

    Parameters pending;
    dsp::comp::CompressorMeter* meter = nullptr;

    dsp::comp::Dynamics dynamics;
    FilterState highPass, lowPass, bell;
    dsp::DelayLine audioDelay;
    dsp::DelayLine sidechainDelay;
    juce::AudioBuffer<float> sidechainBuffer;
    LinearSmoother makeup, mix, listen;

    double baseSampleRate = 44100.0;
    double coreRate = 44100.0;
    int factor = 1;
    int maxLookaheadSamples = 0; // base rate
    int settleSamples = 0;
    int numChannels = 2;

    // For the meters (base rate): the deepest gain of each base-rate sample of the last block.
    std::vector<float> gainTrace;
    int gainTraceLength = 0;
    float lastLevelDb = -120.0f;

    // Meter accumulation (only on the heard module).
    int pointLength = 1;
    int pointCount = 0;
    float pointInput = 0.0f, pointOutput = 0.0f, pointGain = 0.0f;
    int silentSamples = 0;
};

} // namespace pm::comp
