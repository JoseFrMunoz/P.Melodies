// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace pm
{

/** The four effect modules. The numeric values are stored in presets: never reorder them. */
enum class ModuleId : int
{
    eq = 0,
    compressor = 1,
    saturation = 2,
    distortion = 3
};

inline constexpr int numModules = 4;

constexpr int toIndex (ModuleId id) noexcept { return static_cast<int> (id); }
constexpr ModuleId moduleAt (int index) noexcept { return static_cast<ModuleId> (index); }

/** Read-only view of the (optional) external sidechain input, at the base sample rate. */
using SidechainBlock = juce::dsp::AudioBlock<const float>;

/**
    Common interface of the four effect modules.

    Contract:
    - prepare() is the only place that may allocate.
    - process() must be real-time safe: no allocation, locks, I/O or logging.
    - getLatencySamples() is constant between two prepare() calls, whatever the parameters are.
      Modules pad themselves internally so that the plugin latency never changes while playing.
    - setBypassed() must not click: the module fades to its latency-aligned input.
*/
class Module
{
public:
    virtual ~Module() = default;

    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept = 0;
    virtual int getLatencySamples() const noexcept = 0;
    virtual void setBypassed (bool shouldBeBypassed) noexcept = 0;
};

} // namespace pm
