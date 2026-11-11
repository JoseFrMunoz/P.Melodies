// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/components/LevelMeterView.h"

#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

void MeterBallistics::update (float peak, float rms, double elapsed) noexcept
{
    const auto fall = (float) (fallDbPerSecond * elapsed);
    const auto newPeakDb = juce::jmax (floorDb, juce::Decibels::gainToDecibels (peak, floorDb));

    peakDb = juce::jmax (newPeakDb, peakDb - fall);
    rmsDb = juce::jmax (floorDb, juce::Decibels::gainToDecibels (rms, floorDb));

    if (newPeakDb >= holdDb)
    {
        holdDb = newPeakDb;
        holdAge = 0.0;
    }
    else
    {
        holdAge += elapsed;
        if (holdAge > holdSeconds)
            holdDb = juce::jmax (floorDb, juce::jmax (peakDb, holdDb - fall));
    }
}

void MeterBallistics::resetHold() noexcept
{
    holdDb = peakDb;
    holdAge = 0.0;
}

float MeterBallistics::proportionOf (float db) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);
}

//==============================================================================
LevelMeterView::LevelMeterView (dsp::MeterReadout& readout) : source (readout)
{
    setTooltip (juce::String (juce::CharPointer_UTF8 ("Pico y RMS. Clic: borra el indicador de clip y la retenci\xc3\xb3n.")));
}

void LevelMeterView::refresh (double elapsedSeconds)
{
    numChannels = juce::jlimit (1, dsp::MeterReadout::maxChannels, source.numChannels.load());

    for (int ch = 0; ch < dsp::MeterReadout::maxChannels; ++ch)
    {
        const auto peak = source.takePeak (ch);
        const auto rms = ch < numChannels ? source.getRms (ch) : 0.0f;
        channels[(size_t) ch].update (peak, rms, elapsedSeconds);
    }

    clipShown = source.clipped.load();
    repaint();
}

void LevelMeterView::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto width = (float) getWidth();

    for (int ch = 0; ch < 2; ++ch)
    {
        // A mono signal fills both bars with the same channel.
        const auto& m = channels[(size_t) (ch < numChannels ? ch : 0)];
        const auto bar = juce::Rectangle<float> (0.0f, (float) (ch * (barHeight + barGap)), width, (float) barHeight);

        g.setColour (c.borderSubtle);
        g.fillRoundedRectangle (bar, 2.0f);

        const auto peakWidth = width * MeterBallistics::proportionOf (m.peakDb);
        const auto rmsWidth = width * MeterBallistics::proportionOf (m.rmsDb);

        g.setColour (c.accent.withAlpha (0.4f));
        g.fillRoundedRectangle (bar.withWidth (peakWidth), 2.0f);
        g.setColour (c.accent);
        g.fillRoundedRectangle (bar.withWidth (rmsWidth), 2.0f);

        if (m.holdDb > MeterBallistics::floorDb)
        {
            const auto x = juce::jlimit (0.0f, width - 2.0f, width * MeterBallistics::proportionOf (m.holdDb) - 1.0f);
            g.setColour (c.text);
            g.fillRect (bar.withX (x).withWidth (2.0f));
        }

        if (clipShown)
        {
            g.setColour (c.danger);
            g.fillRoundedRectangle (bar.withTrimmedLeft (width - 6.0f), 2.0f);
        }
    }
}

void LevelMeterView::mouseDown (const juce::MouseEvent&)
{
    source.resetClip();
    clipShown = false;
    for (auto& m : channels)
        m.resetHold();
    repaint();
}

} // namespace pm::ui
