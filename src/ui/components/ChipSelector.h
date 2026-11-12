// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

namespace pm::ui
{

/**
    Row of separate option chips, one of them selected in the module's colour (the compressor's
    styles in design 02). Optionally bound to a choice parameter. Each chip can have its own help
    text, shown as the tooltip while the pointer is over it.
*/
class ChipSelector final : public juce::Component, public juce::TooltipClient
{
public:
    explicit ChipSelector (juce::StringArray options);

    /** Binds the selector to a choice parameter whose choices match the options. */
    void attachTo (juce::RangedAudioParameter& parameter);

    void setSelectedIndex (int index, juce::NotificationType notification = juce::sendNotificationSync);
    int getSelectedIndex() const noexcept { return selected; }
    int getNumOptions() const noexcept { return options.size(); }

    void setAccentColour (juce::Colour colour);

    /** Help text of each chip (same order as the options); `general` when the pointer is between chips. */
    void setTooltips (juce::StringArray perChip, juce::String general);

    std::function<void (int)> onChange;

    juce::Rectangle<int> getPreferredBounds() const;

    /** Bounds of a chip, for tests. */
    juce::Rectangle<float> getChipBounds (int index) const;

    juce::String getTooltip() override;
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void lookAndFeelChanged() override { repaint(); }
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    static constexpr float gap = 4.0f;
    static constexpr float paddingX = 11.0f;
    static constexpr int height = 25;

private:
    int chipAt (juce::Point<float> position) const;

    juce::StringArray options;
    juce::StringArray chipTooltips;
    juce::String generalTooltip;
    juce::Colour accent;
    int selected = 0;
    int hovered = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace pm::ui
