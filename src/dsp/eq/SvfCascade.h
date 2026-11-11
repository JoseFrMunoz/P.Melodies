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

    The state of each section is its two integrator "currents", which change smoothly when the
    coefficients change, so new coefficients can be set at any time (every few samples while a
    parameter glides) without clicks or instability. Mono or stereo, no allocation.
*/
class SvfCascade
{
public:
    static constexpr int maxChannels = 2;

    /** Takes new coefficients and keeps the state. Sections that were not running start from rest. */
    void setCoefficients (const DigitalBand& band) noexcept
    {
        const auto count = (size_t) std::clamp (band.numSections, 0, maxSections);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& s = band.sections[i];
            auto& c = coefficients[i];
            c.firstOrder = s.firstOrder;
            if (s.firstOrder)
            {
                c.a1 = (float) (s.g / (1.0 + s.g));
                c.a2 = c.a3 = 0.0f;
            }
            else
            {
                const auto a1 = 1.0 / (1.0 + s.g * (s.g + s.k));
                c.a1 = (float) a1;
                c.a2 = (float) (s.g * a1);
                c.a3 = (float) (s.g * s.g * a1);
            }
            c.m0 = (float) s.m0;
            c.m1 = (float) s.m1;
            c.m2 = (float) s.m2;
        }

        for (auto i = count; i < numSections; ++i)
            for (auto& channel : state)
                channel[i] = {};

        numSections = count;
    }

    void reset() noexcept
    {
        for (auto& channel : state)
            channel.fill ({});
    }

    /** channel is 0 or 1. */
    float processSample (float x, size_t channel) noexcept
    {
        auto& channelState = state[channel & 1u];
        for (size_t i = 0; i < numSections; ++i)
        {
            const auto& c = coefficients[i];
            auto& s = channelState[i];
            if (c.firstOrder)
            {
                const auto v = (x - s[0]) * c.a1;
                const auto low = v + s[0];
                s[0] = low + v;
                x = c.m0 * x + c.m2 * low;
            }
            else
            {
                const auto v3 = x - s[1];
                const auto v1 = c.a1 * s[0] + c.a2 * v3;
                const auto v2 = s[1] + c.a2 * s[0] + c.a3 * v3;
                s[0] = 2.0f * v1 - s[0];
                s[1] = 2.0f * v2 - s[1];
                x = c.m0 * x + c.m1 * v1 + c.m2 * v2;
            }
        }
        return x;
    }

    int getNumSections() const noexcept { return (int) numSections; }

private:
    struct Coefficients
    {
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
        bool firstOrder = false;
    };

    std::array<Coefficients, maxSections> coefficients {};
    std::array<std::array<std::array<float, 2>, maxSections>, maxChannels> state {};
    size_t numSections = 0;
};

} // namespace pm::dsp::eq
