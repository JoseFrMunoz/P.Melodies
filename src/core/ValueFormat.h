// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace pm::format
{

/** U+2212 MINUS SIGN, used by the interface (the host strings use the ASCII hyphen). */
inline juce::String unicodeMinus() { return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")); }

/**
    Spanish number formatting: decimal comma, no "-0,0".
    When useUnicodeMinus is true, negative numbers use the typographic minus sign.
*/
inline juce::String number (double value, int decimals, bool useUnicodeMinus = false)
{
    const auto scale = std::pow (10.0, decimals);
    auto rounded = std::round (value * scale) / scale;
    if (juce::exactlyEqual (rounded, 0.0))
        rounded = 0.0; // drops the sign of -0

    auto text = juce::String (std::abs (rounded), decimals).replaceCharacter ('.', ',');

    if (rounded < 0.0)
        text = (useUnicodeMinus ? unicodeMinus() : juce::String ("-")) + text;

    return text;
}

inline juce::String decibels (double value, int decimals = 1, bool useUnicodeMinus = false)
{
    return number (value, decimals, useUnicodeMinus) + " dB";
}

inline juce::String percent (double value)
{
    return number (value, 0) + " %";
}

/** Parses user text such as "-3,5 dB", "−3.5", "12 %". Returns the number, ignoring units. */
inline double parseNumber (const juce::String& text)
{
    auto cleaned = text.trim()
                       .replace (unicodeMinus(), "-")
                       .replaceCharacter (',', '.')
                       .retainCharacters ("0123456789.-+");
    return cleaned.getDoubleValue();
}

} // namespace pm::format
