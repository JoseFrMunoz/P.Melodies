// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "ui/layout/ChainPanel.h"
#include "ui/layout/FooterBar.h"
#include "ui/layout/HeaderBar.h"
#include "ui/layout/ModulePanel.h"
#include "ui/layout/SettingsPanel.h"
#include "ui/theme/LookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace pm
{

class PMelodiesProcessor;

/**
    Plugin window (1000 × 640): header, chain panel on the left, selected module on the right and
    the global section in the footer. The interface reads the audio thread's meters through
    atomics on a 30 Hz timer and never blocks the audio thread.
*/
class PMelodiesEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PMelodiesEditor (PMelodiesProcessor& processor);
    ~PMelodiesEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void showSettings (bool shouldShow);
    bool isShowingSettings() const noexcept { return settings != nullptr; }

    void selectModule (ModuleId id);

    /** Follows the plugin (meters, chain order…). The timer calls it 30 times a second; public for tests. */
    void refresh();

    // Access for tests and screenshots.
    ui::ChainPanel& getChainPanel() noexcept { return chainPanel; }
    ui::ModulePanel& getModulePanel() noexcept { return modulePanel; }
    ui::FooterBar& getFooter() noexcept { return footer; }

    static constexpr int baseWidth = 1000;
    static constexpr int baseHeight = 640;
    static constexpr int refreshRateHz = 30;

private:
    void timerCallback() override;

    PMelodiesProcessor& processor;
    ui::LookAndFeel lookAndFeel;

    ui::HeaderBar header;
    ui::ChainPanel chainPanel;
    ui::ModulePanel modulePanel;
    ui::FooterBar footer;
    std::unique_ptr<ui::SettingsOverlay> settings;
    juce::TooltipWindow tooltips { this, 600 };

    double lastRefresh = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PMelodiesEditor)
};

} // namespace pm
