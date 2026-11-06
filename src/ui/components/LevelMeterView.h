// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/LevelMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace pm::ui
{

/**
    Display-side meter ballistics for one channel: the peak falls at a fixed rate, the peak-hold
    marker stays for a while, then falls too.
*/
struct MeterBallistics
{
    static constexpr float floorDb = -60.0f;
    static constexpr float fallDbPerSecond = 24.0f;
    static constexpr double holdSeconds = 1.5;

    float peakDb = floorDb;
    float rmsDb = floorDb;
    float holdDb = floorDb;
    double holdAge = 0.0;

    /** Feeds a new reading (linear peak since the last update, linear RMS) after `elapsed` seconds. */
    void update (float peak, float rms, double elapsed) noexcept;
    void resetHold() noexcept;

    /** Position of a level on the meter, 0 (floor) to 1 (0 dBFS). */
    static float proportionOf (float db) noexcept;
};

/**
    Horizontal two-channel meter (footer): RMS bar, peak extension, peak-hold tick and a clip
    indicator at the end. Clicking it resets the clip indicator and the hold.
*/
class LevelMeterView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LevelMeterView (dsp::MeterReadout& source);

    /** Pulls the latest values from the audio thread. Call from the editor timer. */
    void refresh (double elapsedSeconds);

    bool isClipShown() const noexcept { return clipShown; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int barHeight = 4;
    static constexpr int barGap = 4;

private:
    dsp::MeterReadout& source;
    std::array<MeterBallistics, dsp::MeterReadout::maxChannels> channels;
    int numChannels = 2;
    bool clipShown = false;
};

} // namespace pm::ui
