// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 2 interface tests: the compressor page (layout, controls bound to the parameters,
// Vocal's automatic knobs, listen as toggle and momentary), the level graph and the transfer
// curve fed by the audio, the external sidechain warning, the chain row and the sidechain EQ.

#include "TestHelpers.h"

#include "PluginEditor.h"
#include "core/ValueFormat.h"
#include "ui/comp/CompPanel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;
using Filter = ui::SidechainEqView::Filter;

namespace
{
/** An editor at its real size, with the compressor page shown. */
struct CompUi
{
    CompUi()
    {
        prepareProcessor (processor, 48000.0, 512, 2);
        editor.reset (processor.createEditorAndMakeActive());
        auto& pm = dynamic_cast<PMelodiesEditor&> (*editor);
        pm.selectModule (ModuleId::compressor);
        page = &pm.getModulePanel().getCompPanel();
    }
    ~CompUi() { processor.editorBeingDeleted (editor.get()); }

    PMelodiesEditor& pmEditor() { return dynamic_cast<PMelodiesEditor&> (*editor); }
    float value (const char* id) { return processor.getState().getParameter (id)->convertFrom0to1 (processor.getState().getParameter (id)->getValue()); }

    /** One second of a 1 kHz tone at 0 dBFS through the plugin, then a refresh of the window. */
    void play (double seconds = 1.0, float amplitude = 1.0f, const juce::AudioBuffer<float>* sidechain = nullptr)
    {
        const auto n = (int) (seconds * 48000.0);
        runProcessor (processor, makeSine (2, n, 1000.0, 48000.0, amplitude), blockSizes (n, 512, 512), sidechain);
        pmEditor().refresh();
    }

    PMelodiesProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    ui::CompPanel* page = nullptr;
};

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> position, juce::ModifierKeys mods)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position, mods, juce::MouseInputSource::defaultPressure,
             0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, position, now, 1, false };
}
} // namespace

TEST_CASE ("Compressor page: laid out as design 02, inside the panel and without overlaps", "[ui][comp][phase2]")
{
    CompUi ui;
    auto& page = *ui.page;
    REQUIRE (page.isVisible());
    CHECK_FALSE (ui.pmEditor().getModulePanel().getEqPanel().isVisible());

    const auto inside = page.getLocalBounds();
    std::vector<juce::Component*> parts { &page.getOversamplingSelector(), &page.getStyleSelector(), &page.getLevelGraph(),
                                          &page.getCurveView(), &page.getSidechainEq(), &page.getSourceSelector(),
                                          &page.getListenButton(), &page.getAutoGainSwitch(), &page.getAutoReleaseSwitch() };
    for (int i = 0; i < ui::CompPanel::numKnobs; ++i)
        parts.push_back (&page.getKnob (i));

    for (size_t i = 0; i < parts.size(); ++i)
    {
        INFO (parts[i]->getTitle() << " " << parts[i]->getBounds().toString());
        CHECK (inside.contains (parts[i]->getBounds()));
        CHECK_FALSE (parts[i]->getBounds().isEmpty());
        for (size_t j = i + 1; j < parts.size(); ++j)
            CHECK_FALSE (parts[i]->getBounds().intersects (parts[j]->getBounds()));
    }

    CHECK (page.getLevelGraph().getHeight() == ui::CompPanel::graphHeight);
    CHECK (page.getCurveView().getWidth() == ui::CompPanel::curveWidth);
    CHECK (page.getStyleSelector().getNumOptions() == 8);

    // Knobs in the order of the design.
    const juce::StringArray order { param::id::compThreshold, param::id::compRatio, param::id::compKnee, param::id::compRange,
                                    param::id::compAttack, param::id::compRelease, param::id::compLookahead, param::id::compHold,
                                    param::id::compMakeup, param::id::compMix };
    for (int i = 0; i < ui::CompPanel::numKnobs; ++i)
    {
        CHECK (page.getKnob (i).getParameter().getParameterID() == order[i]);
        if (i > 0)
            CHECK (page.getKnob (i).getX() > page.getKnob (i - 1).getRight());
    }
}

TEST_CASE ("Compressor page: controls follow the parameters both ways", "[ui][comp][phase2]")
{
    CompUi ui;
    auto& page = *ui.page;

    page.getStyleSelector().setSelectedIndex (5); // Bus
    CHECK (ui.value (param::id::compStyle) == 5.0f);
    setParameter (ui.processor, param::id::compStyle, 2.0f);
    CHECK (page.getStyleSelector().getSelectedIndex() == 2);

    page.getOversamplingSelector().setSelectedIndex (2);
    CHECK (ui.value (param::id::compOversampling) == 2.0f);
    page.getSourceSelector().setSelectedIndex (1);
    CHECK (ui.value (param::id::compSidechain) == 1.0f);
    page.getAutoGainSwitch().setToggleState (true, juce::sendNotificationSync);
    CHECK (ui.value (param::id::compAutoGain) == 1.0f);
    page.getAutoReleaseSwitch().setToggleState (true, juce::sendNotificationSync);
    CHECK (ui.value (param::id::compAutoRelease) == 1.0f);

    // Values as in the design.
    setParameter (ui.processor, param::id::compThreshold, -18.0f);
    setParameter (ui.processor, param::id::compMakeup, 2.5f);
    CHECK (page.findKnob (param::id::compThreshold)->getValueText() == juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "18 dB")));
    CHECK (page.findKnob (param::id::compRatio)->getValueText() == "4:1");
    CHECK (page.findKnob (param::id::compAttack)->getValueText() == "10 ms");
    CHECK (page.findKnob (param::id::compRelease)->getValueText() == "120 ms");
    CHECK (page.findKnob (param::id::compMakeup)->getValueText() == "+2,5 dB");
    CHECK (page.findKnob (param::id::compMix)->getValueText() == "100 %");

    // Vocal sets ratio and knee itself: their knobs are dimmed (§12).
    setParameter (ui.processor, param::id::compStyle, 3.0f);
    page.refresh();
    CHECK_FALSE (page.findKnob (param::id::compRatio)->isEnabled());
    CHECK_FALSE (page.findKnob (param::id::compKnee)->isEnabled());
    CHECK (page.findKnob (param::id::compThreshold)->isEnabled());
    CHECK (page.getCurveView().caption().contains (juce::String (juce::CharPointer_UTF8 ("AUTOM\xc3\x81TICA"))));
    setParameter (ui.processor, param::id::compStyle, 0.0f);
    page.refresh();
    CHECK (page.findKnob (param::id::compRatio)->isEnabled());
}

TEST_CASE ("Compressor page: listen is a toggle on a click and momentary while held", "[ui][comp][phase2]")
{
    CompUi ui;
    auto& listen = ui.page->getListenButton();
    CHECK_FALSE (ui.processor.isCompressorListening());

    listen.press (10.0);
    CHECK (ui.processor.isCompressorListening()); // on at once
    listen.release (10.1);                         // a click: stays on
    CHECK (ui.processor.isCompressorListening());
    listen.press (11.0);
    listen.release (11.1);                         // another click: off
    CHECK_FALSE (ui.processor.isCompressorListening());

    listen.press (20.0);
    CHECK (ui.processor.isCompressorListening());
    listen.release (21.0);                         // held: only while pressed
    CHECK_FALSE (ui.processor.isCompressorListening());

    // Closing the window turns it off.
    listen.press (30.0);
    listen.release (30.1);
    CHECK (ui.processor.isCompressorListening());
    ui.editor.reset();
    CHECK_FALSE (ui.processor.isCompressorListening());
    ui.editor.reset (ui.processor.createEditorAndMakeActive());
}

TEST_CASE ("Compressor page: the graph, the curve and the chain row show what the audio does", "[ui][comp][phase2]")
{
    CompUi ui;
    auto& page = *ui.page;
    setParameter (ui.processor, param::id::compThreshold, -20.0f);
    setParameter (ui.processor, param::id::compAttack, 1.0f);
    setParameter (ui.processor, param::id::compStyle, 5.0f); // Bus

    ui.play (1.0);

    auto& graph = page.getLevelGraph();
    CHECK (std::abs (graph.getNumStoredPoints() - 200) <= 3); // 5 ms each
    CHECK (graph.getPoint (0).gainDb == Catch::Approx (-15.0).margin (0.3));
    CHECK (graph.getPoint (0).inputDb == Catch::Approx (0.0).margin (0.2));
    CHECK (page.getShownReductionDb() == Catch::Approx (-15.0).margin (0.3));
    CHECK (page.getCurveView().getLevel() == Catch::Approx (0.0).margin (0.3));
    CHECK (page.getCurveView().getCurve().thresholdDb() == Catch::Approx (-20.0));

    // The curve is drawn over the whole width and goes through the point of each level.
    auto& curve = page.getCurveView();
    const auto path = curve.makeCurvePath();
    CHECK (path.getBounds().getWidth() == Catch::Approx ((float) curve.getWidth() - 2.0f).margin (0.5));
    for (const auto db : { -50.0f, -20.0f, -6.0f })
    {
        const auto p = curve.pointFor (db);
        juce::Point<float> nearest;
        path.getNearestPoint (p, nearest);
        CHECK (nearest.getDistanceFrom (p) < 0.5f);
    }
    CHECK (curve.pointFor (-6.0f).y < curve.pointFor (-20.0f).y);

    const auto status = ui.pmEditor().getChainPanel().getStatusText (ModuleId::compressor);
    CHECK (status.startsWith (juce::String (juce::CharPointer_UTF8 ("Bus \xc2\xb7 \xe2\x88\x92" "1"))));
    CHECK (status.endsWith (" dB"));
    CHECK (page.subtitle().startsWith ("Estilo Bus"));
    CHECK (page.subtitle().endsWith (juce::String (juce::CharPointer_UTF8 ("dB de reducci\xc3\xb3n"))));

    // The meter keeps running while another module is shown: the chain row stays live.
    ui.pmEditor().selectModule (ModuleId::eq);
    setParameter (ui.processor, param::id::compThreshold, 0.0f);
    ui.play (5.0, 0.5f); // Bus keeps part of the reduction for a while after long compression
    CHECK (page.getShownReductionDb() == Catch::Approx (0.0f).margin (0.05));
    CHECK (ui.pmEditor().getChainPanel().getStatusText (ModuleId::compressor)
           == juce::String (juce::CharPointer_UTF8 ("Bus \xc2\xb7 0,0 dB")));
}

TEST_CASE ("Compressor page: an external sidechain without signal is pointed out", "[ui][comp][sidechain][phase2]")
{
    CompUi ui;
    setParameter (ui.processor, param::id::compThreshold, -20.0f);
    setParameter (ui.processor, param::id::compSidechain, 1.0f);
    ui.play (1.0);
    CHECK (ui.page->getLevelGraph().isShowingSidechainWarning());
    CHECK (ui.page->getShownReductionDb() == Catch::Approx (0.0f)); // and it does not compress

    const auto key = makeSine (2, 48000, 200.0, 48000.0, 1.0f);
    ui.play (1.0, 0.1f, &key);
    CHECK_FALSE (ui.page->getLevelGraph().isShowingSidechainWarning());
    CHECK (ui.page->getShownReductionDb() < -10.0f);
}

TEST_CASE ("Sidechain EQ display: drag, wheel, Alt+click and typing change its filters", "[ui][comp][sidechain][phase2]")
{
    CompUi ui;
    auto& view = ui.page->getSidechainEq();
    REQUIRE (view.getWidth() == 200);
    REQUIRE (view.getHeight() == 40);

    // Default: high-pass at 80 Hz, 24 dB/oct; low-pass open; bell flat.
    CHECK (view.describe (Filter::highPass) == juce::String (juce::CharPointer_UTF8 ("HP 80,0 Hz \xc2\xb7 24 dB/oct")));
    CHECK (view.describe (Filter::lowPass) == "LP abierto");
    CHECK (view.filterAt (view.pointOf (Filter::highPass)) == (int) Filter::highPass);
    CHECK (view.xForFrequency (view.frequencyForX (77.0f)) == Catch::Approx (77.0f));

    // Drag the high-pass to 200 Hz with the mouse.
    const auto start = view.pointOf (Filter::highPass);
    view.mouseDown (mouseEvent (view, start, juce::ModifierKeys::leftButtonModifier));
    view.mouseDrag (mouseEvent (view, { view.xForFrequency (200.0f), start.y }, juce::ModifierKeys::leftButtonModifier));
    view.mouseUp (mouseEvent (view, { view.xForFrequency (200.0f), start.y }, {}));
    CHECK (ui.value (param::id::compScHpFreq) == Catch::Approx (200.0f).epsilon (0.01));

    // Wheel: one slope step per notch (24 → 36 dB/oct).
    view.stepWheel (Filter::highPass, 1);
    CHECK (juce::roundToInt (ui.value (param::id::compScHpSlope)) == 4);
    juce::MouseWheelDetails notch {};
    notch.deltaY = 0.234f;
    view.mouseWheelMove (mouseEvent (view, view.pointOf (Filter::highPass), {}), notch);
    CHECK (juce::roundToInt (ui.value (param::id::compScHpSlope)) == 5);

    // The bell: vertical drag sets its gain, wheel its Q.
    view.dragTo (Filter::bell, { view.xForFrequency (3000.0f), view.yForDb (6.0f) });
    CHECK (ui.value (param::id::compScBellFreq) == Catch::Approx (3000.0f).epsilon (0.01));
    CHECK (ui.value (param::id::compScBellGain) == Catch::Approx (6.0f).margin (0.15));
    view.stepWheel (Filter::bell, 2);
    CHECK (ui.value (param::id::compScBellQ) == Catch::Approx (1.12f * 1.12f).epsilon (0.01));

    // Alt+click turns a filter off and on.
    const auto hp = view.pointOf (Filter::highPass);
    view.mouseDown (mouseEvent (view, hp, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier)));
    CHECK (ui.value (param::id::compScHpOn) == 0.0f);
    CHECK (view.describe (Filter::highPass) == "HP apagado");
    view.toggle (Filter::highPass);
    CHECK (ui.value (param::id::compScHpOn) == 1.0f);

    // Double click: type the frequency (also as a note).
    view.mouseDoubleClick (mouseEvent (view, view.pointOf (Filter::lowPass), {}));
    REQUIRE (view.isTyping());
    auto* editor = dynamic_cast<juce::TextEditor*> (view.getChildComponent (0));
    REQUIRE (editor != nullptr);
    editor->setText ("8k");
    editor->onReturnKey();
    CHECK (ui.value (param::id::compScLpFreq) == Catch::Approx (8000.0f).epsilon (0.001));
    CHECK (view.describe (Filter::lowPass).startsWith ("LP 8,00 kHz"));
}
