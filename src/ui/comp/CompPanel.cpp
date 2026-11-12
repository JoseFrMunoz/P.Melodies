// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/comp/CompPanel.h"

#include "PluginProcessor.h"
#include "core/ValueFormat.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

struct KnobSpec
{
    const char* id;
    const char* label;
    const char* help; // UTF-8
};

const std::array<KnobSpec, CompPanel::numKnobs> knobSpecs { {
    { param::id::compThreshold, "Threshold",
      "Threshold\nNivel a partir del cual empieza a comprimir. A 0 dB el compresor est\xc3\xa1 en reposo." },
    { param::id::compRatio, "Ratio",
      "Ratio\nCu\xc3\xa1nto comprime por encima del threshold: con 4:1, cada 4 dB de m\xc3\xa1s salen como 1. "
      "Con \xe2\x88\x9e:1 no pasa del threshold. En Vocal es autom\xc3\xa1tico." },
    { param::id::compKnee, "Knee",
      "Knee\nAnchura de la rodilla: cu\xc3\xa1nto se suaviza la entrada en compresi\xc3\xb3n. En Vocal es autom\xc3\xa1tico; "
      "Opto la ensancha 12 dB." },
    { param::id::compRange, "Range", "Range\nReducci\xc3\xb3n m\xc3\xa1xima: nunca reduce m\xc3\xa1s que esto." },
    { param::id::compAttack, "Attack", "Attack\nTiempo en llegar al 63 % de la reducci\xc3\xb3n." },
    { param::id::compRelease, "Release",
      "Release\nTiempo en recuperar el 63 % de la reducci\xc3\xb3n. Con Auto-release se alarga tras compresiones largas." },
    { param::id::compLookahead, "Lookahead",
      "Lookahead\nEl compresor ve la se\xc3\xb1" "al hasta 20 ms antes de que llegue. La latencia del plugin no cambia." },
    { param::id::compHold, "Hold", "Hold\nMantiene la reducci\xc3\xb3n este tiempo antes de empezar a soltar." },
    { param::id::compMakeup, "Makeup", "Makeup\nGanancia a la salida. Con Auto-gain se suma a la calculada." },
    { param::id::compMix, "Mix", "Mix\nMezcla de la se\xc3\xb1" "al original y la comprimida: compresi\xc3\xb3n en paralelo." },
} };

juce::StringArray styleHelp()
{
    return {
        text ("Clean\nPreciso y limpio: los tiempos son los de los knobs."),
        text ("Classic\nComo los compresores antiguos: escucha en parte su propia salida. Cuanto m\xc3\xa1s comprime, m\xc3\xa1s "
              "r\xc3\xa1pido reacciona, y tras una compresi\xc3\xb3n larga vuelve m\xc3\xa1s despacio."),
        text ("Opto\nLento y muy suave, con la rodilla m\xc3\xa1s ancha. Tras una compresi\xc3\xb3n larga, la mitad de la "
              "reducci\xc3\xb3n tarda mucho en volver."),
        text ("Vocal\nPone el ratio y el knee solo: m\xc3\xa1s ratio cuanto m\xc3\xa1s se pasa la voz del umbral, para "
              "mantenerla delante y estable."),
        text ("Mastering\nEscucha el nivel medio y cambia la ganancia muy suavemente: para mezclas completas."),
        text ("Bus\nNivel medio, algo de realimentaci\xc3\xb3n y release en dos etapas: el \xc2\xab" "pegamento\xc2\xbb de "
              "grupos y buses."),
        text ("Punch\nDeja pasar el principio de cada golpe y recupera r\xc3\xa1pido: m\xc3\xa1s pegada."),
        text ("Pumping\nEl release vuelve despacio al principio y r\xc3\xa1pido al final: bombeo marcado, m\xc3\xa1s a\xc3\xban "
              "con Hold."),
    };
}

juce::String signedDb (float v)
{
    return (v > 0.04f ? "+" : "") + format::compact (v, 1, true) + " dB";
}
} // namespace

CompPanel::CompPanel (PMelodiesProcessor& p)
    : processor (p),
      oversampling (param::compOversamplingNames()),
      styles (param::compStyleNames()),
      sidechainEq (p),
      source (param::compSidechainNames()),
      listen ("Escuchar"),
      autoGain ("Auto-gain"),
      autoRelease ("Auto-release")
{
    setTitle ("Compresor");
    auto& state = processor.getState();
    const auto steel = themeOf (*this).moduleColour (ModuleId::compressor);

    oversampling.attachTo (*state.getParameter (param::id::compOversampling));
    oversampling.setTooltip (text ("Oversampling\nProcesa el compresor a 2 o 4 veces la frecuencia de muestreo para reducir el "
                                   "aliasing de los cambios r\xc3\xa1pidos de ganancia. Si la calidad global es mayor, manda "
                                   "la global. La latencia del plugin no cambia."));

    styles.attachTo (*state.getParameter (param::id::compStyle));
    styles.setAccentColour (steel);
    styles.setTooltips (styleHelp(), text ("Estilo\nTodos comparten threshold, ratio, knee y range; cambia c\xc3\xb3mo escuchan "
                                           "la se\xc3\xb1" "al y c\xc3\xb3mo reaccionan."));

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& spec = knobSpecs[i];
        knobs[i] = std::make_unique<Knob> (*state.getParameter (spec.id), spec.label);
        knobs[i]->setAccentColour (steel);
        knobs[i]->setTooltip (text (spec.help));
        addAndMakeVisible (*knobs[i]);
    }

    const auto decibels = [] (float v) { return format::compact (v, 1, true) + " dB"; };
    findKnob (param::id::compThreshold)->setFormatter (decibels);
    findKnob (param::id::compRatio)->setFormatter ([] (float v) { return format::ratio (v, dsp::comp::infiniteRatio); });
    findKnob (param::id::compKnee)->setFormatter (decibels);
    findKnob (param::id::compRange)->setFormatter (decibels);
    for (const auto* id : { param::id::compAttack, param::id::compRelease, param::id::compLookahead, param::id::compHold })
        findKnob (id)->setFormatter ([] (float v) { return format::milliseconds (v); });
    findKnob (param::id::compMakeup)->setFormatter (signedDb);
    findKnob (param::id::compMix)->setFormatter ([] (float v) { return format::percent (v); });

    graph.setAccentColour (steel);
    curve.setAccentColour (steel);
    sidechainEq.setAccentColour (steel);

    source.attachTo (*state.getParameter (param::id::compSidechain));
    source.setTooltip (text ("Fuente del sidechain\nInterno: el compresor escucha la propia se\xc3\xb1" "al. Externo: la que "
                             "FL Studio le mande por el sidechain (por ejemplo, un bombo para el bombeo)."));

    listen.setIcon (icons::Icon::speaker);
    listen.setTooltip (text ("Escuchar\nSustituye la salida por lo que oye el compresor, ya filtrado. Un clic lo deja "
                             "activado; mantenerlo pulsado lo activa solo mientras lo pulsas."));
    listen.onChange = [this] (bool on) { processor.setCompressorListen (on); };

    autoGain.attachTo (*state.getParameter (param::id::compAutoGain));
    autoGain.setTooltip (text ("Auto-gain\nSube la salida lo que se calcula a partir del threshold, el ratio y el knee, para "
                               "que los niveles a medio camino entre el threshold y 0 dBFS suenen igual. No sube y baja "
                               "con la m\xc3\xbasica. El Makeup se suma encima."));
    autoRelease.attachTo (*state.getParameter (param::id::compAutoRelease));
    autoRelease.setTooltip (text ("Auto-release\nEl release se alarga tras compresiones largas y se queda corto tras golpes "
                                  "sueltos. El knob Release escala el efecto."));

    for (auto* c : std::initializer_list<juce::Component*> { &oversampling, &styles, &graph, &curve, &sidechainEq, &source,
                                                             &listen, &autoGain, &autoRelease })
        addAndMakeVisible (c);

    // Points left from before the window opened are stale.
    auto& points = processor.getEngine().getCompressorMeter().getPoints();
    points.discard (points.getNumReady());

    refresh();
}

CompPanel::~CompPanel()
{
    // Closing the window never leaves the listen switch on behind the user's back.
    if (listen.isOn())
        processor.setCompressorListen (false);
}

float CompPanel::read (const char* parameterId) const
{
    const auto* value = processor.getState().getRawParameterValue (parameterId);
    return value != nullptr ? value->load() : 0.0f;
}

Knob* CompPanel::findKnob (const juce::String& parameterId) noexcept
{
    for (auto& k : knobs)
        if (k->getParameter().getParameterID() == parameterId)
            return k.get();
    return nullptr;
}

void CompPanel::updateMeter()
{
    auto& meter = processor.getEngine().getCompressorMeter();
    auto& fifo = meter.getPoints();
    std::array<dsp::comp::MeterPoint, 256> points;
    for (int n = fifo.pop (points.data(), (int) points.size()); n > 0; n = fifo.pop (points.data(), (int) points.size()))
        graph.push (points.data(), n);
}

juce::String CompPanel::subtitle() const
{
    const auto style = param::compStyleNames()[juce::roundToInt (read (param::id::compStyle))];
    return "Estilo " + style + text (" \xc2\xb7 ") + format::number (getShownReductionDb(), 1, true) + text (" dB de reducci\xc3\xb3n");
}

juce::String CompPanel::chainStatus() const
{
    const auto style = param::compStyleNames()[juce::roundToInt (read (param::id::compStyle))];
    return style + text (" \xc2\xb7 ") + format::number (getShownReductionDb(), 1, true) + " dB";
}

void CompPanel::refresh()
{
    auto& meter = processor.getEngine().getCompressorMeter();
    const auto style = (dsp::comp::Style) juce::roundToInt (read (param::id::compStyle));
    const auto automatic = dsp::comp::traitsOf (style).automaticCurve;

    // Vocal sets ratio and knee itself (§12): their knobs are shown dimmed.
    findKnob (param::id::compRatio)->setEnabled (! automatic);
    findKnob (param::id::compKnee)->setEnabled (! automatic);

    const dsp::comp::CurveSettings settings { read (param::id::compThreshold), read (param::id::compRatio),
                                              read (param::id::compKnee), read (param::id::compRange), style };
    curve.setCurve (dsp::comp::StaticCurve (settings));
    curve.setLevel (meter.levelDb.load());
    graph.setThreshold (read (param::id::compThreshold));
    graph.setSidechainMissing (meter.sidechainMissing.load() && juce::roundToInt (read (param::id::compSidechain)) == 1);
    listen.setOn (processor.isCompressorListening());
    sidechainEq.refresh();

    if (const auto s = subtitle(); s != shownSubtitle)
    {
        shownSubtitle = s;
        repaint (headerArea);
    }
}

void CompPanel::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    g.fillAll (c.background);

    // Header: colour, title and what it is doing now.
    auto header = headerArea;
    g.setColour (theme.moduleColour (ModuleId::compressor));
    g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) header.getX() + 5.0f, (float) header.getCentreY() }), 3.0f);
    header.removeFromLeft (20);
    const auto titleFont = theme.display (16.0f);
    g.setColour (c.text);
    g.setFont (titleFont);
    g.drawText ("Compresor", header.removeFromLeft ((int) std::ceil (textWidth (titleFont, "Compresor"))),
                juce::Justification::centredLeft, false);
    header.removeFromLeft (10);
    g.setColour (c.textMuted);
    g.setFont (theme.ui (12.0f));
    g.drawText (subtitle(), header.withRight (oversamplingLabel.getX() - 8), juce::Justification::centredLeft, true);
    g.setFont (theme.label());
    g.drawText ("OVERSAMPLING", oversamplingLabel, juce::Justification::centredRight, false);

    // Sidechain card.
    const auto card = sidechainCard.toFloat().reduced (0.5f);
    g.setColour (c.panelRaised);
    g.fillRoundedRectangle (card, theme.metrics.radiusCard);
    g.setColour (c.border);
    g.drawRoundedRectangle (card, theme.metrics.radiusCard, 1.0f);
    g.setColour (c.textMuted);
    g.setFont (theme.label());
    g.drawText ("SIDECHAIN", sidechainLabel, juce::Justification::centredLeft, false);
}

void CompPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 14);
    headerArea = area.removeFromTop (28);

    {
        auto row = headerArea;
        const auto w = oversampling.getPreferredBounds().getWidth();
        oversampling.setBounds (row.removeFromRight (w).withSizeKeepingCentre (w, 25));
        row.removeFromRight (8);
        oversamplingLabel = row.removeFromRight ((int) std::ceil (textWidth (themeOf (*this).label(), "OVERSAMPLING")) + 4);
    }

    area.removeFromTop (10);
    styles.setBounds (area.removeFromTop (ChipSelector::height).withWidth (styles.getPreferredBounds().getWidth()));

    area.removeFromTop (10);
    auto graphs = area.removeFromTop (graphHeight);
    curve.setBounds (graphs.removeFromRight (curveWidth));
    graphs.removeFromRight (10);
    graph.setBounds (graphs);

    // Knobs spread over the whole width, the first and the last at the edges.
    area.removeFromTop (10);
    const auto knobSize = knobs[0]->getPreferredBounds();
    auto knobRow = area.removeFromTop (knobSize.getHeight());
    const auto slotWidth = (float) knobRow.getWidth() / (float) numKnobs;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto w = juce::jmax (knobSize.getWidth(), knobs[i]->getPreferredBounds().getWidth());
        const auto centreX = knobRow.getX() + (int) std::round (slotWidth * ((float) i + 0.5f));
        knobs[i]->setBounds (juce::Rectangle<int> (w, knobRow.getHeight()).withCentre ({ centreX, knobRow.getCentreY() }));
    }

    area.removeFromTop (10);
    sidechainCard = area.removeFromTop (58);
    auto row = sidechainCard.reduced (12, 8);
    sidechainLabel = row.removeFromLeft ((int) std::ceil (textWidth (themeOf (*this).label(), "SIDECHAIN")) + 4);
    row.removeFromLeft (12);
    sidechainEq.setBounds (row.removeFromLeft (200).withSizeKeepingCentre (200, 40));
    row.removeFromLeft (12);
    const auto sw = source.getPreferredBounds().getWidth();
    source.setBounds (row.removeFromLeft (sw).withSizeKeepingCentre (sw, 25));
    row.removeFromLeft (12);
    const auto lw = listen.getPreferredBounds().getWidth() + 20;
    listen.setBounds (row.removeFromLeft (lw).withSizeKeepingCentre (lw, 28));

    const auto rw = autoRelease.getPreferredBounds().getWidth();
    autoRelease.setBounds (row.removeFromRight (rw).withSizeKeepingCentre (rw, 18));
    row.removeFromRight (16);
    const auto gw = autoGain.getPreferredBounds().getWidth();
    autoGain.setBounds (row.removeFromRight (gw).withSizeKeepingCentre (gw, 18));
}

} // namespace pm::ui
