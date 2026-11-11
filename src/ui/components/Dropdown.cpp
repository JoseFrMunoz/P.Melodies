// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/Dropdown.h"

namespace pm::ui
{

Dropdown::Dropdown (juce::StringArray initialItems) : items (std::move (initialItems))
{
    button.setShowsChevron (true);
    button.onClick = [this] { showMenu(); };
    addAndMakeVisible (button);
    if (! items.isEmpty())
        setSelectedIndex (0, juce::dontSendNotification);
}

void Dropdown::setItems (juce::StringArray newItems)
{
    items = std::move (newItems);
    setSelectedIndex (juce::jlimit (0, juce::jmax (0, items.size() - 1), selected), juce::dontSendNotification);
}

void Dropdown::attachTo (juce::RangedAudioParameter& parameter)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&parameter))
        setItems (choice->choices);

    attachment.reset(); // the old one first, so it never sees the new state (see PillButton::attachTo)
    attachment = std::make_unique<juce::ParameterAttachment> (
        parameter, [this] (float v) { setSelectedIndex (juce::roundToInt (v), juce::dontSendNotification); }, nullptr);
    button.setTitle (parameter.getName (64));
    attachment->sendInitialUpdate();
}

void Dropdown::setSelectedIndex (int index, juce::NotificationType notification)
{
    if (items.isEmpty())
        return;

    index = juce::jlimit (0, items.size() - 1, index);
    const auto changed = index != selected;
    selected = index;
    button.setPrefix ({});
    button.setButtonText (items[index]);

    if (changed && notification != juce::dontSendNotification)
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture ((float) index);
        if (onChange)
            onChange (index);
    }
}

void Dropdown::setDisplayText (const juce::String& prefix, const juce::String& text)
{
    button.setPrefix (prefix);
    button.setButtonText (text);
}

void Dropdown::showMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < items.size(); ++i)
        menu.addItem (i + 1, items[i], true, i == selected);

    juce::Component::SafePointer<Dropdown> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button).withMinimumWidth (getWidth()),
                        [safeThis] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->setSelectedIndex (result - 1);
                        });
}

} // namespace pm::ui
