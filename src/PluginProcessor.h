// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/AudioEngine.h"
#include "core/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>

namespace pm
{

/**
    The JUCE AudioProcessor: owns the parameters and the audio engine, handles bus layouts and
    saves/restores the plugin state in the FL Studio project.
*/
class PMelodiesProcessor final : public juce::AudioProcessor,
                                 private juce::ValueTree::Listener
{
public:
    /**
        Version of the saved-state format. Increase it and extend migrateState() when it changes.
        2: the chain order moved from the "chainOrder" parameter to the chainOrder property.
    */
    static constexpr int stateVersion = 2;

    static inline const juce::Identifier stateType { "PMelodiesState" };
    static inline const juce::Identifier stateVersionProperty { "stateVersion" };
    static inline const juce::Identifier pluginVersionProperty { "pluginVersion" };
    static inline const juce::Identifier selectedModuleProperty { "selectedModule" };
    static inline const juce::Identifier eqAnalyzerProperty { "eqAnalyzer" };
    static inline const juce::Identifier eqRangeProperty { "eqRange" };
    static inline const juce::Identifier chainOrderProperty { "chainOrder" };

    /** EQ analyser source, as in the selector: Pre, Post or Off. */
    enum class EqAnalyzer
    {
        pre = 0,
        post = 1,
        off = 2
    };

    PMelodiesProcessor();
    ~PMelodiesProcessor() override;

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
        Order of the module chain. It is part of the state (projects, and presets and undo from
        phase 5) but not an automatable parameter (§12, 2026-11-11). Set it on the message thread;
        it reaches the audio thread lock-free and the chain crossfades to it without clicks.
    */
    chain::Order getChainOrder() const noexcept { return chain::unpack (packedChainOrder.load()); }
    void setChainOrder (const chain::Order& order);

    /**
        EQ display settings (§12): saved with the project, not plugin parameters, and loading a
        preset does not change them. Message thread.
    */
    EqAnalyzer getEqAnalyzer() const;
    void setEqAnalyzer (EqAnalyzer mode);

    /** Gain scales of the EQ display, in dB (±6, ±12 or ±30). ±12 by default. */
    static constexpr std::array<int, 3> eqRangesDb { 6, 12, 30 };
    int getEqRangeDb() const;
    /** Snaps to the smallest scale that holds rangeDb. */
    void setEqRangeDb (int rangeDb);
    /** Index of the current scale in eqRangesDb. */
    int getEqRangeIndex() const;

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

    /** Parameters plus the chain order and the EQ solo band, as the engine reads them each block. */
    EngineParameters readEngineParameters() const noexcept;

    /** Copies the order from the state tree to the audio thread. */
    void syncChainOrder();

    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeRedirected (juce::ValueTree& tree) override;

    juce::AudioProcessorValueTreeState state;
    std::atomic<int> eqSoloBand { -1 };
    param::ParameterReader reader;
    AudioEngine engine;
    std::atomic<std::uint64_t> packedChainOrder { chain::pack (chain::defaultOrder()) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PMelodiesProcessor)
};

} // namespace pm
