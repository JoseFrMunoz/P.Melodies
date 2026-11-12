// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "dsp/comp/CompressorCurve.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pm::dsp::comp
{

namespace
{
StyleTraits makeTraits (Style style) noexcept
{
    StyleTraits t;

    switch (style)
    {
        case Style::clean:
            break;

        case Style::classic: // feedback: detects part of its own output
            t.feedback = 0.5;
            t.memoryWeight = 0.5;
            t.memoryAttackMs = 300.0;
            t.memoryReleaseScale = 4.0;
            break;

        case Style::opto: // slow, very soft knee, release that lengthens with the history
            t.rmsDetector = true;
            t.rmsMs = 10.0;
            t.extraKneeDb = 12.0;
            t.memoryWeight = 0.5;
            t.memoryAttackMs = 1000.0;
            t.memoryReleaseScale = 10.0;
            break;

        case Style::vocal: // automatic ratio and knee, adaptive release
            t.automaticCurve = true;
            t.memoryWeight = 0.5;
            t.memoryAttackMs = 200.0;
            t.memoryReleaseScale = 4.0;
            break;

        case Style::mastering: // average level, very smooth changes
            t.rmsDetector = true;
            t.rmsMs = 25.0;
            t.memoryWeight = 0.5;
            t.memoryAttackMs = 1000.0;
            t.memoryReleaseScale = 5.0;
            t.smoothingMs = 2.0;
            break;

        case Style::bus: // average level, some feedback, two-stage release
            t.rmsDetector = true;
            t.rmsMs = 10.0;
            t.feedback = 0.25;
            t.memoryWeight = 0.6;
            t.memoryAttackMs = 250.0;
            t.memoryReleaseScale = 5.0;
            break;

        case Style::punch: // lets the start of the transient through, recovers fast
            t.slowOnsetAttack = true;
            t.releaseScale = 0.6;
            break;

        case Style::pumping: // release slow at first and fast at the end
            t.pumpingRelease = true;
            break;
    }

    return t;
}

const std::array<StyleTraits, numStyles> allTraits {
    makeTraits (Style::clean),     makeTraits (Style::classic), makeTraits (Style::opto), makeTraits (Style::vocal),
    makeTraits (Style::mastering), makeTraits (Style::bus),     makeTraits (Style::punch), makeTraits (Style::pumping)
};
} // namespace

const StyleTraits& traitsOf (Style style) noexcept
{
    const auto index = std::clamp ((int) style, 0, numStyles - 1);
    return allTraits[(size_t) index];
}

StyleTraits withAutoRelease (StyleTraits traits) noexcept
{
    if (traits.memoryWeight <= 0.0)
        traits.memoryAttackMs = 400.0;

    traits.memoryWeight = std::max (traits.memoryWeight, 0.6);
    traits.memoryReleaseScale = std::max (traits.memoryReleaseScale, 5.0);
    return traits;
}

double ratioSlope (double ratio) noexcept
{
    if (! (ratio > minRatio))
        return 0.0;
    if (ratio >= infiniteRatio)
        return -1.0;
    return 1.0 / ratio - 1.0;
}

StaticCurve::StaticCurve (const CurveSettings& s) noexcept
    : StaticCurve (s.thresholdDb, s.kneeDb, ratioSlope (s.ratio), s.rangeDb, s.style)
{
}

StaticCurve::StaticCurve (double thresholdDb, double kneeDb, double slopeAboveKnee, double rangeDb, Style style) noexcept
{
    const auto& traits = traitsOf (style);

    threshold = thresholdDb;
    vocal = traits.automaticCurve;
    range = std::clamp (rangeDb, 0.0, maxRangeDb);

    if (vocal)
    {
        knee = vocalKneeDb;
        slope = vocalStartSlope;
    }
    else
    {
        knee = std::max (0.0, kneeDb) + traits.extraKneeDb;
        slope = std::clamp (slopeAboveKnee, -1.0, 0.0);
    }

    x0 = threshold - 0.5 * knee;
    x1 = threshold + 0.5 * knee;
    a = knee > 0.0 ? slope / (2.0 * knee) : 0.0;
}

double StaticCurve::unclampedGain (double x) const noexcept
{
    if (x <= x0)
        return 0.0;

    if (x < x1)
        return a * (x - x0) * (x - x0);

    if (! vocal)
        return slope * (x - threshold);

    // Vocal: the slope keeps growing above the knee, up to 20:1.
    const auto atKneeEnd = 0.5 * slope * knee;
    const auto e = x - x1;
    const auto eCap = (vocalStartSlope - vocalMaxSlope) / vocalSlopeGrowth;

    if (e <= eCap)
        return atKneeEnd + vocalStartSlope * e - 0.5 * vocalSlopeGrowth * e * e;

    const auto atCap = atKneeEnd + vocalStartSlope * eCap - 0.5 * vocalSlopeGrowth * eCap * eCap;
    return atCap + vocalMaxSlope * (e - eCap);
}

double StaticCurve::unclampedSlope (double x) const noexcept
{
    if (x <= x0)
        return 0.0;

    if (x < x1)
        return 2.0 * a * (x - x0);

    if (! vocal)
        return slope;

    const auto e = x - x1;
    return std::max (vocalMaxSlope, vocalStartSlope - vocalSlopeGrowth * e);
}

double StaticCurve::gainDb (double levelDb) const noexcept
{
    return std::max (unclampedGain (levelDb), -range);
}

double StaticCurve::slopeAt (double levelDb) const noexcept
{
    return unclampedGain (levelDb) < -range ? 0.0 : unclampedSlope (levelDb);
}

void StaticCurve::feedbackGain (double u, double beta, double& gain, double& derivative) const noexcept
{
    if (beta <= 0.0 || vocal)
    {
        gain = gainDb (u);
        derivative = slopeAt (u);
        return;
    }

    beta = std::min (beta, 0.95);

    // Detected level u = x + β·G(x) is monotonic in x, so it can be inverted region by region.
    // Range first: with G = −range, x = u + β·range.
    if (unclampedGain (u + beta * range) <= -range)
    {
        gain = -range;
        derivative = 0.0;
        return;
    }

    double x = u;

    if (u > x0)
    {
        if (knee > 0.0 && u < x1 + beta * slope * 0.5 * knee)
        {
            // Inside the knee: u − x0 = d + β·a·d², with d = x − x0 ≥ 0.
            const auto w = u - x0;
            const auto root = std::sqrt (std::max (0.0, 1.0 + 4.0 * beta * a * w));
            x = x0 + 2.0 * w / (1.0 + root);
        }
        else
        {
            x = threshold + (u - threshold) / (1.0 + beta * slope);
        }
    }

    gain = std::max (unclampedGain (x), -range);
    const auto g = unclampedSlope (x);
    derivative = g / (1.0 + beta * g);
}

double autoGainDb (const CurveSettings& settings) noexcept
{
    const StaticCurve curve (settings);
    return std::max (0.0, -curve.gainDb (0.5 * settings.thresholdDb));
}

} // namespace pm::dsp::comp
