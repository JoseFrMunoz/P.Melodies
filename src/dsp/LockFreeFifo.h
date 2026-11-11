// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace pm::dsp
{

/**
    Single-producer, single-consumer lock-free FIFO of trivially copyable items.

    Hands data from the audio thread to the interface: samples for the spectrum analyser, level
    histories for the compressor graph, waveforms for the distortion display.

    - push() and pop() never block, never allocate and never take a lock (juce::AbstractFifo
      keeps the read and write positions in atomics).
    - When the FIFO is full, push() writes what fits and returns how many items it wrote: the
      audio thread never waits for the interface, which simply misses a few items.
    - Exactly one thread may push and exactly one thread may pop. prepare() and reset() must be
      called while neither is running (for example in prepareToPlay).
*/
template <typename T>
class LockFreeFifo
{
    static_assert (std::is_trivially_copyable_v<T>, "items are copied with plain assignment");

public:
    /** Allocates room for `capacity` items. Not real-time safe. */
    void prepare (int capacity)
    {
        jassert (capacity > 0);
        storage.assign ((size_t) capacity + 1, T {}); // AbstractFifo holds one item less than its size
        fifo.setTotalSize (capacity + 1);
        fifo.reset();
    }

    /** Empties the FIFO. Only while neither thread is using it. */
    void reset() noexcept { fifo.reset(); }

    int getCapacity() const noexcept { return fifo.getTotalSize() - 1; }
    int getNumReady() const noexcept { return fifo.getNumReady(); }
    int getFreeSpace() const noexcept { return fifo.getFreeSpace(); }

    /** Producer thread. Writes up to `count` items and returns how many fitted. */
    int push (const T* items, int count) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (count, start1, size1, start2, size2);
        std::copy_n (items, size1, storage.begin() + start1);
        std::copy_n (items + size1, size2, storage.begin() + start2);
        fifo.finishedWrite (size1 + size2);
        return size1 + size2;
    }

    /** Consumer thread. Reads up to `count` items into `destination`, oldest first. */
    int pop (T* destination, int count) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (count, start1, size1, start2, size2);
        std::copy_n (storage.begin() + start1, size1, destination);
        std::copy_n (storage.begin() + start2, size2, destination + size1);
        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

    /** Consumer thread. Drops up to `count` of the oldest items, for example to catch up. */
    int discard (int count) noexcept
    {
        const auto n = juce::jmin (count, fifo.getNumReady());
        fifo.finishedRead (n);
        return n;
    }

private:
    juce::AbstractFifo fifo { 1 };
    std::vector<T> storage;
};

} // namespace pm::dsp
