// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Spec §3.1: no memory allocation on the audio thread.

#include "AllocationTracker.h"
#include "TestHelpers.h"

#include "core/Parameters.h"

#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;

TEST_CASE ("The tracker itself detects allocations", "[realtime][phase0]")
{
    long long allocations = 0;
    {
        ScopedAllocationCounter counter;
        // Direct calls to the allocation functions cannot be optimised away like a new-expression.
        void* p = ::operator new (64);
        ::operator delete (p);
        allocations = counter.getCount();
    }
    CHECK (allocations >= 1);
}

TEST_CASE ("processBlock never allocates, whatever the block size and settings", "[realtime][phase0]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);

    const int maxSize = 3000; // larger than the prepared size: exercises the internal split
    juce::AudioBuffer<float> buffer (4, maxSize);
    juce::MidiBuffer midi;
    juce::Random random (1);

    const auto processBlocks = [&]
    {
        long long allocations = 0;
        for (const auto n : { 1, 7, 512, maxSize, 64, 33 })
        {
            juce::AudioBuffer<float> io (buffer.getArrayOfWritePointers(), 4, n);
            for (int ch = 0; ch < 4; ++ch)
                for (int i = 0; i < n; ++i)
                    io.setSample (ch, i, 0.5f * (2.0f * random.nextFloat() - 1.0f));

            ScopedAllocationCounter counter;
            processor.processBlock (io, midi);
            allocations += counter.getCount();
        }
        return allocations;
    };

    CHECK (processBlocks() == 0);

    // Settings changes happen on another thread in a host; only processing is counted.
    for (int round = 0; round < 30; ++round)
    {
        processor.setChainOrder (chain::fromLegacyIndex (round % 24));
        setParameter (processor, param::id::compare, (float) (round % 2));
        setParameter (processor, param::id::bypass, (float) ((round / 3) % 2));
        setParameter (processor, param::id::clipperEnabled, (float) ((round / 2) % 2));
        setParameter (processor, param::id::qualityRealtime, (float) (round % 3));
        setParameter (processor, param::id::eqEnabled, (float) ((round / 5) % 2));
        setParameter (processor, param::id::mix, (float) (round * 3 % 100));
        setParameter (processor, param::id::inputGain, (float) (round - 15));
        processor.setNonRealtime (round % 4 == 0);

        INFO ("round " << round);
        CHECK (processBlocks() == 0);
    }

    if (! tracksMalloc())
        WARN ("Only operator new is tracked on this platform (malloc is tracked on Linux CI).");
}

TEST_CASE ("The engine with active oversampling never allocates", "[realtime][phase0]")
{
    std::array<TestModule::Settings, numModules> s {};
    for (int i = 0; i < numModules; ++i)
    {
        s[(size_t) i].usesOversampling = i != 0;
        s[(size_t) i].gain = 0.9f;
    }

    AudioEngine engine (testFactory (s));
    EngineParameters p;
    engine.prepare (96000.0, 256, 2, p);

    juce::AudioBuffer<float> buffer (2, 4096);
    juce::Random random (2);

    for (int round = 0; round < 40; ++round)
    {
        p.chainOrder = chain::fromLegacyIndex (round % 24);
        p.realtimeOversamplingIndex = round % 3;
        p.offlineOversamplingIndex = 1 + round % 4;
        p.moduleEnabled[(size_t) (round % numModules)] = round % 2 == 0;

        const auto n = 1 + random.nextInt (4095);
        juce::AudioBuffer<float> io (buffer.getArrayOfWritePointers(), 2, n);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                io.setSample (ch, i, 2.0f * random.nextFloat() - 1.0f);

        long long allocations = 0;
        {
            ScopedAllocationCounter counter;
            engine.process (io, nullptr, p, round % 5 == 0);
            allocations = counter.getCount();
        }
        INFO ("round " << round << ", block " << n);
        CHECK (allocations == 0);
    }
}
