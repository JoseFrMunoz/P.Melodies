// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace pm
{

PMelodiesEditor::PMelodiesEditor (PMelodiesProcessor& p) : juce::AudioProcessorEditor (p), processor (p)
{
    setSize (baseWidth, baseHeight);
}

PMelodiesEditor::~PMelodiesEditor() = default;

void PMelodiesEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f1115));
}

void PMelodiesEditor::resized() {}

} // namespace pm
