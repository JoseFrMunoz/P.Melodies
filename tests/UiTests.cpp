// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Interface tests. Mouse gestures are tested through the same functions the mouse handlers
// call, so they run headless. The [.snapshot] test renders PNGs for visual review.

#include "TestHelpers.h"

#include "PluginEditor.h"
#include "core/Parameters.h"
#include "ui/components/Knob.h"
#include "ui/components/LevelMeterView.h"
#include "ui/components/Dropdown.h"
#include "ui/components/SegmentedSelector.h"
#include "ui/components/ToggleSwitch.h"
#include "ui/theme/Theme.h"
#include "ui/eq/EqPanel.h"
#include "ui/layout/SettingsPanel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;

namespace
{
juce::RangedAudioParameter& get (PMelodiesProcessor& p, const char* id) { return *p.getState().getParameter (id); }

float valueOf (PMelodiesProcessor& p, const char* id)
{
    auto& parameter = get (p, id);
    return parameter.convertFrom0to1 (parameter.getValue());
}

template <typename Fn>
void forEachDescendant (juce::Component& root, Fn&& fn)
{
    for (auto* child : root.getChildren())
    {
        fn (*child);
        forEachDescendant (*child, fn);
    }
}

void savePng (const juce::Image& image, const juce::String& name)
{
    const auto dir = juce::SystemStats::getEnvironmentVariable ("PM_SNAPSHOT_DIR", {});
    if (dir.isEmpty())
        return;
    juce::File file = juce::File (dir).getChildFile (name + ".png");
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
}
} // namespace

TEST_CASE ("Theme: text and controls are readable (WCAG 2.1 contrast)", "[ui][theme][phase0]")
{
    // Relative luminance and contrast ratio as defined by WCAG 2.1.
    const auto luminance = [] (juce::Colour c)
    {
        const auto channel = [] (juce::uint8 v)
        {
            const auto x = v / 255.0;
            return x <= 0.03928 ? x / 12.92 : std::pow ((x + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * channel (c.getRed()) + 0.7152 * channel (c.getGreen()) + 0.0722 * channel (c.getBlue());
    };
    const auto contrast = [&] (juce::Colour a, juce::Colour b)
    {
        const auto la = luminance (a), lb = luminance (b);
        return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
    };

    const auto& c = ui::Theme::dark().colours;
    // Normal text needs 4.5:1, on every surface it is drawn on.
    for (const auto surface : { c.window, c.background, c.panel, c.panelRaised, c.elevated })
    {
        CHECK (contrast (c.text, surface) >= 4.5);
        CHECK (contrast (c.textSecondary, surface) >= 4.5);
        CHECK (contrast (c.textMuted, surface) >= 4.5);
    }
    CHECK (contrast (c.onAccent, c.accent) >= 4.5); // text on accent buttons
    // Graphical elements (curves, knob arcs, module colours) need 3:1 against the background.
    for (const auto surface : { c.background, c.panel })
    {
        CHECK (contrast (c.accent, surface) >= 3.0);
        for (const auto colour : c.module)
            CHECK (contrast (colour, surface) >= 3.0);
    }
}

TEST_CASE ("Theme: module colours and embedded fonts", "[ui][theme][phase0]")
{
    const auto& theme = ui::Theme::dark();
    CHECK (theme.moduleColour (ModuleId::eq) == juce::Colour (0xffd4af37));
    CHECK (theme.moduleColour (ModuleId::compressor) == juce::Colour (0xff8fa6bf));
    CHECK (theme.moduleColour (ModuleId::saturation) == juce::Colour (0xffc8743c));
    CHECK (theme.moduleColour (ModuleId::distortion) == juce::Colour (0xffad374e));

    // Brand rule: the EQ wears the signature colour, which is also the accent.
    CHECK (theme.colours.accent == theme.moduleColour (ModuleId::eq));

    // The embedded typefaces load (not a system fallback).
    CHECK (theme.ui (12.0f).getTypefacePtr()->getName().contains ("Inter"));
    CHECK (theme.mono (11.0f).getTypefacePtr()->getName().contains ("JetBrains Mono"));
    CHECK (theme.display (16.0f).getTypefacePtr()->getName().contains ("Space Grotesk"));
}

TEST_CASE ("The editor opens at 1000 x 640 and paints", "[ui][phase0]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == 1000);
    CHECK (editor->getHeight() == 640);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    CHECK (image.getWidth() == 1000);
    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Knob gestures: drag, fine drag, default value", "[ui][knob][phase0]")
{
    CHECK (ui::Knob::valueAfterDrag (0.5f, 200.0f, false) == 1.0f);  // full range in 200 px
    CHECK (ui::Knob::valueAfterDrag (0.5f, -100.0f, false) == 0.0f);
    CHECK (ui::Knob::valueAfterDrag (0.5f, 20.0f, false) == Catch::Approx (0.6f));
    CHECK (ui::Knob::valueAfterDrag (0.5f, 20.0f, true) == Catch::Approx (0.51f)); // Ctrl: 10x finer
    CHECK (ui::Knob::valueAfterDrag (0.99f, 50.0f, false) == 1.0f);                // clamped

    PMelodiesProcessor processor;
    setParameter (processor, param::id::outputGain, -7.5f);
    ui::Knob knob (get (processor, param::id::outputGain), "Salida");
    CHECK (knob.getValueText() == juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "7,5 dB")));

    knob.resetToDefault(); // what a double click does
    CHECK (valueOf (processor, param::id::outputGain) == Catch::Approx (0.0f));
}

namespace
{
/** Runs the chain animation at 60 frames per second until it settles; returns the frames used. */
int settle (ui::ChainPanel& panel, int maxFrames = 240)
{
    int frames = 0;
    while (panel.isAnimating() && frames < maxFrames)
    {
        panel.advanceAnimation (1.0 / 60.0);
        ++frames;
    }
    return frames;
}

int centreY (const juce::Component& c) { return c.getBounds().getCentreY(); }
} // namespace

TEST_CASE ("Chain panel: rows are laid out in the processing order and follow outside changes smoothly", "[ui][chain][phase0]")
{
    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& panel = dynamic_cast<PMelodiesEditor&> (*editor).getChainPanel();

    // Opening the window places the rows at once, without an animation.
    CHECK_FALSE (panel.isAnimating());
    for (int p = 0; p < numModules; ++p)
        CHECK (panel.getRow (moduleAt (p)).getBounds() == panel.getSlotBounds (p));

    // Nearest slot for a row's top edge: switches half-way between two slots, clamped at the ends.
    const auto pitch = (float) (panel.getSlotBounds (1).getY() - panel.getSlotBounds (0).getY());
    const auto top0 = (float) panel.getSlotBounds (0).getY();
    CHECK (panel.positionForRowTop (top0) == 0);
    CHECK (panel.positionForRowTop (top0 + 0.45f * pitch) == 0);
    CHECK (panel.positionForRowTop (top0 + 0.55f * pitch) == 1);
    CHECK (panel.positionForRowTop (top0 + 3.0f * pitch) == 3);
    CHECK (panel.positionForRowTop (top0 - 40.0f) == 0);
    CHECK (panel.positionForRowTop (top0 + 9.0f * pitch) == 3);

    // Moving a module (what a drop does) updates the plugin's order and the panel's.
    panel.moveModule (3, 0); // distortion to the front
    const auto expected = chain::Order { ModuleId::distortion, ModuleId::eq, ModuleId::compressor, ModuleId::saturation };
    CHECK (processor.getChainOrder() == expected);
    CHECK (panel.getOrder() == expected);

    // The rows glide to their new slots instead of jumping, and settle in well under half a second.
    auto& distortion = panel.getRow (ModuleId::distortion);
    CHECK (panel.getRowTargetBounds (ModuleId::distortion) == panel.getSlotBounds (0));
    CHECK (distortion.getBounds() == panel.getSlotBounds (3));
    panel.advanceAnimation (1.0 / 60.0);
    CHECK (distortion.getY() < panel.getSlotBounds (3).getY());
    CHECK (distortion.getY() > panel.getSlotBounds (0).getY());
    CHECK (settle (panel) < 25);
    for (int p = 0; p < numModules; ++p)
        CHECK (panel.getRow (expected[(size_t) p]).getBounds() == panel.getSlotBounds (p));

    // A change from outside (a project loaded with the window open; presets and undo in phase 5)
    // moves the rows the same way on the next refresh.
    processor.setChainOrder (chain::defaultOrder());
    dynamic_cast<PMelodiesEditor&> (*editor).refresh();
    CHECK (panel.getRowTargetBounds (ModuleId::eq) == panel.getSlotBounds (0));
    CHECK (panel.isAnimating());
    settle (panel);
    CHECK (panel.getRow (ModuleId::eq).getBounds() == panel.getSlotBounds (0));
    CHECK (panel.getRow (ModuleId::distortion).getBounds() == panel.getSlotBounds (3));

    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Chain panel: while dragging, the other rows make room live and the drop needs no aiming", "[ui][chain][phase0]")
{
    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& panel = dynamic_cast<PMelodiesEditor&> (*editor).getChainPanel();
    const auto defaultOrder = chain::defaultOrder();
    const auto pitch = panel.getSlotBounds (1).getY() - panel.getSlotBounds (0).getY();

    auto& distortion = panel.getRow (ModuleId::distortion);
    auto& compressor = panel.getRow (ModuleId::compressor);
    auto& saturation = panel.getRow (ModuleId::saturation);

    // Grab distortion (last) by its handle and pull it up, a few pixels per frame like a real drag.
    auto pointer = (float) centreY (distortion);
    panel.beginDrag (distortion, pointer);
    REQUIRE (panel.isDragging());

    const auto dragTo = [&] (float target)
    {
        while (std::abs (pointer - target) > 0.5f)
        {
            pointer += juce::jlimit (-6.0f, 6.0f, target - pointer);
            panel.continueDrag (pointer);
            panel.advanceAnimation (1.0 / 60.0);
        }
    };

    // Less than half a slot: nothing moves out of the way yet.
    dragTo (pointer - 0.4f * (float) pitch);
    CHECK (panel.getDisplayedOrder() == defaultOrder);

    // Past half a slot: saturation slides down into distortion's place, live, before any drop.
    dragTo (pointer - 0.3f * (float) pitch);
    CHECK (panel.getDisplayedOrder()
           == chain::Order { ModuleId::eq, ModuleId::compressor, ModuleId::distortion, ModuleId::saturation });
    CHECK (panel.getRowTargetBounds (ModuleId::saturation) == panel.getSlotBounds (3));
    settle (panel);
    CHECK (saturation.getBounds() == panel.getSlotBounds (3));

    // Keep pulling: compressor makes room too. The dragged row follows the pointer exactly.
    const auto grab = pointer - (float) distortion.getY();
    dragTo (pointer - 1.0f * (float) pitch);
    CHECK (distortion.getY() == juce::roundToInt (pointer - grab));
    CHECK (panel.getDisplayedOrder()
           == chain::Order { ModuleId::eq, ModuleId::distortion, ModuleId::compressor, ModuleId::saturation });
    settle (panel);
    CHECK (compressor.getBounds() == panel.getSlotBounds (2));

    // Nothing reaches the plugin until the drop (one gesture, one undo step in phase 5).
    CHECK (processor.getChainOrder() == chain::defaultOrder());

    // Drop anywhere: no need to aim at a line. The row glides into the gap it was showing.
    panel.endDrag();
    CHECK_FALSE (panel.isDragging());
    const auto dropped = chain::Order { ModuleId::eq, ModuleId::distortion, ModuleId::compressor, ModuleId::saturation };
    CHECK (processor.getChainOrder() == dropped);
    CHECK (panel.getRowTargetBounds (ModuleId::distortion) == panel.getSlotBounds (1));
    CHECK (panel.isAnimating());
    CHECK (settle (panel) < 25);
    for (int p = 0; p < numModules; ++p)
        CHECK (panel.getRow (dropped[(size_t) p]).getBounds() == panel.getSlotBounds (p));

    // A drag that goes somewhere else and comes back changes nothing.
    const auto start = (float) centreY (distortion);
    pointer = start;
    panel.beginDrag (distortion, pointer);
    dragTo (start - 0.8f * (float) pitch);
    CHECK (panel.getDisplayedOrder()[0] == ModuleId::distortion);
    dragTo (start);
    CHECK (panel.getDisplayedOrder() == dropped);
    panel.endDrag();
    settle (panel);
    CHECK (processor.getChainOrder() == dropped);
    CHECK (distortion.getBounds() == panel.getSlotBounds (1));

    // Dragging far beyond the ends is clamped to the first and last slots.
    pointer = (float) centreY (distortion);
    panel.beginDrag (distortion, pointer);
    dragTo (pointer + 10.0f * (float) pitch);
    CHECK (distortion.getY() <= panel.getSlotBounds (numModules - 1).getY() + 12);
    panel.endDrag();
    settle (panel);
    CHECK (panel.getOrder()
           == chain::Order { ModuleId::eq, ModuleId::compressor, ModuleId::saturation, ModuleId::distortion });

    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Chain panel: power buttons bypass modules, clicking selects", "[ui][chain][phase0]")
{
    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& pm = dynamic_cast<PMelodiesEditor&> (*editor);
    auto& panel = pm.getChainPanel();

    panel.getRow (ModuleId::saturation).togglePower();
    CHECK (valueOf (processor, param::id::saturationEnabled) == 0.0f);
    CHECK_FALSE (panel.getRow (ModuleId::saturation).isModuleEnabled());
    panel.getRow (ModuleId::saturation).togglePower();
    CHECK (valueOf (processor, param::id::saturationEnabled) == 1.0f);

    panel.rowClicked (panel.getRow (ModuleId::compressor));
    CHECK (pm.getModulePanel().getModule() == ModuleId::compressor);
    CHECK (processor.getSelectedModule() == ModuleId::compressor); // saved with the project

    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Settings: quality selectors and clipper are bound to the parameters", "[ui][settings][phase0]")
{
    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& pm = dynamic_cast<PMelodiesEditor&> (*editor);

    pm.showSettings (true);
    REQUIRE (pm.isShowingSettings());

    // The selectors of the settings panel (the EQ page has its own).
    juce::Component* overlay = nullptr;
    for (auto* child : pm.getChildren())
        if (dynamic_cast<ui::SettingsOverlay*> (child) != nullptr)
            overlay = child;
    REQUIRE (overlay != nullptr);

    std::vector<ui::SegmentedSelector*> selectors;
    forEachDescendant (*overlay, [&] (juce::Component& c)
                       {
                           if (auto* s = dynamic_cast<ui::SegmentedSelector*> (&c); s != nullptr && s->isEnabled())
                               selectors.push_back (s);
                       });
    REQUIRE (selectors.size() == 2);

    CHECK (selectors[0]->getSelectedIndex() == 1); // playback 2x
    CHECK (selectors[1]->getSelectedIndex() == 2); // export 8x
    selectors[0]->setSelectedIndex (2);
    selectors[1]->setSelectedIndex (3);
    CHECK (get (processor, param::id::qualityRealtime).getCurrentValueAsText() == "4x");
    CHECK (get (processor, param::id::qualityOffline).getCurrentValueAsText() == "16x");
    CHECK (pm.getFooter().getQualityText() == "Calidad 4x / 16x");

    pm.showSettings (false);
    CHECK_FALSE (pm.isShowingSettings());
    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Footer shows the latency the plugin reports", "[ui][footer][phase0]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& footer = dynamic_cast<PMelodiesEditor&> (*editor).getFooter();

    const auto expectedMs = 1000.0 * processor.getLatencySamples() / 48000.0;
    CHECK (footer.getLatencyText() == "Latencia " + juce::String (expectedMs, 1).replaceCharacter ('.', ',') + " ms");
    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Footer: dragging the Mix knob changes the mix the audio engine applies", "[ui][footer][phase0]")
{
    // Reported in the phase 0 review: "the Mix does not work". In phase 0 every module passes the
    // audio through, so processed and original are the same signal and any mix sounds identical.
    // This test follows the whole path with real mouse events and a module that does change the
    // audio: knob → parameter → snapshot the engine reads → blended output.
    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& footer = dynamic_cast<PMelodiesEditor&> (*editor).getFooter();

    ui::Knob* mixKnob = nullptr;
    forEachDescendant (footer, [&] (juce::Component& c)
                       {
                           if (auto* k = dynamic_cast<ui::Knob*> (&c); k != nullptr && &k->getParameter() == &get (processor, param::id::mix))
                               mixKnob = k;
                       });
    REQUIRE (mixKnob != nullptr);
    CHECK (mixKnob->getValueText() == "100 %");

    // Drag down 100 px: half of the full range (200 px), so 100 % → 50 %.
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto start = mixKnob->getLocalBounds().getCentre().toFloat();
    const auto now = juce::Time::getCurrentTime();
    const auto event = [&] (juce::Point<float> position, bool dragged)
    {
        return juce::MouseEvent (source, position, juce::ModifierKeys::leftButtonModifier, juce::MouseInputSource::defaultPressure,
                                 0.0f, 0.0f, 0.0f, 0.0f, mixKnob, mixKnob, now, start, now, 1, dragged);
    };
    mixKnob->mouseDown (event (start, false));
    mixKnob->mouseDrag (event (start + juce::Point<float> (0.0f, 50.0f), true));
    mixKnob->mouseDrag (event (start + juce::Point<float> (0.0f, 100.0f), true));
    mixKnob->mouseUp (event (start + juce::Point<float> (0.0f, 100.0f), true));

    CHECK (valueOf (processor, param::id::mix) == Catch::Approx (50.0f));
    CHECK (mixKnob->getValueText() == "50 %");

    auto p = param::ParameterReader (processor.getState()).read();
    CHECK (p.mix == Catch::Approx (0.5f));

    // A module that doubles the level (as a +6 dB EQ band will in phase 1): 50 % gives 1.5x.
    auto settings = neutralSettings();
    settings[0].neutral = false;
    settings[0].gain = 2.0f;
    p.clipperEnabled = false;
    AudioEngine engine (testFactory (settings));
    engine.prepare (48000.0, 512, 2, p);
    const auto latency = engine.getLatencySamples();
    const auto input = makeNoise (2, 20000, 0.2f);
    const auto out = runEngine (engine, input, blockSizes (20000, 512), p);

    float error = 0.0f;
    for (int i = latency + 2000; i < 20000; ++i)
        error = juce::jmax (error, std::abs (out.getSample (0, i) - 1.5f * input.getSample (0, i - latency)));
    CHECK (error < 1.0e-6f);

    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Every interactive control has a help tooltip", "[ui][tooltips][phase0]")
{
    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& pm = dynamic_cast<PMelodiesEditor&> (*editor);
    pm.showSettings (true);

    int checked = 0;
    forEachDescendant (pm, [&] (juce::Component& c)
                       {
                           const auto interactive = dynamic_cast<juce::Button*> (&c) != nullptr
                                                    || dynamic_cast<ui::Knob*> (&c) != nullptr
                                                    || dynamic_cast<ui::SegmentedSelector*> (&c) != nullptr;
                           if (! interactive || dynamic_cast<juce::HyperlinkButton*> (&c) != nullptr)
                               return;
                           if (c.findParentComponentOfClass<ui::Dropdown>() != nullptr)
                               return; // the dropdown itself carries the tooltip

                           auto* client = dynamic_cast<juce::TooltipClient*> (&c);
                           INFO (c.getTitle());
                           REQUIRE (client != nullptr);
                           CHECK (client->getTooltip().isNotEmpty());
                           ++checked;
                       });
    CHECK (checked > 15);
    processor.editorBeingDeleted (editor.get());
}

TEST_CASE ("Meter ballistics: hold, fall and floor", "[ui][meters][phase0]")
{
    ui::MeterBallistics m;
    m.update (1.0f, 0.5f, 1.0 / 30.0);
    CHECK (m.peakDb == Catch::Approx (0.0f));
    CHECK (m.holdDb == Catch::Approx (0.0f));
    CHECK (m.rmsDb == Catch::Approx (-6.02f).margin (0.01));

    for (int i = 0; i < 30; ++i) // one second of silence
        m.update (0.0f, 0.0f, 1.0 / 30.0);
    CHECK (m.peakDb == Catch::Approx (-ui::MeterBallistics::fallDbPerSecond).margin (0.5));
    CHECK (m.holdDb == Catch::Approx (0.0f)); // still held

    for (int i = 0; i < 300; ++i)
        m.update (0.0f, 0.0f, 1.0 / 30.0);
    CHECK (m.peakDb == ui::MeterBallistics::floorDb);
    CHECK (m.holdDb == ui::MeterBallistics::floorDb);
    CHECK (ui::MeterBallistics::proportionOf (0.0f) == 1.0f);
    CHECK (ui::MeterBallistics::proportionOf (-120.0f) == 0.0f);
}

TEST_CASE ("Snapshot of the interface for visual review", "[.snapshot]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);

    // Some signal so the meters show something.
    const auto input = makeSine (2, 48000, 220.0, 48000.0, 0.6f);
    runProcessor (processor, input, blockSizes (48000, 512));

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& pm = dynamic_cast<PMelodiesEditor&> (*editor);
    setParameter (processor, param::id::distortionEnabled, 0.0f);
    runProcessor (processor, input, blockSizes (4800, 512));
    pm.getFooter().refresh (1.0 / 30.0);

    savePng (pm.createComponentSnapshot (pm.getLocalBounds(), true, 2.0f), "01-main");

    // EQ page with bands, as in design 01, with a spectrum from real audio.
    {
        auto& eqPanel = pm.getModulePanel().getEqPanel();
        auto& controller = eqPanel.getController();
        controller.createBand (dsp::eq::FilterType::lowCut, 32.0, 0.0);
        controller.createBand (dsp::eq::FilterType::bell, 110.0, 4.5);
        const auto bell = controller.createBand (dsp::eq::FilterType::bell, 400.0, -4.0);
        controller.setValue (bell, param::EqField::q, 2.5f);
        const auto bypassed = controller.createBand (dsp::eq::FilterType::bell, 2000.0, 0.0);
        controller.setBypassed (bypassed, true);
        controller.createBand (dsp::eq::FilterType::highShelf, 8000.0, 3.0);
        controller.setSelectedBand (bell);
        runProcessor (processor, makeNoise (2, 48000, 0.2f), blockSizes (48000, 512));
        eqPanel.refresh();
        auto& display = eqPanel.getGraph();
        display.beginDrag (bell, display.pointForBand (bell)); // the point held: its mini panel shows
        display.updatePopup();
        pm.getFooter().refresh (1.0 / 30.0);
        pm.getChainPanel().refreshStatus();
        savePng (pm.createComponentSnapshot (pm.getLocalBounds(), true, 2.0f), "01b-eq");
        display.endDrag();

        // Solo held from the mini panel and dragged to 1.1 kHz: the range heard, the rest dimmed.
        auto& graph = eqPanel.getGraph();
        graph.beginSolo (bell);
        graph.soloDragTo ({ graph.xForFrequency (1100.0) - graph.xForFrequency (400.0), 0.0f }, false);
        eqPanel.refresh();
        savePng (pm.createComponentSnapshot (pm.getLocalBounds(), true, 2.0f), "01c-eq-solo");
        graph.endSolo();
    }

    // Component gallery: standard knobs with module colours, selector, dropdown, switch.
    {
        struct Gallery final : juce::Component
        {
            explicit Gallery (PMelodiesProcessor& p)
                : a (*p.getState().getParameter (param::id::inputGain), "Drive"),
                  b (*p.getState().getParameter (param::id::mix), "Mix"),
                  c (*p.getState().getParameter (param::id::outputGain), "Nivel"),
                  selector ({ "Off", "2x", "4x" }),
                  dropdown ({ juce::String (juce::CharPointer_UTF8 ("V\xc3\xa1lvula c\xc3\xa1lida")), "Cinta limpia" }),
                  toggle ("Interruptor")
            {
                setLookAndFeel (&lnf);
                a.setAccentColour (lnf.getTheme().moduleColour (ModuleId::saturation));
                b.setAccentColour (lnf.getTheme().moduleColour (ModuleId::compressor));
                selector.setSelectedIndex (1, juce::dontSendNotification);
                for (auto* comp : std::initializer_list<juce::Component*> { &a, &b, &c, &selector, &dropdown, &toggle })
                    addAndMakeVisible (comp);
                a.setBounds (20, 30, 70, 80);
                b.setBounds (100, 30, 70, 80);
                c.setBounds (180, 30, 70, 80);
                selector.setBounds (20, 130, selector.getPreferredBounds().getWidth(), 25);
                dropdown.setBounds (20, 170, 150, 28);
                toggle.setBounds (200, 135, 120, 20);
                setSize (340, 220);
            }
            ~Gallery() override { setLookAndFeel (nullptr); }
            void paint (juce::Graphics& g) override { g.fillAll (lnf.getTheme().colours.panel); }

            ui::LookAndFeel lnf;
            ui::Knob a, b, c;
            ui::SegmentedSelector selector;
            ui::Dropdown dropdown;
            ui::ToggleSwitch toggle;
        };

        setParameter (processor, param::id::inputGain, 6.5f);
        setParameter (processor, param::id::mix, 80.0f);
        Gallery gallery (processor);
        savePng (gallery.createComponentSnapshot (gallery.getLocalBounds(), true, 2.0f), "03-components");
    }

    // Chain panel while dragging distortion up: mid-slide, then with the gap open.
    {
        auto& panel = pm.getChainPanel();
        auto& row = panel.getRow (ModuleId::distortion);
        const auto pitch = panel.getSlotBounds (1).getY() - panel.getSlotBounds (0).getY();
        auto y = (float) row.getBounds().getCentreY();
        panel.beginDrag (row, y);
        for (int i = 0; i < 12; ++i)
        {
            y -= 1.4f * (float) pitch / 12.0f;
            panel.continueDrag (y);
        }
        panel.advanceAnimation (2.0 / 60.0);
        savePng (panel.createComponentSnapshot (panel.getLocalBounds(), true, 2.0f), "04-chain-drag-sliding");
        panel.advanceAnimation (1.0);
        panel.advanceAnimation (1.0);
        savePng (panel.createComponentSnapshot (panel.getLocalBounds(), true, 2.0f), "05-chain-drag-gap");
        panel.endDrag();
        panel.advanceAnimation (1.0);
        panel.advanceAnimation (1.0);
        savePng (panel.createComponentSnapshot (panel.getLocalBounds(), true, 2.0f), "06-chain-dropped");
    }

    pm.selectModule (ModuleId::compressor);
    pm.showSettings (true);
    savePng (pm.createComponentSnapshot (pm.getLocalBounds(), true, 2.0f), "02-settings");
    pm.showSettings (false);

    processor.editorBeingDeleted (editor.get());
}
