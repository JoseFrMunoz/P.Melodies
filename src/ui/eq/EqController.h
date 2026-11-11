// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Parameters.h"
#include "modules/eq/EqParameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace pm
{
class PMelodiesProcessor;
}

namespace pm::ui
{

/**
    The EQ's bands as the interface sees them: reads the band parameters, writes them the way a
    host expects (every change inside a gesture, so automation and, in phase 5, undo get one step
    per action) and keeps the selected band. Shared by the display and the band editor.
    Message thread only.
*/
class EqController
{
public:
    explicit EqController (PMelodiesProcessor& processor);

    PMelodiesProcessor& getProcessor() noexcept { return processor; }

    /** The band as the parameters describe it now. */
    eq::BandParameters getBand (int index) const;
    int getNumActiveBands() const;

    /** Sample rate the audio runs at (48 kHz before the plugin is prepared). */
    double getSampleRate() const;

    juce::RangedAudioParameter& parameter (int index, param::EqField field) const;
    float getValue (int index, param::EqField field) const;

    // Selection (-1: none).
    int getSelectedBand() const noexcept { return selected; }
    void setSelectedBand (int index);
    std::function<void()> onSelectionChanged;

    /** Shape a double click creates at a frequency (§12); bottomStrip: the lowest part of the display. */
    static dsp::eq::FilterType typeForNewBand (double hz, bool bottomStrip) noexcept;

    /** Creates a band in the first free slot and selects it. Returns its index, or -1 if all 8 are in use. */
    int createBand (dsp::eq::FilterType type, double hz, double gainDb);

    void deleteBand (int index);
    void setBypassed (int index, bool bypassed);
    void setType (int index, dsp::eq::FilterType type);

    /** Sets one value as a complete gesture. */
    void setValue (int index, param::EqField field, float value);

    /** For drags: one gesture from begin to end per parameter. */
    void beginGesture (int index, param::EqField field);
    void setValueInGesture (int index, param::EqField field, float value);
    void endGesture (int index, param::EqField field);

    // Solo is interface state (not a parameter, §12).
    int getSoloBand() const;
    void setSoloBand (int index);

private:
    PMelodiesProcessor& processor;
    int selected = -1;
};

} // namespace pm::ui
