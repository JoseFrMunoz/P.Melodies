// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/eq/EqFilterDesign.h"

#include <array>

namespace pm::eq
{

inline constexpr int numBands = 8;

/** Character modes (§4). The numeric values are stored in presets: never reorder them. */
enum class Character : int
{
    clean = 0,
    subtle = 1,
    warm = 2
};

inline constexpr int numCharacters = 3;

/** One band as the plugin parameters describe it. */
struct BandParameters
{
    bool active = false;   // the band exists (created in the display)
    bool bypassed = false; // Alt+click / Bypass button
    dsp::eq::BandSettings settings;

    /** True when the band changes the sound: it exists, is not bypassed and is not a flat line. */
    bool isAudible() const noexcept
    {
        if (! active || bypassed)
            return false;
        return ! dsp::eq::usesGain (settings.type) || settings.gainDb > 0.0 || settings.gainDb < 0.0;
    }
};

/** Snapshot of everything the EQ module needs, taken once per audio block. */
struct Parameters
{
    std::array<BandParameters, numBands> bands {};
    Character character = Character::clean;
    int soloBand = -1; // band heard in solo, -1 for none (interface state, not a plugin parameter)

    bool isNeutral() const noexcept
    {
        if (character != Character::clean || (soloBand >= 0 && soloBand < numBands && bands[(size_t) soloBand].active))
            return false;
        for (const auto& band : bands)
            if (band.isAudible())
                return false;
        return true;
    }
};

/** What solo plays for a band: the part of the spectrum the band acts on (§12). */
inline dsp::eq::BandSettings soloSettings (const dsp::eq::BandSettings& band) noexcept
{
    using dsp::eq::FilterType;
    dsp::eq::BandSettings solo;
    solo.frequency = band.frequency;
    solo.gainDb = 0.0;

    switch (band.type)
    {
        case FilterType::lowShelf: // the lows the shelf moves
            solo.type = FilterType::highCut;
            solo.q = 1.0;
            solo.slopeIndex = dsp::eq::defaultSlopeIndex;
            break;
        case FilterType::highShelf:
            solo.type = FilterType::lowCut;
            solo.q = 1.0;
            solo.slopeIndex = dsp::eq::defaultSlopeIndex;
            break;
        case FilterType::lowCut: // what the cut removes
            solo.type = FilterType::highCut;
            solo.q = 1.0;
            solo.slopeIndex = band.slopeIndex;
            break;
        case FilterType::highCut:
            solo.type = FilterType::lowCut;
            solo.q = 1.0;
            solo.slopeIndex = band.slopeIndex;
            break;
        case FilterType::bell:
        case FilterType::notch:
        case FilterType::bandPass:
        case FilterType::tiltShelf:
            solo.type = FilterType::bandPass;
            solo.q = band.q;
            break;
    }
    return solo;
}

} // namespace pm::eq
