// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/Buttons.h"
#include "ui/components/Knob.h"
#include "ui/eq/EqController.h"

#include <array>
#include <memory>

namespace pm::ui
{

/** The eight band shapes as icons, one selected (".shape" buttons in the design). */
class ShapeSelector final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ShapeSelector();

    void setSelected (int typeIndex);
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onChange;

    static constexpr int buttonSize = 28;
    juce::Rectangle<int> getPreferredBounds() const { return { 0, 0, 8 * buttonSize + 7 * 2 + 4, buttonSize + 4 }; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    juce::Rectangle<float> buttonBounds (int index) const;
    int buttonAt (juce::Point<float> position) const;

    int selected = 0;
    int hovered = -1;
};

/**
    Editor of the selected band, under the display (design 01): number and name, shape, the
    frequency, gain, Q and slope knobs (those that do not apply are disabled), Solo, Bypass and
    delete. With no band selected it shows how to create one.
*/
class EqBandPanel final : public juce::Component
{
public:
    explicit EqBandPanel (EqController& controller);

    /** Follows the selected band (and changes made from the host). */
    void refresh();

    // For tests.
    int getBand() const noexcept { return band; }
    Knob* getKnob (param::EqField field) const;
    ShapeSelector& getShapeSelector() noexcept { return shapes; }
    PillButton& getSoloButton() noexcept { return solo; }
    PillButton& getBypassButton() noexcept { return bypass; }
    IconButton& getDeleteButton() noexcept { return remove; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuild();
    void updateEnablement();

    EqController& controller;
    int band = -1;
    int shownType = -1;

    ShapeSelector shapes;
    std::unique_ptr<Knob> frequency, gain, q, slope;
    PillButton solo, bypass;
    IconButton remove;
    juce::Rectangle<int> titleArea, separator;
};

} // namespace pm::ui
