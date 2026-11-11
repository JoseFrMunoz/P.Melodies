// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/ModuleChain.h"
#include "dsp/DelayLine.h"
#include "dsp/LevelMeter.h"
#include "dsp/LoudnessMatcher.h"
#include "modules/PlaceholderModules.h"

#include <array>
#include <atomic>

namespace pm
{

/** Plain snapshot of every parameter the engine needs, taken once per audio block. */
struct EngineParameters
{
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    float mix = 1.0f; // 0..1
    bool compare = false;
    bool clipperEnabled = true;
    float clipperCeilingDb = -0.3f;
    int realtimeOversamplingIndex = 1; // factor index: 0 = 1x … 4 = 16x
    int offlineOversamplingIndex = 3;
    chain::Order chainOrder = chain::defaultOrder(); // not a parameter: saved in the state (§12)
    std::array<bool, numModules> moduleEnabled { true, true, true, true };
    bool bypassed = false;
    ModuleParameters modules;
};

/**
    The whole signal path of the plugin, independent of the JUCE AudioProcessor so it can be
    tested directly.

        input → input gain → [input meter] ─┬─ module chain ───────────── wet ─┐
                                            └─ dry delay (chain latency) ─ dry ─┴─ mix
        mix → output gain → compare at equal loudness → safety clipper → host bypass → [output meter]

    The "original" signal (input before the input gain, delayed by the latency) feeds the
    loudness comparison and the host bypass.

    Every gain and switch is smoothed or crossfaded over 20 ms. process() is real-time safe and
    accepts any block size: blocks larger than the prepared size are split internally.
*/
class AudioEngine
{
public:
    static constexpr double smoothingSeconds = 0.020;
    static constexpr double matchGainSmoothingSeconds = 0.2;
    static constexpr float minCeilingDb = -6.0f;
    static constexpr float maxCeilingDb = 0.0f;

    explicit AudioEngine (const ModuleChain::Factory& factory = createModule);

    /** Allocates everything. Not real-time safe. */
    void prepare (double sampleRate, int hostBlockSize, int numChannels, const EngineParameters& initial);

    void reset();

    /**
        Processes the main buffer in place. The sidechain buffer is optional (nullptr when the
        host provides none). isOffline selects the export quality instead of the playback one.
    */
    void process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain,
                  const EngineParameters& parameters, bool isOffline) noexcept;

    int getLatencySamples() const noexcept { return latencySamples.load(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }
    int getInternalBlockSize() const noexcept { return internalBlockSize; }

    dsp::MeterReadout& getInputMeter() noexcept { return inputMeter; }
    dsp::MeterReadout& getOutputMeter() noexcept { return outputMeter; }

    /** Gain currently applied to the original signal when comparing, in dB. */
    float getMatchGainDb() const noexcept { return matchGainDb.load(); }

    ModuleChain& getChain() noexcept { return chain; }

    /** The EQ's spectrum analyser feed (read by the interface). */
    dsp::AnalyzerTap& getEqAnalyzer() noexcept { return eqAnalyzer; }

    /** Size of the internal processing blocks for a given host block size. */
    static int internalBlockSizeFor (int hostBlockSize) noexcept;

private:
    using GainSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;
    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    void applyParameters (const EngineParameters& parameters, bool isOffline, bool jumpToTargets) noexcept;
    void processChunk (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept;

    static void applyGain (juce::dsp::AudioBlock<float> block, GainSmoother& smoother) noexcept;

    ModuleChain chain;

    dsp::DelayLine originalDelay;
    dsp::DelayLine dryDelay;
    juce::AudioBuffer<float> originalBuffer;
    juce::AudioBuffer<float> dryBuffer;

    GainSmoother inputGain, outputGain, ceilingGain, matchGain;
    LinearSmoother mix, compareFade, clipperFade, bypassFade;

    dsp::AnalyzerTap eqAnalyzer;
    dsp::LoudnessMatcher loudness;
    dsp::LevelMeter inputLevel, outputLevel;
    dsp::MeterReadout inputMeter, outputMeter;

    int numPreparedChannels = 0;
    int internalBlockSize = 0;
    std::atomic<int> latencySamples { 0 };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<float> matchGainDb { 0.0f };
};

} // namespace pm
