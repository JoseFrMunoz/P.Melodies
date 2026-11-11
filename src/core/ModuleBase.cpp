// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/ModuleBase.h"

#include "core/DebugChecks.h"

namespace pm
{

ModuleBase::ModuleBase (ModuleId moduleId, bool moduleUsesOversampling) noexcept
    : id (moduleId), usesOversampling (moduleUsesOversampling)
{
}

void ModuleBase::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.sampleRate > 0.0 && spec.maximumBlockSize > 0 && spec.numChannels > 0);

    baseSampleRate = spec.sampleRate;
    maxBlockSize = (int) spec.maximumBlockSize;
    const auto numChannels = (int) spec.numChannels;

    prepareCore (baseSampleRate, maxBlockSize, numChannels);

    maxOversamplingLatency = 0;

    if (usesOversampling)
    {
        oversampling.prepare (numChannels, maxBlockSize);
        maxOversamplingLatency = oversampling.getMaxLatencySamples();
    }

    totalLatency = getCoreLatencySamples() + maxOversamplingLatency;
    preRollSamples = totalLatency + maxOversamplingLatency + getCoreSettleSamples();
    gainStep = (float) (1.0 / juce::jmax (1.0, fadeSeconds * baseSampleRate));

    dryBuffer.setSize (numChannels, maxBlockSize, false, true, false);
    dryDelay.prepare (numChannels, totalLatency);
    dryDelay.setDelay (totalLatency);
    wetPadding.prepare (numChannels, maxOversamplingLatency);

    activeOversamplingIndex = desiredOversamplingIndex();
    applyOversamplingIndex (activeOversamplingIndex);

    reset();
}

void ModuleBase::reset()
{
    dryDelay.reset();
    wetPadding.reset();

    if (usesOversampling)
        oversampling.reset();

    resetCore();

    stage = Stage::disengaged;
    gain = 0.0f;
    preRollRemaining = 0;
}

void ModuleBase::setMinimumOversamplingIndex (int index) noexcept
{
    minimumOversamplingIndex = juce::jlimit (0, dsp::OversamplingEngine::maxFactorIndex, index);
}

int ModuleBase::desiredOversamplingIndex() const noexcept
{
    if (! usesOversampling)
        return 0;

    return juce::jlimit (0, dsp::OversamplingEngine::maxFactorIndex,
                         juce::jmax (getRequestedOversamplingIndex(), minimumOversamplingIndex));
}

void ModuleBase::applyOversamplingIndex (int index) noexcept
{
    activeOversamplingIndex = index;
    const auto pathLatency = usesOversampling ? oversampling.getLatencySamples (index) : 0;
    wetPadding.setDelay (maxOversamplingLatency - pathLatency);
    setCoreSampleRate (baseSampleRate * dsp::OversamplingEngine::factorForIndex (index));
}

void ModuleBase::updateStageAtBlockStart() noexcept
{
    const auto desiredIndex = desiredOversamplingIndex();
    const auto needsFactorChange = desiredIndex != activeOversamplingIndex;
    const auto shouldRun = ! bypassed && ! isCoreNeutral();
    const auto wantsWet = shouldRun && ! needsFactorChange;

    switch (stage)
    {
        case Stage::engaged:
        case Stage::fadingIn:
            if (! wantsWet)
                stage = Stage::fadingOut;
            break;

        case Stage::fadingOut:
            if (wantsWet)
                stage = Stage::fadingIn;
            break;

        case Stage::preRoll:
            if (! wantsWet)
            {
                stage = Stage::disengaged;
                gain = 0.0f;
            }
            break;

        case Stage::disengaged:
            break;
    }

    if (stage == Stage::disengaged)
    {
        if (needsFactorChange)
            applyOversamplingIndex (desiredIndex);

        if (shouldRun)
        {
            resetCore();

            if (usesOversampling)
                oversampling.reset();

            wetPadding.reset();
            stage = Stage::preRoll;
            preRollRemaining = preRollSamples;
            gain = 0.0f;
        }
    }
}

float ModuleBase::nextGain() noexcept
{
    switch (stage)
    {
        case Stage::preRoll:
            if (preRollRemaining > 0)
            {
                --preRollRemaining;
                return 0.0f;
            }
            stage = Stage::fadingIn;
            [[fallthrough]];

        case Stage::fadingIn:
            gain += gainStep;
            if (gain >= 1.0f)
            {
                gain = 1.0f;
                stage = Stage::engaged;
            }
            return gain;

        case Stage::fadingOut:
            gain -= gainStep;
            if (gain <= 0.0f)
            {
                gain = 0.0f;
                stage = Stage::disengaged;
            }
            return gain;

        case Stage::engaged:
            return 1.0f;

        case Stage::disengaged:
            break;
    }

    return 0.0f;
}

void ModuleBase::process (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept
{
    const auto numSamples = block.getNumSamples();
    const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) dryBuffer.getNumChannels());

    jassert ((int) numSamples <= maxBlockSize);

    if (numSamples == 0 || numChannels == 0)
        return;

    block = block.getSubsetChannelBlock (0, numChannels);

    // 1. Latency-aligned dry path (read before the block is overwritten).
    auto dry = juce::dsp::AudioBlock<float> (dryBuffer).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
    dryDelay.process (juce::dsp::AudioBlock<const float> (block), dry);

    // 2. Decide what the wet path does during this block.
    updateStageAtBlockStart();

    if (stage == Stage::disengaged)
    {
        block.copyFrom (dry);
        return;
    }

    // 3. Wet path, in place.
    oversampling.process (usesOversampling ? activeOversamplingIndex : 0, block,
                          [this, sidechain] (juce::dsp::AudioBlock<float> highRate) { processCore (highRate, sidechain); });
    wetPadding.process (block);

    debug::checkFinite (block, "module wet path");

    // 4. Fade between dry and wet.
    if (stage == Stage::engaged)
        return;

    for (size_t i = 0; i < numSamples; ++i)
    {
        const auto g = nextGain();

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* out = block.getChannelPointer (ch);
            const auto d = dry.getSample ((int) ch, (int) i);

            if (g <= 0.0f)
                out[i] = d;
            else if (g < 1.0f)
                out[i] = d + g * (out[i] - d);
        }
    }
}

void ModuleBase::copyStateFrom (const ModuleBase& other) noexcept
{
    jassert (other.id == id && other.totalLatency == totalLatency);

    if (other.id != id || other.totalLatency != totalLatency)
        return;

    dryDelay.copyStateFrom (other.dryDelay);

    if (other.activeOversamplingIndex != activeOversamplingIndex)
        applyOversamplingIndex (other.activeOversamplingIndex);

    wetPadding.copyStateFrom (other.wetPadding);

    if (usesOversampling)
        oversampling.reset();

    bypassed = other.bypassed;
    minimumOversamplingIndex = other.minimumOversamplingIndex;
    stage = other.stage;
    gain = other.gain;
    preRollRemaining = other.preRollRemaining;

    copyCoreStateFrom (other);
}

} // namespace pm
