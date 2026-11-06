// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>

namespace pm::dsp
{

/**
    Measures the integrated RMS of two signals and returns the gain that makes the reference
    as loud as the processed one.

    Each signal's mean power is integrated with a one-pole average (time constant of a few
    seconds), in double precision so that very long time constants at high sample rates stay
    accurate. While either signal is near silence the last valid gain is held, so pauses in the
    music do not make the match drift.
*/
class LoudnessMatcher
{
public:
    static constexpr double integrationSeconds = 3.0;
    static constexpr float maxGainDb = 24.0f;

    void prepare (double sampleRate) noexcept
    {
        coefficient = std::exp (-1.0 / (integrationSeconds * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        processedPower = 0.0;
        referencePower = 0.0;
        gain = 1.0f;
    }

    /** Integrates both signals. They must have the same size. */
    void push (const juce::dsp::AudioBlock<const float>& processed,
               const juce::dsp::AudioBlock<const float>& reference) noexcept
    {
        jassert (processed.getNumSamples() == reference.getNumSamples());

        const auto numSamples = processed.getNumSamples();
        const auto numChannels = juce::jmin (processed.getNumChannels(), reference.getNumChannels());

        if (numChannels == 0)
            return;

        const auto channelScale = 1.0 / (double) numChannels;
        const auto a = coefficient;

        for (size_t i = 0; i < numSamples; ++i)
        {
            double p = 0.0, r = 0.0;

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                const auto x = (double) processed.getSample ((int) ch, (int) i);
                const auto y = (double) reference.getSample ((int) ch, (int) i);
                p += x * x;
                r += y * y;
            }

            processedPower = a * processedPower + (1.0 - a) * p * channelScale;
            referencePower = a * referencePower + (1.0 - a) * r * channelScale;
        }

        updateGain();
    }

    /** Linear gain to apply to the reference. */
    float getGain() const noexcept { return gain; }

private:
    void updateGain() noexcept
    {
        constexpr double silenceFloor = 1.0e-10; // -100 dBFS mean power

        if (processedPower < silenceFloor || referencePower < silenceFloor)
            return; // hold

        const auto limit = (double) juce::Decibels::decibelsToGain (maxGainDb);
        const auto g = std::sqrt (processedPower / referencePower);
        gain = (float) juce::jlimit (1.0 / limit, limit, g);
    }

    double coefficient = 0.0;
    double processedPower = 0.0;
    double referencePower = 0.0;
    float gain = 1.0f;
};

} // namespace pm::dsp
