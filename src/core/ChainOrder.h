// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/Module.h"

#include <juce_core/juce_core.h>

#include <array>

namespace pm::chain
{

/** Processing order: position 0 is processed first. */
using Order = std::array<ModuleId, numModules>;

/** 4! permutations. */
inline constexpr int numOrders = 24;

/** Index 0: EQ → Compressor → Saturation → Distortion. */
inline constexpr int defaultOrderIndex = 0;

/**
    Permutation index → order, using the Lehmer code (factorial number system).
    Index 0 is the identity order; indices are stable and are stored in projects and presets.
*/
Order decode (int index) noexcept;

/** Order → permutation index. Inverse of decode(). */
int encode (const Order& order) noexcept;

/** Moves the module at fromPosition so that it ends up at toPosition (drag and drop). */
Order move (Order order, int fromPosition, int toPosition) noexcept;

/** Position of a module inside an order. */
int positionOf (const Order& order, ModuleId id) noexcept;

/** Short readable form used as the automation value text: "EQ > Comp > Sat > Dist". */
juce::String describe (int index);

} // namespace pm::chain
