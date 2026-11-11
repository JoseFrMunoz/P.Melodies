// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/theme/Theme.h"

namespace pm::ui
{

/**
    Draws the JUCE stock widgets (tooltips, menus, text editors, links) with the current theme
    and carries the theme down the component tree: components call themeOf (*this).
*/
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (const Theme& initialTheme = Theme::dark());

    const Theme& getTheme() const noexcept { return *theme; }

    /** Swaps the theme. The owner then calls sendLookAndFeelChange() on the editor. */
    void setTheme (const Theme& newTheme);

    // Tooltips: first line is the title, the rest the description.
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    // Popup menus
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 4; }

    // Text editors (typed knob values)
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

private:
    void applyColours();

    const Theme* theme;
};

/** The theme of the editor a component belongs to (falls back to the default theme). */
const Theme& themeOf (const juce::Component& component);

/** Lays out "title\nbody" tooltip text. */
juce::TextLayout layoutTooltip (const Theme& theme, const juce::String& text, float maxWidth);

} // namespace pm::ui
