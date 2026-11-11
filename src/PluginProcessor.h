// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/AudioEngine.h"
#include "core/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace pm
{

/**
    The JUCE AudioProcessor: owns the parameters and the audio engine, handles bus layouts and
    saves/restores the plugin state in the FL Studio project.
*/
class PMelodiesProcessor final : public juce::AudioProcessor
{
public:
    /** Version of the saved-state format. Increase it and extend migrateState() when it changes. */
    static constexpr int stateVersion = 1;

    static inline const juce::Identifier stateType { "PMelodiesState" };
    static inline const juce::Identifier stateVersionProperty { "stateVersion" };
    static inline const juce::Identifier pluginVersionProperty { "pluginVersion" };
    static inline const juce::Identifier selectedModuleProperty { "selectedModule" };
    static inline const juce::Identifier eqAnalyzerProperty { "eqAnalyzer" };
    static inline const juce::Identifier eqRangeProperty { "eqRange" };

    /** EQ analyser source, as in the selector: Pre, Post or Off. */
    enum class EqAnalyzer
    {
        pre = 0,
        post = 1,
        off = 2
    };

    PMelodiesProcessor();
    ~PMelodiesProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // P.Melodies
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    AudioEngine& getEngine() noexcept { return engine; }

    /** Module shown in the right panel (interface state, saved with the project). Message thread. */
    ModuleId getSelectedModule() const;
    void setSelectedModule (ModuleId id);

    /**
        EQ display settings (§12): saved with the project, not plugin parameters, and loading a
        preset does not change them. Message thread.
    */
    EqAnalyzer getEqAnalyzer() const;
    void setEqAnalyzer (EqAnalyzer mode);
    bool isEqRangeWide() const; // ±30 dB instead of ±12 dB
    void setEqRangeWide (bool wide);

    /**
        Band heard in solo in the EQ (-1 for none). Interface state, not a plugin parameter: it is
        not automated and not saved (§12). Any thread.
    */
    void setEqSoloBand (int band) noexcept { eqSoloBand.store (band); }
    int getEqSoloBand() const noexcept { return eqSoloBand.load(); }

    /** Reported latency in milliseconds, for the footer. */
    double getLatencyMilliseconds() const noexcept;

    /** Fills missing parameters with their defaults and upgrades older state versions. */
    static void migrateState (juce::ValueTree& tree, const juce::AudioProcessor& processor);

private:
    static BusesProperties makeBusesProperties();
    void applyEqAnalyzer();

    juce::AudioProcessorValueTreeState state;
    std::atomic<int> eqSoloBand { -1 };
    param::ParameterReader reader;
    AudioEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PMelodiesProcessor)
};

} // namespace pm
