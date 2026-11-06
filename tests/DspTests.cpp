// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "TestHelpers.h"

#include "dsp/DelayLine.h"
#include "dsp/LoudnessMatcher.h"
#include "dsp/OversamplingEngine.h"
#include "dsp/SafetyClipper.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;

TEST_CASE ("Delay line delays by exactly N samples, in place or out of place", "[dsp][delay][phase0]")
{
    for (const int delay : { 0, 1, 7, 300 })
    {
        dsp::DelayLine line;
        line.prepare (2, 300);
        line.setDelay (delay);

        const auto input = makeNoise (2, 2000, 1.0f, delay + 1);
        auto inPlace = input;
        juce::AudioBuffer<float> outOfPlace (2, 2000);

        // Odd block sizes to cross the circular buffer boundary.
        dsp::DelayLine other;
        other.prepare (2, 300);
        other.setDelay (delay);

        int start = 0;
        for (const auto n : blockSizes (2000, -1, 97))
        {
            line.process (juce::dsp::AudioBlock<float> (inPlace).getSubBlock ((size_t) start, (size_t) n));
            other.process (juce::dsp::AudioBlock<const float> (input).getSubBlock ((size_t) start, (size_t) n),
                           juce::dsp::AudioBlock<float> (outOfPlace).getSubBlock ((size_t) start, (size_t) n));
            start += n;
        }

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 2000; ++i)
            {
                const auto expected = i >= delay ? input.getSample (ch, i - delay) : 0.0f;
                REQUIRE (inPlace.getSample (ch, i) == expected);
                REQUIRE (outOfPlace.getSample (ch, i) == expected);
            }
    }
}

TEST_CASE ("Delay line state can be copied", "[dsp][delay][phase0]")
{
    dsp::DelayLine a, b;
    a.prepare (1, 50);
    b.prepare (1, 50);
    a.setDelay (50);

    auto signal = makeNoise (1, 500, 1.0f);
    a.process (juce::dsp::AudioBlock<float> (signal).getSubBlock (0, 400));
    b.copyStateFrom (a);

    auto tailA = makeNoise (1, 100, 1.0f, 3);
    auto tailB = tailA;
    a.process (juce::dsp::AudioBlock<float> (tailA));
    b.process (juce::dsp::AudioBlock<float> (tailB));

    for (int i = 0; i < 100; ++i)
        REQUIRE (tailA.getSample (0, i) == tailB.getSample (0, i));
}

TEST_CASE ("Safety clipper curve", "[dsp][clipper][phase0]")
{
    const auto ceiling = juce::Decibels::decibelsToGain (-0.3f);
    const auto knee = dsp::SafetyClipper::kneeStartFor (ceiling);

    CHECK (juce::Decibels::gainToDecibels (knee) == Catch::Approx (-3.3f).margin (1.0e-4));

    float previous = 0.0f;
    for (int i = 0; i <= 100000; ++i)
    {
        const auto x = 20.0f * (float) i / 100000.0f;
        const auto y = dsp::SafetyClipper::process (x, ceiling, knee);

        if (x <= knee)
            REQUIRE (y == x);                   // bit-exact below the knee
        REQUIRE (y <= ceiling);                 // never above the ceiling
        REQUIRE (y >= previous);                // monotonic: no fold-back
        REQUIRE (dsp::SafetyClipper::process (-x, ceiling, knee) == -y); // symmetric
        previous = y;
    }

    // Continuous slope at the knee: no corner.
    const auto h = 1.0e-4f;
    const auto slopeAbove = (dsp::SafetyClipper::process (knee + h, ceiling, knee) - knee) / h;
    CHECK (slopeAbove == Catch::Approx (1.0f).margin (1.0e-2));
}

TEST_CASE ("Loudness matcher converges to the level ratio and holds on silence", "[dsp][loudness][phase0]")
{
    dsp::LoudnessMatcher matcher;
    matcher.prepare (48000.0);

    const auto reference = makeNoise (2, 48000, 0.1f);
    auto processed = reference;
    processed.applyGain (0.5f); // -6.02 dB

    matcher.push (juce::dsp::AudioBlock<const float> (processed), juce::dsp::AudioBlock<const float> (reference));
    CHECK (juce::Decibels::gainToDecibels (matcher.getGain()) == Catch::Approx (-6.0206f).margin (0.01));

    juce::AudioBuffer<float> silence (2, 480000);
    silence.clear();
    matcher.push (juce::dsp::AudioBlock<const float> (silence), juce::dsp::AudioBlock<const float> (silence));
    CHECK (juce::Decibels::gainToDecibels (matcher.getGain()) == Catch::Approx (-6.0206f).margin (0.01));
}

TEST_CASE ("Oversampling: whole-number latency and transparent identity path at every factor",
           "[dsp][oversampling][phase0]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        dsp::OversamplingEngine engine;
        engine.prepare (2, 512);

        int previousLatency = -1;
        for (int index = 0; index <= dsp::OversamplingEngine::maxFactorIndex; ++index)
        {
            INFO ("sample rate " << sampleRate << ", factor " << dsp::OversamplingEngine::factorForIndex (index));
            const auto latency = engine.getLatencySamples (index);
            CHECK (latency >= previousLatency); // more factor, more (or equal) latency
            previousLatency = latency;

            engine.reset();
            for (const double frequency : { 100.0, 1000.0, 0.3 * sampleRate })
            {
                const int n = 20000;
                auto signal = makeSine (2, n, frequency, sampleRate, 0.5f);
                const auto input = signal;

                int start = 0;
                for (const auto size : blockSizes (n, 512))
                {
                    int calls = 0;
                    engine.process (index, juce::dsp::AudioBlock<float> (signal).getSubBlock ((size_t) start, (size_t) size),
                                    [&calls] (juce::dsp::AudioBlock<float>) { ++calls; });
                    REQUIRE (calls == 1);
                    start += size;
                }

                // Separate the (tiny) passband gain ripple from the timing: least-squares gain first,
                // then what is left must be almost nothing if the latency is exactly right.
                double xy = 0.0, xx = 0.0;
                for (int i = latency + 1000; i < n; ++i)
                {
                    const auto x = (double) input.getSample (1, i - latency);
                    xy += x * signal.getSample (1, i);
                    xx += x * x;
                }
                const auto gain = xy / xx;
                float residual = 0.0f;
                for (int i = latency + 1000; i < n; ++i)
                    residual = juce::jmax (residual, (float) std::abs (signal.getSample (1, i) - gain * input.getSample (1, i - latency)));

                INFO ("frequency " << frequency << ", gain " << 20.0 * std::log10 (gain) << " dB, residual " << residual);
                CHECK (std::abs (20.0 * std::log10 (gain)) < 0.05); // passband ripple below 0.05 dB
                CHECK (residual < 1.0e-4f);                          // timing exact to the sample
            }
        }

        CHECK (engine.getMaxLatencySamples() == engine.getLatencySamples (dsp::OversamplingEngine::maxFactorIndex));
    }
}
