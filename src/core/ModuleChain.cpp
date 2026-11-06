// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/ModuleChain.h"

namespace pm
{

ModuleChain::ModuleChain (const Factory& factory)
{
    for (auto& set : sets)
    {
        for (int i = 0; i < numModules; ++i)
        {
            set[(size_t) i] = factory (moduleAt (i));
            jassert (set[(size_t) i] != nullptr && set[(size_t) i]->getId() == moduleAt (i));
        }
    }
}

void ModuleChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    maxBlockSize = (int) spec.maximumBlockSize;

    for (auto& set : sets)
        for (auto& module : set)
            module->prepare (spec);

    latency = 0;
    for (auto& module : sets[0])
        latency += module->getLatencySamples();

#if JUCE_DEBUG
    int spareLatency = 0;
    for (auto& module : sets[1])
        spareLatency += module->getLatencySamples();
    jassert (spareLatency == latency);
#endif

    // The spare chain must refill every delay and filter before it is heard. Module latencies
    // bound the memory of each path; doubling covers the symmetric FIR responses.
    preRollLength = 2 * latency + 64;
    fadeStep = (float) (1.0 / juce::jmax (1.0, crossfadeSeconds * spec.sampleRate));

    spareBuffer.setSize ((int) spec.numChannels, maxBlockSize, false, true, false);
    fadeGains.assign ((size_t) maxBlockSize, 0.0f);

    reset();
}

void ModuleChain::reset()
{
    for (auto& set : sets)
        for (auto& module : set)
            module->reset();

    // A reset jumps straight to the requested order: there is nothing to crossfade from.
    activeOrder = requestedOrder;
    targetOrder = requestedOrder;
    phase = Phase::idle;
    fade = 0.0f;
    preRollRemaining = 0;
}

void ModuleChain::setModuleBypassed (ModuleId id, bool shouldBeBypassed) noexcept
{
    for (auto& set : sets)
        set[(size_t) toIndex (id)]->setBypassed (shouldBeBypassed);
}

void ModuleChain::setMinimumOversamplingIndex (int index) noexcept
{
    for (auto& set : sets)
        for (auto& module : set)
            module->setMinimumOversamplingIndex (index);
}

void ModuleChain::runSet (int setIndex, int orderIndex, juce::dsp::AudioBlock<float> block,
                          const SidechainBlock* sidechain) noexcept
{
    const auto order = chain::decode (orderIndex);

    for (auto id : order)
        sets[(size_t) setIndex][(size_t) toIndex (id)]->process (block, sidechain);
}

void ModuleChain::beginTransition() noexcept
{
    const auto spare = 1 - activeSet;

    for (int i = 0; i < numModules; ++i)
        sets[(size_t) spare][(size_t) i]->copyStateFrom (*sets[(size_t) activeSet][(size_t) i]);

    targetOrder = requestedOrder;
    phase = Phase::preRoll;
    preRollRemaining = preRollLength;
    fade = 0.0f;
}

void ModuleChain::process (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept
{
    const auto numSamples = block.getNumSamples();
    const auto numChannels = juce::jmin (block.getNumChannels(), (size_t) spareBuffer.getNumChannels());

    jassert ((int) numSamples <= maxBlockSize);

    if (numSamples == 0 || numChannels == 0)
        return;

    block = block.getSubsetChannelBlock (0, numChannels);

    // A new request while the spare chain is still silent simply restarts it with the new order.
    // During the audible crossfade the request waits until the crossfade has finished.
    if ((phase == Phase::idle && requestedOrder != activeOrder)
        || (phase == Phase::preRoll && requestedOrder != targetOrder))
        beginTransition();

    if (phase == Phase::idle)
    {
        runSet (activeSet, activeOrder, block, sidechain);
        return;
    }

    const auto spare = 1 - activeSet;
    auto spareBlock = juce::dsp::AudioBlock<float> (spareBuffer).getSubsetChannelBlock (0, numChannels).getSubBlock (0, numSamples);
    spareBlock.copyFrom (block);

    runSet (activeSet, activeOrder, block, sidechain);
    runSet (spare, targetOrder, spareBlock, sidechain);

    bool finished = false;

    for (size_t i = 0; i < numSamples; ++i)
    {
        if (phase == Phase::preRoll)
        {
            if (preRollRemaining > 0)
            {
                --preRollRemaining;
                fadeGains[i] = 0.0f;
                continue;
            }
            phase = Phase::crossfade;
        }

        fade = juce::jmin (1.0f, fade + fadeStep);
        fadeGains[i] = fade;
        finished = fade >= 1.0f;
    }

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* out = block.getChannelPointer (ch);
        const auto* next = spareBlock.getChannelPointer (ch);

        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto g = fadeGains[i];

            if (g >= 1.0f)
                out[i] = next[i];
            else if (g > 0.0f)
                out[i] += g * (next[i] - out[i]);
        }
    }

    if (finished)
    {
        activeSet = spare;
        activeOrder = targetOrder;
        phase = Phase::idle;
        fade = 0.0f;
    }
}

} // namespace pm
