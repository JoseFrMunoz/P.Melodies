// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "PluginProcessor.h"
#include "core/AudioEngine.h"
#include "core/ModuleBase.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace pmtest
{

inline constexpr double twoPi = 6.283185307179586;

/** Sample rates the plugin must support (spec §2). */
inline const std::vector<double> allSampleRates { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

/** Deterministic multichannel noise in [-amplitude, amplitude]. */
inline juce::AudioBuffer<float> makeNoise (int numChannels, int numSamples, float amplitude, int seed = 1)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    juce::Random random (seed);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (ch, i, amplitude * (2.0f * random.nextFloat() - 1.0f));
    return buffer;
}

inline juce::AudioBuffer<float> makeSine (int numChannels, int numSamples, double frequency, double sampleRate,
                                          float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (ch, i, amplitude * (float) std::sin (twoPi * frequency * i / sampleRate + 0.3 * ch));
    return buffer;
}

inline juce::AudioBuffer<float> makeImpulse (int numChannels, int numSamples, int position, float amplitude)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.setSample (ch, position, amplitude);
    return buffer;
}

/** Block sizes for a run: fixed, or pseudo-random between 1 and maxSize when size <= 0. */
inline std::vector<int> blockSizes (int totalSamples, int size, int maxSize = 3000, int seed = 7)
{
    std::vector<int> sizes;
    juce::Random random (seed);
    int done = 0;
    while (done < totalSamples)
    {
        auto n = size > 0 ? size : 1 + random.nextInt (maxSize);
        n = juce::jmin (n, totalSamples - done);
        sizes.push_back (n);
        done += n;
    }
    return sizes;
}

inline bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;
    return true;
}

inline float peakOf (const juce::AudioBuffer<float>& buffer, int start = 0, int end = -1)
{
    if (end < 0)
        end = buffer.getNumSamples();
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = start; i < end; ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));
    return peak;
}

inline double rmsOf (const juce::AudioBuffer<float>& buffer, int start, int end)
{
    double sum = 0.0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = start; i < end; ++i)
            sum += (double) buffer.getSample (ch, i) * buffer.getSample (ch, i);
    return std::sqrt (sum / ((double) buffer.getNumChannels() * (end - start)));
}

/** Largest |x[n] - x[n-1]| between start and end, over every channel. */
inline float maxStep (const juce::AudioBuffer<float>& buffer, int start, int end)
{
    float step = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = juce::jmax (1, start); i < end; ++i)
            step = juce::jmax (step, std::abs (buffer.getSample (ch, i) - buffer.getSample (ch, i - 1)));
    return step;
}

/** Index of the largest |sample| on channel 0. */
inline int peakIndex (const juce::AudioBuffer<float>& buffer)
{
    int index = 0;
    float best = -1.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        if (std::abs (buffer.getSample (0, i)) > best)
        {
            best = std::abs (buffer.getSample (0, i));
            index = i;
        }
    return index;
}

//==============================================================================
/** Sets a parameter from its real (denormalised) value, as the host would. */
inline void setParameter (pm::PMelodiesProcessor& processor, const juce::String& parameterId, float value)
{
    auto* parameter = processor.getState().getParameter (parameterId);
    jassert (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Prepares a processor for the given layout. sidechainChannels = 0 disables the sidechain bus. */
inline void prepareProcessor (pm::PMelodiesProcessor& processor, double sampleRate, int blockSize, int numChannels,
                              int sidechainChannels = 2)
{
    juce::AudioProcessor::BusesLayout layout;
    const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    layout.inputBuses.add (set);
    layout.outputBuses.add (set);
    layout.inputBuses.add (sidechainChannels == 0   ? juce::AudioChannelSet::disabled()
                           : sidechainChannels == 1 ? juce::AudioChannelSet::mono()
                                                    : juce::AudioChannelSet::stereo());
    const auto ok = processor.setBusesLayout (layout);
    jassert (ok);
    juce::ignoreUnused (ok);
    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
}

/**
    Runs the processor over `input` (main channels) in blocks, as a host would: one buffer per
    call holding the main inputs followed by the sidechain inputs. Returns the main output.
*/
inline juce::AudioBuffer<float> runProcessor (pm::PMelodiesProcessor& processor, const juce::AudioBuffer<float>& input,
                                              const std::vector<int>& sizes,
                                              const juce::AudioBuffer<float>* sidechain = nullptr,
                                              const std::function<void (int blockIndex, int startSample)>& beforeBlock = {})
{
    const auto mainChannels = input.getNumChannels();
    const auto totalChannels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    const auto sidechainChannels = processor.getTotalNumInputChannels() - processor.getMainBusNumInputChannels();

    juce::AudioBuffer<float> output (mainChannels, input.getNumSamples());
    juce::AudioBuffer<float> block (totalChannels, juce::jmax (1, *std::max_element (sizes.begin(), sizes.end())));
    juce::MidiBuffer midi;

    int start = 0;
    for (size_t b = 0; b < sizes.size(); ++b)
    {
        const auto n = sizes[b];
        if (beforeBlock)
            beforeBlock ((int) b, start);

        juce::AudioBuffer<float> io (block.getArrayOfWritePointers(), totalChannels, n);
        io.clear();
        for (int ch = 0; ch < mainChannels; ++ch)
            io.copyFrom (ch, 0, input, ch, start, n);
        if (sidechain != nullptr)
            for (int ch = 0; ch < sidechainChannels; ++ch)
                io.copyFrom (mainChannels + ch, 0, *sidechain, ch % sidechain->getNumChannels(), start, n);

        processor.processBlock (io, midi);

        for (int ch = 0; ch < mainChannels; ++ch)
            output.copyFrom (ch, start, io, ch, 0, n);
        start += n;
    }

    return output;
}

/** Same for the bare engine. */
inline juce::AudioBuffer<float> runEngine (pm::AudioEngine& engine, const juce::AudioBuffer<float>& input,
                                           const std::vector<int>& sizes, const pm::EngineParameters& parameters,
                                           bool offline = false,
                                           const std::function<void (int blockIndex, int startSample, pm::EngineParameters&)>& beforeBlock = {})
{
    juce::AudioBuffer<float> output (input);
    auto p = parameters;
    int start = 0;
    for (size_t b = 0; b < sizes.size(); ++b)
    {
        const auto n = sizes[b];
        if (beforeBlock)
            beforeBlock ((int) b, start, p);
        juce::AudioBuffer<float> io (output.getArrayOfWritePointers(), output.getNumChannels(), start, n);
        engine.process (io, nullptr, p, offline);
        start += n;
    }
    return output;
}

//==============================================================================
/**
    Configurable module used to give the chain tests something audible: a gain, an optional
    hard clip, an optional one-pole low-pass (stateful) and an optional constant core latency.
*/
class TestModule final : public pm::ModuleBase
{
public:
    struct Settings
    {
        float gain = 1.0f;
        float clipLevel = 0.0f;        // 0 = no clipping
        double lowpassHz = 0.0;        // 0 = no filter
        int coreLatency = 0;           // base-rate samples (only without oversampling)
        bool usesOversampling = false;
        int requestedOversampling = 0; // factor index
        bool neutral = false;
    };

    TestModule (pm::ModuleId slot, Settings s) : pm::ModuleBase (slot, s.usesOversampling), settings (s) {}

    Settings settings;

protected:
    void prepareCore (double baseRate, int maxBlock, int numChannels) override
    {
        juce::ignoreUnused (baseRate, maxBlock);
        z.assign ((size_t) numChannels, 0.0f);
        latencyLine.prepare (numChannels, settings.coreLatency);
        latencyLine.setDelay (settings.coreLatency);
    }

    void setCoreSampleRate (double rate) noexcept override
    {
        coefficient = settings.lowpassHz > 0.0 ? (float) std::exp (-twoPi * settings.lowpassHz / rate) : 0.0f;
    }

    void resetCore() noexcept override
    {
        std::fill (z.begin(), z.end(), 0.0f);
        latencyLine.reset();
    }

    void processCore (juce::dsp::AudioBlock<float> block, const pm::SidechainBlock*) noexcept override
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* x = block.getChannelPointer (ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                auto v = x[i] * settings.gain;
                if (settings.clipLevel > 0.0f)
                    v = juce::jlimit (-settings.clipLevel, settings.clipLevel, v);
                if (settings.lowpassHz > 0.0)
                {
                    z[ch] = v + coefficient * (z[ch] - v);
                    v = z[ch];
                }
                x[i] = v;
            }
        }
        if (settings.coreLatency > 0)
            latencyLine.process (block);
    }

    bool isCoreNeutral() const noexcept override { return settings.neutral; }
    int getRequestedOversamplingIndex() const noexcept override { return settings.requestedOversampling; }
    int getCoreLatencySamples() const noexcept override { return settings.coreLatency; }

    void copyCoreStateFrom (const pm::ModuleBase& other) noexcept override
    {
        if (auto* o = dynamic_cast<const TestModule*> (&other))
        {
            z = o->z;
            latencyLine.copyStateFrom (o->latencyLine);
        }
    }

private:
    std::vector<float> z;
    float coefficient = 0.0f;
    pm::dsp::DelayLine latencyLine;
};

/** Factory giving each slot its own TestModule settings. */
inline pm::ModuleChain::Factory testFactory (std::array<TestModule::Settings, pm::numModules> settings)
{
    return [settings] (pm::ModuleId id) -> std::unique_ptr<pm::ModuleBase>
    { return std::make_unique<TestModule> (id, settings[(size_t) pm::toIndex (id)]); };
}

/** Every module neutral except the given settings. */
inline std::array<TestModule::Settings, pm::numModules> neutralSettings()
{
    std::array<TestModule::Settings, pm::numModules> s {};
    for (auto& m : s)
        m.neutral = true;
    return s;
}

} // namespace pmtest
