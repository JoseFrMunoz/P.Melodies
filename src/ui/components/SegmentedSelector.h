// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

namespace pm::ui
{

/** Row of mutually exclusive options ("Off · 2x · 4x"), optionally bound to a choice parameter. */
class SegmentedSelector final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SegmentedSelector (juce::StringArray options);

    /** Binds the selector to a choice parameter whose choices match the options. */
    void attachTo (juce::RangedAudioParameter& parameter);

    void setSelectedIndex (int index, juce::NotificationType notification = juce::sendNotificationSync);
    int getSelectedIndex() const noexcept { return selected; }
    int getNumOptions() const noexcept { return options.size(); }

    std::function<void (int)> onChange;

    juce::Rectangle<int> getPreferredBounds() const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void lookAndFeelChanged() override { repaint(); }
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    juce::Rectangle<float> getSegmentBounds (int index) const;
    int segmentAt (juce::Point<float> position) const;

    juce::StringArray options;
    int selected = 0;
    int hovered = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace pm::ui
