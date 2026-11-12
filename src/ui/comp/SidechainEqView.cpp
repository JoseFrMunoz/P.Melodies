// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/comp/SidechainEqView.h"

#include "PluginProcessor.h"
#include "core/ValueFormat.h"
#include "dsp/eq/EqFilterDesign.h"
#include "modules/comp/CompParameters.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

int slopeDb (float index)
{
    const auto i = juce::jlimit (0, (int) dsp::eq::slopesDbPerOctave.size() - 1, juce::roundToInt (index));
    return dsp::eq::slopesDbPerOctave[(size_t) i];
}
} // namespace

SidechainEqView::SidechainEqView (PMelodiesProcessor& processor)
{
    auto& state = processor.getState();
    const auto get = [&state] (const char* id) { return state.getParameter (id); };
    parameters[(size_t) Filter::highPass] = { get (param::id::compScHpOn), get (param::id::compScHpFreq), get (param::id::compScHpSlope), nullptr, nullptr };
    parameters[(size_t) Filter::bell] = { get (param::id::compScBellOn), get (param::id::compScBellFreq), nullptr,
                                          get (param::id::compScBellGain), get (param::id::compScBellQ) };
    parameters[(size_t) Filter::lowPass] = { get (param::id::compScLpOn), get (param::id::compScLpFreq), get (param::id::compScLpSlope), nullptr, nullptr };

    setTitle ("EQ del sidechain");
    setTooltip (text ("EQ del sidechain\nFiltra lo que oye el compresor (no lo que suena). Arrastra los puntos: pasa-altos, "
                      "campana y pasa-bajos. Rueda: pendiente o Q. Alt+clic: apaga o enciende un filtro. "
                      "Doble clic: escribe la frecuencia."));
    setWantsKeyboardFocus (false);
    lastSnapshot = snapshot();
}

SidechainEqView::~SidechainEqView() = default;

float SidechainEqView::value (const juce::RangedAudioParameter* p) const
{
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
}

void SidechainEqView::set (juce::RangedAudioParameter* p, float realValue)
{
    if (p != nullptr)
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

bool SidechainEqView::isEnabled (Filter filter) const { return value (parametersOf (filter).on) >= 0.5f; }

juce::String SidechainEqView::snapshot() const
{
    juce::String s;
    for (const auto& p : parameters)
        for (const auto* q : { p.on, p.frequency, p.slope, p.gain, p.q })
            s << (q != nullptr ? q->getValue() : 0.0f) << ";";
    return s;
}

void SidechainEqView::refresh()
{
    if (const auto s = snapshot(); s != lastSnapshot)
    {
        lastSnapshot = s;
        repaint();
    }
}

void SidechainEqView::setAccentColour (juce::Colour colour)
{
    accent = colour;
    repaint();
}

//==============================================================================
float SidechainEqView::xForFrequency (float hz) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced (4.0f, 0.0f);
    const auto t = std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
    return area.getX() + t * area.getWidth();
}

float SidechainEqView::frequencyForX (float x) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced (4.0f, 0.0f);
    const auto t = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / juce::jmax (1.0f, area.getWidth()));
    return minHz * std::pow (maxHz / minHz, t);
}

float SidechainEqView::yForDb (float db) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced (0.0f, 3.0f);
    return area.getY() + (topDb - juce::jlimit (bottomDb, topDb, db)) / (topDb - bottomDb) * area.getHeight();
}

float SidechainEqView::dbForY (float y) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced (0.0f, 3.0f);
    return topDb - (y - area.getY()) / juce::jmax (1.0f, area.getHeight()) * (topDb - bottomDb);
}

juce::Point<float> SidechainEqView::pointOf (Filter filter) const
{
    const auto& p = parametersOf (filter);
    const auto x = xForFrequency (value (p.frequency));
    const auto db = filter == Filter::bell ? value (p.gain) : 0.0f;
    return { x, yForDb (db) };
}

int SidechainEqView::filterAt (juce::Point<float> position) const
{
    int best = -1;
    auto bestDistance = grabRadius;
    for (int f = 0; f < numFilters; ++f)
    {
        const auto d = pointOf ((Filter) f).getDistanceFrom (position);
        if (d <= bestDistance)
        {
            best = f;
            bestDistance = d;
        }
    }
    return best;
}

juce::String SidechainEqView::describe (Filter filter) const
{
    const auto& p = parametersOf (filter);
    const auto frequency = value (p.frequency);
    const auto dot = text (" \xc2\xb7 ");

    switch (filter)
    {
        case Filter::highPass:
            if (! isEnabled (filter))
                return "HP apagado";
            return "HP " + format::frequency (frequency) + dot + juce::String (slopeDb (value (p.slope))) + " dB/oct";

        case Filter::lowPass:
            if (! isEnabled (filter))
                return "LP apagado";
            if (frequency >= comp::sidechainLowPassMax)
                return "LP abierto";
            return "LP " + format::frequency (frequency) + dot + juce::String (slopeDb (value (p.slope))) + " dB/oct";

        case Filter::bell:
        {
            if (! isEnabled (filter))
                return "Campana apagada";
            const auto gain = value (p.gain);
            return "Campana " + format::frequency (frequency) + dot + (gain > 0.0f ? "+" : "") + format::decibels (gain, 1, true)
                   + dot + "Q " + format::compact (value (p.q), 2);
        }
    }
    return {};
}

//==============================================================================
void SidechainEqView::dragTo (Filter filter, juce::Point<float> position)
{
    const auto& p = parametersOf (filter);
    set (p.frequency, frequencyForX (position.x));
    if (filter == Filter::bell)
        set (p.gain, juce::jlimit (-comp::sidechainBellMaxGainDb, comp::sidechainBellMaxGainDb, dbForY (position.y)));
    focus = filter;
    refresh();
}

void SidechainEqView::stepWheel (Filter filter, int steps)
{
    if (steps == 0)
        return;

    const auto& p = parametersOf (filter);
    auto* target = filter == Filter::bell ? p.q : p.slope;
    if (target == nullptr)
        return;

    const auto next = filter == Filter::bell
                          ? juce::jlimit (comp::sidechainBellMinQ, comp::sidechainBellMaxQ, value (target) * std::pow (1.12f, (float) steps))
                          : (float) juce::jlimit (0, (int) dsp::eq::slopesDbPerOctave.size() - 1, juce::roundToInt (value (target)) + steps);
    target->beginChangeGesture();
    set (target, next);
    target->endChangeGesture();
    focus = filter;
    refresh();
}

void SidechainEqView::toggle (Filter filter)
{
    auto* on = parametersOf (filter).on;
    if (on == nullptr)
        return;
    on->beginChangeGesture();
    set (on, isEnabled (filter) ? 0.0f : 1.0f);
    on->endChangeGesture();
    focus = filter;
    refresh();
}

void SidechainEqView::beginTyping (Filter filter)
{
    typing = filter;
    focus = filter;
    editor = std::make_unique<juce::TextEditor>();
    editor->setJustification (juce::Justification::centred);
    editor->setFont (themeOf (*this).mono (11.0f));
    editor->setText (format::frequency (value (parametersOf (filter).frequency)), false);
    editor->onReturnKey = [this] { finishTyping (true); };
    editor->onEscapeKey = [this] { finishTyping (false); };
    editor->onFocusLost = [this] { finishTyping (false); };
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 110), juce::jmin (getHeight(), 24)));
    editor->selectAll();
    if (isShowing())
        editor->grabKeyboardFocus();
}

void SidechainEqView::finishTyping (bool apply)
{
    if (editor == nullptr)
        return;

    const auto typed = editor->getText();
    // Deferred: this may run inside one of the editor's own callbacks.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SidechainEqView> (this)]
                                     {
                                         if (safe != nullptr)
                                             safe->editor.reset();
                                     });
    editor->onFocusLost = nullptr;
    editor->setVisible (false);

    if (! apply)
        return;

    if (const auto hz = format::parseFrequency (typed); hz > 0.0)
    {
        auto* p = parametersOf (typing).frequency;
        p->beginChangeGesture();
        set (p, (float) hz);
        p->endChangeGesture();
        refresh();
    }
}

//==============================================================================
void SidechainEqView::mouseMove (const juce::MouseEvent& e)
{
    const auto index = filterAt (e.position);
    if (index != hovered)
    {
        hovered = index;
        if (index >= 0)
            focus = (Filter) index;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SidechainEqView::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void SidechainEqView::mouseDown (const juce::MouseEvent& e)
{
    const auto index = filterAt (e.position);
    if (index < 0)
        return;

    if (e.mods.isAltDown())
    {
        toggle ((Filter) index);
        return;
    }

    dragging = index;
    focus = (Filter) index;
    const auto& p = parametersOf (focus);
    p.frequency->beginChangeGesture();
    if (p.gain != nullptr)
        p.gain->beginChangeGesture();
}

void SidechainEqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging >= 0)
        dragTo ((Filter) dragging, e.position);
}

void SidechainEqView::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0)
        return;
    const auto& p = parametersOf ((Filter) dragging);
    p.frequency->endChangeGesture();
    if (p.gain != nullptr)
        p.gain->endChangeGesture();
    dragging = -1;
}

void SidechainEqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const auto index = filterAt (e.position); index >= 0)
        beginTyping ((Filter) index);
}

void SidechainEqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto index = filterAt (e.position);
    const auto filter = index >= 0 ? (Filter) index : focus;

    // One step per notch, as the EQ's cut slopes (a notch is about 0.23 on Windows).
    wheelAccumulator += wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    const auto steps = (int) (wheelAccumulator / 0.2f);
    if (steps != 0)
    {
        wheelAccumulator -= 0.2f * (float) steps;
        stepWheel (filter, steps);
    }
}

//==============================================================================
juce::Path SidechainEqView::makeCurve() const
{
    std::array<std::optional<dsp::eq::AnalogPrototype>, numFilters> prototypes;
    for (int f = 0; f < numFilters; ++f)
    {
        const auto filter = (Filter) f;
        const auto& p = parametersOf (filter);
        if (! isEnabled (filter))
            continue;

        dsp::eq::BandSettings settings;
        settings.frequency = value (p.frequency);
        if (filter == Filter::bell)
        {
            settings.type = dsp::eq::FilterType::bell;
            settings.gainDb = value (p.gain);
            settings.q = value (p.q);
            if (juce::exactlyEqual (settings.gainDb, 0.0))
                continue;
        }
        else
        {
            if (filter == Filter::lowPass && settings.frequency >= comp::sidechainLowPassMax)
                continue; // open
            settings.type = filter == Filter::highPass ? dsp::eq::FilterType::lowCut : dsp::eq::FilterType::highCut;
            settings.slopeIndex = juce::roundToInt (value (p.slope));
        }
        prototypes[(size_t) f] = dsp::eq::makeAnalogPrototype (settings);
    }

    juce::Path path;
    const auto width = juce::jmax (2, getWidth());
    for (int px = 0; px <= width; px += 2)
    {
        const auto hz = frequencyForX ((float) px);
        auto db = 0.0;
        for (const auto& prototype : prototypes)
            if (prototype.has_value())
                db += 20.0 * std::log10 (std::max (1.0e-9, std::abs (prototype->response (hz))));
        const auto y = yForDb ((float) db);
        if (px == 0)
            path.startNewSubPath ((float) px, y);
        else
            path.lineTo ((float) px, y);
    }
    return path;
}

void SidechainEqView::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = accent.isTransparent() ? c.accent : accent;

    g.setColour (c.window);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (c.borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (getLocalBounds().reduced (1));

    g.setColour (c.grid);
    g.drawHorizontalLine ((int) yForDb (0.0f), bounds.getX(), bounds.getRight());

    g.setColour (colour);
    g.strokePath (makeCurve(), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Information line, kept clear of the points' usual places.
    g.setColour (c.textMuted);
    g.setFont (theme.mono (10.0f, Weight::regular));
    g.drawText (getInfoText(), bounds.withTrimmedTop (bounds.getHeight() * 0.42f).reduced (8.0f, 0.0f),
                juce::Justification::centred, false);

    for (int f = 0; f < numFilters; ++f)
    {
        const auto filter = (Filter) f;
        const auto centre = pointOf (filter);
        const auto radius = (f == hovered || f == dragging) ? 4.5f : 3.5f;
        const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        if (isEnabled (filter))
        {
            g.setColour (colour);
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (c.textDim);
            g.drawEllipse (circle.reduced (0.5f), 1.2f);
        }
    }
}

} // namespace pm::ui
