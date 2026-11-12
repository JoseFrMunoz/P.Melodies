// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

/**
    Compressor static curve and style traits (docs/dsp/compresor.md).

    Everything here is pure maths in double precision: no allocation, safe on the audio thread.
    Levels and gains are in dB. A "gain" is the change the compressor applies (≤ 0 dB, makeup not
    included): output level = input level + gain.
*/
namespace pm::dsp::comp
{

/** Compression styles (§5). The numeric values are stored in presets: never reorder them. */
enum class Style : int
{
    clean = 0,
    classic = 1,
    opto = 2,
    vocal = 3,
    mastering = 4,
    bus = 5,
    punch = 6,
    pumping = 7
};

inline constexpr int numStyles = 8;

// Parameter ranges (§5).
inline constexpr double minThresholdDb = -60.0;
inline constexpr double maxThresholdDb = 0.0;
inline constexpr double minRatio = 1.0;
inline constexpr double maxFiniteRatio = 100.0;
inline constexpr double infiniteRatio = 1000.0; // a ratio at or above this value means ∞:1
inline constexpr double maxKneeDb = 72.0;
inline constexpr double maxRangeDb = 60.0;
inline constexpr double minAttackMs = 0.005;
inline constexpr double maxAttackMs = 250.0;
inline constexpr double minReleaseMs = 10.0;
inline constexpr double maxReleaseMs = 2500.0;
inline constexpr double maxLookaheadMs = 20.0;
inline constexpr double maxHoldMs = 500.0;
inline constexpr double minMakeupDb = -12.0;
inline constexpr double maxMakeupDb = 36.0;

/**
    What makes each style different (§12, P.P.'s answers of phase 2). All of them share the
    threshold, ratio, knee and range; they differ in how they detect the level and in their timing.
*/
struct StyleTraits
{
    bool rmsDetector = false;       // average (RMS) level instead of peaks
    double rmsMs = 10.0;            // RMS window (time constant)
    double feedback = 0.0;          // 0 = feed-forward; > 0: part of the output level is detected (β)
    double extraKneeDb = 0.0;       // added to the knee knob
    bool automaticCurve = false;    // Vocal: its own progressive ratio and knee
    double memoryWeight = 0.0;      // part of the reduction held by the slow release stage (0 = none)
    double memoryAttackMs = 300.0;  // how fast the slow stage charges while compressing
    double memoryReleaseScale = 1.0; // slow stage release = Release × this
    bool slowOnsetAttack = false;   // Punch: S-shaped attack that lets the start of a transient through
    double releaseScale = 1.0;      // effective release = Release × this
    bool pumpingRelease = false;    // release slow at first and fast at the end
    double smoothingMs = 0.0;       // Mastering: extra smoothing of the gain changes
};

const StyleTraits& traitsOf (Style style) noexcept;

/** Auto-release (§12): a two-stage release added to any style, scaled by the Release knob. */
StyleTraits withAutoRelease (StyleTraits traits) noexcept;

/** 1/ratio − 1: the slope of the gain above the knee (−1 for ∞:1, 0 for 1:1). */
double ratioSlope (double ratio) noexcept;

struct CurveSettings
{
    double thresholdDb = 0.0;
    double ratio = 4.0;
    double kneeDb = 0.0;
    double rangeDb = maxRangeDb;
    Style style = Style::clean;
};

/**
    Static curve: the gain a steady level receives, once attack and release have settled.

    Feed-forward soft knee of Giannoulis, Massberg and Reiss (2012), with quadratic interpolation
    inside the knee, then limited by Range. Opto widens the knee; Vocal replaces ratio and knee by
    its own progressive curve (the further over the threshold, the higher the ratio).
*/
class StaticCurve
{
public:
    StaticCurve() noexcept : StaticCurve (CurveSettings {}) {}
    explicit StaticCurve (const CurveSettings& settings) noexcept;

    /** From the raw values (slope = ratioSlope (ratio)); the style applies its own changes. */
    StaticCurve (double thresholdDb, double kneeDb, double slope, double rangeDb, Style style) noexcept;

    /** Gain (≤ 0 dB) for a steady input level. */
    double gainDb (double levelDb) const noexcept;

    /** d gain / d level. */
    double slopeAt (double levelDb) const noexcept;

    double outputDb (double levelDb) const noexcept { return levelDb + gainDb (levelDb); }

    /** Knee really used (knob + the style's extra; Vocal's own). */
    double kneeDb() const noexcept { return knee; }
    double thresholdDb() const noexcept { return threshold; }
    double rangeDb() const noexcept { return range; }
    bool isAutomatic() const noexcept { return vocal; }

    /** Level below which the curve does nothing: the start of the knee. */
    double kneeStartDb() const noexcept { return x0; }

    /**
        For the feedback styles: the detector sees u = level + β·gain. Gives the gain as a function
        of u, chosen so that the steady state is still exactly gainDb(level), and its derivative.
        Only for the standard curve (not Vocal); requires 0 ≤ β < 1.
    */
    void feedbackGain (double u, double beta, double& gain, double& derivative) const noexcept;

private:
    double unclampedGain (double levelDb) const noexcept;
    double unclampedSlope (double levelDb) const noexcept;

    double threshold = 0.0;
    double knee = 0.0;
    double slope = 0.0;
    double range = maxRangeDb;
    bool vocal = false;

    double x0 = 0.0; // start of the knee
    double x1 = 0.0; // end of the knee
    double a = 0.0;  // quadratic coefficient inside the knee
};

/**
    Auto-gain (§12): estimated from the settings, not measured. Levels halfway between the
    threshold and 0 dBFS keep their level: the gain is −(curve gain at threshold / 2).
*/
double autoGainDb (const CurveSettings& settings) noexcept;

// Vocal's automatic curve: 1.5:1 at the threshold, the local ratio rising with the level above
// it up to 20:1, after a 10 dB knee.
inline constexpr double vocalKneeDb = 10.0;
inline constexpr double vocalStartSlope = -1.0 / 3.0; // 1.5:1
inline constexpr double vocalSlopeGrowth = 0.04;       // per dB above the knee
inline constexpr double vocalMaxSlope = -0.95;         // 20:1

} // namespace pm::dsp::comp
