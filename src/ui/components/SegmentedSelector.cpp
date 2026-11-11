// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/SegmentedSelector.h"

#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
constexpr float outerPadding = 2.0f;
constexpr float gap = 2.0f;
constexpr float segmentPaddingX = 9.0f;
constexpr int height = 25;
} // namespace

SegmentedSelector::SegmentedSelector (juce::StringArray optionTexts) : options (std::move (optionTexts))
{
    setWantsKeyboardFocus (false);
}

void SegmentedSelector::attachTo (juce::RangedAudioParameter& parameter)
{
    attachment.reset(); // the old one first, so it never sees the new state (see PillButton::attachTo)
    attachment = std::make_unique<juce::ParameterAttachment> (
        parameter, [this] (float value) { setSelectedIndex (juce::roundToInt (value), juce::dontSendNotification); }, nullptr);
    if (getTitle().isEmpty())
        setTitle (parameter.getName (64));
    attachment->sendInitialUpdate();
}

void SegmentedSelector::setSelectedIndex (int index, juce::NotificationType notification)
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

juce::Rectangle<int> SegmentedSelector::getPreferredBounds() const
{
    const auto font = themeOf (*this).ui (11.0f, Weight::medium);
    float width = outerPadding * 2.0f + gap * (float) juce::jmax (0, options.size() - 1);
    for (const auto& option : options)
        width += textWidth (font, option) + segmentPaddingX * 2.0f;
    return { (int) std::ceil (width), height };
}

juce::Rectangle<float> SegmentedSelector::getSegmentBounds (int index) const
{
    const auto font = themeOf (*this).ui (11.0f, Weight::medium);
    const auto inner = getLocalBounds().toFloat().reduced (outerPadding + 1.0f);

    // Segments keep their text width; extra space (if any) is shared equally.
    float natural = gap * (float) juce::jmax (0, options.size() - 1);
    for (const auto& option : options)
        natural += textWidth (font, option) + segmentPaddingX * 2.0f;
    const auto extra = juce::jmax (0.0f, inner.getWidth() - natural) / (float) juce::jmax (1, options.size());

    auto x = inner.getX();
    for (int i = 0; i < options.size(); ++i)
    {
        const auto w = textWidth (font, options[i]) + segmentPaddingX * 2.0f + extra;
        if (i == index)
            return { x, inner.getY(), w, inner.getHeight() };
        x += w + gap;
    }
    return {};
}

int SegmentedSelector::segmentAt (juce::Point<float> position) const
{
    for (int i = 0; i < options.size(); ++i)
        if (getSegmentBounds (i).expanded (gap * 0.5f, 4.0f).contains (position))
            return i;
    return -1;
}

void SegmentedSelector::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;

    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (c.background);
    g.fillRoundedRectangle (bounds, theme.metrics.radiusControl);
    g.setColour (c.border);
    g.drawRoundedRectangle (bounds, theme.metrics.radiusControl, 1.0f);

    g.setFont (theme.ui (11.0f, Weight::medium));

    for (int i = 0; i < options.size(); ++i)
    {
        const auto segment = getSegmentBounds (i);

        if (i == selected)
        {
            g.setColour (c.border);
            g.fillRoundedRectangle (segment, theme.metrics.radiusSmall);
        }

        g.setColour (i == selected ? c.text : (i == hovered && isEnabled()) ? c.textSecondary : c.textMuted);
        g.drawText (options[i], segment, juce::Justification::centred, false);
    }
}

void SegmentedSelector::mouseMove (const juce::MouseEvent& e)
{
    const auto index = segmentAt (e.position);
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void SegmentedSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void SegmentedSelector::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;

    if (const auto index = segmentAt (e.position); index >= 0)
        setSelectedIndex (index);
}

std::unique_ptr<juce::AccessibilityHandler> SegmentedSelector::createAccessibilityHandler()
{
    struct ValueInterface final : public juce::AccessibilityTextValueInterface
    {
        explicit ValueInterface (SegmentedSelector& s) : selector (s) {}
        bool isReadOnly() const override { return false; }
        juce::String getCurrentValueAsString() const override { return selector.options[selector.selected]; }
        void setValueAsString (const juce::String& text) override
        {
            if (const auto index = selector.options.indexOf (text); index >= 0)
                selector.setSelectedIndex (index);
        }
        SegmentedSelector& selector;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::comboBox, juce::AccessibilityActions(),
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (*this) });
}

} // namespace pm::ui
