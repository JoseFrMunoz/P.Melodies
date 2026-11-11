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

/** "40 Hz", "400 Hz", "1,25 kHz", "12,5 kHz". */
inline juce::String frequency (double hz)
{
    if (hz < 1000.0)
        return number (hz, hz < 100.0 ? 1 : 0) + " Hz";
    return number (hz / 1000.0, hz < 10000.0 ? 2 : 1) + " kHz";
}

/**
    Musical note of a frequency with its deviation in cents, named as in FL Studio's piano roll
    (middle C = C5, A5 = 440 Hz): 400 Hz → "G5 +35", 130 Hz → "C4 −11".
*/
inline juce::String note (double hz)
{
    if (! (hz > 0.0))
        return {};
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto midi = 69.0 + 12.0 * std::log2 (hz / 440.0); // MIDI note number: 60 is middle C
    const auto nearest = (int) std::lround (midi);
    const auto cents = (int) std::lround ((midi - nearest) * 100.0);
    const auto octave = (nearest >= 0 ? nearest : nearest - 11) / 12; // FL Studio: MIDI 60 → C5
    const auto name = names[((nearest % 12) + 12) % 12];
    const auto amount = juce::String (std::abs (cents)).paddedLeft ('0', 2);
    return juce::String (name) + juce::String (octave) + " " + (cents < 0 ? unicodeMinus() : juce::String ("+")) + amount;
}

/**
    Frequency of a note typed by the user, with FL Studio's octave numbers: "A5" (440 Hz), "c#3",
    "Bb4", "C5+13", "A5 −20". Returns 0 when the text is not a note.
*/
inline double parseNote (const juce::String& text)
{
    const auto t = text.trim().replace (unicodeMinus(), "-").removeCharacters (" ");
    if (t.isEmpty())
        return 0.0;

    static constexpr int semitones[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);
    if (letter < 'A' || letter > 'G')
        return 0.0;
    int semitone = semitones[letter - 'A'];
    int i = 1;
    const auto isDigit = [&t] (int at) { return at < t.length() && juce::CharacterFunctions::isDigit (t[at]); };

    if (i < t.length() && t[i] == '#')
    {
        ++semitone;
        ++i;
    }
    else if (i < t.length() && t[i] == 'b' && (isDigit (i + 1) || (i + 1 < t.length() && t[i + 1] == '-')))
    {
        --semitone;
        ++i;
    }

    const auto octaveStart = i;
    if (i < t.length() && t[i] == '-')
        ++i;
    if (! isDigit (i))
        return 0.0;
    while (isDigit (i))
        ++i;
    const auto octave = t.substring (octaveStart, i).getIntValue();

    int cents = 0;
    if (i < t.length())
    {
        if (t[i] != '+' && t[i] != '-')
            return 0.0;
        const auto sign = t[i] == '-' ? -1 : 1;
        ++i;
        const auto centsStart = i;
        while (isDigit (i))
            ++i;
        if (i != t.length() || i == centsStart)
            return 0.0;
        cents = sign * t.substring (centsStart).getIntValue();
    }

    const auto midi = 12 * octave + semitone; // FL Studio: C5 is 60
    return 440.0 * std::pow (2.0, (midi - 69) / 12.0 + cents / 1200.0);
}

/** Parses a frequency typed by the user: "400", "400 Hz", "2k", "2,5 kHz", or a note ("A5", "C#3+13"). */
inline double parseFrequency (const juce::String& text)
{
    if (const auto hz = parseNote (text); hz > 0.0)
        return hz;
    const auto lower = text.trim().toLowerCase();
    auto cleaned = lower.replaceCharacter (',', '.').retainCharacters ("0123456789.");
    const auto value = cleaned.getDoubleValue();
    return lower.containsChar ('k') ? value * 1000.0 : value;
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
