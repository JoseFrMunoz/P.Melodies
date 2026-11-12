// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/comp/CompressorCurve.h"

#include <array>
#include <cstdint>
#include <vector>

namespace pm::dsp::comp
{

/**
    Running maximum over the last N samples (a monotonic queue in a preallocated ring buffer).
    O(1) per sample on average, no allocation after prepare().
*/
class SlidingMaximum
{
public:
    /** Allocates room for windows of up to maxLength samples. */
    void prepare (int maxLength);
    void setLength (int length) noexcept;
    int getLength() const noexcept { return length; }
    void reset() noexcept;

    /** Adds a sample and returns the maximum of the last getLength() samples. */
    float push (float value) noexcept;

    /** Copies the window contents (both must have been prepared with the same size). */
    void copyFrom (const SlidingMaximum& other) noexcept;

private:
    std::vector<float> values;
    std::vector<std::int64_t> times;
    std::size_t head = 0, count = 0;
    std::int64_t now = 0;
    int length = 1;
};

/**
    Level detector and gain ballistics of the compressor, for one linked (stereo) signal
    (docs/dsp/compresor.md).

        detector input ─ level (peaks over 5 ms, or RMS) ─ static curve ─ attack / hold / release ─ gain

    - Attack and release are one-pole smoothers of the gain in dB, the "smooth branching" detector
      of Giannoulis, Massberg and Reiss (2012): with Clean, a step reaches 63 % of its gain change
      in the attack time, and recovers 63 % in the release time.
    - Peaks are held over 5 ms before the curve, so a steady tone above 100 Hz gives a steady gain
      instead of following each half wave.
    - The style adds its own behaviour (StyleTraits): RMS level, feedback, a slow second release
      stage that remembers long compression, an S-shaped attack (Punch), a release that speeds up
      as it ends (Pumping) or extra smoothing (Mastering).
    - Threshold, knee, ratio and range glide over 20 ms, so automating them does not click.

    No allocation after prepare(); everything else is real-time safe.
*/
class Dynamics
{
public:
    struct Settings
    {
        CurveSettings curve;
        double attackMs = 10.0;
        double releaseMs = 120.0;
        double holdMs = 0.0;
        bool autoRelease = false;
    };

    static constexpr double peakWindowMs = 5.0;
    static constexpr double glideMs = 20.0;
    static constexpr int maxChannels = 2;

    /** Allocates for sample rates up to maxSampleRate. */
    void prepare (double maxSampleRate);

    /** The rate the detector runs at (the module's oversampled rate). */
    void setSampleRate (double rate) noexcept;
    double getSampleRate() const noexcept { return sampleRate; }

    /** Clears the detector and the gain (no compression), and ends any glide. */
    void reset() noexcept;

    /** New settings: the curve glides to them, times change at once. */
    void setSettings (const Settings& settings) noexcept;

    /** One sample of the detector signal (up to two channels). Returns the gain in dB (≤ 0). */
    double process (const float* frame, int numChannels) noexcept;

    /** Level the curve sees now (dB), for the display. */
    double getLevelDb() const noexcept { return level; }

    /** Gain applied now (dB, ≤ 0). */
    double getGainDb() const noexcept { return output; }

    /** Curve in use (mid-glide). */
    const StaticCurve& getCurve() const noexcept { return curve; }

    /** Copies the detector and gain state (for the chain's reorder crossfade). */
    void copyStateFrom (const Dynamics& other) noexcept;

private:
    struct CurveValues
    {
        double threshold = 0.0, knee = 0.0, slope = 0.0, range = maxRangeDb;
    };

    void updateCoefficients() noexcept;
    void rebuildCurve() noexcept;
    double coefficient (double milliseconds) const noexcept;

    double sampleRate = 44100.0;
    double maxRate = 44100.0;
    Settings settings;
    StyleTraits traits;
    StaticCurve curve;

    CurveValues current, target, step;
    int glideRemaining = 0;

    // Coefficients.
    double attackCoeff = 0.0, releaseCoeff = 0.0, rmsCoeff = 0.0;
    double memoryAttackCoeff = 0.0, memoryReleaseCoeff = 0.0, smoothingCoeff = 0.0;
    double releaseSeconds = 0.12;
    int holdSamples = 0;

    // State.
    SlidingMaximum peakWindow;
    std::array<double, maxChannels> meanSquare {};
    double level = -200.0;
    double fast = 0.0;    // main attack / release stage
    double onset = 0.0;   // Punch: second attack stage
    double memory = 0.0;  // slow release stage
    double output = 0.0;
    double releasePeak = 0.0; // Pumping: deepest gain since the last attack began
    bool wasReleasing = true;
    int holdRemaining = 0;
};

} // namespace pm::dsp::comp
