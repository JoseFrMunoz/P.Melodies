// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/comp/CompressorCurve.h"
#include "dsp/eq/EqFilterDesign.h"

namespace pm::comp
{

using dsp::comp::Style;

/** One filter of the sidechain EQ (§5, §12). */
struct SidechainFilter
{
    bool enabled = true;
    float frequency = 1000.0f;
    float gainDb = 0.0f;  // bell only
    float q = 1.0f;       // bell only
    int slopeIndex = 1;   // cuts only, index into dsp::eq::slopesDbPerOctave

    /** True when the filter changes the signal. */
    bool isActive (bool isBell) const noexcept { return enabled && (! isBell || gainDb > 0.0f || gainDb < 0.0f); }
};

// Sidechain EQ ranges (§5).
inline constexpr float sidechainHighPassMin = 20.0f;
inline constexpr float sidechainHighPassMax = 2000.0f;
inline constexpr float sidechainLowPassMin = 1000.0f;
inline constexpr float sidechainLowPassMax = 20000.0f;
inline constexpr float sidechainBellMin = 20.0f;
inline constexpr float sidechainBellMax = 20000.0f;
inline constexpr float sidechainBellMaxGainDb = 24.0f;
inline constexpr float sidechainBellMinQ = 0.1f;
inline constexpr float sidechainBellMaxQ = 10.0f;

/** Snapshot of everything the compressor needs, taken once per audio block. */
struct Parameters
{
    Style style = Style::clean;
    float thresholdDb = 0.0f;
    float ratio = 4.0f; // ≥ dsp::comp::infiniteRatio means ∞:1
    float kneeDb = 0.0f;
    float rangeDb = 60.0f;
    float attackMs = 10.0f;
    float releaseMs = 120.0f;
    bool autoRelease = false;
    float lookaheadMs = 0.0f;
    float holdMs = 0.0f;
    float makeupDb = 0.0f;
    bool autoGain = false;
    float mix = 1.0f; // 0..1
    bool externalSidechain = false;
    SidechainFilter highPass { true, 80.0f, 0.0f, 1.0f, 3 };    // 24 dB/oct
    SidechainFilter lowPass { true, 20000.0f, 0.0f, 1.0f, 1 };  // 12 dB/oct; open at 20 kHz
    SidechainFilter bell { true, 1000.0f, 0.0f, 1.0f, 0 };      // 0 dB: no effect
    bool listen = false;        // interface state (like the EQ solo): not automated, not saved
    int oversamplingIndex = 0;  // 0 = Off, 1 = 2x, 2 = 4x

    dsp::comp::CurveSettings curve() const noexcept
    {
        return { thresholdDb, ratio, kneeDb, rangeDb, style };
    }

    /** Makeup plus auto-gain, in dB. */
    double totalMakeupDb() const noexcept
    {
        return (double) makeupDb + (autoGain ? dsp::comp::autoGainDb (curve()) : 0.0);
    }

    /**
        True when these settings cannot change the sound: nothing to listen to and either the mix
        is 0 % or the curve starts above 0 dBFS with no makeup. With the default threshold of
        0 dB the compressor rests (§12: it does nothing until the threshold goes down).
    */
    bool isNeutral() const noexcept
    {
        if (listen)
            return false;
        if (! (mix > 0.0f))
            return true;
        const auto makeup = totalMakeupDb();
        return dsp::comp::StaticCurve (curve()).kneeStartDb() >= 0.0 && ! (makeup > 0.0 || makeup < 0.0);
    }
};

} // namespace pm::comp
