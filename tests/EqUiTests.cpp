// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 1 interface tests: the EQ display (geometry, creating bands by zone, dragging, wheel,
// Alt+click, delete, menu, typed values), the band editor, the header selectors, the chain
// row status and the spectrum analyser.

#include "TestHelpers.h"

#include "PluginEditor.h"
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
    CHECK (chain.getStatusText (ModuleId::compressor) == "Activo");
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
