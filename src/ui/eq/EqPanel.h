// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/components/SegmentedSelector.h"
#include "ui/eq/EqBandPanel.h"
#include "ui/eq/EqController.h"
#include "ui/eq/EqGraph.h"

namespace pm::ui
{

/**
    The EQ module's page (design 01): header with the band count, Character, Analyser and scale
    selectors; the display; and the selected band's editor.
*/
class EqPanel final : public juce::Component
{
public:
    explicit EqPanel (PMelodiesProcessor& processor);

    /** 30 times a second from the editor: analyser, curves, values changed by the host. */
    void refresh();

    /** "5 de 8 bandas · latencia cero". */
    juce::String subtitle() const;

    // For tests and screenshots.
    EqController& getController() noexcept { return controller; }
    EqGraph& getGraph() noexcept { return graph; }
    EqBandPanel& getBandPanel() noexcept { return bandPanel; }
    SegmentedSelector& getCharacterSelector() noexcept { return character; }
    SegmentedSelector& getAnalyserSelector() noexcept { return analyser; }
    SegmentedSelector& getRangeSelector() noexcept { return range; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int graphHeight = 300;
    static constexpr int bandPanelHeight = 104;

private:
    EqController controller;
    SegmentedSelector character, analyser, range;
    EqGraph graph;
    EqBandPanel bandPanel;
    juce::Rectangle<int> headerArea, characterLabel, analyserLabel;
    juce::String shownSubtitle;
};

} // namespace pm::ui
