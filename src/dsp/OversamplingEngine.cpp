// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "dsp/OversamplingEngine.h"

#include <cmath>

namespace pm::dsp
{

void OversamplingEngine::prepare (int numChannels, int maxBlockSize)
{
    jassert (numChannels > 0 && maxBlockSize > 0);

    latency.fill (0);

    for (int index = 1; index < numFactors; ++index)
    {
        auto stage = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels,
            (size_t) index,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,   // maximum quality
            false); // integer latency is handled below, without interpolation

        stage->initProcessing ((size_t) maxBlockSize);

        const auto factor = factorForIndex (index);
        const auto rawLatency = (double) stage->getLatencyInSamples();
        const auto rawAtTopRate = rawLatency * factor;
        const auto roundedAtTopRate = std::round (rawAtTopRate);

        // FIR half-band stages always have a whole-number latency at the highest rate.
        jassert (std::abs (rawAtTopRate - roundedAtTopRate) < 1.0e-3);

        const auto wholeLatency = (int) std::ceil (rawLatency - 1.0e-6);
        const auto extraAtTopRate = wholeLatency * factor - (int) roundedAtTopRate;
        jassert (extraAtTopRate >= 0 && extraAtTopRate < factor);

        alignment[(size_t) index].prepare (numChannels, factor);
        alignment[(size_t) index].setDelay (juce::jlimit (0, factor, extraAtTopRate));

        latency[(size_t) index] = wholeLatency;
        stages[(size_t) index] = std::move (stage);
    }

    alignment[0].prepare (numChannels, 0);
    reset();
}

void OversamplingEngine::reset() noexcept
{
    for (int index = 1; index < numFactors; ++index)
    {
        if (stages[(size_t) index] != nullptr)
            stages[(size_t) index]->reset();

        alignment[(size_t) index].reset();
    }
}

} // namespace pm::dsp
