// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"

#include <juce_core/juce_core.h>

namespace pm::ui
{

/** Interface texts of a module (Spanish). */
struct ModuleInfo
{
    juce::String shortName; // chain list
    juce::String title;     // module panel header
    int phase;              // development phase that implements its DSP
};

inline ModuleInfo moduleInfo (ModuleId id)
{
    const auto t = [] (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); };

    switch (id)
    {
        case ModuleId::eq:         return { "EQ", "EQ", 1 };
        case ModuleId::compressor: return { "Compresor", "Compresor", 2 };
        case ModuleId::saturation: return { t ("Saturaci\xc3\xb3n"), t ("Saturaci\xc3\xb3n multibanda"), 3 };
        case ModuleId::distortion: return { t ("Distorsi\xc3\xb3n"), t ("Distorsi\xc3\xb3n"), 4 };
    }
    return { "?", "?", 0 };
}

/** Text shown for controls that arrive in a later phase. */
inline juce::String comingLaterText()
{
    return juce::String (juce::CharPointer_UTF8 ("Disponible m\xc3\xa1s adelante"));
}

} // namespace pm::ui
