// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace pm
{

namespace
{
std::array<juce::RangedAudioParameter*, numModules> enabledParameters (PMelodiesProcessor& p)
{
    std::array<juce::RangedAudioParameter*, numModules> result {};
    for (int i = 0; i < numModules; ++i)
        result[(size_t) i] = p.getState().getParameter (param::moduleEnabledId (moduleAt (i)));
    return result;
}
} // namespace

PMelodiesEditor::PMelodiesEditor (PMelodiesProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      chainPanel (*p.getState().getParameter (param::id::chainOrder), enabledParameters (p)),
      footer (p)
{
    setLookAndFeel (&lookAndFeel);
    setTitle ("P.Melodies");

    header.onSettingsClicked = [this] { showSettings (! isShowingSettings()); };
    footer.onQualityClicked = [this] { showSettings (true); };
    chainPanel.onModuleSelected = [this] (ModuleId id) { selectModule (id); };

    addAndMakeVisible (header);
    addAndMakeVisible (chainPanel);
    addAndMakeVisible (modulePanel);
    addAndMakeVisible (footer);

    selectModule (processor.getSelectedModule());

    setSize (baseWidth, baseHeight);

    lastRefresh = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (refreshRateHz);
}

PMelodiesEditor::~PMelodiesEditor()
{
    stopTimer();
    settings.reset();
    setLookAndFeel (nullptr);
}

void PMelodiesEditor::paint (juce::Graphics& g)
{
    g.fillAll (lookAndFeel.getTheme().colours.background);
}

void PMelodiesEditor::resized()
{
    const auto& m = lookAndFeel.getTheme().metrics;
    auto area = getLocalBounds();

    header.setBounds (area.removeFromTop (m.headerHeight));
    footer.setBounds (area.removeFromBottom (m.footerHeight));
    chainPanel.setBounds (area.removeFromLeft (m.chainPanelWidth));
    modulePanel.setBounds (area);

    if (settings != nullptr)
        settings->setBounds (getLocalBounds().withTrimmedTop (m.headerHeight));
}

void PMelodiesEditor::selectModule (ModuleId id)
{
    processor.setSelectedModule (id);
    chainPanel.setSelectedModule (id);
    modulePanel.setModule (id);
}

void PMelodiesEditor::showSettings (bool shouldShow)
{
    if (shouldShow == isShowingSettings())
        return;

    if (shouldShow)
    {
        settings = std::make_unique<ui::SettingsOverlay> (processor);
        settings->onClose = [this]
        {
            // Deferred: the overlay may be inside one of its own callbacks.
            juce::Component::SafePointer<PMelodiesEditor> safeThis (this);
            juce::MessageManager::callAsync ([safeThis]
                                             {
                                                 if (safeThis != nullptr)
                                                     safeThis->showSettings (false);
                                             });
        };
        addAndMakeVisible (*settings);
        resized();
        settings->grabKeyboardFocus();
    }
    else
    {
        settings.reset();
    }

    header.setSettingsOpen (shouldShow);
}

bool PMelodiesEditor::keyPressed (const juce::KeyPress& key)
{
    // Esc closes menus and panels. No shortcut ever saves a preset (spec §8.2).
    if (key == juce::KeyPress::escapeKey && isShowingSettings())
    {
        showSettings (false);
        return true;
    }
    return false;
}

void PMelodiesEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = juce::jlimit (0.0, 0.25, (now - lastRefresh) * 0.001);
    lastRefresh = now;
    footer.refresh (elapsed);
}

} // namespace pm
