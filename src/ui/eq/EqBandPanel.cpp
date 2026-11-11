// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/eq/EqBandPanel.h"

#include "core/ValueFormat.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

using dsp::eq::FilterType;
using param::EqField;

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }
} // namespace

//==============================================================================
ShapeSelector::ShapeSelector()
{
    setTitle ("Tipo de banda");
}

juce::Rectangle<float> ShapeSelector::buttonBounds (int index) const
{
    return { 2.0f + (float) index * (buttonSize + 2), 2.0f, (float) buttonSize, (float) buttonSize };
}

int ShapeSelector::buttonAt (juce::Point<float> position) const
{
    for (int i = 0; i < dsp::eq::numFilterTypes; ++i)
        if (buttonBounds (i).contains (position))
            return i;
    return -1;
}

void ShapeSelector::setSelected (int typeIndex)
{
    if (typeIndex != selected)
    {
        selected = typeIndex;
        repaint();
    }
}

void ShapeSelector::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (c.background);
    g.fillRoundedRectangle (bounds, theme.metrics.radiusControl);
    g.setColour (c.border);
    g.drawRoundedRectangle (bounds, theme.metrics.radiusControl, 1.0f);

    for (int i = 0; i < dsp::eq::numFilterTypes; ++i)
    {
        const auto area = buttonBounds (i);
        if (i == selected || i == hovered)
        {
            g.setColour (i == selected ? c.border : c.elevated);
            g.fillRoundedRectangle (area, 5.0f);
        }
        const auto colour = i == selected ? theme.moduleColour (ModuleId::eq) : (i == hovered ? c.textSecondary : c.textMuted);
        icons::draw (g, icons::eqShape (i), area, 16.0f, colour, 1.5f);
    }
}

void ShapeSelector::mouseMove (const juce::MouseEvent& e)
{
    const auto index = buttonAt (e.position);
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
    if (index >= 0)
        setTooltip (param::eqTypeNames()[index]);
}

void ShapeSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ShapeSelector::mouseDown (const juce::MouseEvent& e)
{
    const auto index = buttonAt (e.position);
    if (index < 0 || ! isEnabled())
        return;
    setSelected (index);
    if (onChange)
        onChange (index);
}

std::unique_ptr<juce::AccessibilityHandler> ShapeSelector::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
}

//==============================================================================
EqBandPanel::EqBandPanel (EqController& c)
    : controller (c), solo ("Solo"), bypass ("Bypass"), remove ("Eliminar banda", icons::Icon::trash, 16.0f)
{
    shapes.setTooltip (text ("Tipo de banda\nCampana, shelves, cortes, notch, pasa-banda o tilt."));
    shapes.onChange = [this] (int type)
    {
        if (band >= 0)
            controller.setType (band, (FilterType) type);
        updateEnablement();
        repaint();
    };

    solo.setClickingTogglesState (true);
    solo.setTooltip (text ("Solo\nEscucha solo la zona que toca esta banda (en los cortes, lo que quitan)."));
    solo.onClick = [this] { controller.setSoloBand (solo.getToggleState() ? band : -1); };

    bypass.setTooltip (text ("Bypass de la banda\nLa deja sin efecto sin borrarla (tambi\xc3\xa9n Alt+clic en su punto)."));
    remove.setTooltip (text ("Eliminar banda\nTambi\xc3\xa9n con la tecla Supr."));
    remove.onClick = [this]
    {
        if (band >= 0)
            controller.deleteBand (band);
        refresh();
    };

    for (auto* comp : std::initializer_list<juce::Component*> { &shapes, &solo, &bypass, &remove })
        addChildComponent (comp);

    refresh();
}

Knob* EqBandPanel::getKnob (EqField field) const
{
    switch (field)
    {
        case EqField::frequency: return frequency.get();
        case EqField::gain:      return gain.get();
        case EqField::q:         return q.get();
        case EqField::slope:     return slope.get();
        case EqField::active:
        case EqField::type:
        case EqField::bypass:    break;
    }
    return nullptr;
}

void EqBandPanel::refresh()
{
    const auto selected = controller.getSelectedBand();
    if (selected != band)
    {
        band = selected;
        rebuild();
    }
    if (band < 0)
        return;

    const auto b = controller.getBand (band);
    if ((int) b.settings.type != shownType)
    {
        shownType = (int) b.settings.type;
        shapes.setSelected (shownType);
        repaint (titleArea);
    }
    updateEnablement();
    solo.setToggleState (controller.getSoloBand() == band, juce::dontSendNotification);
}

void EqBandPanel::rebuild()
{
    for (auto* knob : { frequency.get(), gain.get(), q.get(), slope.get() })
        if (knob != nullptr)
            removeChildComponent (knob);
    frequency.reset();
    gain.reset();
    q.reset();
    slope.reset();
    shownType = -1;

    const auto visible = band >= 0;
    for (auto* comp : std::initializer_list<juce::Component*> { &shapes, &solo, &bypass, &remove })
        comp->setVisible (visible);

    if (visible)
    {
        const auto accent = themeOf (*this).moduleColour (ModuleId::eq);
        frequency = std::make_unique<Knob> (controller.parameter (band, EqField::frequency), "Frecuencia");
        gain = std::make_unique<Knob> (controller.parameter (band, EqField::gain), "Ganancia");
        q = std::make_unique<Knob> (controller.parameter (band, EqField::q), "Q");
        slope = std::make_unique<Knob> (controller.parameter (band, EqField::slope), "Pendiente");

        frequency->setFormatter ([] (float v) { return format::frequency (v); });
        gain->setFormatter ([] (float v) { return format::decibels (v, 1, true); });
        q->setFormatter ([] (float v) { return format::number (v, 2); });
        slope->setFormatter ([] (float v)
                             {
                                 const auto index = juce::jlimit (0, (int) dsp::eq::slopesDbPerOctave.size() - 1, juce::roundToInt (v));
                                 return juce::String (dsp::eq::slopesDbPerOctave[(size_t) index]) + " dB/oct";
                             });

        frequency->setTooltip (text ("Frecuencia\nDe 10 Hz a 30 kHz. Doble clic: valor por defecto; Alt+clic: escribirla."));
        gain->setTooltip (text ("Ganancia\n\xc2\xb1" "30 dB. Solo en campana, shelves y tilt."));
        q->setTooltip (text ("Q\nAnchura de la banda: m\xc3\xa1s Q, m\xc3\xa1s estrecha. En los cortes, la resonancia."));
        slope->setTooltip (text ("Pendiente\nDe 6 a 96 dB/oct. Solo en los cortes de graves y agudos."));

        for (auto* knob : { frequency.get(), gain.get(), q.get(), slope.get() })
        {
            knob->setAccentColour (accent);
            addAndMakeVisible (*knob);
        }
        bypass.attachTo (controller.parameter (band, EqField::bypass));
    }

    refresh();
    resized();
    repaint();
}

void EqBandPanel::updateEnablement()
{
    if (band < 0 || gain == nullptr)
        return;
    const auto b = controller.getBand (band);
    gain->setEnabled (dsp::eq::usesGain (b.settings.type));
    q->setEnabled (dsp::eq::usesQ (b.settings.type, b.settings.slopeIndex));
    slope->setEnabled (dsp::eq::usesSlope (b.settings.type));
}

void EqBandPanel::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (c.panelRaised);
    g.fillRoundedRectangle (bounds, theme.metrics.radiusCard);
    g.setColour (c.border);
    g.drawRoundedRectangle (bounds, theme.metrics.radiusCard, 1.0f);

    if (band < 0)
    {
        g.setColour (c.textMuted);
        g.setFont (theme.ui (12.0f));
        g.drawText (text ("Selecciona una banda en la gr\xc3\xa1" "fica, o haz doble clic en ella para crear una."), getLocalBounds(),
                    juce::Justification::centred, true);
        return;
    }

    // Badge with the band number, then "Banda 3 · Campana".
    const auto accent = theme.moduleColour (ModuleId::eq);
    auto title = titleArea;
    const auto badge = title.removeFromLeft (20).withSizeKeepingCentre (20, 20).toFloat();
    g.setColour (controller.getBand (band).bypassed ? c.textDim : accent);
    g.fillEllipse (badge);
    g.setColour (c.onAccent);
    g.setFont (theme.ui (11.0f, Weight::semiBold));
    g.drawText (juce::String (band + 1), badge, juce::Justification::centred, false);
    title.removeFromLeft (8);
    g.setColour (c.text);
    g.setFont (theme.ui (13.0f, Weight::medium));
    const auto typeName = param::eqTypeNames()[juce::jmax (0, shownType)];
    g.drawText ("Banda " + juce::String (band + 1) + text (" \xc2\xb7 ") + typeName, title, juce::Justification::centredLeft, true);

    g.setColour (c.border);
    g.fillRect (separator);
}

void EqBandPanel::resized()
{
    auto area = getLocalBounds().reduced (14, 10);

    // Left column: title over the shape selector.
    const auto shapeSize = shapes.getPreferredBounds();
    auto left = area.removeFromLeft (shapeSize.getWidth());
    const auto columnHeight = 20 + 8 + shapeSize.getHeight();
    left = left.withSizeKeepingCentre (left.getWidth(), columnHeight);
    titleArea = left.removeFromTop (20);
    left.removeFromTop (8);
    shapes.setBounds (left.removeFromTop (shapeSize.getHeight()));

    area.removeFromLeft (14);
    separator = area.removeFromLeft (1);
    area.removeFromLeft (14);

    // Right: delete, then Solo over Bypass.
    remove.setBounds (area.removeFromRight (28).withSizeKeepingCentre (28, 28));
    area.removeFromRight (14);
    auto buttons = area.removeFromRight (64).withSizeKeepingCentre (64, 2 * 28 + 6);
    solo.setBounds (buttons.removeFromTop (28));
    buttons.removeFromTop (6);
    bypass.setBounds (buttons.removeFromTop (28));

    // Knobs, left to right.
    if (frequency != nullptr)
    {
        const auto knobSize = frequency->getPreferredBounds();
        auto x = area.getX();
        for (auto* knob : { frequency.get(), gain.get(), q.get(), slope.get() })
        {
            const auto width = juce::jmax (knobSize.getWidth(), 76);
            knob->setBounds (juce::Rectangle<int> (x, area.getY(), width, area.getHeight()).withSizeKeepingCentre (width, knobSize.getHeight()));
            x += width + 6;
        }
    }
}

} // namespace pm::ui
