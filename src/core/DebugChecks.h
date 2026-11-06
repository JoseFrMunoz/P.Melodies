// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <cmath>

namespace pm::debug
{

/** Debug builds only: asserts that every sample in the block is finite (no NaN or Inf). */
inline void checkFinite ([[maybe_unused]] const juce::dsp::AudioBlock<const float>& block,
                         [[maybe_unused]] const char* where) noexcept
{
#if JUCE_DEBUG
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        const auto* x = block.getChannelPointer (ch);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            if (! std::isfinite (x[i]))
            {
                juce::ignoreUnused (where);
                jassertfalse; // Non-finite sample: see `where`.
                return;
            }
        }
    }
#endif
}

inline void checkFinite (const juce::dsp::AudioBlock<float>& block, const char* where) noexcept
{
    checkFinite (juce::dsp::AudioBlock<const float> (block), where);
}

} // namespace pm::debug
