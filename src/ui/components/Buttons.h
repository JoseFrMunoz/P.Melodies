// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/Icons.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <optional>

namespace pm::ui
{

/** Square icon button (".ico" in the design): no background until hovered or active. */
class IconButton final : public juce::Button
{
public:
    IconButton (const juce::String& accessibleName, icons::Icon icon, float iconSize = 16.0f);

    /** Colour override (e.g. the module power buttons). Transparent = theme default. */
    void setIconColour (juce::Colour colour) { iconColour = colour; repaint(); }

    /** Draws the button as pressed/open (e.g. the settings button while the panel is open). */
    void setActive (bool shouldBeActive) { active = shouldBeActive; repaint(); }

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

private:
    icons::Icon icon;
    float iconSize;
    juce::Colour iconColour;
    bool active = false;
};

/**
    Rounded text button (".btn" in the design), with an optional leading icon and an optional
    trailing chevron (dropdown look). As a toggle it lights up with the accent colour.
*/
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& text);

    void setIcon (std::optional<icons::Icon> newIcon) { icon = newIcon; repaint(); }
    void setShowsChevron (bool shouldShow) { chevron = shouldShow; repaint(); }

    /** Muted text drawn before the main text ("Melodía / "). */
    void setPrefix (const juce::String& newPrefix) { prefix = newPrefix; repaint(); }

    /** Accent filled style (primary action). */
    void setPrimary (bool shouldBePrimary) { primary = shouldBePrimary; repaint(); }

    /** Binds a toggling pill to a boolean parameter. */
    void attachTo (juce::RangedAudioParameter& parameter);

    juce::Rectangle<int> getPreferredBounds() const;

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }

private:
    std::optional<icons::Icon> icon;
    bool chevron = false;
    bool primary = false;
    juce::String prefix;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

} // namespace pm::ui
