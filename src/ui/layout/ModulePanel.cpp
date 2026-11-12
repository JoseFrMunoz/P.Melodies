// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/layout/ModulePanel.h"

#include "ui/ModuleInfo.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

ModulePanel::ModulePanel (PMelodiesProcessor& processor) : eqPanel (processor), compPanel (processor)
{
    addChildComponent (eqPanel);
    addChildComponent (compPanel);
    setModule (ModuleId::eq);
}

void ModulePanel::setModule (ModuleId id)
{
    module = id;
    setTitle (moduleInfo (id).title);
    eqPanel.setVisible (id == ModuleId::eq);
    compPanel.setVisible (id == ModuleId::compressor);
    repaint();
}

void ModulePanel::refresh()
{
    if (eqPanel.isVisible())
        eqPanel.refresh();

    // The compressor's meter is read even while its page is hidden: the chain row shows it.
    compPanel.updateMeter();
    if (compPanel.isVisible())
        compPanel.refresh();
}

void ModulePanel::resized()
{
    eqPanel.setBounds (getLocalBounds());
    compPanel.setBounds (getLocalBounds());
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto info = moduleInfo (module);

    g.fillAll (c.background);
    if (module == ModuleId::eq || module == ModuleId::compressor)
        return; // these pages draw themselves

    // Header row: colour, title, status (padding 14 16, as in the design).
    auto header = getLocalBounds().reduced (16, 14).removeFromTop (28);

    g.setColour (theme.moduleColour (module));
    g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) header.getX() + 5.0f, (float) header.getCentreY() }), 3.0f);
    header.removeFromLeft (20);

    const auto titleFont = theme.display (16.0f);
    const auto titleWidth = (int) std::ceil (textWidth (titleFont, info.title));
    g.setColour (c.text);
    g.setFont (titleFont);
    g.drawText (info.title, header.removeFromLeft (titleWidth), juce::Justification::centredLeft, false);
    header.removeFromLeft (10);

    g.setColour (c.textMuted);
    g.setFont (theme.ui (12.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Se implementa en la fase ")) + juce::String (info.phase), header,
                juce::Justification::centredLeft, true);
}

} // namespace pm::ui
