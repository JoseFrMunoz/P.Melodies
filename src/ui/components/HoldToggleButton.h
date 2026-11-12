// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"

#include <functional>

namespace pm::ui
{

/**
    Pill button that is a toggle on a click and momentary while held (the compressor's
    "Escuchar", §12): pressing turns it on at once; releasing after a short click leaves it
    toggled, releasing after holding it goes back to how it was.
*/
class HoldToggleButton final : public PillButton
{
public:
    explicit HoldToggleButton (const juce::String& buttonText);

    /** Called whenever the state changes (pressed, toggled, released). */
    std::function<void (bool isOn)> onChange;

    /** Sets the state from outside (another thread may have changed it), without calling onChange. */
    void setOn (bool shouldBeOn);
    bool isOn() const noexcept { return getToggleState(); }

    /** Held at least this long, the release restores the previous state. */
    static constexpr double holdSeconds = 0.3;

    // The press logic, public so tests can run it with explicit times.
    void press (double timeSeconds);
    void release (double timeSeconds);

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setState (bool on);

    bool stateBeforePress = false;
    double pressTime = 0.0;
    bool pressing = false;
};

} // namespace pm::ui
