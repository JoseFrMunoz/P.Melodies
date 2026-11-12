// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/Icons.h"

#include <array>

namespace pm::ui::icons
{

namespace
{
constexpr int numIcons = static_cast<int> (Icon::speaker) + 1;

const char* svgOutline (Icon icon)
{
    switch (icon)
    {
        case Icon::chevronLeft:  return "M10 4 6 8l4 4";
        case Icon::chevronRight: return "M6 4l4 4-4 4";
        case Icon::chevronDown:  return "M4 6l4 4 4-4";
        case Icon::save:         return "M3 3h8l2 2v8H3z M5.5 3v3h5V3 M5.5 13V9.5h5V13";
        case Icon::copy:
            return "M7 5.5h5a1.5 1.5 0 0 1 1.5 1.5v5a1.5 1.5 0 0 1-1.5 1.5H7a1.5 1.5 0 0 1-1.5-1.5V7A1.5 1.5 0 0 1 7 5.5z "
                   "M10.5 3.5V3a1 1 0 0 0-1-1h-6a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h.5";
        case Icon::undo:         return "M5 4 2 7l3 3 M2 7h8a3.5 3.5 0 0 1 0 7H8";
        case Icon::redo:         return "M11 4l3 3-3 3 M14 7H6a3.5 3.5 0 0 0 0 7h2";
        case Icon::palette:
            return "M8 1.8a6.2 6.2 0 1 0 0 12.4c1 0 1.4-.7 1.1-1.5-.4-1 .2-1.9 1.3-1.9h1.6a2.2 2.2 0 0 0 2.2-2.2A6.6 6.6 0 0 0 8 1.8z";
        case Icon::settings:
            return "M10.2 8a2.2 2.2 0 1 1-4.4 0a2.2 2.2 0 1 1 4.4 0z "
                   "M8 1.5v2 M8 12.5v2 M1.5 8h2 M12.5 8h2 M3.4 3.4l1.4 1.4 M11.2 11.2l1.4 1.4 M3.4 12.6l1.4-1.4 M11.2 4.8l1.4-1.4";
        case Icon::power:        return "M8 2v6 M4.6 4.2a5 5 0 1 0 6.8 0";
        case Icon::compare:      return "M2 8h3 M11 8h3 M5 5v6 M11 5v6 M8 3v10";
        case Icon::close:        return "M4 4l8 8 M12 4l-8 8";
        case Icon::trash:        return "M3 4.5h10 M6.5 4.5V3h3v1.5 M4.5 4.5l.7 9h5.6l.7-9";
        case Icon::shapeBell:      return "M1 12C5 12 6 4 8 4s3 8 7 8";
        case Icon::shapeLowShelf:  return "M1 5h4c3 0 3 6 6 6h4";
        case Icon::shapeHighShelf: return "M1 11h4c3 0 3-6 6-6h4";
        case Icon::shapeLowCut:    return "M2 14C4 6 6 5 9 5h6";
        case Icon::shapeHighCut:   return "M1 5h6c3 0 5 1 7 9";
        case Icon::shapeNotch:     return "M1 5h5l2 9 2-9h5";
        case Icon::shapeBandPass:  return "M1 14C5 14 6 5 8 5s3 9 7 9";
        case Icon::shapeTilt:      return "M1 11 15 5";
        case Icon::headphones:
            return "M2.5 11V8.5a5.5 5.5 0 0 1 11 0V11 M2.5 10h2.2v4H3.2a.7.7 0 0 1-.7-.7z M13.5 10h-2.2v4h1.5a.7.7 0 0 0 .7-.7z";
        case Icon::speaker:      return "M2 6v4h3l4 3V3L5 6z M11.5 5.5a3.5 3.5 0 0 1 0 5";
    }
    return "";
}

struct IconCache
{
    std::array<juce::Path, numIcons> outlines, fills;

    IconCache()
    {
        for (int i = 0; i < numIcons; ++i)
            outlines[(size_t) i] = juce::Drawable::parseSVGPath (svgOutline (static_cast<Icon> (i)));

        auto& dots = fills[(size_t) Icon::palette];
        for (const auto& [x, y] : { std::pair { 5.0f, 7.0f }, { 8.0f, 4.8f }, { 11.0f, 7.0f } })
            dots.addEllipse (x - 0.9f, y - 0.9f, 1.8f, 1.8f);
    }

    static const IconCache& get()
    {
        static const IconCache cache;
        return cache;
    }
};
} // namespace

const juce::Path& outline (Icon icon) { return IconCache::get().outlines[(size_t) icon]; }
const juce::Path& fill (Icon icon) { return IconCache::get().fills[(size_t) icon]; }

void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, float size, juce::Colour colour, float strokeWidth)
{
    const auto box = juce::Rectangle<float> (size, size).withCentre (area.getCentre());
    const auto scale = size / 16.0f;
    const auto transform = juce::AffineTransform::scale (scale).translated (box.getX(), box.getY());

    g.setColour (colour);
    g.strokePath (outline (icon),
                  juce::PathStrokeType (strokeWidth * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  transform);

    if (! fill (icon).isEmpty())
        g.fillPath (fill (icon), transform);
}

void drawDragHandle (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto box = juce::Rectangle<float> (10.0f, 16.0f).withCentre (area.getCentre());
    g.setColour (colour);
    for (const auto y : { 4.0f, 8.0f, 12.0f })
        for (const auto x : { 3.0f, 7.0f })
            g.fillEllipse (box.getX() + x - 1.2f, box.getY() + y - 1.2f, 2.4f, 2.4f);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ring, juce::Colour letter,
               const juce::Font& letterFont)
{
    const auto box = juce::Rectangle<float> (22.0f, 22.0f).withCentre (area.getCentre());
    const auto centre = box.getCentre();

    // Ring with a gap (stroke-dasharray 44 16 on a 9.5 radius circle, starting at 3 o'clock).
    constexpr float radius = 9.5f;
    constexpr float circumference = juce::MathConstants<float>::twoPi * radius;
    const auto sweep = juce::MathConstants<float>::twoPi * (44.0f / circumference);
    const auto start = juce::MathConstants<float>::halfPi; // JUCE angles start at 12 o'clock

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, start + sweep, true);
    g.setColour (ring);
    g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (letter);
    g.setFont (letterFont);
    g.drawText ("P", box.translated (0.0f, 0.5f), juce::Justification::centred, false);
}

} // namespace pm::ui::icons
