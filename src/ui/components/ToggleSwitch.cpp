// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/ToggleSwitch.h"

#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
constexpr int trackWidth = 30;
constexpr int trackHeight = 16;
constexpr int textGap = 8;
} // namespace

ToggleSwitch::ToggleSwitch (juce::String buttonText) : juce::ToggleButton (std::move (buttonText)) {}

void ToggleSwitch::attachTo (juce::RangedAudioParameter& parameter)
{
    attachment.reset(); // the old one first, so it never sees the new state (see PillButton::attachTo)
    attachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, *this);
    if (getTitle().isEmpty())
        setTitle (parameter.getName (64));
}

juce::Rectangle<int> ToggleSwitch::getPreferredBounds() const
{
    const auto buttonText = getButtonText();
    if (buttonText.isEmpty())
        return { trackWidth, trackHeight };

    const auto& theme = themeOf (*this);
    return { trackWidth + textGap + (int) std::ceil (textWidth (theme.ui (12.0f, Weight::medium), buttonText)), 20 };
}

void ToggleSwitch::paintButton (juce::Graphics& g, bool isMouseOver, bool)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto on = getToggleState();

    const auto track = juce::Rectangle<float> (0.0f, (float) (getHeight() - trackHeight) * 0.5f, (float) trackWidth,
                                               (float) trackHeight);
    g.setColour (on ? (isMouseOver ? c.accentHover : c.accent) : (isMouseOver ? c.borderHover : c.border));
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    const auto thumb = juce::Rectangle<float> (12.0f, 12.0f)
                           .withPosition (track.getX() + (on ? 16.0f : 2.0f), track.getY() + 2.0f);
    g.setColour (on ? c.background : c.textMuted);
    g.fillEllipse (thumb);

    if (getButtonText().isNotEmpty())
    {
        g.setColour (c.textSecondary);
        g.setFont (theme.ui (12.0f, Weight::medium));
        g.drawText (getButtonText(), getLocalBounds().withTrimmedLeft (trackWidth + textGap),
                    juce::Justification::centredLeft, false);
    }
}

} // namespace pm::ui
