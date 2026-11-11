// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <algorithm>
#include <cmath>

namespace pm::dsp
{

/**
    Soft safety clipper.

    - Below (ceiling - kneeWidthDb) the signal passes through bit-exact.
    - Between that point and the ceiling a tanh curve bends the signal towards the ceiling. The
      curve is continuous in value and slope at the knee, so there is no audible corner.
    - The output never exceeds the ceiling (the final clamp guards against rounding).

    It runs at the base sample rate on purpose (no oversampling, no added latency): it is a safety
    net that should rarely act. See docs/dsp/README.md.
*/
struct SafetyClipper
{
    static constexpr float kneeWidthDb = 3.0f;

    /** Linear knee start for a given linear ceiling. */
    static float kneeStartFor (float ceilingGain) noexcept
    {
        return ceilingGain * std::pow (10.0f, -kneeWidthDb / 20.0f);
    }

    static float process (float x, float ceilingGain, float kneeStart) noexcept
    {
        const auto magnitude = std::abs (x);

        if (magnitude <= kneeStart)
            return x;

        const auto range = ceilingGain - kneeStart;
        const auto bent = kneeStart + range * std::tanh ((magnitude - kneeStart) / range);
        return std::copysign (std::min (bent, ceilingGain), x);
    }
};

} // namespace pm::dsp
