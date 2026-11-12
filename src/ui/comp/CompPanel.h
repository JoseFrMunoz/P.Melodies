// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/comp/CompGraphs.h"
#include "ui/comp/SidechainEqView.h"
#include "ui/components/ChipSelector.h"
#include "ui/components/HoldToggleButton.h"
#include "ui/components/Knob.h"
#include "ui/components/SegmentedSelector.h"
#include "ui/components/ToggleSwitch.h"

#include <array>
#include <memory>

namespace pm
{
class PMelodiesProcessor;
}

namespace pm::ui
{

/**
    The compressor module's page (design 02):

        Compresor  Estilo Bus · −3,2 dB de reducción                 OVERSAMPLING [Off 2x 4x]
        [Clean] [Classic] [Opto] [Vocal] [Mastering] [Bus] [Punch] [Pumping]
        ┌ level graph ──────────────────────────────┐ ┌ transfer curve ┐
        Threshold Ratio Knee Range Attack Release Lookahead Hold Makeup Mix
        SIDECHAIN [sidechain EQ] [Interno|Externo] [Escuchar]      Auto-gain  Auto-release
*/
class CompPanel final : public juce::Component
{
public:
    explicit CompPanel (PMelodiesProcessor& processor);
    ~CompPanel() override;

    /** 30 times a second while visible: values changed elsewhere, curve, graph, warnings. */
    void refresh();

    /** Always (also while hidden): takes the meter points, so the chain row stays up to date. */
    void updateMeter();

    /** Gain reduction shown in the header and the chain row (the deepest of the last 0.3 s). */
    float getShownReductionDb() const noexcept { return graph.recentReductionDb(); }

    /** "Estilo Bus · −3,2 dB de reducción". */
    juce::String subtitle() const;

    /** "Bus · −3,2 dB", for the chain panel. */
    juce::String chainStatus() const;

    // For tests and screenshots.
    ChipSelector& getStyleSelector() noexcept { return styles; }
    SegmentedSelector& getOversamplingSelector() noexcept { return oversampling; }
    SegmentedSelector& getSourceSelector() noexcept { return source; }
    HoldToggleButton& getListenButton() noexcept { return listen; }
    ToggleSwitch& getAutoGainSwitch() noexcept { return autoGain; }
    ToggleSwitch& getAutoReleaseSwitch() noexcept { return autoRelease; }
    CompLevelGraph& getLevelGraph() noexcept { return graph; }
    CompCurveView& getCurveView() noexcept { return curve; }
    SidechainEqView& getSidechainEq() noexcept { return sidechainEq; }
    Knob& getKnob (int index) noexcept { return *knobs[(size_t) index]; }
    Knob* findKnob (const juce::String& parameterId) noexcept;

    static constexpr int numKnobs = 10;
    static constexpr int graphHeight = 196;
    static constexpr int curveWidth = 200;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    float read (const char* parameterId) const;

    PMelodiesProcessor& processor;
    SegmentedSelector oversampling;
    ChipSelector styles;
    CompLevelGraph graph;
    CompCurveView curve;
    std::array<std::unique_ptr<Knob>, numKnobs> knobs;
    SidechainEqView sidechainEq;
    SegmentedSelector source;
    HoldToggleButton listen;
    ToggleSwitch autoGain, autoRelease;

    juce::Rectangle<int> headerArea, oversamplingLabel, sidechainCard, sidechainLabel;
    juce::String shownSubtitle;
};

} // namespace pm::ui
