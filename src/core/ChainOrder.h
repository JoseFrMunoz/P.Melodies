// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>

namespace pm::chain
{

/**
    Processing order: position 0 is processed first.

    The order is not an automatable parameter (decision §12, 2026-11-11): it is a list of modules
    saved in the plugin state, so adding modules later never breaks projects or presets. It
    reaches the audio thread packed in one atomic word (pack()).
*/
using Order = std::array<ModuleId, numModules>;

/** EQ → Compressor → Saturation → Distortion (and any later module after them). */
Order defaultOrder() noexcept;

/** True when every module appears exactly once. */
bool isValid (const Order& order) noexcept;

/** Moves the module at fromPosition so that it ends up at toPosition (drag and drop). */
Order move (Order order, int fromPosition, int toPosition) noexcept;

/** Position of a module inside an order. */
int positionOf (const Order& order, ModuleId id) noexcept;

/** Short readable form for the interface and the tests: "EQ > Comp > Sat > Dist". */
juce::String describe (const Order& order);

/** Stable key of a module in the saved state ("eq", "compressor"…): never change them. */
const char* keyOf (ModuleId id) noexcept;

/** Form saved in the state: the keys separated by commas, "eq,compressor,saturation,distortion". */
juce::String toText (const Order& order);

/**
    Reads toText(). Unknown keys and repeats are ignored and missing modules are added after the
    rest in their default order, so a state saved before a module existed still loads.
*/
Order fromText (const juce::String& text);

/** Lock-free form for the audio thread: 4 bits per position. */
std::uint64_t pack (const Order& order) noexcept;

/** Inverse of pack(); anything that is not a valid order gives the default order. */
Order unpack (std::uint64_t packed) noexcept;

/**
    Builds of phase 0 and 1 saved the order as an automatable parameter: an index from 0 to 23
    (the permutations of the first four modules, Lehmer code). Only used to read those projects.
*/
Order fromLegacyIndex (int index) noexcept;

static_assert (numModules <= 16, "pack() keeps each position in 4 bits");

} // namespace pm::chain
