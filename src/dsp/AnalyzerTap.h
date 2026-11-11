// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/LockFreeFifo.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace pm::dsp
{

/**
    Feeds a module's input or output (mono mix) to the interface's spectrum analyser through the
    lock-free FIFO of phase 0. One producer (the audio thread, only the module set that is heard)
    and one consumer (the interface). When the FIFO is full because nobody reads it (window
    closed), the new samples are simply dropped.
*/
class AnalyzerTap
{
public:
    enum class Point
    {
        off,
        pre,  // module input
        post  // module output
    };

    void prepare (double sampleRate, int capacity)
    {
        fifo.prepare (capacity);
        rate.store (sampleRate);
    }

    void setPoint (Point p) noexcept { point.store ((int) p); }
    Point getPoint() const noexcept { return (Point) point.load(); }

    /** Sample rate of the samples in the FIFO (any thread). */
    double getSampleRate() const noexcept { return rate.load(); }

    /** Audio thread: pushes the mono mix of the block if `at` is the selected point. */
    void push (juce::dsp::AudioBlock<const float> block, Point at) noexcept
    {
        if (at != getPoint() || block.getNumChannels() == 0)
            return;

        std::array<float, 256> mono {};
        const auto numChannels = block.getNumChannels();
        const auto scale = 1.0f / (float) numChannels;
        for (size_t start = 0; start < block.getNumSamples(); start += mono.size())
        {
            const auto n = juce::jmin (mono.size(), block.getNumSamples() - start);
            for (size_t i = 0; i < n; ++i)
            {
                float sum = 0.0f;
                for (size_t ch = 0; ch < numChannels; ++ch)
                    sum += block.getSample ((int) ch, (int) (start + i));
                mono[i] = sum * scale;
            }
            fifo.push (mono.data(), (int) n);
        }
    }

    /** Interface thread: takes up to `count` samples, returns how many it got. */
    int pop (float* destination, int count) noexcept { return fifo.pop (destination, count); }
    int getNumReady() const noexcept { return fifo.getNumReady(); }

private:
    std::atomic<int> point { (int) Point::post };
    std::atomic<double> rate { 44100.0 };
    LockFreeFifo<float> fifo;
};

} // namespace pm::dsp
