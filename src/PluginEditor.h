// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pm
{

class PMelodiesProcessor;

class PMelodiesEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PMelodiesEditor (PMelodiesProcessor& processor);
    ~PMelodiesEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static constexpr int baseWidth = 1000;
    static constexpr int baseHeight = 640;

private:
    PMelodiesProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PMelodiesEditor)
};

} // namespace pm
