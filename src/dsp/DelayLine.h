// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace pm::dsp
{

/**
    Multichannel delay line with an integer delay in samples.

    Used for latency compensation: every path that has to stay aligned with another one is
    delayed by a whole number of samples, so alignment is bit-exact. Memory is only allocated
    in prepare(); every other method is real-time safe.
*/
class DelayLine
{
public:
    /** Allocates room for up to maxDelaySamples of delay on numChannels channels. */
    void prepare (int numChannels, int maxDelaySamples)
    {
        jassert (numChannels > 0 && maxDelaySamples >= 0);
        maxDelay = maxDelaySamples;
        size = maxDelaySamples + 1;
        buffer.setSize (numChannels, size, false, true, false);
        delay = juce::jmin (delay, maxDelay);
        reset();
    }

    /** Clears the stored history. */
    void reset() noexcept
    {
        buffer.clear();
        writePosition = 0;
    }

    /** Sets the delay. Must not exceed the maximum given to prepare(). Not click-free on its own. */
    void setDelay (int newDelaySamples) noexcept
    {
        jassert (newDelaySamples >= 0 && newDelaySamples <= maxDelay);
        delay = juce::jlimit (0, maxDelay, newDelaySamples);
    }

    int getDelay() const noexcept { return delay; }
    int getMaximumDelay() const noexcept { return maxDelay; }

    /** Delays the block in place. */
    void process (juce::dsp::AudioBlock<float> block) noexcept
    {
        process (juce::dsp::AudioBlock<const float> (block), block);
    }

    /**
        Writes input into the line and the delayed signal into output. Input and output may be
        the same block; they must have the same size and at most the prepared channel count.
    */
    void process (const juce::dsp::AudioBlock<const float>& input, juce::dsp::AudioBlock<float> output) noexcept
    {
        jassert (input.getNumSamples() == output.getNumSamples());
        jassert (input.getNumChannels() == output.getNumChannels());
        jassert ((int) input.getNumChannels() <= buffer.getNumChannels());

        const auto numSamples = (int) input.getNumSamples();
        const auto numChannels = juce::jmin ((int) input.getNumChannels(), buffer.getNumChannels());

        if (numSamples == 0)
            return;

        if (delay == 0)
        {
            // Keep the history up to date so a later setDelay() still finds valid samples.
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* line = buffer.getWritePointer (ch);
                const auto* in = input.getChannelPointer ((size_t) ch);
                auto* out = output.getChannelPointer ((size_t) ch);
                auto w = writePosition;

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto x = in[i];
                    line[w] = x;
                    out[i] = x;
                    if (++w == size)
                        w = 0;
                }
            }
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* line = buffer.getWritePointer (ch);
                const auto* in = input.getChannelPointer ((size_t) ch);
                auto* out = output.getChannelPointer ((size_t) ch);
                auto w = writePosition;
                auto r = w - delay;
                if (r < 0)
                    r += size;

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto x = in[i];
                    line[w] = x;
                    out[i] = line[r];
                    if (++w == size)
                        w = 0;
                    if (++r == size)
                        r = 0;
                }
            }
        }

        writePosition = (writePosition + numSamples) % size;
    }

    /** Copies history and delay from another line prepared with the same sizes. */
    void copyStateFrom (const DelayLine& other) noexcept
    {
        jassert (other.size == size && other.buffer.getNumChannels() == buffer.getNumChannels());

        if (other.size != size || other.buffer.getNumChannels() != buffer.getNumChannels())
            return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, 0, other.buffer, ch, 0, size);

        writePosition = other.writePosition;
        delay = other.delay;
    }

private:
    juce::AudioBuffer<float> buffer;
    int size = 1;
    int maxDelay = 0;
    int delay = 0;
    int writePosition = 0;
};

} // namespace pm::dsp
