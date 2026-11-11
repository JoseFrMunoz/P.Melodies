// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace pm::ui
{

/** 30 × 16 switch with an optional text to its right (clicking the text toggles too). */
class ToggleSwitch final : public juce::ToggleButton
{
public:
    explicit ToggleSwitch (juce::String buttonText = {});

    /** Binds the switch to a boolean parameter. */
    void attachTo (juce::RangedAudioParameter& parameter);

    juce::Rectangle<int> getPreferredBounds() const;

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }

private:
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

} // namespace pm::ui
