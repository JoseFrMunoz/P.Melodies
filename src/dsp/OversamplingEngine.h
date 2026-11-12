// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/DelayLine.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>

namespace pm::dsp
{

/**
    Oversampling for one module, with every factor (1x to 16x) prepared up front.

    - Uses juce::dsp::Oversampling with linear-phase half-band FIR filters, so the delay of each
      factor is a pure, predictable delay (no phase distortion between the dry and wet paths).
    - The raw latency of a factor is usually fractional at the base rate. A small delay is added
      at the highest internal rate so that every factor has a whole-number latency in base-rate
      samples: alignment with other paths can then use plain integer delays and is exact.

    Factor indices: 0 = 1x (no oversampling), 1 = 2x, 2 = 4x, 3 = 8x, 4 = 16x.
*/
class OversamplingEngine
{
public:
    static constexpr int numFactors = 5;
    static constexpr int maxFactorIndex = numFactors - 1;

    static constexpr int factorForIndex (int index) noexcept { return 1 << index; }

    /** Allocates every factor. Call from prepareToPlay(), never from the audio thread. */
    void prepare (int numChannels, int maxBlockSize);

    /** Clears every filter state. Real-time safe. */
    void reset() noexcept;

    /** Whole-number latency, in base-rate samples, of a factor. */
    int getLatencySamples (int factorIndex) const noexcept { return latency[(size_t) factorIndex]; }

    /** Latency of the slowest (highest) factor. */
    int getMaxLatencySamples() const noexcept { return latency[(size_t) maxFactorIndex]; }

    /**
        Upsamples the block, calls processAtHighRate with the oversampled block, then downsamples
        back into the same block. With factor index 0 the callback receives the block itself.
    */
    template <typename Callback>
    void process (int factorIndex, juce::dsp::AudioBlock<float> block, Callback&& processAtHighRate) noexcept
    {
        jassert (factorIndex >= 0 && factorIndex < numFactors);

        if (factorIndex <= 0 || stages[(size_t) factorIndex] == nullptr)
        {
            processAtHighRate (block);
            return;
        }

        auto& stage = *stages[(size_t) factorIndex];
        auto up = stage.processSamplesUp (block).getSubsetChannelBlock (0, block.getNumChannels());
        processAtHighRate (up);
        alignment[(size_t) factorIndex].process (up);
        stage.processSamplesDown (block);
    }

    /**
        Upsampling only (for a sidechain that is read but not output). Gives the block at the high
        rate, aligned with what process() hands its callback at the same factor. Index 0 returns the
        block itself.
    */
    juce::dsp::AudioBlock<const float> upsample (int factorIndex, juce::dsp::AudioBlock<const float> block) noexcept
    {
        jassert (factorIndex >= 0 && factorIndex < numFactors);

        if (factorIndex <= 0 || stages[(size_t) factorIndex] == nullptr)
            return block;

        return stages[(size_t) factorIndex]->processSamplesUp (block).getSubsetChannelBlock (0, block.getNumChannels());
    }

private:
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, numFactors> stages;
    std::array<DelayLine, numFactors> alignment;
    std::array<int, numFactors> latency {};
};

} // namespace pm::dsp
