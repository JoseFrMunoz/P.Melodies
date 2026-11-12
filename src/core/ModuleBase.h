// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"
#include "core/ModuleParameters.h"
#include "dsp/AnalyzerTap.h"
#include "dsp/DelayLine.h"
#include "dsp/OversamplingEngine.h"

namespace pm
{

/**
    Shared machinery for every module: constant latency, click-free bypass and oversampling.

    Signal flow inside a module:

        input ─┬─ dry delay (total latency) ─────────────────────────────────┐
               └─ oversampling up → processCore() → down → padding delay ──── wet ─┴─ fade → output

    - The dry path is the module input delayed by exactly the module latency.
    - The wet path is padded so it always has the same latency, whatever the oversampling factor.
    - When the module is bypassed, or its current settings make it an identity ("neutral"), the
      wet path is not computed at all and the output is the dry path: bit-exact and cheap.
    - Engaging fades in after a pre-roll that refills the filters; disengaging fades out.
      Oversampling factor changes go through the same fade-out → switch → pre-roll → fade-in
      sequence, so changing the quality never clicks.

    Concrete modules override the protected hooks. Phase 0 modules are neutral placeholders.
*/
class ModuleBase : public Module
{
public:
    /**
        usesSidechain: the module reads the external sidechain in processCore(). It then receives it
        at the oversampled rate, upsampled with the same filters as the audio (so both stay aligned).
    */
    ModuleBase (ModuleId id, bool usesOversampling, bool usesSidechain = false) noexcept;

    ModuleId getId() const noexcept { return id; }

    // Module interface
    void prepare (const juce::dsp::ProcessSpec& spec) final;
    void reset() final;
    void process (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept final;
    int getLatencySamples() const noexcept final { return totalLatency; }
    void setBypassed (bool shouldBeBypassed) noexcept final { bypassed = shouldBeBypassed; }

    bool isBypassed() const noexcept { return bypassed; }

    /**
        New settings for this block (audio thread, before process()). Each module keeps its own
        part; it must not allocate. Both module sets of the chain receive them.
    */
    virtual void setParameters (const ModuleParameters& /*parameters*/) noexcept {}

    /**
        Spectrum analyser feed (the EQ's). The chain enables it only on the module set that is
        heard, so the FIFO has a single producer.
    */
    void setAnalyzerTap (dsp::AnalyzerTap* tap) noexcept { analyzerTap = tap; }
    void setAnalyzerListening (bool shouldListen) noexcept { analyzerListening = shouldListen; }

    /** Global quality floor (factor index, 0 = 1x … 4 = 16x). Ignored by modules without oversampling. */
    void setMinimumOversamplingIndex (int index) noexcept;

    /** Factor index currently used by the wet path. */
    int getActiveOversamplingIndex() const noexcept { return activeOversamplingIndex; }

    /** True while the wet path contributes to the output. */
    bool isWetPathActive() const noexcept { return stage != Stage::disengaged; }

    /**
        Copies the dynamic state from the same module in another chain, for the reorder crossfade.
        Oversampling filter states are reset instead (JUCE does not expose them); the chain runs a
        pre-roll long enough to refill them before it is heard.
    */
    void copyStateFrom (const ModuleBase& other) noexcept;

    static constexpr double fadeSeconds = 0.010;
    static constexpr int maxSidechainChannels = 2;

protected:
    /** Allocate DSP state. Called once per prepare(), for the base rate and the given channel count. */
    virtual void prepareCore (double /*baseSampleRate*/, int /*maxBlockSize*/, int /*numChannels*/) {}

    /** The wet path now runs at this rate (base rate × oversampling factor). Must not allocate. */
    virtual void setCoreSampleRate (double /*sampleRate*/) noexcept {}

    virtual void resetCore() noexcept {}

    /** Processes the wet path in place, at the oversampled rate. */
    virtual void processCore (juce::dsp::AudioBlock<float> /*block*/, const SidechainBlock* /*sidechain*/) noexcept {}

    /** True when the current settings make processCore() an identity. */
    virtual bool isCoreNeutral() const noexcept { return false; }

    /** The module's own oversampling selector (factor index). */
    virtual int getRequestedOversamplingIndex() const noexcept { return 0; }

    /** Constant extra latency of processCore() (e.g. lookahead), in base-rate samples. */
    virtual int getCoreLatencySamples() const noexcept { return 0; }

    /** Extra samples the core needs after a reset before its output is meaningful. */
    virtual int getCoreSettleSamples() const noexcept { return 64; }

    virtual void copyCoreStateFrom (const ModuleBase& /*other*/) noexcept {}

    /**
        Called at the end of process() on the module set that is heard, at the base rate: the
        input aligned with the output (the dry path), the output, the host's sidechain and whether
        processCore() ran in this block. For meters; must not allocate.
    */
    virtual void observe (juce::dsp::AudioBlock<const float> /*alignedInput*/, juce::dsp::AudioBlock<const float> /*output*/,
                          const SidechainBlock* /*sidechain*/, bool /*wetComputed*/) noexcept {}

private:
    enum class Stage
    {
        disengaged,
        preRoll,
        fadingIn,
        engaged,
        fadingOut
    };

    int desiredOversamplingIndex() const noexcept;
    void updateStageAtBlockStart() noexcept;
    float nextGain() noexcept;
    void applyOversamplingIndex (int index) noexcept;

    const ModuleId id;
    const bool usesOversampling;
    const bool usesSidechain;

    dsp::OversamplingEngine oversampling;
    dsp::OversamplingEngine sidechainOversampling; // upsampling only, for modules that read the sidechain
    dsp::DelayLine dryDelay;
    dsp::DelayLine wetPadding;
    juce::AudioBuffer<float> dryBuffer;

    double baseSampleRate = 44100.0;
    int maxBlockSize = 0;
    int totalLatency = 0;
    int maxOversamplingLatency = 0;
    int preRollSamples = 0;

    bool bypassed = false;
    dsp::AnalyzerTap* analyzerTap = nullptr;
    bool analyzerListening = false;
    int minimumOversamplingIndex = 0;
    int activeOversamplingIndex = 0;

    Stage stage = Stage::disengaged;
    float gain = 0.0f;
    float gainStep = 1.0f;
    int preRollRemaining = 0;
};

} // namespace pm
