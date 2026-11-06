// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"
#include "ui/components/Knob.h"
#include "ui/components/LevelMeterView.h"
#include "ui/components/ToggleSwitch.h"

#include <functional>

namespace pm
{
class PMelodiesProcessor;
}

namespace pm::ui
{

/**
    Global section (spec §3.2): input gain with meters, global mix, compare at equal loudness,
    safety clipper, quality, latency and output gain with meters.
*/
class FooterBar final : public juce::Component
{
public:
    explicit FooterBar (PMelodiesProcessor& processor);
    ~FooterBar() override;

    std::function<void()> onQualityClicked;

    /** Pulls meters, latency and quality text. Called by the editor timer. */
    void refresh (double elapsedSeconds);

    juce::String getLatencyText() const;
    juce::String getQualityText() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PMelodiesProcessor& processor;

    Knob inputKnob, mixKnob, outputKnob;
    LevelMeterView inputMeter, outputMeter;
    PillButton compareButton;
    ToggleSwitch clipperSwitch;
    PillButton qualityButton;
    juce::Label latencyLabel;

    juce::Rectangle<int> inputText, mixText, outputText, separatorA, separatorB;
};

} // namespace pm::ui
