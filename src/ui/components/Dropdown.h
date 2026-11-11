// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"

#include <functional>
#include <memory>

namespace pm::ui
{

/** Dropdown in the ".btn" style: shows the current item and opens a themed menu on click. */
class Dropdown final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Dropdown (juce::StringArray items = {});

    void setItems (juce::StringArray newItems);
    void attachTo (juce::RangedAudioParameter& parameter);

    void setSelectedIndex (int index, juce::NotificationType notification = juce::sendNotificationSync);
    int getSelectedIndex() const noexcept { return selected; }

    /** Text shown instead of the selected item (e.g. a preset name), with an optional muted prefix. */
    void setDisplayText (const juce::String& prefix, const juce::String& text);

    std::function<void (int)> onChange;

    void resized() override { button.setBounds (getLocalBounds()); }
    void enablementChanged() override { button.setEnabled (isEnabled()); }

private:
    void showMenu();

    PillButton button { {} };
    juce::StringArray items;
    int selected = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace pm::ui
