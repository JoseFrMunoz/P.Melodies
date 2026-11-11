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

        // "Obsidiana y oro", the P.P. brand palette (specification §12): near-black with a cold
        // tint, warm text and an old-gold signature colour.

        auto& c = t.colours;
        c.window = hex (0x06070A);
        c.background = hex (0x0A0B0F);
        c.panel = hex (0x0F1015);
        c.panelRaised = hex (0x121318);
        c.elevated = hex (0x181A21);
        c.elevatedHigh = hex (0x1F2128);
        c.borderSubtle = hex (0x1C1E25);
        c.border = hex (0x262932);
        c.borderStrong = hex (0x2F323C);
        c.borderHover = hex (0x393C47);
        c.text = hex (0xEDE9E0);
        c.textBright = hex (0xFFFFFF);
        c.textSecondary = hex (0xD2CEC4);
        c.textMuted = hex (0x9B978E);
        c.textDim = hex (0x615E58);
        c.accent = hex (0xD4AF37);
        c.accentHover = hex (0xE8CB6A);
        c.onAccent = hex (0x06070A);
        c.danger = hex (0xE5484D);
        c.knobCapHover = hex (0x2A2D36);
        c.knobCapBorderHover = hex (0x464A55);
        c.backdrop = juce::Colour (0x99000000);
        c.grid = hex (0x15171D);
        c.axisText = hex (0x7E7A72);
        c.module = { hex (0xD4AF37), hex (0x8FA6BF), hex (0xC8743C), hex (0xAD374E) }; // gold, steel, copper, garnet

        // EQ bands (P.P., 2026-11-11): eight close tones around the EQ gold, from light amber (1)
        // to soft olive green (8). OKLCH lightness 0.80 and chroma 0.125, hue 62° to 146° in
        // 12° steps (the gold is at about 92°).
        c.eqBand = { hex (0xF7AB64), hex (0xEEB15B), hex (0xE1B856), hex (0xD3BE58),
                     hex (0xC3C55F), hex (0xB0CA6C), hex (0x9CCF7B), hex (0x87D38C) };
        return t;
    }();

    return theme;
}

float textWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

} // namespace pm::ui
