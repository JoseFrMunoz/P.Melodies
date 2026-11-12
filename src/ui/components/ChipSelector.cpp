// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/ChipSelector.h"

#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

ChipSelector::ChipSelector (juce::StringArray optionTexts) : options (std::move (optionTexts))
{
    setWantsKeyboardFocus (false);
    setRepaintsOnMouseActivity (false);
}

void ChipSelector::attachTo (juce::RangedAudioParameter& parameter)
{
    attachment.reset();
    attachment = std::make_unique<juce::ParameterAttachment> (
        parameter, [this] (float value) { setSelectedIndex (juce::roundToInt (value), juce::dontSendNotification); }, nullptr);
    if (getTitle().isEmpty())
        setTitle (parameter.getName (64));
    attachment->sendInitialUpdate();
}

void ChipSelector::setSelectedIndex (int index, juce::NotificationType notification)
{
    index = juce::jlimit (0, juce::jmax (0, options.size() - 1), index);
    if (index == selected)
        return;

    selected = index;
    repaint();

    if (notification != juce::dontSendNotification)
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture ((float) index);
        if (onChange)
            onChange (index);
    }
}

void ChipSelector::setAccentColour (juce::Colour colour)
{
    accent = colour;
    repaint();
}

void ChipSelector::setTooltips (juce::StringArray perChip, juce::String general)
{
    chipTooltips = std::move (perChip);
    generalTooltip = std::move (general);
}

juce::String ChipSelector::getTooltip()
{
    if (juce::isPositiveAndBelow (hovered, chipTooltips.size()))
        return chipTooltips[hovered];
    return generalTooltip;
}

juce::Rectangle<int> ChipSelector::getPreferredBounds() const
{
    const auto font = themeOf (*this).ui (12.0f, Weight::medium);
    float width = gap * (float) juce::jmax (0, options.size() - 1);
    for (const auto& option : options)
        width += textWidth (font, option) + paddingX * 2.0f;
    return { (int) std::ceil (width), height };
}

juce::Rectangle<float> ChipSelector::getChipBounds (int index) const
{
    const auto font = themeOf (*this).ui (12.0f, Weight::medium);
    auto x = 0.0f;
    for (int i = 0; i < options.size(); ++i)
    {
        const auto w = textWidth (font, options[i]) + paddingX * 2.0f;
        if (i == index)
            return { x, 0.0f, w, (float) getHeight() };
        x += w + gap;
    }
    return {};
}

int ChipSelector::chipAt (juce::Point<float> position) const
{
    for (int i = 0; i < options.size(); ++i)
        if (getChipBounds (i).contains (position))
            return i;
    return -1;
}

void ChipSelector::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto colour = accent.isTransparent() ? c.accent : accent;
    g.setFont (theme.ui (12.0f, Weight::medium));

    for (int i = 0; i < options.size(); ++i)
    {
        const auto chip = getChipBounds (i).reduced (0.5f);
        const auto on = i == selected;
        const auto over = i == hovered && isEnabled();

        // Selected: a dark tint of the module colour with its outline (design 02).
        g.setColour (on ? c.panelRaised.overlaidWith (colour.withAlpha (0.16f)) : c.panelRaised);
        g.fillRoundedRectangle (chip, 5.0f);
        g.setColour (on ? colour : over ? c.borderHover : c.border);
        g.drawRoundedRectangle (chip, 5.0f, 1.0f);

        g.setColour (on ? colour.brighter (0.35f) : over ? c.text : c.textSecondary);
        g.drawText (options[i], chip, juce::Justification::centred, false);
    }
}

void ChipSelector::mouseMove (const juce::MouseEvent& e)
{
    if (const auto index = chipAt (e.position); index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void ChipSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ChipSelector::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;
    if (const auto index = chipAt (e.position); index >= 0)
        setSelectedIndex (index);
}

std::unique_ptr<juce::AccessibilityHandler> ChipSelector::createAccessibilityHandler()
{
    struct ValueInterface final : public juce::AccessibilityTextValueInterface
    {
        explicit ValueInterface (ChipSelector& s) : selector (s) {}
        bool isReadOnly() const override { return false; }
        juce::String getCurrentValueAsString() const override { return selector.options[selector.selected]; }
        void setValueAsString (const juce::String& text) override
        {
            if (const auto index = selector.options.indexOf (text); index >= 0)
                selector.setSelectedIndex (index);
        }
        ChipSelector& selector;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::comboBox, juce::AccessibilityActions(),
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (*this) });
}

} // namespace pm::ui
