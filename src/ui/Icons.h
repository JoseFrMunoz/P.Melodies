// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pm::ui::icons
{

/** Line icons from docs/design, drawn in a 16 × 16 box and stroked. */
enum class Icon
{
    chevronLeft,
    chevronRight,
    chevronDown,
    save,
    copy,
    undo,
    redo,
    palette,
    settings,
    power,
    compare,
    close,
    trash,
    shapeBell, // EQ band shapes, in the order of dsp::eq::FilterType
    shapeLowShelf,
    shapeHighShelf,
    shapeLowCut,
    shapeHighCut,
    shapeNotch,
    shapeBandPass,
    shapeTilt,
    headphones // solo of an EQ band
};

/** Icon of an EQ band shape (FilterType index 0-7). */
inline Icon eqShape (int typeIndex) { return static_cast<Icon> (static_cast<int> (Icon::shapeBell) + typeIndex); }

/** Outline of an icon in its 16 × 16 design box. */
const juce::Path& outline (Icon icon);

/** Small filled details drawn on top of the outline (the dots of the palette). */
const juce::Path& fill (Icon icon);

/** Draws an icon centred in `area`, `size` pixels wide. */
void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, float size, juce::Colour colour,
           float strokeWidth = 1.6f);

/** The six-dot drag handle (10 × 16 design box). */
void drawDragHandle (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour);

/** The P.Melodies logo mark (22 × 22 design box). */
void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ring, juce::Colour letter,
               const juce::Font& letterFont);

} // namespace pm::ui::icons
