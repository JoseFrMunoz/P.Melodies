// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/layout/HeaderBar.h"

#include "Version.h"
#include "ui/ModuleInfo.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

HeaderBar::HeaderBar()
{
    presetMenu.setDisplayText ("Init / ", "Por defecto");

    for (auto* c : std::initializer_list<juce::Component*> { &previousPreset, &presetMenu, &nextPreset, &savePreset,
                                                              &abSelector, &copyAToB, &undo, &redo, &themeButton })
    {
        c->setEnabled (false);
        addAndMakeVisible (c);
    }

    for (auto* b : { &previousPreset, &nextPreset, &savePreset, &copyAToB, &undo, &redo, &themeButton })
        b->setTooltip (b->getTitle() + "\n" + comingLaterText());
    presetMenu.setTooltip ("Presets\n" + comingLaterText());
    abSelector.setTooltip (juce::String (juce::CharPointer_UTF8 ("Comparaci\xc3\xb3n A/B\n")) + comingLaterText());

    settings.setTooltip ("Ajustes\nCalidad del oversampling y clipper de seguridad.");
    settings.onClick = [this]
    {
        if (onSettingsClicked)
            onSettingsClicked();
    };
    addAndMakeVisible (settings);
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;

    g.fillAll (c.panel);
    g.setColour (c.borderSubtle);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    const auto logo = juce::Rectangle<float> (14.0f, 0.0f, 22.0f, (float) getHeight());
    icons::drawLogo (g, logo, c.accent, c.text, theme.display (11.0f));

    g.setColour (c.text);
    g.setFont (theme.display (15.0f));
    g.drawText (info::productName, juce::Rectangle<int> (44, 0, 130, getHeight()), juce::Justification::centredLeft, false);

    g.setColour (c.border);
    g.fillRect (separatorA);
    g.fillRect (separatorB);
}

void HeaderBar::resized()
{
    const auto& theme = themeOf (*this);
    const auto size = theme.metrics.iconButtonSize;
    auto row = getLocalBounds().reduced (14, 0).withTrimmedBottom (1);
    const auto centred = [&row] (int width, int height) { return juce::Rectangle<int> (width, height).withCentre ({ 0, row.getCentreY() }); };

    row.removeFromLeft (166 + 10); // logo and name

    const auto place = [&] (juce::Component& c, int width, int height, int gapAfter, bool fromRight)
    {
        auto area = fromRight ? row.removeFromRight (width) : row.removeFromLeft (width);
        c.setBounds (centred (width, height).withX (area.getX()));
        if (fromRight)
            row.removeFromRight (gapAfter);
        else
            row.removeFromLeft (gapAfter);
    };

    place (previousPreset, size, size, 4, false);
    place (presetMenu, 230, theme.metrics.controlHeight, 4, false);
    place (nextPreset, size, size, 4, false);
    place (savePreset, size, size, 0, false);

    place (settings, size, size, 10, true);
    place (themeButton, size, size, 10, true);
    separatorB = centred (1, 20).withX (row.removeFromRight (1).getX());
    row.removeFromRight (10);
    place (redo, size, size, 10, true);
    place (undo, size, size, 10, true);
    separatorA = centred (1, 20).withX (row.removeFromRight (1).getX());
    row.removeFromRight (10);
    place (copyAToB, size, size, 10, true);
    const auto ab = abSelector.getPreferredBounds();
    place (abSelector, ab.getWidth() + 14, 28, 0, true);
}

} // namespace pm::ui
