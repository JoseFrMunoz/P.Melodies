// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "dsp/comp/CompressorDynamics.h"

#include <algorithm>
#include <cmath>

namespace pm::dsp::comp
{

//==============================================================================
void SlidingMaximum::prepare (int maxLength)
{
    const auto capacity = (std::size_t) std::max (1, maxLength) + 1;
    values.assign (capacity, 0.0f);
    times.assign (capacity, 0);
    length = std::clamp (length, 1, (int) capacity - 1);
    reset();
}

void SlidingMaximum::setLength (int newLength) noexcept
{
    // Older samples leave the queue on the next push(), so no reset is needed.
    length = std::clamp (newLength, 1, std::max (1, (int) values.size() - 1));
}

void SlidingMaximum::reset() noexcept
{
    head = 0;
    count = 0;
    now = 0;
}

float SlidingMaximum::push (float value) noexcept
{
    const auto capacity = values.size();
    if (capacity < 2)
        return value;

    // Samples smaller than the new one can never be the maximum again.
    while (count > 0 && values[(head + count - 1) % capacity] <= value)
        --count;

    const auto slot = (head + count) % capacity;
    values[slot] = value;
    times[slot] = now;
    ++count;

    while (times[head] <= now - length)
    {
        head = (head + 1) % capacity;
        --count;
    }

    ++now;
    return values[head];
}

void SlidingMaximum::copyFrom (const SlidingMaximum& other) noexcept
{
    if (other.values.size() != values.size())
    {
        reset();
        return;
    }

    std::copy (other.values.begin(), other.values.end(), values.begin());
    std::copy (other.times.begin(), other.times.end(), times.begin());
    head = other.head;
    count = other.count;
    now = other.now;
    length = other.length;
}

//==============================================================================
void Dynamics::prepare (double maxSampleRate)
{
    maxRate = std::max (1.0, maxSampleRate);
    peakWindow.prepare ((int) std::ceil (peakWindowMs * 0.001 * maxRate) + 1);
    setSampleRate (std::min (sampleRate, maxRate));
    reset();
}

void Dynamics::setSampleRate (double rate) noexcept
{
    sampleRate = std::clamp (rate, 1.0, maxRate);
    updateCoefficients();
}

double Dynamics::coefficient (double milliseconds) const noexcept
{
    const auto samples = milliseconds * 0.001 * sampleRate;
    return samples > 0.0 ? std::exp (-1.0 / samples) : 0.0;
}

void Dynamics::updateCoefficients() noexcept
{
    traits = settings.autoRelease ? withAutoRelease (traitsOf (settings.curve.style)) : traitsOf (settings.curve.style);

    const auto attackMs = std::clamp (settings.attackMs, minAttackMs, maxAttackMs);
    const auto releaseMs = std::clamp (settings.releaseMs, minReleaseMs, maxReleaseMs) * traits.releaseScale;

    // Punch: two attack stages in series, each 2.15 times faster, so the gain still reaches
    // 63 % in the attack time but starts more gently (the transient's onset gets through).
    attackCoeff = coefficient (traits.slowOnsetAttack ? attackMs / 2.15 : attackMs);
    releaseCoeff = coefficient (releaseMs);
    releaseSeconds = releaseMs * 0.001;
    rmsCoeff = coefficient (traits.rmsMs);
    memoryAttackCoeff = coefficient (traits.memoryAttackMs);
    memoryReleaseCoeff = coefficient (releaseMs * traits.memoryReleaseScale);
    smoothingCoeff = coefficient (traits.smoothingMs);
    holdSamples = (int) std::lround (std::clamp (settings.holdMs, 0.0, maxHoldMs) * 0.001 * sampleRate);
    peakWindow.setLength ((int) std::lround (peakWindowMs * 0.001 * sampleRate));
}

void Dynamics::rebuildCurve() noexcept
{
    curve = StaticCurve (current.threshold, current.knee, current.slope, current.range, settings.curve.style);
}

void Dynamics::reset() noexcept
{
    current = target;
    step = {};
    glideRemaining = 0;
    rebuildCurve();

    peakWindow.reset();
    meanSquare.fill (0.0);
    level = -200.0;
    fast = onset = memory = output = 0.0;
    releasePeak = 0.0;
    wasReleasing = true;
    holdRemaining = 0;
}

void Dynamics::setSettings (const Settings& newSettings) noexcept
{
    const auto styleChanged = newSettings.curve.style != settings.curve.style;
    settings = newSettings;

    const CurveValues next { std::clamp (settings.curve.thresholdDb, minThresholdDb, maxThresholdDb),
                             std::clamp (settings.curve.kneeDb, 0.0, maxKneeDb), ratioSlope (settings.curve.ratio),
                             std::clamp (settings.curve.rangeDb, 0.0, maxRangeDb) };

    const auto different = [] (double x, double y) { return x < y || x > y; };
    const auto differs = [different] (const CurveValues& x, const CurveValues& y)
    {
        return different (x.threshold, y.threshold) || different (x.knee, y.knee) || different (x.slope, y.slope)
               || different (x.range, y.range);
    };

    if (differs (next, target))
    {
        target = next;
        const auto samples = std::max (1, (int) std::lround (glideMs * 0.001 * sampleRate));
        const auto inverse = 1.0 / samples;
        step = { (target.threshold - current.threshold) * inverse, (target.knee - current.knee) * inverse,
                 (target.slope - current.slope) * inverse, (target.range - current.range) * inverse };
        glideRemaining = samples;
    }

    updateCoefficients();

    if (styleChanged)
        rebuildCurve();
}

double Dynamics::process (const float* frame, int numChannels) noexcept
{
    // 1. Level, linked: the loudest channel decides, so the stereo image never moves.
    const auto channels = std::clamp (numChannels, 0, maxChannels);
    auto peak = 0.0f;
    auto squares = 0.0;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto x = (double) frame[ch];
        peak = std::max (peak, std::abs (frame[ch]));
        auto& ms = meanSquare[(size_t) ch];
        ms = x * x + rmsCoeff * (ms - x * x);
        squares = std::max (squares, ms);
    }

    const auto held = (double) peakWindow.push (peak);

    // RMS is read 3 dB higher, so a sine gives the same level with both detectors.
    level = traits.rmsDetector ? 10.0 * std::log10 (squares + 1.0e-30) + 3.0102999566398120
                               : 20.0 * std::log10 (held + 1.0e-30);
    level = std::max (level, -200.0);

    // 2. Settings glide.
    if (glideRemaining > 0)
    {
        if (--glideRemaining == 0)
        {
            current = target;
        }
        else
        {
            current.threshold += step.threshold;
            current.knee += step.knee;
            current.slope += step.slope;
            current.range += step.range;
        }
        rebuildCurve();
    }

    // 3. Main stage: attack, hold and release of the gain, in dB.
    const auto releaseCoefficientNow = [this]
    {
        if (! traits.pumpingRelease || releasePeak > -1.0e-9)
            return releaseCoeff;

        // Pumping: the release starts slowly and speeds up as the gain comes back.
        const auto depth = std::clamp (fast / releasePeak, 0.0, 1.0);
        const auto seconds = releaseSeconds * (0.3 + 1.4 * depth);
        return std::exp (-1.0 / (seconds * sampleRate));
    };

    double targetGain = 0.0;
    bool attack = false;
    const auto beta = traits.feedback;

    if (beta > 0.0 && ! curve.isAutomatic())
    {
        // Feedback: the detector sees level + β·gain. Solved without a one-sample delay (one
        // Newton step around the current state), which keeps the loop stable at any attack.
        double c = 0.0, slope = 0.0;
        curve.feedbackGain (level + beta * fast, beta, c, slope);
        targetGain = c;
        attack = c < fast;

        if (attack || holdRemaining <= 0)
        {
            const auto alpha = attack ? attackCoeff : releaseCoefficientNow();
            fast = (alpha * fast + (1.0 - alpha) * (c - slope * beta * fast)) / (1.0 - (1.0 - alpha) * beta * slope);
        }
        else
        {
            --holdRemaining;
        }
    }
    else
    {
        targetGain = curve.gainDb (level);
        attack = targetGain < fast;

        if (attack || holdRemaining <= 0)
        {
            const auto alpha = attack ? attackCoeff : releaseCoefficientNow();
            fast = targetGain + alpha * (fast - targetGain);
        }
        else
        {
            --holdRemaining;
        }
    }

    if (attack)
    {
        holdRemaining = holdSamples;
        if (wasReleasing)
            releasePeak = fast;
        releasePeak = std::min (releasePeak, fast);
    }
    wasReleasing = ! attack;

    auto gain = fast;

    // 4. Punch: second attack stage (S-shaped onset); the release follows the main stage.
    if (traits.slowOnsetAttack)
    {
        onset = gain < onset ? gain + attackCoeff * (onset - gain) : gain;
        gain = onset;
    }

    // 5. Slow release stage: charges only during long compression and then holds part of the
    //    reduction for longer (short hits recover fast, long compression slowly).
    if (traits.memoryWeight > 0.0)
    {
        const auto alpha = targetGain < memory ? memoryAttackCoeff : memoryReleaseCoeff;
        memory = targetGain + alpha * (memory - targetGain);
        gain = std::min (gain, traits.memoryWeight * memory);
    }

    // 6. Mastering: extra smoothing of the gain changes.
    output = gain + smoothingCoeff * (output - gain);
    return output;
}

void Dynamics::copyStateFrom (const Dynamics& other) noexcept
{
    sampleRate = other.sampleRate;
    settings = other.settings;
    traits = other.traits;
    curve = other.curve;
    current = other.current;
    target = other.target;
    step = other.step;
    glideRemaining = other.glideRemaining;

    attackCoeff = other.attackCoeff;
    releaseCoeff = other.releaseCoeff;
    rmsCoeff = other.rmsCoeff;
    memoryAttackCoeff = other.memoryAttackCoeff;
    memoryReleaseCoeff = other.memoryReleaseCoeff;
    smoothingCoeff = other.smoothingCoeff;
    releaseSeconds = other.releaseSeconds;
    holdSamples = other.holdSamples;

    peakWindow.copyFrom (other.peakWindow);
    meanSquare = other.meanSquare;
    level = other.level;
    fast = other.fast;
    onset = other.onset;
    memory = other.memory;
    output = other.output;
    releasePeak = other.releasePeak;
    wasReleasing = other.wasReleasing;
    holdRemaining = other.holdRemaining;
}

} // namespace pm::dsp::comp
