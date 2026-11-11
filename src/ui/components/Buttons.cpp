// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/Buttons.h"

#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

IconButton::IconButton (const juce::String& accessibleName, icons::Icon iconToUse, float size)
    : juce::Button (accessibleName), icon (iconToUse), iconSize (size)
{
    setTitle (accessibleName);
}

void IconButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat();
    const auto enabled = isEnabled();

    if (enabled && (active || isButtonDown || isMouseOver))
    {
        g.setColour (active || isButtonDown ? c.elevated : c.elevated.withAlpha (0.6f));
        g.fillRoundedRectangle (bounds, theme.metrics.radiusControl);
    }

    auto colour = iconColour.isTransparent() ? (active || isMouseOver ? c.textSecondary : c.textMuted) : iconColour;
    if (! enabled)
        colour = colour.withMultipliedAlpha (0.4f);

    icons::draw (g, icon, bounds, iconSize, colour, iconSize < 15.0f ? 1.7f : theme.metrics.iconStroke);
}

//==============================================================================
PillButton::PillButton (const juce::String& buttonText) : juce::Button (buttonText)
{
    setTitle (buttonText);
}

void PillButton::attachTo (juce::RangedAudioParameter& parameter)
{
    setClickingTogglesState (true);
    attachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, *this);
}

juce::Rectangle<int> PillButton::getPreferredBounds() const
{
    const auto& theme = themeOf (*this);
    const auto font = theme.ui (12.0f, Weight::medium);
    auto width = 20.0f + textWidth (font, prefix + getButtonText());
    if (icon.has_value())
        width += 14.0f + 6.0f;
    if (chevron)
        width += 14.0f + 6.0f;
    return { (int) std::ceil (width), theme.metrics.controlHeight };
}

void PillButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto on = getToggleState();
    const auto enabled = isEnabled();

    auto fill = primary ? c.accent : on ? c.accent.withAlpha (0.14f) : c.elevated;
    auto outline = primary || on ? c.accent : (isMouseOver && enabled ? c.borderHover : c.border);
    auto textColour = primary ? c.onAccent : on ? c.accent : (isMouseOver && enabled ? c.text : c.textSecondary);

    if (isButtonDown && enabled)
        fill = fill.brighter (0.08f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, theme.metrics.radiusControl);
    g.setColour (outline);
    g.drawRoundedRectangle (bounds, theme.metrics.radiusControl, 1.0f);

    auto area = getLocalBounds().toFloat().reduced (10.0f, 0.0f);
    const auto font = theme.ui (12.0f, Weight::medium);

    if (chevron)
        icons::draw (g, icons::Icon::chevronDown, area.removeFromRight (14.0f), 14.0f, textColour);

    const auto contentWidth = textWidth (font, prefix + getButtonText()) + (icon.has_value() ? 20.0f : 0.0f);
    if (! chevron)
        area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), contentWidth), area.getHeight());

    if (icon.has_value())
    {
        icons::draw (g, *icon, area.removeFromLeft (14.0f), 14.0f, textColour, 1.5f);
        area.removeFromLeft (6.0f);
    }

    g.setFont (font);
    if (prefix.isNotEmpty())
    {
        g.setColour (c.textMuted);
        const auto prefixWidth = textWidth (font, prefix);
        g.drawText (prefix, area.removeFromLeft (prefixWidth), juce::Justification::centredLeft, false);
    }

    g.setColour (textColour);
    g.drawText (getButtonText(), area, juce::Justification::centredLeft, true);
}

} // namespace pm::ui
