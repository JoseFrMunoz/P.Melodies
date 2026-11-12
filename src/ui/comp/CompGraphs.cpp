// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/comp/CompGraphs.h"

#include "core/ValueFormat.h"
#include "ui/theme/LookAndFeel.h"

namespace pm::ui
{

namespace
{
juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

/** Graph frame: background, border, and the clip for what is drawn inside. */
void drawFrame (juce::Graphics& g, const Theme& theme, juce::Rectangle<float> bounds)
{
    g.setColour (theme.colours.window);
    g.fillRoundedRectangle (bounds, theme.metrics.radiusCard);
    g.setColour (theme.colours.borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme.metrics.radiusCard, 1.0f);

    juce::Path clip;
    clip.addRoundedRectangle (bounds.reduced (1.0f), theme.metrics.radiusCard - 1.0f);
    g.reduceClipRegion (clip);
}

void drawDashedLine (juce::Graphics& g, juce::Line<float> line, float dash, float space, float thickness = 1.0f)
{
    const float pattern[] { dash, space };
    g.drawDashedLine (line, pattern, 2, thickness);
}
} // namespace

//==============================================================================
CompLevelGraph::CompLevelGraph()
{
    setTitle (text ("Gr\xc3\xa1" "fico de niveles"));
    setTooltip (text ("Niveles de los \xc3\xbaltimos 4 segundos\nZona oscura: entrada. Zona dorada: salida. "
                      "L\xc3\xadnea de arriba: reducci\xc3\xb3n de ganancia. L\xc3\xadnea discontinua: threshold."));
    setInterceptsMouseClicks (false, false);
}

void CompLevelGraph::push (const dsp::comp::MeterPoint* points, int count)
{
    for (int i = 0; i < count; ++i)
    {
        newest = (newest + 1) % numPoints;
        history[(size_t) newest] = points[i];
    }
    stored = juce::jmin (numPoints, stored + count);
    if (count > 0)
        repaint();
}

const dsp::comp::MeterPoint& CompLevelGraph::getPoint (int age) const noexcept
{
    const auto index = ((newest - juce::jlimit (0, numPoints - 1, age)) % numPoints + numPoints) % numPoints;
    return history[(size_t) index];
}

void CompLevelGraph::setThreshold (float db)
{
    if (! juce::exactlyEqual (db, threshold))
    {
        threshold = db;
        repaint();
    }
}

void CompLevelGraph::setSidechainMissing (bool missing)
{
    if (missing != sidechainMissing)
    {
        sidechainMissing = missing;
        repaint();
    }
}

void CompLevelGraph::setAccentColour (juce::Colour colour)
{
    accent = colour;
    repaint();
}

float CompLevelGraph::recentReductionDb (double seconds) const noexcept
{
    const auto count = juce::jmin (stored, juce::jmax (1, (int) (seconds / dsp::comp::CompressorMeter::pointSeconds)));
    auto deepest = 0.0f;
    for (int age = 0; age < count; ++age)
        deepest = juce::jmin (deepest, getPoint (age).gainDb);
    return deepest;
}

float CompLevelGraph::yForDb (float db) const noexcept
{
    const auto top = 1.0f;
    const auto bottom = (float) getHeight() - 1.0f;
    return top + (-juce::jlimit (floorDb, 0.0f, db) / -floorDb) * (bottom - top);
}

void CompLevelGraph::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = accent.isTransparent() ? c.accent : accent;

    juce::Graphics::ScopedSaveState state (g);
    drawFrame (g, theme, bounds);

    const auto area = bounds.reduced (1.0f);
    g.setColour (c.grid);
    for (const auto db : { -15.0f, -30.0f, -45.0f })
        g.drawHorizontalLine ((int) yForDb (db), area.getX(), area.getRight());

    // Newest point at the right edge.
    if (stored > 1)
    {
        const auto dx = area.getWidth() / (float) (numPoints - 1);
        const auto xForAge = [&] (int age) { return area.getRight() - (float) age * dx; };
        const auto oldest = stored - 1;

        juce::Path input, output, reduction;
        input.startNewSubPath (xForAge (oldest), area.getBottom());
        output.startNewSubPath (xForAge (oldest), area.getBottom());
        for (int age = oldest; age >= 0; --age)
        {
            const auto& p = getPoint (age);
            const auto x = xForAge (age);
            input.lineTo (x, yForDb (p.inputDb));
            output.lineTo (x, yForDb (p.outputDb));
            if (age == oldest)
                reduction.startNewSubPath (x, yForDb (p.gainDb));
            else
                reduction.lineTo (x, yForDb (p.gainDb));
        }
        input.lineTo (area.getRight(), area.getBottom());
        output.lineTo (area.getRight(), area.getBottom());
        input.closeSubPath();
        output.closeSubPath();

        g.setColour (c.border);
        g.fillPath (input);
        g.setColour (c.accent.withAlpha (0.34f));
        g.fillPath (output);
        g.setColour (colour);
        g.strokePath (reduction, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Threshold.
    const auto ty = yForDb (threshold);
    g.setColour (colour);
    drawDashedLine (g, { area.getX(), ty, area.getRight(), ty }, 4.0f, 4.0f);
    const auto label = format::compact (threshold, 1, true) + " dB";
    const auto font = theme.mono (10.0f, Weight::regular);
    g.setFont (font);
    const auto labelWidth = textWidth (font, label) + 4.0f;
    const auto labelY = ty > area.getY() + 16.0f ? ty - 15.0f : ty + 3.0f;
    g.drawText (label, juce::Rectangle<float> (area.getRight() - labelWidth - 38.0f, labelY, labelWidth, 13.0f),
                juce::Justification::centredLeft, false);

    // Legend.
    auto legend = juce::Rectangle<float> (area.getX() + 10.0f, area.getBottom() - 21.0f, area.getWidth() - 20.0f, 14.0f);
    g.setFont (theme.ui (11.0f));
    const auto item = [&] (juce::Colour swatch, const juce::String& name, bool line)
    {
        auto box = legend.removeFromLeft (10.0f);
        g.setColour (swatch);
        if (line)
            g.fillRect (box.withSizeKeepingCentre (10.0f, 2.0f));
        else
            g.fillRoundedRectangle (box.withSizeKeepingCentre (10.0f, 10.0f), 2.0f);
        legend.removeFromLeft (5.0f);
        g.setColour (c.textMuted);
        const auto w = textWidth (theme.ui (11.0f), name) + 1.0f;
        g.drawText (name, legend.removeFromLeft (w), juce::Justification::centredLeft, false);
        legend.removeFromLeft (12.0f);
    };
    item (c.border, "Entrada", false);
    item (c.accent, "Salida", false);
    item (colour, text ("Reducci\xc3\xb3n"), true);

    // External sidechain selected but silent: it does not compress (§12).
    if (sidechainMissing)
    {
        const auto box = area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 40.0f, 330.0f), 46.0f);
        g.setColour (c.panelRaised.withAlpha (0.94f));
        g.fillRoundedRectangle (box, theme.metrics.radiusControl);
        g.setColour (colour);
        g.drawRoundedRectangle (box, theme.metrics.radiusControl, 1.0f);
        g.setColour (c.text);
        g.setFont (theme.ui (12.0f, Weight::medium));
        g.drawText (text ("El sidechain externo no recibe se\xc3\xb1" "al"), box.withTrimmedBottom (20.0f).withTrimmedTop (6.0f),
                    juce::Justification::centred, false);
        g.setColour (c.textMuted);
        g.setFont (theme.ui (11.0f));
        g.drawText (text ("No comprime. Env\xc3\xad" "ale se\xc3\xb1" "al desde FL Studio o elige Interno."),
                    box.withTrimmedTop (24.0f).withTrimmedBottom (4.0f), juce::Justification::centred, false);
    }
}

//==============================================================================
CompCurveView::CompCurveView()
{
    setTitle ("Curva de transferencia");
    setTooltip (text ("Curva de transferencia\nNivel de entrada (horizontal) frente a nivel de salida (vertical), "
                      "con el knee. El punto marca el nivel que el compresor oye ahora."));
    setInterceptsMouseClicks (false, false);
}

void CompCurveView::setCurve (const dsp::comp::StaticCurve& newCurve)
{
    const auto same = juce::exactlyEqual (newCurve.thresholdDb(), curve.thresholdDb())
                      && juce::exactlyEqual (newCurve.kneeDb(), curve.kneeDb())
                      && juce::exactlyEqual (newCurve.rangeDb(), curve.rangeDb())
                      && juce::exactlyEqual (newCurve.gainDb (0.0), curve.gainDb (0.0))
                      && newCurve.isAutomatic() == curve.isAutomatic();
    curve = newCurve;
    if (! same)
        repaint();
}

void CompCurveView::setLevel (float db)
{
    if (std::abs (db - level) > 0.05f)
    {
        level = db;
        repaint();
    }
}

void CompCurveView::setAccentColour (juce::Colour colour)
{
    accent = colour;
    repaint();
}

juce::Point<float> CompCurveView::pointFor (float inputDb) const noexcept
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto x = juce::jlimit (floorDb, 0.0f, inputDb);
    const auto y = juce::jlimit (floorDb, 0.0f, (float) curve.outputDb (x));
    return { area.getX() + (x - floorDb) / -floorDb * area.getWidth(), area.getBottom() - (y - floorDb) / -floorDb * area.getHeight() };
}

juce::Path CompCurveView::makeCurvePath() const
{
    juce::Path path;
    path.startNewSubPath (pointFor (floorDb));
    for (auto db = floorDb + 0.25f; db <= 0.001f; db += 0.25f)
        path.lineTo (pointFor (db));
    return path;
}

juce::String CompCurveView::caption() const
{
    if (curve.isAutomatic())
        return text ("CURVA \xc2\xb7 AUTOM\xc3\x81TICA");
    return text ("CURVA \xc2\xb7 KNEE ") + format::compact (curve.kneeDb(), 1) + " DB";
}

void CompCurveView::paint (juce::Graphics& g)
{
    const auto& theme = themeOf (*this);
    const auto& c = theme.colours;
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = accent.isTransparent() ? c.accent : accent;

    juce::Graphics::ScopedSaveState state (g);
    drawFrame (g, theme, bounds);
    const auto area = bounds.reduced (1.0f);

    g.setColour (c.grid);
    for (const auto f : { 0.25f, 0.5f, 0.75f })
    {
        g.drawVerticalLine ((int) (area.getX() + f * area.getWidth()), area.getY(), area.getBottom());
        g.drawHorizontalLine ((int) (area.getY() + f * area.getHeight()), area.getX(), area.getRight());
    }

    // 1:1 reference and threshold.
    g.setColour (c.border);
    drawDashedLine (g, { area.getX(), area.getBottom(), area.getRight(), area.getY() }, 3.0f, 3.0f);
    const auto tx = area.getX() + ((float) curve.thresholdDb() - floorDb) / -floorDb * area.getWidth();
    g.setColour (colour.withAlpha (0.4f));
    drawDashedLine (g, { tx, area.getY(), tx, area.getBottom() }, 2.0f, 4.0f);

    const auto path = makeCurvePath();
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (level > floorDb)
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (pointFor (level)));

    g.setColour (c.textMuted);
    g.setFont (theme.label());
    g.drawText (caption(), juce::Rectangle<float> (area.getX() + 10.0f, area.getY() + 6.0f, area.getWidth() - 20.0f, 14.0f),
                juce::Justification::centredLeft, true);
}

} // namespace pm::ui
