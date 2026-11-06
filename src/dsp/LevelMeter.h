// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>

namespace pm::dsp
{

/**
    Lock-free hand-over of meter values from the audio thread to the interface.

    The audio thread only ever raises the peak (atomic max) and overwrites the RMS. The interface
    takes the peak (resetting it to zero), reads the RMS and clears the clip flag on click.
*/
struct MeterReadout
{
    static constexpr int maxChannels = 2;

    std::array<std::atomic<float>, maxChannels> peak {};
    std::array<std::atomic<float>, maxChannels> rms {};
    std::atomic<bool> clipped { false };
    std::atomic<int> numChannels { maxChannels };

    /** Interface thread: returns the highest peak since the last call and resets it. */
    float takePeak (int channel) noexcept { return peak[(size_t) channel].exchange (0.0f); }

    float getRms (int channel) const noexcept { return rms[(size_t) channel].load(); }

    void resetClip() noexcept { clipped.store (false); }
};

/** Audio-thread side of a peak + RMS meter. */
class LevelMeter
{
public:
    static constexpr double rmsWindowSeconds = 0.3;
    static constexpr float clipThreshold = 1.0f; // 0 dBFS

    void prepare (double sampleRate) noexcept
    {
        coefficient = std::exp (-1.0 / (rmsWindowSeconds * sampleRate));
        reset();
    }

    void reset() noexcept { meanSquare.fill (0.0); }

    void process (const juce::dsp::AudioBlock<const float>& block, MeterReadout& out) noexcept
    {
        const auto numChannels = juce::jmin ((int) block.getNumChannels(), MeterReadout::maxChannels);
        const auto numSamples = block.getNumSamples();
        out.numChannels.store (numChannels);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* x = block.getChannelPointer ((size_t) ch);
            auto ms = meanSquare[(size_t) ch];
            float blockPeak = 0.0f;

            for (size_t i = 0; i < numSamples; ++i)
            {
                const auto v = x[i];
                blockPeak = juce::jmax (blockPeak, std::abs (v));
                ms = coefficient * ms + (1.0 - coefficient) * (double) v * (double) v;
            }

            meanSquare[(size_t) ch] = ms;
            raise (out.peak[(size_t) ch], blockPeak);
            out.rms[(size_t) ch].store ((float) std::sqrt (ms));

            if (blockPeak >= clipThreshold)
                out.clipped.store (true);
        }
    }

private:
    static void raise (std::atomic<float>& target, float value) noexcept
    {
        auto previous = target.load();
        while (previous < value && ! target.compare_exchange_weak (previous, value))
        {
        }
    }

    double coefficient = 0.0;
    std::array<double, MeterReadout::maxChannels> meanSquare {};
};

} // namespace pm::dsp
