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

TEST_CASE ("Chain panel: drag and drop reorders the chain parameter", "[ui][chain][phase0]")
{
    // Slot semantics: dropping into slot s (0 = before the first row … 4 = after the last).
    CHECK (ui::ChainPanel::targetPosition (0, 4) == 3); // first module to the end
    CHECK (ui::ChainPanel::targetPosition (3, 0) == 0); // last module to the start
    CHECK (ui::ChainPanel::targetPosition (1, 1) == 1); // dropped where it was
    CHECK (ui::ChainPanel::targetPosition (1, 2) == 1); // just below itself: no move

    PMelodiesProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditorAndMakeActive());
    auto& panel = dynamic_cast<PMelodiesEditor&> (*editor).getChainPanel();

    // Drop slots follow the row centres.
    CHECK (panel.dropSlotForY ((float) panel.getSlotBounds (0).getY()) == 0);
    CHECK (panel.dropSlotForY ((float) panel.getSlotBounds (2).getBottom()) == 3);
    CHECK (panel.dropSlotForY ((float) panel.getSlotBounds (3).getBottom() + 20.0f) == 4);

    panel.moveModule (3, 0); // distortion to the front
    const auto expected = chain::Order { ModuleId::distortion, ModuleId::eq, ModuleId::compressor, ModuleId::saturation };
    CHECK (juce::roundToInt (valueOf (processor, param::id::chainOrder)) == chain::encode (expected));
    CHECK (panel.getOrder() == expected);

    // Rows are laid out in the processing order.
    CHECK (panel.getRow (ModuleId::distortion).getBounds() == panel.getSlotBounds (0));
    CHECK (panel.getRow (ModuleId::saturation).getBounds() == panel.getSlotBounds (3));

    // Automation from the host moves the rows too.
    setParameter (processor, param::id::chainOrder, 0.0f);
    CHECK (panel.getRow (ModuleId::eq).getBounds() == panel.getSlotBounds (0));

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

    std::vector<ui::SegmentedSelector*> selectors;
    forEachDescendant (pm, [&] (juce::Component& c)
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

    pm.selectModule (ModuleId::compressor);
    pm.showSettings (true);
    savePng (pm.createComponentSnapshot (pm.getLocalBounds(), true, 2.0f), "02-settings");
    pm.showSettings (false);

    processor.editorBeingDeleted (editor.get());
}
