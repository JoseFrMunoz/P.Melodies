// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/layout/FooterBar.h"

#include "PluginProcessor.h"
#include "core/ValueFormat.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

juce::RangedAudioParameter& parameter (PMelodiesProcessor& p, const char* id)
{
    auto* result = p.getState().getParameter (id);
    jassert (result != nullptr);
    return *result;
}

constexpr int meterWidth = 96;
} // namespace

FooterBar::FooterBar (PMelodiesProcessor& p)
    : processor (p),
      inputKnob (parameter (p, param::id::inputGain), "Entrada", Knob::Style::compact),
      mixKnob (parameter (p, param::id::mix), "Mix global", Knob::Style::compact),
      outputKnob (parameter (p, param::id::outputGain), "Salida", Knob::Style::compact),
      inputMeter (p.getEngine().getInputMeter()),
      outputMeter (p.getEngine().getOutputMeter()),
      compareButton ("Comparar a igual volumen"),
      clipperSwitch ("Clipper"),
      qualityButton ("Calidad")
{
    const auto decibels = [] (float v) { return format::decibels (v, 1, true); };
    inputKnob.setFormatter (decibels);
    outputKnob.setFormatter (decibels);
    mixKnob.setFormatter ([] (float v) { return format::percent (v); });

    inputKnob.setTooltip (text ("Ganancia de entrada\nAjusta el nivel que entra en la cadena (\xc2\xb1" "24 dB). Doble clic: 0 dB."));
    mixKnob.setTooltip (text ("Mix global\nMezcla la se\xc3\xb1" "al procesada con la original, alineada en latencia."));
    outputKnob.setTooltip (text ("Ganancia de salida\nAjusta el nivel final (\xc2\xb1" "24 dB). Doble clic: 0 dB."));

    compareButton.setIcon (icons::Icon::compare);
    compareButton.attachTo (parameter (p, param::id::compare));
    compareButton.setTooltip (text ("Comparar a igual volumen\nEscucha la se\xc3\xb1" "al original con la misma sonoridad que la "
                                    "procesada, para comparar sin que lo m\xc3\xa1s fuerte parezca mejor."));

    clipperSwitch.attachTo (parameter (p, param::id::clipperEnabled));
    clipperSwitch.setTooltip (text ("Clipper de seguridad\nRecorte suave que impide que la salida supere el techo "
                                    "(\xe2\x88\x92" "0,3 dBFS por defecto)."));

    qualityButton.setTooltip (text ("Calidad\nOversampling m\xc3\xadnimo al reproducir y al exportar. Clic para cambiarlo."));
    qualityButton.onClick = [this]
    {
        if (onQualityClicked)
            onQualityClicked();
    };

    latencyLabel.setTooltip (text ("Latencia que se comunica a FL Studio. Es fija: no cambia al mover par\xc3\xa1metros "
                                   "ni al reordenar, y FL Studio la compensa."));
    latencyLabel.setJustificationType (juce::Justification::centredRight);
    latencyLabel.setInterceptsMouseClicks (true, false);

    for (auto* c : std::initializer_list<juce::Component*> { &inputKnob, &mixKnob, &outputKnob, &inputMeter, &outputMeter,
                                                              &compareButton, &clipperSwitch, &qualityButton, &latencyLabel })
        addAndMakeVisible (c);

    refresh (0.0);
}

FooterBar::~FooterBar() = default;

juce::String FooterBar::getLatencyText() const
{
    return "Latencia " + format::number (processor.getLatencyMilliseconds(), 1) + " ms";
}

juce::String FooterBar::getQualityText() const
{
    auto& realtime = parameter (processor, param::id::qualityRealtime);
    auto& offline = parameter (processor, param::id::qualityOffline);
    return "Calidad " + realtime.getCurrentValueAsText() + " / " + offline.getCurrentValueAsText();
}

void FooterBar::refresh (double elapsedSeconds)
{
    inputMeter.refresh (elapsedSeconds);
    outputMeter.refresh (elapsedSeconds);

    const auto& theme = themeOf (*this);
    latencyLabel.setFont (theme.mono (11.0f));
    latencyLabel.setColour (juce::Label::textColourId, theme.colours.textMuted);
    latencyLabel.setText (getLatencyText(), juce::dontSendNotification);

    if (const auto quality = getQualityText(); quality != qualityButton.getButtonText())
    {
        qualityButton.setButtonText (quality);
        resized();
    }

    repaint (inputText.getUnion (mixText).getUnion (outputText));
}

void FooterBar::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;

    g.fillAll (c.panel);
    g.setColour (c.borderSubtle);
    g.fillRect (getLocalBounds().removeFromTop (1));

    g.setColour (c.border);
    g.fillRect (separatorA);
    g.fillRect (separatorB);

    const auto labelFont = theme.label();
    const auto valueFont = theme.mono (11.0f);

    // Label and value on one line, right above the meters.
    const auto drawLabelValue = [&] (juce::Rectangle<int> area, const juce::String& label, const juce::String& value,
                                     bool alignRight)
    {
        const auto labelWidth = (int) std::ceil (textWidth (labelFont, label));
        const auto valueWidth = (int) std::ceil (textWidth (valueFont, value));
        auto x = alignRight ? area.getRight() - labelWidth - 6 - valueWidth : area.getX();
        g.setColour (c.textMuted);
        g.setFont (labelFont);
        g.drawText (label, juce::Rectangle<int> (x, area.getY(), labelWidth, area.getHeight()), juce::Justification::bottomLeft, false);
        x += labelWidth + 6;
        g.setColour (c.text);
        g.setFont (valueFont);
        g.drawText (value, juce::Rectangle<int> (x, area.getY(), valueWidth + 2, area.getHeight()), juce::Justification::bottomLeft, false);
    };

    drawLabelValue (inputText, "ENTRADA", inputKnob.getValueText(), false);
    drawLabelValue (outputText, "SALIDA", outputKnob.getValueText(), true);

    // Mix: label above value.
    g.setColour (c.textMuted);
    g.setFont (labelFont);
    g.drawText ("MIX GLOBAL", mixText.withHeight (14), juce::Justification::bottomLeft, false);
    g.setColour (c.text);
    g.setFont (valueFont);
    g.drawText (mixKnob.getValueText(), mixText.withTrimmedTop (14), juce::Justification::topLeft, false);
}

void FooterBar::resized()
{
    const auto& theme = themeOf (*this);
    auto row = getLocalBounds().withTrimmedTop (1).reduced (14, 0);
    const auto centreY = row.getCentreY();
    const auto centred = [centreY] (int x, int w, int h) { return juce::Rectangle<int> (x, centreY - h / 2, w, h); };

    // Input: knob, then label/value over two meters.
    inputKnob.setBounds (centred (row.getX(), 32, 32));
    row.removeFromLeft (32 + 8);
    const auto columnTop = centreY - 15;
    inputText = { row.getX(), columnTop, meterWidth + 20, 14 };
    inputMeter.setBounds (row.getX(), columnTop + 18, meterWidth, 2 * LevelMeterView::barHeight + LevelMeterView::barGap);
    row.removeFromLeft (meterWidth + 12);

    separatorA = centred (row.getX(), 1, 32);
    row.removeFromLeft (1 + 12);

    // Mix.
    mixKnob.setBounds (centred (row.getX(), 32, 32));
    row.removeFromLeft (32 + 8);
    mixText = { row.getX(), centreY - 14, 80, 28 };
    row.removeFromLeft (80 + 12);

    const auto compareWidth = compareButton.getPreferredBounds().getWidth();
    compareButton.setBounds (centred (row.getX(), compareWidth, theme.metrics.controlHeight));
    row.removeFromLeft (compareWidth + 12);

    const auto clipperWidth = clipperSwitch.getPreferredBounds().getWidth();
    clipperSwitch.setBounds (centred (row.getX(), clipperWidth, 20));
    row.removeFromLeft (clipperWidth + 12);

    const auto qualityWidth = qualityButton.getPreferredBounds().getWidth();
    qualityButton.setBounds (centred (row.getX(), qualityWidth, theme.metrics.controlHeight));
    row.removeFromLeft (qualityWidth + 12);

    // Right side, from the right edge: output knob, its meters, separator, latency.
    outputKnob.setBounds (centred (row.getRight() - 32, 32, 32));
    row.removeFromRight (32 + 8);
    outputText = { row.getRight() - (meterWidth + 20), columnTop, meterWidth + 20, 14 };
    outputMeter.setBounds (row.getRight() - meterWidth, columnTop + 18, meterWidth,
                           2 * LevelMeterView::barHeight + LevelMeterView::barGap);
    row.removeFromRight (meterWidth + 12);

    separatorB = centred (row.getRight() - 1, 1, 32);
    row.removeFromRight (1 + 12);

    latencyLabel.setBounds (centred (row.getX(), row.getWidth(), 20));
    latencyLabel.setBorderSize ({});
}

} // namespace pm::ui
