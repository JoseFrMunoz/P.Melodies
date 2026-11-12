// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/comp/CompressorCurve.h"
#include "dsp/comp/CompressorMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace pm::ui
{

/**
    Scrolling level graph of the compressor (design 02): input and output levels as areas, the
    gain reduction as a line from the top and the threshold as a dashed line. 4 seconds of
    history, from 0 dB at the top to −60 dB at the bottom (§12).
*/
class CompLevelGraph final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr double historySeconds = 4.0;
    static constexpr int numPoints = (int) (historySeconds / dsp::comp::CompressorMeter::pointSeconds);
    static constexpr float floorDb = -60.0f;

    CompLevelGraph();

    /** Appends the newest points (oldest first). */
    void push (const dsp::comp::MeterPoint* points, int count);

    void setThreshold (float db);
    void setSidechainMissing (bool missing);
    bool isShowingSidechainWarning() const noexcept { return sidechainMissing; }
    void setAccentColour (juce::Colour colour);

    /** Deepest gain reduction of the last `seconds` (dB, ≤ 0): for the header and the chain row. */
    float recentReductionDb (double seconds = 0.3) const noexcept;

    /** Points received so far (capped at numPoints): for tests. */
    int getNumStoredPoints() const noexcept { return stored; }
    const dsp::comp::MeterPoint& getPoint (int age) const noexcept; // 0 = newest

    /** y of a level in dB, in this component (0 dB at the top, −60 dB at the bottom). */
    float yForDb (float db) const noexcept;

    void paint (juce::Graphics&) override;

private:
    std::array<dsp::comp::MeterPoint, numPoints> history {};
    int newest = numPoints - 1;
    int stored = 0;
    float threshold = 0.0f;
    bool sidechainMissing = false;
    juce::Colour accent;
};

/**
    Transfer curve (design 02): input level → output level, with the knee, the threshold and a dot
    at the level the detector sees now. Uses the same StaticCurve as the audio, so what is drawn is
    what is heard (Opto's wider knee and Vocal's automatic curve included).
*/
class CompCurveView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float floorDb = -60.0f;

    CompCurveView();

    void setCurve (const dsp::comp::StaticCurve& curve);
    void setLevel (float db);
    void setAccentColour (juce::Colour colour);

    const dsp::comp::StaticCurve& getCurve() const noexcept { return curve; }
    float getLevel() const noexcept { return level; }

    /** Position of an input level on the curve, in this component. */
    juce::Point<float> pointFor (float inputDb) const noexcept;

    /** The curve as drawn, from −60 dB to 0 dB of input. */
    juce::Path makeCurvePath() const;

    /** "CURVA · KNEE 12 DB" or "CURVA · AUTOMÁTICA" (Vocal). */
    juce::String caption() const;

    void paint (juce::Graphics&) override;

private:
    dsp::comp::StaticCurve curve;
    float level = -120.0f;
    juce::Colour accent;
};

} // namespace pm::ui
