// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/eq/EqGraph.h"

#include "PluginProcessor.h"
#include "core/ValueFormat.h"
#include "ui/theme/LookAndFeel.h"

#include <cmath>

namespace pm::ui
{

using dsp::eq::FilterType;
using param::EqField;

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

constexpr float verticalMargin = 17.0f; // the ±range lines sit this far from the edges (design)
constexpr float pointRadius = 9.0f;
constexpr float hitRadius = 13.0f;
constexpr int curveStep = 2; // pixels per curve sample
constexpr double analyserTopDb = 6.0, analyserBottomDb = -84.0;

double clampQ (double q) { return juce::jlimit (dsp::eq::minQ, dsp::eq::maxQ, q); }
} // namespace

//==============================================================================
/** Small panel to type a band's frequency, gain and Q (double click on its point). */
class EqGraph::ValueEditor final : public juce::Component
{
public:
    ValueEditor (EqGraph& g, int bandIndex) : graph (g), band (bandIndex)
    {
        const auto b = graph.controller.getBand (band);
        setup (frequency, format::frequency (b.settings.frequency), text ("Frecuencia"));
        setup (gain, format::number (b.settings.gainDb, 1), "Ganancia");
        setup (q, format::number (b.settings.q, 2), "Q");
        gain.setEnabled (dsp::eq::usesGain (b.settings.type));
        setSize (3 * 74 + 4 * 6, 50);
    }

    void open()
    {
        frequency.grabKeyboardFocus();
        frequency.selectAll();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = themeOf (*this);
        g.setColour (theme.colours.elevated);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (theme.colours.border);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme.colours.textMuted);
        g.setFont (theme.label (9.0f));
        int x = 6;
        for (const auto* name : { "FRECUENCIA", "GANANCIA", "Q" })
        {
            g.drawText (name, x, 4, 74, 12, juce::Justification::centredLeft, false);
            x += 80;
        }
    }

    void resized() override
    {
        int x = 6;
        for (auto* e : { &frequency, &gain, &q })
        {
            e->setBounds (x, 18, 74, 26);
            x += 80;
        }
    }

private:
    void setup (juce::TextEditor& editor, const juce::String& value, const juce::String& title)
    {
        editor.setTitle (title);
        editor.setText (value, false);
        editor.setJustification (juce::Justification::centred);
        editor.setSelectAllWhenFocused (true);
        editor.onReturnKey = [this] { commit(); };
        editor.onEscapeKey = [this] { close(); };
        editor.onFocusLost = [this]
        {
            // Closing when the focus leaves the whole panel, not when it moves between fields.
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ValueEditor> (this)]
                                             {
                                                 if (safe != nullptr && ! safe->hasKeyboardFocus (true))
                                                     safe->close();
                                             });
        };
        addAndMakeVisible (editor);
    }

    void commit()
    {
        graph.applyTypedValues (band, frequency.getText(), gain.isEnabled() ? gain.getText() : juce::String(), q.getText());
        close();
    }

    void close()
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<EqGraph> (&graph)]
                                         {
                                             if (safe != nullptr)
                                                 safe->valueEditor.reset();
                                         });
    }

    EqGraph& graph;
    const int band;
    juce::TextEditor frequency, gain, q;
};

//==============================================================================
EqGraph::EqGraph (EqController& c) : controller (c), popup (std::make_unique<EqBandPopup> (*this, c))
{
    setTitle ("EQ");
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    addChildComponent (*popup);
}

EqGraph::~EqGraph() = default;

juce::Rectangle<float> EqGraph::plotArea() const noexcept
{
    return getLocalBounds().toFloat();
}

double EqGraph::getRangeDb() const
{
    return (double) controller.getProcessor().getEqRangeDb();
}

float EqGraph::xForFrequency (double hz) const noexcept
{
    const auto area = plotArea();
    const auto t = std::log (juce::jmax (1.0, hz) / minDisplayHz) / std::log (maxDisplayHz / minDisplayHz);
    return area.getX() + (float) t * area.getWidth();
}

double EqGraph::frequencyForX (float x) const noexcept
{
    const auto area = plotArea();
    const auto t = (double) (x - area.getX()) / juce::jmax (1.0f, area.getWidth());
    return minDisplayHz * std::pow (maxDisplayHz / minDisplayHz, t);
}

float EqGraph::yForGain (double db) const noexcept
{
    const auto area = plotArea();
    const auto range = getRangeDb();
    const auto top = area.getY() + verticalMargin, bottom = area.getBottom() - verticalMargin;
    return (float) juce::jmap (db, range, -range, (double) top, (double) bottom);
}

double EqGraph::gainForY (float y) const noexcept
{
    const auto area = plotArea();
    const auto range = getRangeDb();
    const auto top = area.getY() + verticalMargin, bottom = area.getBottom() - verticalMargin;
    return juce::jmap ((double) y, (double) top, (double) bottom, range, -range);
}

juce::Point<float> EqGraph::pointForBand (int index) const
{
    const auto b = controller.getBand (index);
    const auto y = dsp::eq::usesGain (b.settings.type) ? yForGain (b.settings.gainDb) : yForGain (0.0);
    return { xForFrequency (b.settings.frequency), juce::jlimit (pointRadius, (float) getHeight() - pointRadius, y) };
}

int EqGraph::bandAt (juce::Point<float> position) const
{
    int best = -1;
    auto bestDistance = hitRadius;
    for (int b = 0; b < eq::numBands; ++b)
    {
        if (! controller.getBand (b).active)
            continue;
        const auto d = pointForBand (b).getDistanceFrom (position);
        if (d <= bestDistance)
        {
            bestDistance = d;
            best = b;
        }
    }
    return best;
}

juce::String EqGraph::labelText (int index) const
{
    const auto b = controller.getBand (index);
    const auto dot = text (" \xc2\xb7 ");
    const auto first = format::frequency (b.settings.frequency);
    if (dsp::eq::usesGain (b.settings.type))
        return first + dot + format::decibels (b.settings.gainDb, 1, true);
    return first + dot + "Q " + format::number (b.settings.q, 2);
}

//==============================================================================
int EqGraph::createBandAt (juce::Point<float> position)
{
    const auto hz = frequencyForX (position.x);
    const auto bottomStrip = position.y > (float) getHeight() * 0.85f;
    const auto type = EqController::typeForNewBand (hz, bottomStrip);
    const auto gain = juce::jlimit (-dsp::eq::maxGainDb, dsp::eq::maxGainDb, std::round (gainForY (position.y) * 10.0) / 10.0);
    const auto band = controller.createBand (type, hz, gain);
    bandsChanged();
    return band;
}

void EqGraph::beginDrag (int index, juce::Point<float> position)
{
    if (index < 0)
        return;
    dragging = index;
    lastDragPosition = position;
    const auto b = controller.getBand (index);
    dragUsesGain = dsp::eq::usesGain (b.settings.type);
    dragFrequency = b.settings.frequency;
    dragValue = dragUsesGain ? b.settings.gainDb : b.settings.q;
    controller.setSelectedBand (index);
    controller.beginGesture (index, EqField::frequency);
    controller.beginGesture (index, dragUsesGain ? EqField::gain : EqField::q);
}

void EqGraph::dragTo (juce::Point<float> position, bool fine)
{
    if (dragging < 0)
        return;

    // Relative movement, so a fine drag (Ctrl) does not jump to the pointer.
    const auto factor = fine ? 0.1f : 1.0f;
    const auto delta = (position - lastDragPosition) * factor;
    lastDragPosition = position;

    const auto x = xForFrequency (dragFrequency) + delta.x;
    dragFrequency = juce::jlimit (dsp::eq::minFrequency, dsp::eq::maxFrequency, frequencyForX (x));
    controller.setValueInGesture (dragging, EqField::frequency, (float) dragFrequency);

    if (dragUsesGain)
    {
        dragValue = juce::jlimit (-dsp::eq::maxGainDb, dsp::eq::maxGainDb, gainForY (yForGain (dragValue) + delta.y));
        controller.setValueInGesture (dragging, EqField::gain, (float) dragValue);
        showGain (dragValue); // beyond the scale the display widens by itself (§12)
    }
    else
    {
        // Shapes without gain: up narrows (more Q), down widens.
        dragValue = clampQ (dragValue * std::pow (2.0, -delta.y / 40.0));
        controller.setValueInGesture (dragging, EqField::q, (float) dragValue);
    }
    bandsChanged(); // the mini panel follows the point
}

void EqGraph::endDrag()
{
    if (dragging < 0)
        return;
    controller.endGesture (dragging, EqField::frequency);
    controller.endGesture (dragging, dragUsesGain ? EqField::gain : EqField::q);
    dragging = -1;
}

void EqGraph::showGain (double gainDb)
{
    auto& processor = controller.getProcessor();
    if (std::abs (gainDb) > (double) processor.getEqRangeDb() + 1.0e-6)
        processor.setEqRangeDb ((int) std::ceil (std::abs (gainDb) - 1.0e-6));
}

void EqGraph::wheel (int index, float deltaY, bool qInsteadOfSlope, bool fine)
{
    if (index < 0 || std::abs (deltaY) < 1.0e-4f)
        return;

    const auto settings = controller.getBand (index).settings;

    // Cuts: the wheel walks the slopes, one step per notch (a trackpad has to travel as far).
    if (dsp::eq::usesSlope (settings.type) && ! qInsteadOfSlope)
    {
        if (index != slopeWheelBand || (slopeWheel > 0.0f) != (deltaY > 0.0f))
            slopeWheel = 0.0f; // another band, or the direction changed
        slopeWheelBand = index;
        slopeWheel += deltaY;
        if (std::abs (slopeWheel) + 1.0e-6f < slopeWheelStep)
            return;
        const auto step = slopeWheel > 0.0f ? 1 : -1;
        slopeWheel = 0.0f;
        const auto slope = juce::jlimit (0, (int) dsp::eq::slopesDbPerOctave.size() - 1, settings.slopeIndex + step);
        if (slope != settings.slopeIndex)
            controller.setValue (index, EqField::slope, (float) slope);
    }
    else
    {
        if (! dsp::eq::usesQ (settings.type, settings.slopeIndex))
            return; // a 6 dB/oct cut has no Q
        const auto steps = deltaY > 0.0f ? 1.0 : -1.0;
        const auto q = clampQ (settings.q * std::pow (fine ? 1.02 : 1.15, steps));
        controller.setValue (index, EqField::q, (float) q);
    }
    bandsChanged();
}

void EqGraph::bandsChanged()
{
    curvesValid = false;
    updatePopup();
    repaint();
}

void EqGraph::updatePopup()
{
    const auto selected = controller.getSelectedBand();
    popup->update (selected, selected >= 0 ? pointForBand (selected) : juce::Point<float>(), getLocalBounds());
}

void EqGraph::beginSolo (int index)
{
    if (index < 0 || ! controller.getBand (index).active)
        return;
    if (soloBand >= 0)
        endSolo();
    soloPrevious = controller.getSoloBand();
    soloBand = index;
    controller.setSoloBand (index);

    const auto settings = controller.getBand (index).settings;
    soloStartX = xForFrequency (settings.frequency);
    soloQ = settings.q;
    soloLastOffset = {};
    controller.beginGesture (index, EqField::frequency);
    controller.beginGesture (index, EqField::q);
    bandsChanged();
}

void EqGraph::soloDragTo (juce::Point<float> offset, bool changeQ)
{
    if (soloBand < 0)
        return;

    // Horizontal: the frequency, from where the press started (the vertical alone does nothing).
    const auto hz = juce::jlimit (dsp::eq::minFrequency, dsp::eq::maxFrequency, frequencyForX (soloStartX + offset.x));
    controller.setValueInGesture (soloBand, EqField::frequency, (float) hz);

    // Ctrl: the vertical movement narrows (up) or widens (down) what is heard.
    const auto delta = offset - soloLastOffset;
    soloLastOffset = offset;
    const auto settings = controller.getBand (soloBand).settings;
    if (changeQ && dsp::eq::usesQ (settings.type, settings.slopeIndex) && ! juce::exactlyEqual (delta.y, 0.0f))
    {
        soloQ = clampQ (soloQ * std::pow (2.0, -delta.y / 40.0));
        controller.setValueInGesture (soloBand, EqField::q, (float) soloQ);
    }
    bandsChanged();
}

juce::Range<double> EqGraph::soloRangeHz() const
{
    const auto band = controller.getSoloBand();
    if (band < 0 || ! controller.getBand (band).active)
        return {};
    const auto solo = eq::soloSettings (controller.getBand (band).settings);
    switch (solo.type)
    {
        case FilterType::highCut: return { minDisplayHz, solo.frequency }; // the lows: low shelf and low cut
        case FilterType::lowCut:  return { solo.frequency, maxDisplayHz }; // the highs
        case FilterType::bell:
        case FilterType::lowShelf:
        case FilterType::highShelf:
        case FilterType::notch:
        case FilterType::bandPass:
        case FilterType::tiltShelf: break;
    }
    // Band-pass: the edges where it is 3 dB down.
    const auto k = 1.0 / (2.0 * solo.q);
    const auto r = std::sqrt (1.0 + k * k);
    return { solo.frequency * (r - k), solo.frequency * (r + k) };
}

void EqGraph::endSolo()
{
    if (soloBand < 0)
        return;
    controller.endGesture (soloBand, EqField::frequency);
    controller.endGesture (soloBand, EqField::q);
    // Back to what was soloed before (the editor's Solo button may have latched this band).
    controller.setSoloBand (soloPrevious);
    soloBand = -1;
    soloPrevious = -1;
    bandsChanged();
}

juce::PopupMenu EqGraph::buildBandMenu (int index) const
{
    const auto b = controller.getBand (index);
    juce::PopupMenu shapes;
    const auto names = param::eqTypeNames();
    for (int t = 0; t < names.size(); ++t)
        shapes.addItem (menuShapeBase + t, names[t], true, (int) b.settings.type == t);

    juce::PopupMenu menu;
    menu.addSectionHeader ("Banda " + juce::String (index + 1));
    menu.addSubMenu ("Tipo", shapes);
    menu.addItem (menuBypass, "Bypass", true, b.bypassed);
    menu.addSeparator();
    menu.addItem (menuDelete, "Eliminar");
    return menu;
}

void EqGraph::handleMenuResult (int index, int result)
{
    if (index < 0 || result == 0)
        return;
    if (result == menuBypass)
        controller.setBypassed (index, ! controller.getBand (index).bypassed);
    else if (result == menuDelete)
        controller.deleteBand (index);
    else if (result >= menuShapeBase && result < menuShapeBase + dsp::eq::numFilterTypes)
        controller.setType (index, (FilterType) (result - menuShapeBase));
    bandsChanged();
}

bool EqGraph::applyTypedValues (int index, const juce::String& frequency, const juce::String& gain, const juce::String& q)
{
    if (index < 0)
        return false;
    bool any = false;
    if (const auto hz = format::parseFrequency (frequency); frequency.trim().isNotEmpty() && hz > 0.0)
    {
        controller.setValue (index, EqField::frequency, (float) juce::jlimit (dsp::eq::minFrequency, dsp::eq::maxFrequency, hz));
        any = true;
    }
    if (gain.trim().isNotEmpty() && gain.containsAnyOf ("0123456789"))
    {
        const auto db = juce::jlimit (-dsp::eq::maxGainDb, dsp::eq::maxGainDb, format::parseNumber (gain));
        controller.setValue (index, EqField::gain, (float) db);
        showGain (db);
        any = true;
    }
    if (const auto value = format::parseNumber (q); q.trim().isNotEmpty() && value > 0.0)
    {
        controller.setValue (index, EqField::q, (float) clampQ (value));
        any = true;
    }
    curvesValid = false;
    repaint();
    return any;
}

void EqGraph::showValueEditor (int index)
{
    if (index < 0)
        return;
    valueEditor = std::make_unique<ValueEditor> (*this, index);
    const auto p = pointForBand (index);
    auto bounds = valueEditor->getBounds().withPosition ((int) p.x + 14, (int) p.y + 12);
    bounds = bounds.constrainedWithin (getLocalBounds().reduced (4));
    valueEditor->setBounds (bounds);
    addAndMakeVisible (*valueEditor);
    valueEditor->open();
}

//==============================================================================
void EqGraph::refresh()
{
    auto& tap = controller.getProcessor().getEngine().getEqAnalyzer();
    const auto analyserOn = tap.getPoint() != dsp::AnalyzerTap::Point::off;
    if (analyserOn)
    {
        analyser.update (tap);
    }
    else if (analyserWasOn)
    {
        analyser.clear();
    }
    analyserWasOn = analyserOn;

    for (int b = 0; b < eq::numBands; ++b)
    {
        const auto band = controller.getBand (b);
        const auto& cached = cachedBands[(size_t) b];
        if (band.active != cached.active || band.bypassed != cached.bypassed || band.settings.type != cached.settings.type
            || ! juce::exactlyEqual (band.settings.frequency, cached.settings.frequency)
            || ! juce::exactlyEqual (band.settings.gainDb, cached.settings.gainDb)
            || ! juce::exactlyEqual (band.settings.q, cached.settings.q) || band.settings.slopeIndex != cached.settings.slopeIndex)
            curvesValid = false;
    }
    if (! juce::exactlyEqual (controller.getSampleRate(), cachedSampleRate))
        curvesValid = false;

    if (controller.getSelectedBand() >= 0 && ! controller.getBand (controller.getSelectedBand()).active)
        controller.setSelectedBand (-1);

    updatePopup(); // follows automation and changes from the editor
    repaint();
}

void EqGraph::updateCurves()
{
    const auto fs = controller.getSampleRate();
    const auto numPoints = getWidth() / curveStep + 2;
    totalDb.assign ((size_t) numPoints, 0.0f);

    std::vector<double> total ((size_t) numPoints, 1.0);
    for (int b = 0; b < eq::numBands; ++b)
    {
        auto& curve = bandDb[(size_t) b];
        const auto band = controller.getBand (b);
        cachedBands[(size_t) b] = band;
        if (! band.active)
        {
            curve.clear();
            continue;
        }
        const auto digital = dsp::eq::designBand (band.settings, fs);
        curve.assign ((size_t) numPoints, 0.0f);
        for (int i = 0; i < numPoints; ++i)
        {
            const auto hz = frequencyForX ((float) (i * curveStep));
            const auto magnitude = hz < 0.5 * fs ? std::abs (digital.response (hz, fs)) : std::abs (digital.response (0.4999 * fs, fs));
            curve[(size_t) i] = (float) (20.0 * std::log10 (std::max (magnitude, 1.0e-6)));
            if (! band.bypassed)
                total[(size_t) i] *= magnitude;
        }
    }
    for (int i = 0; i < numPoints; ++i)
        totalDb[(size_t) i] = (float) (20.0 * std::log10 (std::max (total[(size_t) i], 1.0e-6)));

    cachedSampleRate = fs;
    curvesValid = true;
}

void EqGraph::resized()
{
    curvesValid = false;
    updatePopup();
}

//==============================================================================
void EqGraph::paint (juce::Graphics& g)
{
    if (! curvesValid)
        updateCurves();

    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat();
    const auto radius = theme.metrics.radiusCard;
    const auto accent = theme.moduleColour (ModuleId::eq);

    g.setColour (c.window);
    g.fillRoundedRectangle (bounds, radius);

    {
        juce::Graphics::ScopedSaveState clip (g);
        juce::Path rounded;
        rounded.addRoundedRectangle (bounds.reduced (1.0f), radius - 1.0f);
        g.reduceClipRegion (rounded);

        // Grid: frequency lines and the gain scale.
        g.setColour (c.grid);
        for (const auto hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            g.fillRect (juce::Rectangle<float> (std::round (xForFrequency (hz)), 0.0f, 1.0f, bounds.getHeight()));
        const auto range = getRangeDb();
        const auto gridStep = range > 12.0 ? 10.0 : (range > 6.0 ? 6.0 : 3.0);
        for (double db = gridStep; db <= range + 0.01; db += gridStep)
            for (const auto sign : { 1.0, -1.0 })
                g.fillRect (juce::Rectangle<float> (0.0f, std::round (yForGain (sign * db)), bounds.getWidth(), 1.0f));
        g.setColour (c.border);
        g.fillRect (juce::Rectangle<float> (0.0f, std::round (yForGain (0.0)), bounds.getWidth(), 1.0f));

        // Solo (held or latched): the range heard, between two dashed lines; the rest dims.
        const auto soloed = controller.getSoloBand();
        const auto soloRange = soloRangeHz();
        const auto soloing = soloed >= 0 && ! soloRange.isEmpty();
        if (soloing)
        {
            const auto colour = theme.eqBandColour (soloed);
            const auto x0 = xForFrequency (soloRange.getStart());
            const auto x1 = xForFrequency (soloRange.getEnd());
            g.setColour (colour.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (x0, 0.0f, x1 - x0, bounds.getHeight()));
            g.setColour (colour.withAlpha (0.75f));
            const float dashes[] = { 4.0f, 4.0f };
            for (const auto x : { x0, x1 })
                if (x > 1.0f && x < bounds.getWidth() - 1.0f)
                    g.drawDashedLine (juce::Line<float> (x, 0.0f, x, bounds.getHeight()), dashes, 2, 1.0f);
        }

        // Spectrum.
        if (controller.getProcessor().getEqAnalyzer() != PMelodiesProcessor::EqAnalyzer::off)
        {
            juce::Path spectrum;
            const auto toY = [&] (float db)
            { return (float) juce::jmap ((double) db, analyserBottomDb, analyserTopDb, (double) bounds.getBottom(), (double) bounds.getY()); };
            spectrum.startNewSubPath (0.0f, bounds.getBottom());
            for (int x = 0; x <= getWidth(); x += curveStep)
                spectrum.lineTo ((float) x, juce::jlimit (bounds.getY(), bounds.getBottom(), toY (analyser.levelAt (frequencyForX ((float) x)))));
            spectrum.lineTo (bounds.getRight(), bounds.getBottom());
            spectrum.closeSubPath();
            g.setColour (accent.withAlpha (soloing ? 0.05f : 0.10f));
            g.fillPath (spectrum);
            g.setColour (accent.withAlpha (soloing ? 0.15f : 0.35f));
            g.strokePath (spectrum, juce::PathStrokeType (1.0f));
        }

        const auto curvePath = [&] (const std::vector<float>& db)
        {
            juce::Path p;
            for (size_t i = 0; i < db.size(); ++i)
            {
                const auto point = juce::Point<float> ((float) i * curveStep, yForGain (db[i]));
                if (i == 0)
                    p.startNewSubPath (point);
                else
                    p.lineTo (point);
            }
            return p;
        };

        // Selected (or soloed) band: its own contribution, filled from the 0 dB line, only where it acts.
        const auto selected = soloing ? soloed : controller.getSelectedBand();
        if (selected >= 0 && ! bandDb[(size_t) selected].empty())
        {
            const auto& db = bandDb[(size_t) selected];
            size_t first = db.size(), last = 0;
            for (size_t i = 0; i < db.size(); ++i)
                if (std::abs (db[i]) > 0.05f)
                {
                    first = std::min (first, i);
                    last = i;
                }
            if (first <= last)
            {
                first = first > 0 ? first - 1 : first;
                last = std::min (db.size() - 1, last + 1);
                juce::Path contribution;
                for (auto i = first; i <= last; ++i)
                {
                    const auto point = juce::Point<float> ((float) i * curveStep, yForGain (db[i]));
                    if (i == first)
                        contribution.startNewSubPath (point);
                    else
                        contribution.lineTo (point);
                }
                juce::Path area (contribution);
                area.lineTo ((float) last * curveStep, yForGain (0.0));
                area.lineTo ((float) first * curveStep, yForGain (0.0));
                area.closeSubPath();
                const auto colour = theme.eqBandColour (selected);
                g.setColour (colour.withAlpha (0.14f));
                g.fillPath (area);
                juce::Path dashed;
                const float dashes[] = { 3.0f, 3.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, contribution, dashes, 2);
                g.setColour (colour.withAlpha (0.8f));
                g.fillPath (dashed);
            }
        }

        // Overall curve.
        g.setColour (accent.withAlpha (soloing ? 0.3f : 1.0f));
        g.strokePath (curvePath (totalDb), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Scales.
        g.setFont (theme.mono (10.0f, Weight::regular));
        g.setColour (c.axisText);
        const std::array<std::pair<double, const char*>, 8> labels { std::pair { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" },
                                                                    { 500.0, "500" }, { 1000.0, "1k" }, { 2000.0, "2k" },
                                                                    { 5000.0, "5k" }, { 10000.0, "10k" } };
        for (const auto& [hz, name] : labels)
            g.drawText (name, juce::Rectangle<float> (xForFrequency (hz) - 20.0f, bounds.getBottom() - 16.0f, 40.0f, 12.0f),
                        juce::Justification::centred, false);
        for (double db = -range; db <= range + 0.01; db += gridStep)
        {
            const auto label = juce::exactlyEqual (db, 0.0) ? juce::String ("0")
                                                             : (db > 0.0 ? "+" + juce::String ((int) db) : format::unicodeMinus() + juce::String ((int) -db));
            g.drawText (label, juce::Rectangle<float> (bounds.getRight() - 40.0f, yForGain (db) - 13.0f, 34.0f, 12.0f),
                        juce::Justification::centredRight, false);
        }

        // Hint.
        g.setFont (theme.ui (11.0f));
        g.setColour (c.textMuted);
        g.drawText (text ("Doble clic: nueva banda \xc2\xb7 Rueda: Q (cortes: pendiente; Ctrl: Q) \xc2\xb7 Alt+clic: bypass"),
                    juce::Rectangle<float> (12.0f, 6.0f, bounds.getWidth() - 80.0f, 16.0f), juce::Justification::centredLeft, true);
    }

    // Points, selected last (on top). In solo the other bands fade.
    const auto selected = controller.getSelectedBand();
    const auto soloed = controller.getSoloBand();
    const auto drawPoint = [&] (int b)
    {
        const auto band = controller.getBand (b);
        const auto p = pointForBand (b);
        const auto number = juce::String (b + 1);
        const auto colour = theme.eqBandColour (b);
        const auto fade = soloed >= 0 && b != soloed ? 0.3f : 1.0f;
        const auto set = [&g, fade] (juce::Colour col) { g.setColour (col.withMultipliedAlpha (fade)); };
        g.setFont (theme.ui (10.0f, Weight::semiBold));
        if (b == selected)
        {
            set (colour.withAlpha (0.18f));
            g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (p));
            set (band.bypassed ? c.textDim : colour);
            g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (p));
            set (c.onAccent);
        }
        else
        {
            const auto circle = juce::Rectangle<float> (2.0f * pointRadius, 2.0f * pointRadius).withCentre (p);
            set (c.window);
            g.fillEllipse (circle);
            if (band.bypassed)
            {
                juce::Path ring, dashed;
                ring.addEllipse (circle);
                const float dashes[] = { 2.0f, 2.0f };
                juce::PathStrokeType (1.6f).createDashedStroke (dashed, ring, dashes, 2);
                set (c.textDim);
                g.fillPath (dashed);
                set (c.textMuted);
            }
            else
            {
                set (b == hovered ? colour.brighter (0.3f) : colour);
                g.drawEllipse (circle, 1.6f);
                set (c.text);
            }
        }
        g.drawText (number, juce::Rectangle<float> (20.0f, 14.0f).withCentre (p), juce::Justification::centred, false);
    };
    for (int b = 0; b < eq::numBands; ++b)
        if (b != selected && controller.getBand (b).active)
            drawPoint (b);
    if (selected >= 0)
    {
        drawPoint (selected); // its values are in the mini panel (EqBandPopup)
    }

    g.setColour (c.borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

//==============================================================================
void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    const auto band = bandAt (e.position);
    if (band != hovered)
    {
        hovered = band;
        repaint();
    }
    setMouseCursor (band >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void EqGraph::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    const auto band = bandAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (band >= 0)
        {
            controller.setSelectedBand (band);
            buildBandMenu (band).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                                                [safe = juce::Component::SafePointer<EqGraph> (this), band] (int result)
                                                {
                                                    if (safe != nullptr)
                                                        safe->handleMenuResult (band, result);
                                                });
        }
        return;
    }

    if (band < 0)
    {
        controller.setSelectedBand (-1); // a click on the background deselects (and closes the mini panel)
        updatePopup();
        repaint();
        return;
    }

    if (e.mods.isAltDown() && ! e.mods.isCtrlDown() && ! e.mods.isCommandDown())
    {
        controller.setSelectedBand (band);
        controller.setBypassed (band, ! controller.getBand (band).bypassed);
        bandsChanged();
        return;
    }

    beginDrag (band, e.position);
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    dragTo (e.position, e.mods.isCtrlDown() || e.mods.isCommandDown());
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    endDrag();
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown() || e.mods.isPopupMenu())
        return;
    if (const auto band = bandAt (e.position); band >= 0)
        showValueEditor (band);
    else
        createBandAt (e.position);
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheelDetails)
{
    auto band = bandAt (e.position);
    if (band < 0)
        band = controller.getSelectedBand();
    const auto delta = wheelDetails.isReversed ? -wheelDetails.deltaY : wheelDetails.deltaY;
    wheel (band, delta, e.mods.isCtrlDown() || e.mods.isCommandDown(), e.mods.isShiftDown());
}

bool EqGraph::keyPressed (const juce::KeyPress& key)
{
    // Only the keys the EQ uses: everything else goes on to FL Studio (the space bar, etc.).
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && controller.getSelectedBand() >= 0)
    {
        controller.deleteBand (controller.getSelectedBand());
        bandsChanged();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        // First the row of shapes, then the mini panel (deselecting the band).
        if (popup->isShapeRowOpen())
        {
            popup->setShapeRowOpen (false);
            return true;
        }
        if (controller.getSelectedBand() >= 0)
        {
            controller.setSelectedBand (-1);
            bandsChanged();
            return true;
        }
    }
    return false;
}

} // namespace pm::ui
