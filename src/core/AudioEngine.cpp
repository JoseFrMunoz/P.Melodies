// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/AudioEngine.h"

#include "core/DebugChecks.h"
#include "dsp/SafetyClipper.h"

namespace pm
{

namespace
{
juce::dsp::AudioBlock<float> view (juce::AudioBuffer<float>& buffer, size_t numChannels, size_t numSamples) noexcept
{
    return juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
}

bool isSettledAt (const juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& s, float value) noexcept
{
    return ! s.isSmoothing() && juce::exactlyEqual (s.getTargetValue(), value);
}
} // namespace

AudioEngine::AudioEngine (const ModuleChain::Factory& factory) : chain (factory)
{
    chain.setAnalyzerTap (ModuleId::eq, &eqAnalyzer);
    chain.forEachModule (ModuleId::compressor, [this] (ModuleBase& module)
                         {
                             if (auto* compressor = dynamic_cast<comp::CompressorModule*> (&module))
                                 compressor->setMeter (&compressorMeter);
                         });
}

int AudioEngine::internalBlockSizeFor (int hostBlockSize) noexcept
{
    // At least 512 so tiny host blocks do not force tiny allocations; at most 2048 to bound the
    // memory of the oversampling buffers. Larger host blocks are split.
    return juce::jlimit (512, 2048, hostBlockSize);
}

void AudioEngine::prepare (double newSampleRate, int hostBlockSize, int numChannels, const EngineParameters& initial)
{
    jassert (newSampleRate > 0.0 && numChannels > 0);

    numPreparedChannels = juce::jlimit (1, dsp::MeterReadout::maxChannels, numChannels);
    internalBlockSize = internalBlockSizeFor (hostBlockSize);
    sampleRate.store (newSampleRate);

    const juce::dsp::ProcessSpec spec { newSampleRate, (juce::uint32) internalBlockSize, (juce::uint32) numPreparedChannels };
    chain.setOrder (initial.chainOrder);
    chain.prepare (spec);
    // A quarter of a second: plenty for an interface that reads 30 times a second.
    eqAnalyzer.prepare (newSampleRate, juce::nextPowerOfTwo ((int) (0.25 * newSampleRate)));
    compressorMeter.prepare();

    const auto latency = chain.getLatencySamples();
    latencySamples.store (latency);

    originalDelay.prepare (numPreparedChannels, latency);
    originalDelay.setDelay (latency);
    dryDelay.prepare (numPreparedChannels, latency);
    dryDelay.setDelay (latency);
    originalBuffer.setSize (numPreparedChannels, internalBlockSize, false, true, false);
    dryBuffer.setSize (numPreparedChannels, internalBlockSize, false, true, false);

    for (auto* s : { &inputGain, &outputGain, &ceilingGain })
        s->reset (newSampleRate, smoothingSeconds);
    matchGain.reset (newSampleRate, matchGainSmoothingSeconds);
    for (auto* s : { &mix, &compareFade, &clipperFade, &bypassFade })
        s->reset (newSampleRate, smoothingSeconds);

    loudness.prepare (newSampleRate);
    inputLevel.prepare (newSampleRate);
    outputLevel.prepare (newSampleRate);

    applyParameters (initial, false, true);
    reset();
}

void AudioEngine::reset()
{
    chain.reset();
    originalDelay.reset();
    dryDelay.reset();
    loudness.reset();
    matchGain.setCurrentAndTargetValue (1.0f);
    matchGainDb.store (0.0f);
    inputLevel.reset();
    outputLevel.reset();
}

void AudioEngine::applyParameters (const EngineParameters& p, bool isOffline, bool jumpToTargets) noexcept
{
    const auto setTarget = [jumpToTargets] (auto& smoother, float value)
    {
        if (jumpToTargets)
            smoother.setCurrentAndTargetValue (value);
        else
            smoother.setTargetValue (value);
    };

    setTarget (inputGain, juce::Decibels::decibelsToGain (p.inputGainDb));
    setTarget (outputGain, juce::Decibels::decibelsToGain (p.outputGainDb));
    setTarget (mix, juce::jlimit (0.0f, 1.0f, p.mix));
    setTarget (compareFade, p.compare ? 1.0f : 0.0f);
    setTarget (clipperFade, p.clipperEnabled ? 1.0f : 0.0f);
    setTarget (ceilingGain, juce::Decibels::decibelsToGain (juce::jlimit (minCeilingDb, maxCeilingDb, p.clipperCeilingDb)));
    setTarget (bypassFade, p.bypassed ? 1.0f : 0.0f);

    chain.setOrder (p.chainOrder);

    for (int i = 0; i < numModules; ++i)
        chain.setModuleBypassed (moduleAt (i), ! p.moduleEnabled[(size_t) i]);

    chain.setMinimumOversamplingIndex (isOffline ? p.offlineOversamplingIndex : p.realtimeOversamplingIndex);
    chain.setModuleParameters (p.modules);
}

void AudioEngine::process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain,
                           const EngineParameters& parameters, bool isOffline) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    jassert (internalBlockSize > 0); // prepare() must be called first
    if (internalBlockSize <= 0)
        return;

    applyParameters (parameters, isOffline, false);

    const auto numChannels = (size_t) juce::jmin (buffer.getNumChannels(), numPreparedChannels);
    const auto totalSamples = (size_t) buffer.getNumSamples();

    if (numChannels == 0 || totalSamples == 0)
        return;

    auto full = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, numChannels);

    SidechainBlock sidechainFull;
    const bool hasSidechain = sidechain != nullptr && sidechain->getNumChannels() > 0
                              && sidechain->getNumSamples() >= (int) totalSamples;
    if (hasSidechain)
        sidechainFull = SidechainBlock (sidechain->getArrayOfReadPointers(), (size_t) sidechain->getNumChannels(), totalSamples);

    for (size_t start = 0; start < totalSamples; start += (size_t) internalBlockSize)
    {
        const auto length = juce::jmin ((size_t) internalBlockSize, totalSamples - start);
        auto sidechainChunk = hasSidechain ? sidechainFull.getSubBlock (start, length) : SidechainBlock();
        processChunk (full.getSubBlock (start, length), hasSidechain ? &sidechainChunk : nullptr);
    }

    matchGainDb.store (juce::Decibels::gainToDecibels (matchGain.getCurrentValue()));
}

void AudioEngine::applyGain (juce::dsp::AudioBlock<float> block, GainSmoother& smoother) noexcept
{
    if (! smoother.isSmoothing())
    {
        const auto g = smoother.getTargetValue();
        if (! juce::exactlyEqual (g, 1.0f))
            block.multiplyBy (g);
        return;
    }

    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        const auto g = smoother.getNextValue();
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            block.getChannelPointer (ch)[i] *= g;
    }
}

void AudioEngine::processChunk (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept
{
    const auto numSamples = block.getNumSamples();
    const auto numChannels = block.getNumChannels();

    // Original input, latency-aligned: reference for the comparison and the host bypass.
    auto original = view (originalBuffer, numChannels, numSamples);
    originalDelay.process (juce::dsp::AudioBlock<const float> (block), original);

    applyGain (block, inputGain);
    inputLevel.process (block, inputMeter);

    // Dry path of the global mix (after the input gain), latency-aligned with the chain.
    auto dry = view (dryBuffer, numChannels, numSamples);
    dryDelay.process (juce::dsp::AudioBlock<const float> (block), dry);

    chain.process (block, sidechain);
    debug::checkFinite (block, "module chain");

    // Global mix.
    if (isSettledAt (mix, 0.0f))
    {
        block.copyFrom (dry);
    }
    else if (! isSettledAt (mix, 1.0f))
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto m = mix.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& y = block.getChannelPointer (ch)[i];
                const auto d = dry.getSample ((int) ch, (int) i);
                y = d + m * (y - d);
            }
        }
    }

    applyGain (block, outputGain);

    // Compare at equal loudness: always measure, crossfade to the gain-matched original when on.
    loudness.push (block, original);
    matchGain.setTargetValue (loudness.getGain());

    if (isSettledAt (compareFade, 0.0f))
    {
        matchGain.skip ((int) numSamples);
    }
    else
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto c = compareFade.getNextValue();
            const auto g = matchGain.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& y = block.getChannelPointer (ch)[i];
                const auto reference = original.getSample ((int) ch, (int) i) * g;
                y += c * (reference - y);
            }
        }
    }

    // Safety clipper (identity below ceiling − 3 dB).
    if (isSettledAt (clipperFade, 0.0f))
    {
        ceilingGain.skip ((int) numSamples);
    }
    else
    {
        const auto kneeRatio = dsp::SafetyClipper::kneeStartFor (1.0f);

        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto k = clipperFade.getNextValue();
            const auto ceiling = ceilingGain.getNextValue();
            const auto knee = ceiling * kneeRatio;

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& y = block.getChannelPointer (ch)[i];
                const auto clipped = dsp::SafetyClipper::process (y, ceiling, knee);
                y = (k >= 1.0f) ? clipped : y + k * (clipped - y);
            }
        }
    }

    // Host bypass: crossfade to the untouched original.
    if (isSettledAt (bypassFade, 1.0f))
    {
        block.copyFrom (original);
    }
    else if (! isSettledAt (bypassFade, 0.0f))
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto b = bypassFade.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& y = block.getChannelPointer (ch)[i];
                y += b * (original.getSample ((int) ch, (int) i) - y);
            }
        }
    }

    outputLevel.process (block, outputMeter);
    debug::checkFinite (block, "engine output");
}

} // namespace pm
