// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/theme/Theme.h"

#include "BinaryData.h"

namespace pm::ui
{

namespace
{
/** Embedded typefaces, loaded once per process and shared by every plugin instance. */
struct Typefaces
{
    juce::Typeface::Ptr interRegular, interMedium, interSemiBold;
    juce::Typeface::Ptr monoRegular, monoMedium;
    juce::Typeface::Ptr displayMedium, displaySemiBold;

    static const Typefaces& get()
    {
        static const Typefaces instance;
        return instance;
    }

private:
    Typefaces()
    {
        const auto load = [] (const char* data, int size)
        { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };

        interRegular = load (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
        interMedium = load (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
        interSemiBold = load (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
        monoRegular = load (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
        monoMedium = load (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize);
        displayMedium = load (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize);
        displaySemiBold = load (BinaryData::SpaceGroteskSemiBold_ttf, BinaryData::SpaceGroteskSemiBold_ttfSize);
    }
};

juce::Font makeFont (const juce::Typeface::Ptr& typeface, float size, float tracking = 0.0f)
{
    return juce::Font (juce::FontOptions().withTypeface (typeface).withPointHeight (size).withKerningFactor (tracking));
}

juce::Colour hex (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }
} // namespace

juce::Font Theme::ui (float size, Weight weight) const
{
    const auto& t = Typefaces::get();
    switch (weight)
    {
        case Weight::regular:  return makeFont (t.interRegular, size);
        case Weight::medium:   return makeFont (t.interMedium, size);
        case Weight::semiBold: return makeFont (t.interSemiBold, size);
    }
    return makeFont (t.interRegular, size);
}

juce::Font Theme::mono (float size, Weight weight) const
{
    const auto& t = Typefaces::get();
    return makeFont (weight == Weight::regular ? t.monoRegular : t.monoMedium, size);
}

juce::Font Theme::display (float size, Weight weight) const
{
    const auto& t = Typefaces::get();
    return makeFont (weight == Weight::semiBold ? t.displaySemiBold : t.displayMedium, size);
}

juce::Font Theme::label (float size) const
{
    return makeFont (Typefaces::get().interMedium, size, metrics.labelTracking);
}

const Theme& Theme::dark()
{
    static const Theme theme = []
    {
        Theme t;
        t.name = juce::String (juce::CharPointer_UTF8 ("Oscuro"));

        auto& c = t.colours;
        c.window = hex (0x0B0D11);
        c.background = hex (0x0F1115);
        c.panel = hex (0x13161C);
        c.panelRaised = hex (0x161A21);
        c.elevated = hex (0x1D222B);
        c.elevatedHigh = hex (0x232833);
        c.borderSubtle = hex (0x222831);
        c.border = hex (0x2A303B);
        c.borderStrong = hex (0x323946);
        c.borderHover = hex (0x3A414E);
        c.text = hex (0xE7EAF0);
        c.textBright = hex (0xFFFFFF);
        c.textSecondary = hex (0xC9CED8);
        c.textMuted = hex (0x8A93A3);
        c.textDim = hex (0x5B6473);
        c.accent = hex (0x5EE6C4);
        c.accentHover = hex (0x9CF0DA);
        c.onAccent = hex (0x0B0D11);
        c.danger = hex (0xE5484D);
        c.knobCapHover = hex (0x2E3440);
        c.knobCapBorderHover = hex (0x4A5260);
        c.backdrop = juce::Colour (0x99000000);
        c.module = { hex (0x5EE6C4), hex (0xF5A524), hex (0xFF7A59), hex (0xE85D9C) };
        return t;
    }();

    return theme;
}

float textWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

} // namespace pm::ui
