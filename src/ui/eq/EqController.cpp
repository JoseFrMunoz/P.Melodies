// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/eq/EqController.h"

#include "PluginProcessor.h"

namespace pm::ui
{

using dsp::eq::FilterType;
using param::EqField;

EqController::EqController (PMelodiesProcessor& p) : processor (p)
{
}

juce::RangedAudioParameter& EqController::parameter (int index, EqField field) const
{
    auto* p = processor.getState().getParameter (param::eqBandId (index, field));
    jassert (p != nullptr);
    return *p;
}

float EqController::getValue (int index, EqField field) const
{
    const auto& p = parameter (index, field);
    return p.convertFrom0to1 (p.getValue());
}

eq::BandParameters EqController::getBand (int index) const
{
    eq::BandParameters b;
    b.active = getValue (index, EqField::active) >= 0.5f;
    b.bypassed = getValue (index, EqField::bypass) >= 0.5f;
    b.settings.type = (FilterType) juce::jlimit (0, dsp::eq::numFilterTypes - 1, juce::roundToInt (getValue (index, EqField::type)));
    b.settings.frequency = getValue (index, EqField::frequency);
    b.settings.gainDb = getValue (index, EqField::gain);
    b.settings.q = getValue (index, EqField::q);
    b.settings.slopeIndex = juce::jlimit (0, (int) dsp::eq::slopesDbPerOctave.size() - 1, juce::roundToInt (getValue (index, EqField::slope)));
    return b;
}

int EqController::getNumActiveBands() const
{
    int count = 0;
    for (int b = 0; b < eq::numBands; ++b)
        count += getBand (b).active ? 1 : 0;
    return count;
}

double EqController::getSampleRate() const
{
    const auto rate = processor.getEngine().getSampleRate();
    return rate > 0.0 ? rate : 48000.0;
}

void EqController::setSelectedBand (int index)
{
    index = juce::jlimit (-1, eq::numBands - 1, index);
    if (index >= 0 && ! getBand (index).active)
        index = -1;
    if (index == selected)
        return;
    selected = index;
    if (onSelectionChanged)
        onSelectionChanged();
}

FilterType EqController::typeForNewBand (double hz, bool bottomStrip) noexcept
{
    if (hz < 30.0)
        return FilterType::lowCut;
    if (hz < 80.0)
        return FilterType::lowShelf;
    if (hz > 16000.0)
        return FilterType::highCut;
    if (hz > 10000.0)
        return FilterType::highShelf;
    return bottomStrip ? FilterType::notch : FilterType::bell;
}

void EqController::setValue (int index, EqField field, float value)
{
    auto& p = parameter (index, field);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 (value));
    p.endChangeGesture();
}

void EqController::beginGesture (int index, EqField field) { parameter (index, field).beginChangeGesture(); }

void EqController::setValueInGesture (int index, EqField field, float value)
{
    auto& p = parameter (index, field);
    p.setValueNotifyingHost (p.convertTo0to1 (value));
}

void EqController::endGesture (int index, EqField field) { parameter (index, field).endChangeGesture(); }

int EqController::createBand (FilterType type, double hz, double gainDb)
{
    int slot = -1;
    for (int b = 0; b < eq::numBands && slot < 0; ++b)
        if (! getBand (b).active)
            slot = b;
    if (slot < 0)
        return -1;

    // Settings first, "active" last, so the audio thread never sees the band with old values.
    setValue (slot, EqField::type, (float) (int) type);
    setValue (slot, EqField::frequency, (float) juce::jlimit (dsp::eq::minFrequency, dsp::eq::maxFrequency, hz));
    setValue (slot, EqField::gain, dsp::eq::usesGain (type) ? (float) juce::jlimit (-dsp::eq::maxGainDb, dsp::eq::maxGainDb, gainDb) : 0.0f);
    setValue (slot, EqField::q, 1.0f);
    setValue (slot, EqField::slope, (float) dsp::eq::defaultSlopeIndex);
    setValue (slot, EqField::bypass, 0.0f);
    setValue (slot, EqField::active, 1.0f);

    setSelectedBand (slot);
    return slot;
}

void EqController::deleteBand (int index)
{
    if (index < 0 || index >= eq::numBands)
        return;
    if (getSoloBand() == index)
        setSoloBand (-1);
    setValue (index, EqField::active, 0.0f);
    if (selected == index)
        setSelectedBand (-1);
}

void EqController::setBypassed (int index, bool bypassed) { setValue (index, EqField::bypass, bypassed ? 1.0f : 0.0f); }

void EqController::setType (int index, FilterType type) { setValue (index, EqField::type, (float) (int) type); }

int EqController::getSoloBand() const { return processor.getEqSoloBand(); }

void EqController::setSoloBand (int index) { processor.setEqSoloBand (juce::jlimit (-1, eq::numBands - 1, index)); }

} // namespace pm::ui
