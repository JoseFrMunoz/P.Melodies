// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/HoldToggleButton.h"

namespace pm::ui
{

HoldToggleButton::HoldToggleButton (const juce::String& buttonText) : PillButton (buttonText)
{
    setClickingTogglesState (false);
}

void HoldToggleButton::setOn (bool shouldBeOn)
{
    if (! pressing)
        setToggleState (shouldBeOn, juce::dontSendNotification);
}

void HoldToggleButton::setState (bool on)
{
    const auto changed = on != getToggleState();
    setToggleState (on, juce::dontSendNotification);
    if (changed && onChange)
        onChange (on);
}

void HoldToggleButton::press (double timeSeconds)
{
    stateBeforePress = getToggleState();
    pressTime = timeSeconds;
    pressing = true;
    setState (true);
}

void HoldToggleButton::release (double timeSeconds)
{
    if (! pressing)
        return;
    pressing = false;
    const auto held = timeSeconds - pressTime >= holdSeconds;
    setState (held ? stateBeforePress : ! stateBeforePress);
}

void HoldToggleButton::mouseDown (const juce::MouseEvent& e)
{
    PillButton::mouseDown (e);
    if (isEnabled() && e.mods.isLeftButtonDown())
        press (juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void HoldToggleButton::mouseUp (const juce::MouseEvent& e)
{
    PillButton::mouseUp (e);
    release (juce::Time::getMillisecondCounterHiRes() * 0.001);
}

} // namespace pm::ui
