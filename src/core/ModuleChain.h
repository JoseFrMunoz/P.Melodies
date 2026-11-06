// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/ChainOrder.h"
#include "core/ModuleBase.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace pm
{

/**
    The four modules in a reorderable series chain.

    Click-free reordering: the chain owns two complete sets of modules. While idle only the active
    set runs. When the order changes, the spare set copies the active set's state, starts
    processing the new order (a short pre-roll that is not heard, to refill its filters) and then
    the output crossfades from the old order to the new one over 20 ms. Both sets have the same
    constant latency, so they are sample-aligned during the crossfade.
*/
class ModuleChain
{
public:
    using Factory = std::function<std::unique_ptr<ModuleBase> (ModuleId)>;

    static constexpr double crossfadeSeconds = 0.020;

    explicit ModuleChain (const Factory& factory);

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept;

    /** Sum of the module latencies. Constant between prepare() calls and independent of the order. */
    int getLatencySamples() const noexcept { return latency; }

    /** Requests a new order (permutation index). Applied by the next process() call. */
    void setOrder (int orderIndex) noexcept { requestedOrder = juce::jlimit (0, chain::numOrders - 1, orderIndex); }

    /** Order currently heard (the target order once a crossfade has finished). */
    int getActiveOrder() const noexcept { return activeOrder; }

    bool isReordering() const noexcept { return phase != Phase::idle; }

    void setModuleBypassed (ModuleId id, bool shouldBeBypassed) noexcept;
    void setMinimumOversamplingIndex (int index) noexcept;

    /** Module of the active set (for state queries; process() owns it on the audio thread). */
    ModuleBase& getModule (ModuleId id) noexcept { return *sets[(size_t) activeSet][(size_t) toIndex (id)]; }

    /** Samples between a reorder request and the start of the crossfade. */
    int getPreRollSamples() const noexcept { return preRollLength; }

private:
    enum class Phase
    {
        idle,
        preRoll,
        crossfade
    };

    using ModuleSet = std::array<std::unique_ptr<ModuleBase>, numModules>;

    void runSet (int setIndex, int orderIndex, juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept;
    void beginTransition() noexcept;

    std::array<ModuleSet, 2> sets;
    int activeSet = 0;

    int requestedOrder = chain::defaultOrderIndex;
    int activeOrder = chain::defaultOrderIndex;
    int targetOrder = chain::defaultOrderIndex;

    Phase phase = Phase::idle;
    int preRollRemaining = 0;
    int preRollLength = 0;
    float fade = 0.0f;
    float fadeStep = 1.0f;

    int latency = 0;
    int maxBlockSize = 0;
    juce::AudioBuffer<float> spareBuffer;
    std::vector<float> fadeGains;
};

} // namespace pm
