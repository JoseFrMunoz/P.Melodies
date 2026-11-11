// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/eq/EqPanel.h"

#include "PluginProcessor.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }
} // namespace

EqPanel::EqPanel (PMelodiesProcessor& processor)
    : controller (processor),
      character (param::eqCharacterNames()),
      analyser ({ "Pre", "Post", "Off" }),
      range ({ text ("\xc2\xb1" "12"), text ("\xc2\xb1" "30") }),
      graph (controller),
      bandPanel (controller)
{
    setTitle ("EQ");

    if (auto* p = processor.getState().getParameter (param::id::eqCharacter))
        character.attachTo (*p);
    character.setTooltip (text ("Car\xc3\xa1" "cter\nClean: transparente. Subtle: saturaci\xc3\xb3n suave tipo transformador. "
                                "Warm: m\xc3\xa1s marcada, tipo v\xc3\xa1lvula. Muy sutil a niveles normales."));

    analyser.setSelectedIndex ((int) processor.getEqAnalyzer(), juce::dontSendNotification);
    analyser.onChange = [this] (int index) { controller.getProcessor().setEqAnalyzer ((PMelodiesProcessor::EqAnalyzer) index); };
    analyser.setTooltip (text ("Analizador\nEspectro de la entrada (Pre) o la salida (Post) del EQ, o apagado."));

    range.setSelectedIndex (processor.isEqRangeWide() ? 1 : 0, juce::dontSendNotification);
    range.onChange = [this] (int index) { controller.getProcessor().setEqRangeWide (index == 1); };
    range.setTooltip (text ("Escala de la gr\xc3\xa1" "fica\n\xc2\xb1" "12 o \xc2\xb1" "30 dB. Pasa sola a \xc2\xb1" "30 al llevar una banda m\xc3\xa1s all\xc3\xa1 de 12 dB."));

    controller.onSelectionChanged = [this]
    {
        bandPanel.refresh();
        graph.repaint();
    };

    for (auto* c : std::initializer_list<juce::Component*> { &character, &analyser, &range, &graph, &bandPanel })
        addAndMakeVisible (c);

    refresh();
}

juce::String EqPanel::subtitle() const
{
    return juce::String (controller.getNumActiveBands()) + " de " + juce::String (eq::numBands) + text (" bandas \xc2\xb7 latencia cero");
}

void EqPanel::refresh()
{
    auto& processor = controller.getProcessor();
    analyser.setSelectedIndex ((int) processor.getEqAnalyzer(), juce::dontSendNotification);
    range.setSelectedIndex (processor.isEqRangeWide() ? 1 : 0, juce::dontSendNotification);
    graph.refresh();
    bandPanel.refresh();

    if (const auto s = subtitle(); s != shownSubtitle)
    {
        shownSubtitle = s;
        repaint (headerArea);
    }
}

void EqPanel::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    g.fillAll (c.background);

    auto header = headerArea;
    g.setColour (theme.moduleColour (ModuleId::eq));
    g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) header.getX() + 5.0f, (float) header.getCentreY() }), 3.0f);
    header.removeFromLeft (20);

    const auto titleFont = theme.display (16.0f);
    g.setColour (c.text);
    g.setFont (titleFont);
    g.drawText ("EQ", header.removeFromLeft ((int) std::ceil (textWidth (titleFont, "EQ"))), juce::Justification::centredLeft, false);
    header.removeFromLeft (10);
    g.setColour (c.textMuted);
    g.setFont (theme.ui (12.0f));
    g.drawText (subtitle(), header.withRight (characterLabel.getX() - 8), juce::Justification::centredLeft, true);

    g.setFont (theme.label());
    g.drawText (text ("CAR\xc3\x81" "CTER"), characterLabel, juce::Justification::centredRight, false);
    g.drawText ("ANALIZADOR", analyserLabel, juce::Justification::centredRight, false);
}

void EqPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 14);
    headerArea = area.removeFromTop (28);

    // Header, from the right: scale, analyser, character.
    auto row = headerArea;
    const auto place = [&row] (juce::Component& c)
    {
        const auto w = dynamic_cast<SegmentedSelector&> (c).getPreferredBounds().getWidth();
        c.setBounds (row.removeFromRight (w).withSizeKeepingCentre (w, 25));
        row.removeFromRight (8);
    };
    const auto labelFont = themeOf (*this).label();
    place (range);
    place (analyser);
    analyserLabel = row.removeFromRight ((int) std::ceil (textWidth (labelFont, "ANALIZADOR")) + 4);
    row.removeFromRight (14);
    place (character);
    characterLabel = row.removeFromRight ((int) std::ceil (textWidth (labelFont, text ("CAR\xc3\x81" "CTER"))) + 4);

    area.removeFromTop (10);
    graph.setBounds (area.removeFromTop (graphHeight));
    area.removeFromTop (10);
    bandPanel.setBounds (area.removeFromTop (bandPanelHeight));
}

} // namespace pm::ui
