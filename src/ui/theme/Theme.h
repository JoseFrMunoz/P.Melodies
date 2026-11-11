// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace pm::ui
{

/**
    Every colour of the interface. Components never hard-code a colour: they read these tokens,
    so a theme can be changed (or swapped live) without touching any component.
*/
struct Palette
{
    juce::Colour window;         // behind everything, graph backgrounds
    juce::Colour background;     // main content area
    juce::Colour panel;          // header, chain panel, footer, cards
    juce::Colour panelRaised;    // panels inside the content area (settings, band editors)
    juce::Colour elevated;       // buttons, selected row
    juce::Colour elevatedHigh;   // knob caps, tooltips, dragged row
    juce::Colour borderSubtle;   // separators between big areas
    juce::Colour border;         // control borders, separators
    juce::Colour borderStrong;   // knob cap outline
    juce::Colour borderHover;    // hovered tracks and outlines
    juce::Colour text;           // primary text
    juce::Colour textBright;     // pointer on hover
    juce::Colour textSecondary;  // button text
    juce::Colour textMuted;      // labels, secondary info
    juce::Colour textDim;        // drag handles, inactive icons
    juce::Colour accent;
    juce::Colour accentHover;
    juce::Colour onAccent;       // text on an accent background
    juce::Colour danger;         // clip indicator
    juce::Colour knobCapHover;
    juce::Colour knobCapBorderHover;
    juce::Colour backdrop;       // dims the window behind overlays
    juce::Colour grid;           // grid lines of graphs (EQ display)
    juce::Colour axisText;       // scale numbers of graphs
    std::array<juce::Colour, numModules> module; // EQ, compressor, saturation, distortion
    std::array<juce::Colour, 8> eqBand;          // one per EQ band, close to the EQ colour (§12)
};

/** Sizes in base (100 %) pixels, from docs/design. */
struct Metrics
{
    int windowWidth = 1000;
    int windowHeight = 640;
    int headerHeight = 48;
    int footerHeight = 64;
    int chainPanelWidth = 180;
    int settingsPanelWidth = 330;

    float radiusSmall = 4.0f;
    float radiusControl = 6.0f;
    float radiusRow = 7.0f;
    float radiusCard = 8.0f;
    float radiusPanel = 10.0f;

    int controlHeight = 28;
    int iconButtonSize = 28;
    float iconStroke = 1.6f;
    float labelTracking = 0.08f; // letter-spacing of uppercase labels, in em
};

enum class Weight
{
    regular,
    medium,
    semiBold
};

/** A complete look: name, colours, metrics and fonts. */
struct Theme
{
    juce::String name;
    Palette colours;
    Metrics metrics;

    /** Inter: interface text. */
    juce::Font ui (float size, Weight weight = Weight::regular) const;

    /** JetBrains Mono: values. */
    juce::Font mono (float size, Weight weight = Weight::medium) const;

    /** Space Grotesk: titles. */
    juce::Font display (float size, Weight weight = Weight::semiBold) const;

    /** Small uppercase label (10 px, medium, tracked). Draw it with the text upper-cased. */
    juce::Font label (float size = 10.0f) const;

    juce::Colour moduleColour (ModuleId id) const noexcept { return colours.module[(size_t) toIndex (id)]; }

    /** Colour of an EQ band's point, area and editor (band 0 to 7). */
    juce::Colour eqBandColour (int band) const noexcept
    {
        return colours.eqBand[(size_t) juce::jlimit (0, (int) colours.eqBand.size() - 1, band)];
    }

    /** "Oscuro", the default theme (docs/design/html/07-plantillas-de-aspecto.html). */
    static const Theme& dark();
};

/** Width of a single line of text in a font. */
float textWidth (const juce::Font& font, const juce::String& text);

} // namespace pm::ui
