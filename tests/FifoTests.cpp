// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "AllocationTracker.h"

#include "dsp/LockFreeFifo.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

using pm::dsp::LockFreeFifo;

TEST_CASE ("FIFO: items come out in order, also across the wrap-around", "[fifo][phase0]")
{
    LockFreeFifo<int> fifo;
    fifo.prepare (10);
    CHECK (fifo.getCapacity() == 10);

    std::vector<int> in (20);
    std::iota (in.begin(), in.end(), 0);
    std::vector<int> out (20, -1);

    CHECK (fifo.push (in.data(), 7) == 7);
    CHECK (fifo.pop (out.data(), 5) == 5);
    CHECK (fifo.push (in.data() + 7, 7) == 7); // wraps around the end of the storage
    CHECK (fifo.getNumReady() == 9);
    CHECK (fifo.pop (out.data() + 5, 20) == 9);

    for (int i = 0; i < 14; ++i)
        CHECK (out[(size_t) i] == i);
    CHECK (fifo.getNumReady() == 0);
}

TEST_CASE ("FIFO: a full FIFO takes what fits and never blocks; an empty one gives nothing", "[fifo][phase0]")
{
    LockFreeFifo<float> fifo;
    fifo.prepare (10);

    std::vector<float> in (15, 1.0f), out (15, 0.0f);
    CHECK (fifo.pop (out.data(), 5) == 0);
    CHECK (fifo.push (in.data(), 15) == 10);
    CHECK (fifo.getFreeSpace() == 0);
    CHECK (fifo.push (in.data(), 1) == 0);

    CHECK (fifo.discard (4) == 4);
    CHECK (fifo.getNumReady() == 6);
    CHECK (fifo.discard (100) == 6);
    CHECK (fifo.getNumReady() == 0);

    CHECK (fifo.push (in.data(), 3) == 3);
    fifo.reset();
    CHECK (fifo.getNumReady() == 0);
    CHECK (fifo.getFreeSpace() == 10);
}

TEST_CASE ("FIFO: push and pop never allocate", "[fifo][realtime][phase0]")
{
    struct LevelPoint
    {
        float input, output, gainReduction;
    };

    LockFreeFifo<LevelPoint> fifo;
    fifo.prepare (256);
    std::vector<LevelPoint> items (300, LevelPoint { 0.5f, 0.25f, -6.0f });
    std::vector<LevelPoint> out (300);

    long long allocations = 0;
    {
        pmtest::ScopedAllocationCounter counter;
        for (int round = 0; round < 100; ++round)
        {
            fifo.push (items.data(), 1 + round % 300);
            fifo.pop (out.data(), 1 + (round * 7) % 300);
        }
        fifo.discard (1000);
        allocations = counter.getCount();
    }
    CHECK (allocations == 0);
}

TEST_CASE ("FIFO: one producer and one consumer thread lose and reorder nothing", "[fifo][phase0]")
{
    constexpr int total = 2'000'000;
    LockFreeFifo<int> fifo;
    fifo.prepare (1024);

    std::thread producer ([&fifo]
    {
        juce::Random random (11);
        std::vector<int> chunk (700);
        int next = 0;
        while (next < total)
        {
            const auto n = juce::jmin (1 + random.nextInt (700), total - next);
            for (int i = 0; i < n; ++i)
                chunk[(size_t) i] = next + i;
            int written = 0;
            while (written < n) // a real audio thread would drop instead; here every item must arrive
            {
                written += fifo.push (chunk.data() + written, n - written);
                if (written < n)
                    std::this_thread::yield();
            }
            next += n;
        }
    });

    juce::Random random (12);
    std::vector<int> chunk (900);
    int expected = 0;
    bool inOrder = true;
    while (expected < total)
    {
        const auto n = fifo.pop (chunk.data(), 1 + random.nextInt (900));
        for (int i = 0; i < n; ++i)
            inOrder = inOrder && chunk[(size_t) i] == expected + i;
        expected += n;
        if (n == 0)
            std::this_thread::yield();
    }

    producer.join();
    CHECK (inOrder);
    CHECK (expected == total);
    CHECK (fifo.getNumReady() == 0);
}
