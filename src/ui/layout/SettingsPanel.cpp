// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/layout/SettingsPanel.h"

#include "PluginProcessor.h"
#include "Version.h"
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

constexpr int padding = 14;
constexpr int rowLabelWidth = 110;
constexpr int qualityTop = 62;
constexpr int safetyTop = 168;
} // namespace

SettingsOverlay::SettingsOverlay (PMelodiesProcessor& p)
    : realtimeQuality (param::realtimeQualityChoices()),
      offlineQuality (param::offlineQualityChoices()),
      clipperSwitch (text ("Clipper de seguridad")),
      ceiling (parameter (p, param::id::clipperCeiling), "Techo", Knob::Style::valueOnly),
      repositoryLink ("Ver en GitHub", juce::URL (info::repositoryUrl))
{
    setTitle ("Ajustes");
    setWantsKeyboardFocus (true);

    realtimeQuality.attachTo (parameter (p, param::id::qualityRealtime));
    realtimeQuality.setTooltip (text ("Al reproducir\nOversampling m\xc3\xadnimo de los m\xc3\xb3" "dulos mientras suena el proyecto."));
    offlineQuality.attachTo (parameter (p, param::id::qualityOffline));
    offlineQuality.setTooltip (text ("Al exportar\nOversampling m\xc3\xadnimo al renderizar. M\xc3\xa1s calidad, m\xc3\xa1s CPU."));

    clipperSwitch.attachTo (parameter (p, param::id::clipperEnabled));
    clipperSwitch.setTooltip (text ("Clipper de seguridad\nRecorte suave al final de la cadena: la salida nunca supera el techo."));

    ceiling.setFormatter ([] (float v) { return "techo " + format::decibels (v, 1, true); });
    ceiling.setTooltip (text ("Techo del clipper\nDe \xe2\x88\x92" "6 a 0 dBFS. Arrastra, Alt+clic para escribirlo, doble clic: \xe2\x88\x92" "0,3 dB."));

    closeButton.setTooltip ("Cerrar");
    closeButton.onClick = [this] { close(); };

    repositoryLink.setFont (themeOf (*this).ui (11.0f), false, juce::Justification::centredLeft);

    for (auto* c : std::initializer_list<juce::Component*> { &closeButton, &realtimeQuality, &offlineQuality, &clipperSwitch,
                                                              &ceiling, &repositoryLink })
        addAndMakeVisible (c);
}

SettingsOverlay::~SettingsOverlay() = default;

juce::Rectangle<int> SettingsOverlay::getPanelBounds() const
{
    const auto& m = themeOf (*this).metrics;
    return getLocalBounds().reduced (padding).removeFromRight (m.settingsPanelWidth);
}

void SettingsOverlay::close()
{
    if (onClose)
        onClose();
}

void SettingsOverlay::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;

    g.fillAll (c.backdrop);

    const auto bounds = panel.toFloat();
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 24, { 0, 8 }).drawForRectangle (g, panel);
    g.setColour (c.panelRaised);
    g.fillRoundedRectangle (bounds, theme.metrics.radiusPanel);
    g.setColour (c.border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme.metrics.radiusPanel, 1.0f);

    auto inner = panel.reduced (padding);

    g.setColour (c.text);
    g.setFont (theme.display (15.0f));
    g.drawText ("Ajustes", inner.removeFromTop (28), juce::Justification::centredLeft, false);

    const auto sectionLabel = [&] (const char* utf8, int y)
    {
        g.setColour (c.textMuted);
        g.setFont (theme.label());
        g.drawText (text (utf8), juce::Rectangle<int> (inner.getX(), panel.getY() + y, inner.getWidth(), 14),
                    juce::Justification::centredLeft, false);
    };

    sectionLabel ("CALIDAD (OVERSAMPLING GLOBAL)", qualityTop);
    sectionLabel ("SEGURIDAD", safetyTop);

    g.setColour (c.textSecondary);
    g.setFont (theme.ui (12.0f));
    g.drawText ("Al reproducir", realtimeQuality.getBounds().withX (inner.getX()).withWidth (rowLabelWidth),
                juce::Justification::centredLeft, false);
    g.drawText ("Al exportar", offlineQuality.getBounds().withX (inner.getX()).withWidth (rowLabelWidth),
                juce::Justification::centredLeft, false);

    // Footer: version and licence above a separator; the repository link sits underneath.
    auto footer = inner.removeFromBottom (50);
    g.setColour (c.border);
    g.fillRect (footer.removeFromTop (1));
    footer.removeFromTop (9);
    g.setColour (c.textMuted);
    g.setFont (theme.ui (11.0f));
    const auto about = juce::String (info::productName) + " v" + info::versionString + text (" \xc2\xb7 c\xc3\xb3" "digo abierto (AGPLv3)");
    g.drawText (about, footer.removeFromTop (16), juce::Justification::centredLeft, false);
}

void SettingsOverlay::resized()
{
    panel = getPanelBounds();
    const auto inner = panel.reduced (padding);

    closeButton.setBounds (juce::Rectangle<int> (28, 28).withPosition (inner.getRight() - 28, inner.getY()));

    const auto selectorRow = [&] (SegmentedSelector& selector, int y)
    {
        const auto size = selector.getPreferredBounds();
        selector.setBounds (inner.getX() + rowLabelWidth + 8, panel.getY() + y, size.getWidth(), size.getHeight());
    };
    selectorRow (realtimeQuality, qualityTop + 22);
    selectorRow (offlineQuality, qualityTop + 22 + 34);

    const auto switchSize = clipperSwitch.getPreferredBounds();
    clipperSwitch.setBounds (inner.getX(), panel.getY() + safetyTop + 22, switchSize.getWidth(), 20);
    const auto ceilingSize = ceiling.getPreferredBounds();
    ceiling.setBounds (inner.getRight() - ceilingSize.getWidth() - 10, panel.getY() + safetyTop + 23, ceilingSize.getWidth() + 10, 18);

    repositoryLink.setBounds (inner.getX() - 2, inner.getBottom() - 22, 90, 20);
}

void SettingsOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panel.contains (e.getPosition()))
        close();
}

bool SettingsOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

} // namespace pm::ui
