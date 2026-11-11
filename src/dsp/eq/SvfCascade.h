// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/eq/EqFilterDesign.h"

#include <algorithm>
#include <array>

namespace pm::dsp::eq
{

/**
    Runs a designed band on audio: a cascade of TPT state-variable sections (Simper 2013).

    The state of each section is its two integrator "currents", which do not jump when the
    coefficients change. New coefficients can also be reached gradually: setCoefficients() with a
    ramp length moves g, k and the output mix linearly, sample by sample, from the current values
    to the new ones. A gliding band designs its filter every few samples and ramps between those
    designs, so neither the filter state nor the output mix ever steps (no zipper noise, even
    with large gain changes). Mono or stereo, no allocation.
*/
class SvfCascade
{
public:
    static constexpr int maxChannels = 2;

    /**
        Takes new coefficients and keeps the state. With rampSamples > 0 they are reached
        linearly over that many samples; otherwise at once. Sections that were not running start
        from rest.
    */
    void setCoefficients (const DigitalBand& band, int rampSamples = 0) noexcept
    {
        const auto count = (size_t) std::clamp (band.numSections, 0, maxSections);
        const auto ramp = rampSamples > 0 && count == numSections ? rampSamples : 0;

        for (size_t i = 0; i < count; ++i)
        {
            const auto& s = band.sections[i];
            const Values next { (float) s.g, (float) s.k, (float) s.m0, (float) s.m1, (float) s.m2 };
            auto& section = sections[i];
            section.firstOrder = s.firstOrder;
            if (ramp > 0)
            {
                section.step = (next - section.current) * (1.0f / (float) ramp);
                section.target = next;
            }
            else
            {
                section.current = section.target = next;
                section.step = {};
                section.updateDerived();
            }
        }

        for (auto i = count; i < numSections; ++i)
            for (auto& channel : state)
                channel[i] = {};

        numSections = count;
        rampRemaining = ramp;
    }

    void reset() noexcept
    {
        for (auto& channel : state)
            channel.fill ({});
    }

    /** Processes one sample on every channel (frame[0 .. numChannels)) in place. */
    void processFrame (float* frame, int numChannels) noexcept
    {
        if (rampRemaining > 0)
            advanceRamp();

        const auto channels = (size_t) std::min (numChannels, maxChannels);
        for (size_t i = 0; i < numSections; ++i)
        {
            const auto& c = sections[i];
            for (size_t ch = 0; ch < channels; ++ch)
            {
                auto& s = state[ch][i];
                const auto x = frame[ch];
                if (c.firstOrder)
                {
                    const auto v = (x - s[0]) * c.a1;
                    const auto low = v + s[0];
                    s[0] = low + v;
                    frame[ch] = c.current.m0 * x + c.current.m2 * low;
                }
                else
                {
                    const auto v3 = x - s[1];
                    const auto v1 = c.a1 * s[0] + c.a2 * v3;
                    const auto v2 = s[1] + c.a2 * s[0] + c.a3 * v3;
                    s[0] = 2.0f * v1 - s[0];
                    s[1] = 2.0f * v2 - s[1];
                    frame[ch] = c.current.m0 * x + c.current.m1 * v1 + c.current.m2 * v2;
                }
            }
        }
    }

    /** One sample of a single (mono) signal. */
    float processSample (float x) noexcept
    {
        processFrame (&x, 1);
        return x;
    }

    int getNumSections() const noexcept { return (int) numSections; }
    bool isRamping() const noexcept { return rampRemaining > 0; }

private:
    struct Values
    {
        float g = 0.0f, k = 0.0f, m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;

        Values operator- (const Values& o) const noexcept { return { g - o.g, k - o.k, m0 - o.m0, m1 - o.m1, m2 - o.m2 }; }
        Values operator* (float f) const noexcept { return { g * f, k * f, m0 * f, m1 * f, m2 * f }; }
        Values& operator+= (const Values& o) noexcept
        {
            g += o.g;
            k += o.k;
            m0 += o.m0;
            m1 += o.m1;
            m2 += o.m2;
            return *this;
        }
    };

    struct Section
    {
        Values current, target, step;
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f; // derived from g and k
        bool firstOrder = false;

        void updateDerived() noexcept
        {
            if (firstOrder)
            {
                a1 = current.g / (1.0f + current.g);
                a2 = a3 = 0.0f;
            }
            else
            {
                a1 = 1.0f / (1.0f + current.g * (current.g + current.k));
                a2 = current.g * a1;
                a3 = current.g * a2;
            }
        }
    };

    void advanceRamp() noexcept
    {
        --rampRemaining;
        for (size_t i = 0; i < numSections; ++i)
        {
            auto& section = sections[i];
            if (rampRemaining == 0)
                section.current = section.target;
            else
                section.current += section.step;
            section.updateDerived();
        }
    }

    std::array<Section, maxSections> sections {};
    std::array<std::array<std::array<float, 2>, maxSections>, maxChannels> state {};
    size_t numSections = 0;
    int rampRemaining = 0;
};

} // namespace pm::dsp::eq
