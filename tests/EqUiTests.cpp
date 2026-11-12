// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 1 interface tests: the EQ display (geometry, creating bands by zone, dragging, wheel,
// Alt+click, delete, menu, typed values), the band editor, the header selectors, the chain
// row status and the spectrum analyser.

#include "TestHelpers.h"

#include "PluginEditor.h"
#include "core/ValueFormat.h"
#include "ui/eq/EqPanel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;
using dsp::eq::FilterType;
using param::EqField;

namespace
{
/** An editor at its real size, with the EQ page shown. */
struct EqUi
{
    EqUi()
    {
        prepareProcessor (processor, 48000.0, 512, 2);
        editor.reset (processor.createEditorAndMakeActive());
        page = &dynamic_cast<PMelodiesEditor&> (*editor).getModulePanel().getEqPanel();
    }

    ~EqUi() { processor.editorBeingDeleted (editor.get()); }

    ui::EqController& controller() { return page->getController(); }
    ui::EqGraph& graph() { return page->getGraph(); }
    ui::EqBandPanel& bandPanel() { return page->getBandPanel(); }

    PMelodiesProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    ui::EqPanel* page = nullptr;
};

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> position, juce::ModifierKeys mods)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position, mods, juce::MouseInputSource::defaultPressure,
             0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, position, now, 1, false };
}
} // namespace

TEST_CASE ("EQ display: frequency and gain scales", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    REQUIRE (g.getWidth() > 600);
    REQUIRE (g.getHeight() == ui::EqPanel::graphHeight);

    // 20 Hz to 20 kHz, logarithmic.
    CHECK (g.xForFrequency (20.0) == Catch::Approx (0.0f).margin (0.01));
    CHECK (g.xForFrequency (20000.0) == Catch::Approx ((float) g.getWidth()).margin (0.01));
    CHECK (g.xForFrequency (632.5) == Catch::Approx (g.getWidth() * 0.5f).margin (0.5)); // geometric middle
    for (const auto hz : { 31.0, 400.0, 7777.0 })
        CHECK (g.frequencyForX (g.xForFrequency (hz)) == Catch::Approx (hz).epsilon (1.0e-4));

    // ±12 dB between the design's margins by default; also ±6 and ±30.
    CHECK (ui.processor.getEqRangeDb() == 12);
    CHECK (g.yForGain (12.0) == Catch::Approx (17.0f));
    CHECK (g.yForGain (-12.0) == Catch::Approx (g.getHeight() - 17.0f));
    CHECK (g.yForGain (0.0) == Catch::Approx (g.getHeight() * 0.5f));
    ui.processor.setEqRangeDb (30);
    CHECK (g.yForGain (30.0) == Catch::Approx (17.0f));
    CHECK (g.gainForY (g.yForGain (-21.5)) == Catch::Approx (-21.5));
    ui.processor.setEqRangeDb (6);
    CHECK (g.yForGain (6.0) == Catch::Approx (17.0f));
    CHECK (g.yForGain (-6.0) == Catch::Approx (g.getHeight() - 17.0f));

    // Any other value snaps to the smallest scale that holds it.
    ui.processor.setEqRangeDb (7);
    CHECK (ui.processor.getEqRangeDb() == 12);
    ui.processor.setEqRangeDb (100);
    CHECK (ui.processor.getEqRangeDb() == 30);
}

TEST_CASE ("EQ display: a double click creates the shape of its zone, with the gain of its height", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto middle = g.getHeight() * 0.5f;

    struct Case
    {
        double hz;
        float y;
        FilterType expected;
    };
    const std::array<Case, 6> cases { Case { 22.0, middle, FilterType::lowCut },   Case { 50.0, g.yForGain (4.0), FilterType::lowShelf },
                                      Case { 1000.0, g.yForGain (6.0), FilterType::bell }, Case { 1000.0, g.getHeight() - 10.0f, FilterType::notch },
                                      Case { 12000.0, g.yForGain (-3.0), FilterType::highShelf }, Case { 18000.0, middle, FilterType::highCut } };

    for (size_t i = 0; i < cases.size(); ++i)
    {
        const auto& k = cases[i];
        const auto band = g.createBandAt ({ g.xForFrequency (k.hz), k.y });
        INFO (k.hz << " Hz");
        REQUIRE (band == (int) i); // first free slot
        const auto b = c.getBand (band);
        CHECK (b.active);
        CHECK (b.settings.type == k.expected);
        CHECK (b.settings.frequency == Catch::Approx (k.hz).epsilon (0.002));
        CHECK (b.settings.q == Catch::Approx (1.0));
        CHECK (dsp::eq::slopesDbPerOctave[(size_t) b.settings.slopeIndex] == 12);
        CHECK (c.getSelectedBand() == band);
        if (dsp::eq::usesGain (k.expected))
            CHECK (b.settings.gainDb == Catch::Approx (g.gainForY (k.y)).margin (0.06));
        else
            CHECK (b.settings.gainDb == 0.0);
    }

    // Eight bands at most.
    CHECK (g.createBandAt ({ g.xForFrequency (300.0), middle }) == 6);
    CHECK (g.createBandAt ({ g.xForFrequency (500.0), middle }) == 7);
    CHECK (g.createBandAt ({ g.xForFrequency (700.0), middle }) == -1);
    CHECK (c.getNumActiveBands() == 8);
    CHECK (ui.page->subtitle() == juce::String (juce::CharPointer_UTF8 ("8 de 8 bandas \xc2\xb7 latencia cero")));

    // Deleting does not renumber the others (§12).
    c.deleteBand (2);
    CHECK_FALSE (c.getBand (2).active);
    CHECK (c.getBand (3).settings.type == FilterType::notch);
    CHECK (g.createBandAt ({ g.xForFrequency (2000.0), middle }) == 2); // the free slot is reused
}

TEST_CASE ("EQ display: dragging a point moves frequency and gain (or Q), Ctrl is finer", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto band = c.createBand (FilterType::bell, 1000.0, 0.0);
    const auto start = g.pointForBand (band);
    REQUIRE (g.bandAt (start) == band);

    // One octave right and 6 dB up.
    const auto octave = g.xForFrequency (2000.0) - g.xForFrequency (1000.0);
    const auto sixDb = g.yForGain (6.0) - g.yForGain (0.0);
    g.beginDrag (band, start);
    g.dragTo (start + juce::Point<float> (octave, sixDb), false);
    g.endDrag();
    CHECK (c.getBand (band).settings.frequency == Catch::Approx (2000.0).epsilon (0.002));
    CHECK (c.getBand (band).settings.gainDb == Catch::Approx (6.0).margin (0.06));

    // With Ctrl the same movement is ten times smaller.
    const auto from = g.pointForBand (band);
    g.beginDrag (band, from);
    g.dragTo (from + juce::Point<float> (octave, sixDb), true);
    g.endDrag();
    CHECK (c.getBand (band).settings.frequency == Catch::Approx (2000.0 * std::pow (2.0, 0.1)).epsilon (0.003));
    CHECK (c.getBand (band).settings.gainDb == Catch::Approx (6.6).margin (0.06));

    // Beyond ±12 dB the scale widens by itself.
    CHECK (ui.processor.getEqRangeDb() == 12);
    g.beginDrag (band, g.pointForBand (band));
    g.dragTo (g.pointForBand (band) + juce::Point<float> (0.0f, 2.0f * sixDb), false);
    g.endDrag();
    CHECK (c.getBand (band).settings.gainDb > 12.0);
    CHECK (ui.processor.getEqRangeDb() == 30);

    // From ±6, a band dragged past 6 dB takes it to ±12, not straight to ±30; going back
    // inside does not shrink it.
    c.setValue (band, EqField::gain, 4.0f);
    ui.processor.setEqRangeDb (6);
    const auto oneDb = (g.yForGain (0.0) - g.yForGain (1.0));
    g.beginDrag (band, g.pointForBand (band));
    g.dragTo (g.pointForBand (band) - juce::Point<float> (0.0f, 3.0f * oneDb), false);
    CHECK (c.getBand (band).settings.gainDb == Catch::Approx (7.0).margin (0.1));
    CHECK (ui.processor.getEqRangeDb() == 12);
    g.endDrag();
    const auto at = g.pointForBand (band); // now on the ±12 scale
    g.beginDrag (band, at);
    g.dragTo (at + juce::Point<float> (0.0f, g.yForGain (0.0) - g.yForGain (4.0)), false);
    g.endDrag();
    CHECK (c.getBand (band).settings.gainDb == Catch::Approx (3.0).margin (0.1));
    CHECK (ui.processor.getEqRangeDb() == 12);
    ui.processor.setEqRangeDb (12);

    // Shapes without gain: the vertical drag sets the Q (up = narrower).
    const auto cut = c.createBand (FilterType::lowCut, 100.0, 0.0);
    const auto q0 = c.getBand (cut).settings.q;
    g.beginDrag (cut, g.pointForBand (cut));
    g.dragTo (g.pointForBand (cut) + juce::Point<float> (0.0f, -40.0f), false);
    g.endDrag();
    CHECK (c.getBand (cut).settings.q == Catch::Approx (2.0 * q0).epsilon (0.01));
    CHECK (c.getBand (cut).settings.gainDb == 0.0);
}

TEST_CASE ("EQ display: wheel sets the Q, Alt+click bypasses, Delete removes, menu and typed values", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto band = c.createBand (FilterType::bell, 400.0, -4.0);

    // Wheel: Q up or down by steps, finer with Shift; Ctrl+wheel is the Q as well.
    g.wheel (band, 0.2f, false, false);
    CHECK (c.getBand (band).settings.q == Catch::Approx (1.15).epsilon (0.002));
    g.wheel (band, -0.2f, false, false);
    g.wheel (band, -0.2f, false, true);
    CHECK (c.getBand (band).settings.q == Catch::Approx (1.0 / 1.02).epsilon (0.002));
    g.wheel (band, 0.2f, true, false);
    CHECK (c.getBand (band).settings.q == Catch::Approx (1.15 / 1.02).epsilon (0.002));
    g.wheel (band, -0.2f, true, false);
    CHECK (c.getBand (band).settings.q == Catch::Approx (1.0 / 1.02).epsilon (0.002));

    // Alt+click on the point toggles the bypass (through the real mouse handler).
    const auto point = g.pointForBand (band);
    g.mouseDown (mouseEvent (g, point, juce::ModifierKeys (juce::ModifierKeys::altModifier | juce::ModifierKeys::leftButtonModifier)));
    CHECK (c.getBand (band).bypassed);
    g.mouseDown (mouseEvent (g, point, juce::ModifierKeys (juce::ModifierKeys::altModifier | juce::ModifierKeys::leftButtonModifier)));
    CHECK_FALSE (c.getBand (band).bypassed);

    // Floating label.
    CHECK (g.labelText (band) == juce::String (juce::CharPointer_UTF8 ("400 Hz \xc2\xb7 \xe2\x88\x92" "4,0 dB")));

    // Menu: shape, bypass, delete.
    CHECK (g.buildBandMenu (band).getNumItems() >= 3);
    g.handleMenuResult (band, ui::EqGraph::menuShapeBase + (int) FilterType::notch);
    CHECK (c.getBand (band).settings.type == FilterType::notch);
    CHECK (g.labelText (band) == juce::String (juce::CharPointer_UTF8 ("400 Hz \xc2\xb7 Q 0,98")));
    g.handleMenuResult (band, ui::EqGraph::menuBypass);
    CHECK (c.getBand (band).bypassed);
    g.handleMenuResult (band, ui::EqGraph::menuShapeBase + (int) FilterType::bell);

    // Typed values ("2,5k" and Spanish decimals).
    CHECK (g.applyTypedValues (band, "2,5k", "-3,5", "4"));
    CHECK (c.getBand (band).settings.frequency == Catch::Approx (2500.0));
    CHECK (c.getBand (band).settings.gainDb == Catch::Approx (-3.5));
    CHECK (c.getBand (band).settings.q == Catch::Approx (4.0));
    CHECK_FALSE (g.applyTypedValues (band, "", "", ""));

    // Delete removes the selected band; other keys go on to the host (the space bar).
    c.setSelectedBand (band);
    CHECK_FALSE (g.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (g.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK_FALSE (c.getBand (band).active);
    CHECK (c.getSelectedBand() == -1);
    CHECK_FALSE (g.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));

    // A click on the background deselects.
    const auto other = c.createBand (FilterType::bell, 100.0, 3.0);
    CHECK (c.getSelectedBand() == other);
    g.mouseDown (mouseEvent (g, { g.xForFrequency (5000.0), 40.0f }, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
    CHECK (c.getSelectedBand() == -1);
}

TEST_CASE ("EQ display: in the cuts the wheel walks the slopes and Ctrl+wheel sets the Q", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto slopeOf = [&] (int b) { return dsp::eq::slopesDbPerOctave[(size_t) c.getBand (b).settings.slopeIndex]; };

    const auto cut = c.createBand (FilterType::lowCut, 80.0, 0.0);
    REQUIRE (slopeOf (cut) == 12);

    // One notch of a mouse wheel is one step: 12 → 18 → 24 dB/oct. The Q does not move.
    constexpr float notch = 0.234f; // what JUCE reports for one notch on Windows
    g.wheel (cut, notch, false, false);
    CHECK (slopeOf (cut) == 18);
    g.wheel (cut, notch, false, false);
    CHECK (slopeOf (cut) == 24);
    CHECK (c.getBand (cut).settings.q == Catch::Approx (1.0));

    // Small trackpad movements add up to one step.
    for (int i = 0; i < 3; ++i)
        g.wheel (cut, 0.05f, false, false);
    CHECK (slopeOf (cut) == 24);
    g.wheel (cut, 0.05f, false, false);
    CHECK (slopeOf (cut) == 36);

    // Down to 6 and up to 96, never beyond.
    for (int i = 0; i < 12; ++i)
        g.wheel (cut, -notch, false, false);
    CHECK (slopeOf (cut) == 6);
    for (int i = 0; i < 12; ++i)
        g.wheel (cut, notch, false, false);
    CHECK (slopeOf (cut) == 96);

    // Ctrl+wheel: the Q (resonance); the slope stays.
    g.wheel (cut, 0.2f, true, false);
    CHECK (c.getBand (cut).settings.q == Catch::Approx (1.15).epsilon (0.002));
    CHECK (slopeOf (cut) == 96);

    // A 6 dB/oct cut has no Q: Ctrl+wheel leaves it alone.
    c.setValue (cut, EqField::slope, 0.0f);
    const auto q6 = c.getBand (cut).settings.q;
    g.wheel (cut, 0.2f, true, false);
    CHECK (c.getBand (cut).settings.q == Catch::Approx (q6));

    // Through the real mouse handler, on a high cut: plain wheel = slope, Ctrl = Q.
    const auto high = c.createBand (FilterType::highCut, 12000.0, 0.0);
    juce::MouseWheelDetails details {};
    details.deltaY = notch;
    const auto at = g.pointForBand (high);
    g.mouseWheelMove (mouseEvent (g, at, juce::ModifierKeys()), details);
    CHECK (slopeOf (high) == 18);
    g.mouseWheelMove (mouseEvent (g, at, juce::ModifierKeys (juce::ModifierKeys::ctrlModifier)), details);
    CHECK (slopeOf (high) == 18);
    CHECK (c.getBand (high).settings.q == Catch::Approx (1.15).epsilon (0.002));
}

TEST_CASE ("EQ display: projects saved with the two-way scale keep it", "[ui][eq][state][phase1]")
{
    // Earlier phase 1 builds saved a flag: true for ±30, false for ±12.
    for (const auto& [flag, expected] : { std::pair { true, 30 }, std::pair { false, 12 } })
    {
        PMelodiesProcessor original;
        juce::MemoryBlock saved;
        original.getStateInformation (saved);
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        REQUIRE (xml != nullptr);
        xml->setAttribute (PMelodiesProcessor::eqRangeProperty.toString(), flag ? "1" : "0");
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary (*xml, old);

        PMelodiesProcessor restored;
        restored.setStateInformation (old.getData(), (int) old.getSize());
        CHECK (restored.getEqRangeDb() == expected);
    }
}

TEST_CASE ("EQ: every band has its own colour, close to the EQ gold", "[ui][eq][theme][phase1]")
{
    const auto& theme = ui::Theme::dark();
    const auto gold = theme.moduleColour (ModuleId::eq);
    const auto luminance = [] (juce::Colour c)
    {
        const auto channel = [] (juce::uint8 v)
        {
            const auto x = v / 255.0;
            return x <= 0.03928 ? x / 12.92 : std::pow ((x + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * channel (c.getRed()) + 0.7152 * channel (c.getGreen()) + 0.0722 * channel (c.getBlue());
    };
    const auto distance = [] (juce::Colour a, juce::Colour b)
    { return std::abs (a.getRed() - b.getRed()) + std::abs (a.getGreen() - b.getGreen()) + std::abs (a.getBlue() - b.getBlue()); };

    for (int b = 0; b < eq::numBands; ++b)
    {
        INFO ("band " << b + 1);
        const auto colour = theme.eqBandColour (b);
        // Readable as a graphic on the display background (WCAG 3:1).
        CHECK ((luminance (colour) + 0.05) / (luminance (theme.colours.window) + 0.05) >= 3.0);
        // A family around the gold: from amber to olive green, within a quarter turn of its hue.
        auto hueDifference = std::abs (colour.getHue() - gold.getHue());
        hueDifference = std::min (hueDifference, 1.0f - hueDifference);
        CHECK (hueDifference < 0.25f);
        // And each one differs from its neighbour.
        if (b > 0)
            CHECK (distance (colour, theme.eqBandColour (b - 1)) >= 15);
    }

    // On screen: the selected point is filled with its band's colour, and the editor uses it too.
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto first = c.createBand (FilterType::bell, 200.0, 3.0);
    const auto second = c.createBand (FilterType::bell, 3000.0, -3.0);
    for (const auto band : { first, second })
    {
        INFO ("band " << band + 1);
        c.setSelectedBand (band);
        ui.page->refresh();
        const auto image = g.createComponentSnapshot (g.getLocalBounds(), true, 1.0f);
        const auto p = g.pointForBand (band).toInt();
        const auto pixel = image.getPixelAt (p.x - 7, p.y); // inside the filled circle, left of the number
        CHECK (distance (pixel, theme.eqBandColour (band)) < 12);
        CHECK (ui.bandPanel().getKnob (EqField::frequency)->getAccentColour() == theme.eqBandColour (band));
        CHECK (ui.bandPanel().getShapeSelector().getAccentColour() == theme.eqBandColour (band));
    }
}

TEST_CASE ("Notes are named and typed as in FL Studio (A5 = 440 Hz)", "[format][eq][phase1]")
{
    const auto minus = format::unicodeMinus();
    const auto hzOf = [] (int midi, int cents = 0) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0 + cents / 1200.0); };

    CHECK (format::note (440.0) == "A5 +00");
    CHECK (format::note (hzOf (60)) == "C5 +00"); // middle C, as in FL Studio's piano roll
    CHECK (format::note (400.0) == "G5 +35");
    CHECK (format::note (130.0) == "C4 " + minus + "11");
    CHECK (format::note (27.5) == "A1 +00");
    CHECK (format::note (0.0).isEmpty());

    CHECK (format::parseNote ("A5") == Catch::Approx (440.0));
    CHECK (format::parseNote ("a5") == Catch::Approx (440.0));
    CHECK (format::parseNote ("C#3+13") == Catch::Approx (hzOf (37, 13)));
    CHECK (format::parseNote ("Bb4") == Catch::Approx (hzOf (58)));
    CHECK (format::parseNote ("A5 " + minus + "20") == Catch::Approx (hzOf (69, -20)));
    CHECK (format::parseNote ("C0") == Catch::Approx (hzOf (0)));
    for (const auto* notANote : { "2k", "400", "H5", "A", "A5x", "A5+", "#5", "" })
    {
        INFO (notANote);
        CHECK (format::parseNote (notANote) == 0.0);
    }

    // Every note name reads back to its frequency.
    for (int midi = 0; midi < 136; ++midi)
    {
        const auto name = format::note (hzOf (midi)).upToFirstOccurrenceOf (" ", false, false);
        INFO (name);
        CHECK (format::parseNote (name) == Catch::Approx (hzOf (midi)));
    }

    // The band's value editor accepts notes as well as numbers.
    CHECK (format::parseFrequency ("A5") == Catch::Approx (440.0));
    CHECK (format::parseFrequency ("2,5k") == Catch::Approx (2500.0));
    CHECK (format::parseFrequency ("400 Hz") == Catch::Approx (400.0));
    EqUi ui;
    const auto band = ui.controller().createBand (FilterType::bell, 1000.0, 0.0);
    CHECK (ui.graph().applyTypedValues (band, "E2+50", "", ""));
    CHECK (ui.controller().getBand (band).settings.frequency == Catch::Approx (hzOf (28, 50)).epsilon (2.0e-4));
}

TEST_CASE ("EQ mini panel: shows while the pointer is on the point or the panel, and while dragging", "[ui][eq][popup][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    auto& popup = g.getPopup();
    const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
    const auto hover = [&] (juce::Point<float> at) { g.mouseMove (mouseEvent (g, at, juce::ModifierKeys())); };
    const auto empty = juce::Point<float> (g.xForFrequency (5000.0), g.yForGain (-9.0));

    const auto band = c.createBand (FilterType::bell, 400.0, -4.0); // creating selects it
    REQUIRE (c.getSelectedBand() == band);
    // Selecting is not enough (P.P., 2026-11-12): the panel waits for the pointer.
    CHECK_FALSE (popup.isVisible());

    auto p = g.pointForBand (band);
    hover (p + juce::Point<float> (3.0f, 2.0f));
    REQUIRE (popup.isVisible());
    CHECK (popup.getBand() == band);
    auto card = popup.getCardBoundsInParent();
    CHECK (popup.isAbove());
    CHECK ((float) card.getBottom() < p.y - 10.0f);
    CHECK (std::abs ((float) card.getCentreX() - p.x) <= 1.0f);
    CHECK (g.getLocalBounds().contains (card));

    const auto values = popup.getValueTexts();
    CHECK (values[0] == "400 Hz");
    CHECK (values[1] == "G5 +35");
    CHECK (values[2] == format::unicodeMinus() + "4,0 dB");
    CHECK (values[3] == "Q 1,00");

    // Away from the point it goes.
    hover (empty);
    CHECK_FALSE (popup.isVisible());

    // On the way up to the panel it stays (the corridor), and on the panel too.
    hover (p);
    REQUIRE (popup.isVisible());
    const auto wayUp = juce::Point<float> (p.x + 30.0f, popup.getCorridor().getY() + 3.0f);
    REQUIRE (g.bandAt (wayUp) < 0);
    hover (wayUp);
    CHECK (popup.isVisible());
    const auto inCard = popup.getCardBoundsInParent().getCentre().toFloat();
    g.mouseExit (mouseEvent (g, inCard, juce::ModifierKeys())); // the display hands the pointer to the panel
    CHECK (popup.isHovered());
    CHECK (popup.isVisible());
    popup.setHovered (false, inCard); // from the card onto one of its buttons: still on the panel
    CHECK (popup.isHovered());
    CHECK (popup.isVisible());

    // Leaving it for the empty display, or leaving the display, closes it.
    popup.setHovered (false, empty);
    CHECK_FALSE (popup.isVisible());
    hover (p);
    CHECK (popup.isVisible());
    g.mouseExit (mouseEvent (g, { -5.0f, p.y }, juce::ModifierKeys()));
    CHECK_FALSE (popup.isVisible());

    // While a point is held or dragged it shows, even if the pointer falls behind (Ctrl: fine).
    g.mouseDown (mouseEvent (g, p, left));
    CHECK (popup.isVisible());
    const auto far = p + juce::Point<float> (200.0f, 0.0f);
    g.mouseDrag (mouseEvent (g, far, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier)));
    CHECK (popup.isVisible());
    CHECK (std::abs ((float) popup.getCardBoundsInParent().getCentreX() - g.pointForBand (band).x) <= 1.0f);
    g.mouseUp (mouseEvent (g, far, juce::ModifierKeys()));
    CHECK_FALSE (popup.isVisible()); // released away from the point
    p = g.pointForBand (band);
    g.mouseDown (mouseEvent (g, p, left));
    g.mouseUp (mouseEvent (g, p, juce::ModifierKeys()));
    CHECK (popup.isVisible()); // released on it

    // While shown it follows changes from outside (automation).
    popup.setHovered (true, popup.getCardBoundsInParent().getCentre().toFloat());
    c.setValue (band, EqField::frequency, 2000.0f);
    g.refresh();
    CHECK (popup.getValueTexts()[0] == "2,00 kHz");
    CHECK (std::abs ((float) popup.getCardBoundsInParent().getCentreX() - g.pointForBand (band).x) <= 1.0f);

    // Near the top there is no room: it goes below the point, sliding there.
    c.setValue (band, EqField::gain, 11.5f);
    g.refresh();
    CHECK_FALSE (popup.isAbove());
    CHECK (popup.isAnimating());
    for (int frame = 0; frame < 60 && popup.isAnimating(); ++frame)
        popup.advanceAnimation (1.0 / 60.0);
    CHECK_FALSE (popup.isAnimating());
    p = g.pointForBand (band);
    CHECK ((float) popup.getCardBoundsInParent().getY() > p.y + 10.0f);

    // At the edges it stays inside the display.
    for (const auto hz : { 25.0f, 19000.0f })
    {
        c.setValue (band, EqField::frequency, hz);
        g.refresh();
        CHECK (g.getLocalBounds().contains (popup.getCardBoundsInParent()));
    }
    popup.setHovered (false, empty);

    // Cuts show their slope with the wheel hint; a 6 dB/oct cut has no Q.
    const auto cut = c.createBand (FilterType::lowCut, 60.0, 0.0);
    hover (g.pointForBand (cut));
    REQUIRE (popup.getBand() == cut);
    CHECK (popup.getValueTexts()[2] == "12 dB/oct");
    CHECK (popup.getValueTexts()[3] == "Q 1,00");
    c.setValue (cut, EqField::slope, 0.0f);
    g.refresh();
    CHECK (popup.getValueTexts()[3] == juce::String (juce::CharPointer_UTF8 ("Q \xe2\x80\x94")));

    // A click on the background deselects; the panel is gone since the pointer left the point.
    g.mouseDown (mouseEvent (g, empty, left));
    CHECK_FALSE (popup.isVisible());
    CHECK (c.getSelectedBand() == -1);
}

TEST_CASE ("EQ mini panel: bypass, shape row, solo while held, delete and Esc", "[ui][eq][popup][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    auto& popup = g.getPopup();
    const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
    const auto band = c.createBand (FilterType::bell, 1000.0, 3.0);
    g.mouseMove (mouseEvent (g, g.pointForBand (band), juce::ModifierKeys()));
    REQUIRE (popup.isVisible());
    popup.setHovered (true, popup.getCardBoundsInParent().getCentre().toFloat()); // the pointer is on the panel

    // No control of the panel takes the keyboard: the space bar keeps reaching FL Studio.
    for (auto* comp : { (juce::Component*) &popup.getBypassButton(), (juce::Component*) &popup.getShapeButton(),
                        &popup.getSoloButton(), (juce::Component*) &popup.getDeleteButton(), (juce::Component*) &popup.getShapeRow() })
        CHECK_FALSE (comp->getWantsKeyboardFocus());

    // Bypass, both ways.
    popup.getBypassButton().onClick();
    CHECK (c.getBand (band).bypassed);
    popup.getBypassButton().onClick();
    CHECK_FALSE (c.getBand (band).bypassed);

    // Shape: the row opens with the current shape lit; a click changes it and closes the row.
    popup.getShapeButton().onClick();
    REQUIRE (popup.isShapeRowOpen());
    auto& row = popup.getShapeRow();
    CHECK (row.isVisible());
    CHECK (row.getSelected() == (int) FilterType::bell);
    const auto notchCentre = juce::Point<float> (2.0f + 5.0f * (ui::ShapeSelector::buttonSize + 2.0f) + 14.0f, 16.0f);
    row.mouseDown (mouseEvent (row, notchCentre, left));
    CHECK (c.getBand (band).settings.type == FilterType::notch);
    CHECK_FALSE (popup.isShapeRowOpen());

    // Esc: first the row, then the selection; with nothing left it goes on to the host.
    popup.getShapeButton().onClick();
    CHECK (g.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (popup.isShapeRowOpen());
    CHECK (popup.isVisible());
    CHECK (g.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (c.getSelectedBand() == -1);
    CHECK (popup.isVisible()); // still under the pointer
    CHECK_FALSE (g.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (g.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));

    // Solo while the headphones are held; on release, back to what was soloed before.
    auto& solo = popup.getSoloButton();
    const auto press = [&] { solo.mouseDown (mouseEvent (solo, { 12.0f, 12.0f }, left)); };
    const auto release = [&] { solo.mouseUp (mouseEvent (solo, { 12.0f, 12.0f }, juce::ModifierKeys())); };
    press();
    CHECK (c.getSoloBand() == band);
    CHECK (g.isSoloHeld());
    CHECK (ui.processor.getEqSoloBand() == band);
    release();
    CHECK (c.getSoloBand() == -1);
    CHECK_FALSE (g.isSoloHeld());

    const auto other = c.createBand (FilterType::bell, 200.0, 2.0);
    c.setSoloBand (other); // latched with the editor's Solo button
    REQUIRE (popup.getBand() == band);
    press();
    CHECK (c.getSoloBand() == band);
    release();
    CHECK (c.getSoloBand() == other);
    c.setSoloBand (band);
    press();
    release();
    CHECK (c.getSoloBand() == band); // it was latched on this band: it stays
    c.setSoloBand (-1);

    // Delete.
    popup.getDeleteButton().onClick();
    CHECK_FALSE (c.getBand (band).active);
    CHECK_FALSE (popup.isVisible());
}

TEST_CASE ("EQ solo: dragging the held headphones moves the band, Ctrl sets the Q, the heard range is drawn", "[ui][eq][popup][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
    const auto band = c.createBand (FilterType::bell, 1000.0, 4.0);
    const auto settings = [&] { return c.getBand (band).settings; };

    // Automation records one gesture per held solo.
    struct GestureCounter final : juce::AudioProcessorParameter::Listener
    {
        void parameterValueChanged (int, float) override {}
        void parameterGestureChanged (int, bool starting) override { ++(starting ? begins : ends); }
        int begins = 0, ends = 0;
    } gestures;
    auto& frequency = c.parameter (band, EqField::frequency);
    frequency.addListener (&gestures);

    CHECK (g.soloRangeHz().isEmpty());
    g.beginSolo (band);
    CHECK (c.getSoloBand() == band);
    // What is heard: the band-pass edges 3 dB down around 1 kHz with Q 1.
    CHECK (g.soloRangeHz().getStart() == Catch::Approx (618.0).epsilon (0.002));
    CHECK (g.soloRangeHz().getEnd() == Catch::Approx (1618.0).epsilon (0.002));

    // Sideways: the frequency (one octave of screen = one octave).
    const auto octave = g.xForFrequency (2000.0) - g.xForFrequency (1000.0);
    g.soloDragTo ({ octave, 0.0f }, false);
    CHECK (settings().frequency == Catch::Approx (2000.0).epsilon (0.003));
    CHECK (g.soloRangeHz().getStart() == Catch::Approx (1236.0).epsilon (0.004));

    // The vertical movement alone changes nothing.
    g.soloDragTo ({ octave, -60.0f }, false);
    CHECK (settings().q == Catch::Approx (1.0));
    CHECK (settings().gainDb == Catch::Approx (4.0));

    // With Ctrl, 40 px up doubles the Q (narrower).
    g.soloDragTo ({ octave, -100.0f }, true);
    CHECK (settings().q == Catch::Approx (2.0).epsilon (0.01));

    // Back to where it started.
    g.soloDragTo ({ 0.0f, -100.0f }, false);
    CHECK (settings().frequency == Catch::Approx (1000.0).epsilon (0.003));
    CHECK (std::abs ((float) g.getPopup().getCardBoundsInParent().getCentreX() - g.pointForBand (band).x) <= 1.0f);

    g.endSolo();
    CHECK (c.getSoloBand() == -1);
    CHECK (g.soloRangeHz().isEmpty());
    CHECK (gestures.begins == 1);
    CHECK (gestures.ends == 1);
    frequency.removeListener (&gestures);

    // Shelves and cuts: everything below or above the frequency. The latched solo is drawn too.
    const auto cut = c.createBand (FilterType::lowCut, 100.0, 0.0);
    c.setSoloBand (cut);
    CHECK (g.soloRangeHz().getStart() == Catch::Approx (ui::EqGraph::minDisplayHz));
    CHECK (g.soloRangeHz().getEnd() == Catch::Approx (100.0).epsilon (0.003));
    const auto shelf = c.createBand (FilterType::highShelf, 8000.0, 3.0);
    c.setSoloBand (shelf);
    CHECK (g.soloRangeHz().getStart() == Catch::Approx (8000.0).epsilon (0.003));
    CHECK (g.soloRangeHz().getEnd() == Catch::Approx (ui::EqGraph::maxDisplayHz));
    c.setSoloBand (-1);

    // Through the panel's headphones (the pointer on the point shows it): press, drag sideways, release.
    g.mouseMove (mouseEvent (g, g.pointForBand (band), juce::ModifierKeys()));
    REQUIRE (g.getPopup().getBand() == band);
    auto& headphones = g.getPopup().getSoloButton();
    const auto start = juce::Point<float> (12.0f, 12.0f);
    headphones.mouseDown (mouseEvent (headphones, start, left));
    CHECK (c.getSoloBand() == band);
    headphones.mouseDrag (mouseEvent (headphones, start + juce::Point<float> (octave, 0.0f), left));
    CHECK (settings().frequency == Catch::Approx (2000.0).epsilon (0.003));
    headphones.mouseUp (mouseEvent (headphones, start + juce::Point<float> (octave, 0.0f), juce::ModifierKeys()));
    CHECK (c.getSoloBand() == -1);
}

TEST_CASE ("EQ display: over the curve a ghost point shows the shape; dragging the curve creates the band", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& g = ui.graph();
    auto& c = ui.controller();
    const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
    const auto on = [&] (double hz, double db) { return juce::Point<float> (g.xForFrequency (hz), g.yForGain (db)); };

    // No bands: the curve is the 0 dB line. The shape follows the zone, as with the double click.
    for (const auto& [hz, type] : { std::pair { 25.0, FilterType::lowCut }, std::pair { 50.0, FilterType::lowShelf },
                                    std::pair { 1000.0, FilterType::bell }, std::pair { 12000.0, FilterType::highShelf },
                                    std::pair { 18000.0, FilterType::highCut } })
    {
        INFO (hz << " Hz");
        g.mouseMove (mouseEvent (g, on (hz, 0.0) + juce::Point<float> (0.0f, 5.0f), juce::ModifierKeys()));
        REQUIRE (g.getLinePreview().has_value());
        CHECK (g.getLinePreview()->type == type);
        CHECK (g.getLinePreview()->point.y == Catch::Approx (g.yForGain (0.0)).margin (0.5));
    }

    // Away from the curve, nothing; leaving the display hides it.
    g.mouseMove (mouseEvent (g, on (1000.0, 0.0) + juce::Point<float> (0.0f, ui::EqGraph::lineGrabDistance + 4.0f), juce::ModifierKeys()));
    CHECK_FALSE (g.getLinePreview().has_value());
    g.mouseMove (mouseEvent (g, on (1000.0, 0.0), juce::ModifierKeys()));
    CHECK (g.getLinePreview().has_value());
    g.mouseExit (mouseEvent (g, on (1000.0, 0.0), juce::ModifierKeys()));
    CHECK_FALSE (g.getLinePreview().has_value());

    // A click on the curve (no drag) creates nothing.
    g.mouseDown (mouseEvent (g, on (1000.0, 0.0), left));
    g.mouseUp (mouseEvent (g, on (1000.0, 0.0), juce::ModifierKeys()));
    CHECK (c.getNumActiveBands() == 0);

    // Dragging it up creates a bell there and raises it, as one drag.
    g.mouseDown (mouseEvent (g, on (1000.0, 0.0), left));
    g.mouseDrag (mouseEvent (g, on (1000.0, 0.0) + juce::Point<float> (0.0f, -2.0f), left)); // under the threshold
    CHECK (c.getNumActiveBands() == 0);
    g.mouseDrag (mouseEvent (g, on (1000.0, 6.0), left));
    REQUIRE (c.getNumActiveBands() == 1);
    const auto band = c.getSelectedBand();
    REQUIRE (band >= 0);
    CHECK (c.getBand (band).settings.type == FilterType::bell);
    CHECK (c.getBand (band).settings.frequency == Catch::Approx (1000.0).epsilon (0.003));
    CHECK (c.getBand (band).settings.gainDb == Catch::Approx (6.0).margin (0.1));
    g.mouseUp (mouseEvent (g, on (1000.0, 6.0), juce::ModifierKeys()));
    CHECK (g.getPopup().isVisible());

    // With bands, the curve is the overall response: grab it where it is, not on the 0 dB line.
    // (Over a point there is no ghost: the point is grabbed instead.)
    g.mouseMove (mouseEvent (g, g.pointForBand (band), juce::ModifierKeys()));
    CHECK_FALSE (g.getLinePreview().has_value());
    ui.page->refresh();
    const auto curveAt600 = [&]
    {
        g.mouseMove (mouseEvent (g, on (600.0, 0.0), juce::ModifierKeys()));
        return g.getLinePreview();
    }();
    CHECK_FALSE (curveAt600.has_value()); // at 600 Hz the curve is well above 0 dB
    g.mouseDown (mouseEvent (g, { g.xForFrequency (300.0), 40.0f }, left)); // deselect
    g.mouseUp (mouseEvent (g, { g.xForFrequency (300.0), 40.0f }, juce::ModifierKeys()));
    const auto preview = g.linePreviewAt ({ g.xForFrequency (700.0), g.yForGain (4.0) });
    REQUIRE (preview.has_value());
    CHECK (g.gainForY (preview->point.y) > 2.0);
    g.mouseDown (mouseEvent (g, preview->point, left));
    g.mouseDrag (mouseEvent (g, preview->point + juce::Point<float> (0.0f, g.yForGain (0.0) - g.yForGain (3.0)), left));
    g.mouseUp (mouseEvent (g, preview->point, juce::ModifierKeys()));
    REQUIRE (c.getNumActiveBands() == 2);
    CHECK (c.getBand (c.getSelectedBand()).settings.gainDb == Catch::Approx (-3.0).margin (0.1)); // starts at 0 dB, moved by the drag

    // With all eight bands in use there is nothing to create.
    while (c.getNumActiveBands() < eq::numBands)
        c.createBand (FilterType::bell, 5000.0 + 100.0 * c.getNumActiveBands(), 0.0);
    CHECK_FALSE (g.linePreviewAt (on (40.0, 0.0)).has_value());
}

TEST_CASE ("EQ band editor follows the selected band", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& c = ui.controller();
    auto& panel = ui.bandPanel();

    panel.refresh();
    CHECK (panel.getBand() == -1);
    CHECK (panel.getKnob (EqField::frequency) == nullptr);

    const auto bell = c.createBand (FilterType::bell, 400.0, -4.0);
    const auto cut = c.createBand (FilterType::lowCut, 80.0, 0.0);
    c.setSelectedBand (bell);
    panel.refresh();
    REQUIRE (panel.getKnob (EqField::frequency) != nullptr);
    CHECK (&panel.getKnob (EqField::frequency)->getParameter() == &c.parameter (bell, EqField::frequency));
    CHECK (panel.getKnob (EqField::frequency)->getValueText() == "400 Hz");
    CHECK (panel.getKnob (EqField::gain)->isEnabled());
    CHECK_FALSE (panel.getKnob (EqField::slope)->isEnabled());

    // A cut: no gain, slope yes, and no Q at 6 dB/oct.
    c.setSelectedBand (cut);
    panel.refresh();
    CHECK (&panel.getKnob (EqField::slope)->getParameter() == &c.parameter (cut, EqField::slope));
    CHECK (panel.getKnob (EqField::slope)->getValueText() == "12 dB/oct");
    CHECK_FALSE (panel.getKnob (EqField::gain)->isEnabled());
    CHECK (panel.getKnob (EqField::slope)->isEnabled());
    CHECK (panel.getKnob (EqField::q)->isEnabled());
    c.setValue (cut, EqField::slope, 0.0f);
    panel.refresh();
    CHECK_FALSE (panel.getKnob (EqField::q)->isEnabled());

    // Shape selector.
    panel.getShapeSelector().onChange ((int) FilterType::highShelf);
    CHECK (c.getBand (cut).settings.type == FilterType::highShelf);

    // Bypass button: bound to the selected band only. Regression: switching bands used to write
    // the new band's bypass state into the previous one.
    panel.getBypassButton().setToggleState (true, juce::sendNotificationSync); // a click, synchronously
    CHECK (c.getBand (cut).bypassed);
    c.setSelectedBand (bell);
    panel.refresh();
    CHECK (c.getBand (cut).bypassed);
    CHECK_FALSE (c.getBand (bell).bypassed);
    CHECK_FALSE (panel.getBypassButton().getToggleState());

    // Solo: interface state in the processor.
    panel.getSoloButton().setToggleState (true, juce::sendNotificationSync);
    CHECK (ui.processor.getEqSoloBand() == bell);
    panel.getSoloButton().setToggleState (false, juce::sendNotificationSync);
    CHECK (ui.processor.getEqSoloBand() == -1);

    // Delete.
    panel.getDeleteButton().onClick();
    CHECK_FALSE (c.getBand (bell).active);
    CHECK (panel.getBand() == -1);
}

TEST_CASE ("EQ header: character, analyser and scale; chain row shows the band count", "[ui][eq][phase1]")
{
    EqUi ui;
    auto& page = *ui.page;

    page.getCharacterSelector().setSelectedIndex (2);
    CHECK (param::ParameterReader (ui.processor.getState()).read().modules.eq.character == eq::Character::warm);

    page.getAnalyserSelector().setSelectedIndex (0);
    CHECK (ui.processor.getEqAnalyzer() == PMelodiesProcessor::EqAnalyzer::pre);
    CHECK (ui.processor.getEngine().getEqAnalyzer().getPoint() == dsp::AnalyzerTap::Point::pre);
    page.getAnalyserSelector().setSelectedIndex (2);
    CHECK (ui.processor.getEngine().getEqAnalyzer().getPoint() == dsp::AnalyzerTap::Point::off);

    page.getRangeSelector().setSelectedIndex (0);
    CHECK (ui.processor.getEqRangeDb() == 6);
    page.getRangeSelector().setSelectedIndex (2);
    CHECK (ui.processor.getEqRangeDb() == 30);

    // Saved with the project.
    juce::MemoryBlock saved;
    ui.processor.getStateInformation (saved);
    PMelodiesProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    CHECK (restored.getEqAnalyzer() == PMelodiesProcessor::EqAnalyzer::off);
    CHECK (restored.getEqRangeDb() == 30);
    CHECK (restored.getEngine().getEqAnalyzer().getPoint() == dsp::AnalyzerTap::Point::off);

    // Chain row.
    auto& chain = dynamic_cast<PMelodiesEditor&> (*ui.editor).getChainPanel();
    CHECK (chain.getStatusText (ModuleId::eq) == "Sin bandas");
    ui.controller().createBand (FilterType::bell, 300.0, 2.0);
    CHECK (chain.getStatusText (ModuleId::eq) == "1 banda");
    ui.controller().createBand (FilterType::bell, 900.0, 2.0);
    CHECK (chain.getStatusText (ModuleId::eq) == "2 bandas");
    CHECK (chain.getStatusText (ModuleId::saturation) == "Activo"); // until phase 3 (the compressor shows its style since phase 2)
}

TEST_CASE ("Spectrum analyser: level of a tone, display tilt, and the feed from the audio thread", "[ui][eq][analyser][phase1]")
{
    // A 1 kHz tone at -12 dBFS reads -12 dB at 1 kHz (where the tilt is 0) and far less elsewhere.
    ui::SpectrumAnalyser analyser;
    analyser.prepare (48000.0);
    CHECK (analyser.getFftSize() == 4096);
    const auto hz = 1000.0 * 4096.0 / 48000.0; // on a bin
    const auto binHz = std::round (hz) * 48000.0 / 4096.0;
    const auto tone = makeSine (1, 48000, binHz, 48000.0, (float) std::pow (10.0, -12.0 / 20.0));
    analyser.push (tone.getReadPointer (0), tone.getNumSamples());
    CHECK (analyser.levelAt (binHz) == Catch::Approx (-12.0).margin (0.5));
    CHECK (analyser.levelAt (100.0) < -60.0f);

    // White noise: flat per bin, so with the tilt the display rises 4.5 dB per octave.
    ui::SpectrumAnalyser noise;
    noise.prepare (48000.0);
    const auto white = makeNoise (1, 48000 * 4, 0.3f);
    noise.push (white.getReadPointer (0), white.getNumSamples());
    double rise = 0.0;
    for (const auto f : { 1000.0, 1100.0, 1200.0 })
        rise += (noise.levelAt (8.0 * f) - noise.levelAt (f)) / 3.0;
    CHECK (rise == Catch::Approx (3.0 * ui::SpectrumAnalyser::tiltDbPerOctave).margin (3.0));
    CHECK (analyser.getFftSize() == 4096);
    ui::SpectrumAnalyser high;
    high.prepare (192000.0);
    CHECK (high.getFftSize() == 16384);

    // From the plugin: Post feeds the analyser, Off feeds nothing.
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);
    auto& tap = processor.getEngine().getEqAnalyzer();
    ui::SpectrumAnalyser fed;
    for (int round = 0; round < 3; ++round) // as the interface does, several times a second
    {
        runProcessor (processor, makeSine (2, 4800, binHz, 48000.0, 0.25f), blockSizes (4800, 512));
        CHECK (tap.getNumReady() == 4800);
        fed.update (tap);
        CHECK (tap.getNumReady() == 0);
    }
    CHECK (fed.levelAt (binHz) == Catch::Approx (-12.0).margin (1.0));

    processor.setEqAnalyzer (PMelodiesProcessor::EqAnalyzer::off);
    runProcessor (processor, makeSine (2, 4800, binHz, 48000.0, 0.25f), blockSizes (4800, 512));
    CHECK (tap.getNumReady() == 0);
}
