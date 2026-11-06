// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 0 tests of the whole signal path: passthrough, latency, reordering, bypass, global
// section, extreme values and sample-rate / block-size coverage.

#include "TestHelpers.h"

#include "core/ChainOrder.h"
#include "core/Parameters.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;

namespace
{
/** Counts samples where out[n] != in[n - latency] (and out[n] != 0 before the latency). */
int countPassthroughMismatches (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int latency,
                                int from = 0)
{
    int mismatches = 0;
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        for (int i = from; i < in.getNumSamples(); ++i)
        {
            const auto expected = i >= latency ? in.getSample (ch, i - latency) : 0.0f;
            if (! juce::exactlyEqual (out.getSample (ch, i), expected))
                ++mismatches;
        }
    return mismatches;
}

float maxDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from, int to)
{
    float diff = 0.0f;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i)
            diff = juce::jmax (diff, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
    return diff;
}

struct DelayedCopyFit
{
    double gainDb = 0.0;
    float residual = 0.0f;
};

/** Least-squares gain between out[n] and in[n - latency], and what is left after removing it. */
DelayedCopyFit fitDelayedCopy (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int latency,
                               int from, int to)
{
    double xy = 0.0, xx = 0.0;
    for (int i = from; i < to; ++i)
    {
        const auto x = (double) in.getSample (0, i - latency);
        xy += x * out.getSample (0, i);
        xx += x * x;
    }
    const auto g = xy / xx;
    float residual = 0.0f;
    for (int i = from; i < to; ++i)
        residual = juce::jmax (residual, (float) std::abs (out.getSample (0, i) - g * in.getSample (0, i - latency)));
    return { 20.0 * std::log10 (g), residual };
}

EngineParameters plainParameters()
{
    EngineParameters p;
    p.clipperEnabled = false;
    return p;
}

std::unique_ptr<AudioEngine> makeEngine (const ModuleChain::Factory& factory, double sampleRate, int blockSize,
                                         int channels, const EngineParameters& p)
{
    auto engine = std::make_unique<AudioEngine> (factory);
    engine->prepare (sampleRate, blockSize, channels, p);
    return engine;
}
} // namespace

//==============================================================================
TEST_CASE ("Null passthrough: with everything neutral the output is the input, delayed by the declared latency",
           "[engine][phase0]")
{
    const std::vector<int> patterns { 1, 7, 64, 511, 4096, -1 }; // -1 = variable sizes between calls

    for (const auto sampleRate : allSampleRates)
    {
        for (const int channels : { 1, 2 })
        {
            for (const auto pattern : patterns)
            {
                INFO ("sample rate " << sampleRate << ", channels " << channels << ", block pattern " << pattern);

                PMelodiesProcessor processor;
                prepareProcessor (processor, sampleRate, pattern > 0 ? pattern : 512, channels);

                const auto latency = processor.getLatencySamples();
                REQUIRE (latency == processor.getEngine().getLatencySamples());

                const auto numSamples = (int) (sampleRate * 0.2) + latency;
                const auto input = makeNoise (channels, numSamples, 0.5f, channels * 31 + (int) sampleRate);
                const auto sidechain = makeNoise (2, numSamples, 0.9f, 99);

                const auto output = runProcessor (processor, input, blockSizes (numSamples, pattern), &sidechain);

                CHECK (countPassthroughMismatches (input, output, latency) == 0);
            }
        }
    }
}

TEST_CASE ("Null passthrough also holds without a sidechain bus and when rendering offline", "[engine][phase0]")
{
    for (const bool offline : { false, true })
    {
        PMelodiesProcessor processor;
        processor.setNonRealtime (offline);
        prepareProcessor (processor, 48000.0, 256, 2, 0);

        const auto latency = processor.getLatencySamples();
        const auto input = makeNoise (2, 24000, 0.5f);
        const auto output = runProcessor (processor, input, blockSizes (24000, 256));
        CHECK (countPassthroughMismatches (input, output, latency) == 0);
    }
}

//==============================================================================
TEST_CASE ("The declared latency equals the latency measured with an impulse", "[engine][latency][phase0]")
{
    SECTION ("Real plugin")
    {
        for (const auto sampleRate : allSampleRates)
        {
            PMelodiesProcessor processor;
            prepareProcessor (processor, sampleRate, 512, 2);
            const auto latency = processor.getLatencySamples();
            CHECK (latency > 0); // phase 0 already reserves the oversampling latency

            const auto input = makeImpulse (2, latency + 2000, 300, 0.5f);
            const auto output = runProcessor (processor, input, blockSizes (input.getNumSamples(), 512));

            CHECK (peakIndex (output) == 300 + latency);
            CHECK (output.getSample (0, 300 + latency) == 0.5f);
        }
    }

    SECTION ("Active oversampling path at every factor")
    {
        // Every module runs a real (identity) core, so the audio actually goes through the
        // oversampling filters chosen by the global quality.
        std::array<TestModule::Settings, numModules> s {};
        for (int i = 0; i < numModules; ++i)
            s[(size_t) i].usesOversampling = moduleUsesOversampling (moduleAt (i));

        for (const auto sampleRate : { 44100.0, 96000.0, 192000.0 })
        {
            int expectedLatency = -1;

            for (int factorIndex = 0; factorIndex <= dsp::OversamplingEngine::maxFactorIndex; ++factorIndex)
            {
                INFO ("sample rate " << sampleRate << ", factor index " << factorIndex);
                auto p = plainParameters();
                p.realtimeOversamplingIndex = factorIndex;

                auto engine = makeEngine (testFactory (s), sampleRate, 512, 1, p);
                const auto latency = engine->getLatencySamples();

                if (expectedLatency < 0)
                    expectedLatency = latency;
                CHECK (latency == expectedLatency); // constant whatever the factor

                // Band-limited content goes through unchanged apart from the delay.
                const auto n = latency + (int) sampleRate / 4;
                const auto input = makeSine (1, n, 1000.0, sampleRate, 0.5f);
                const auto output = runEngine (*engine, input, blockSizes (n, 512), p);

                const auto fit = fitDelayedCopy (input, output, latency, latency + 2000, n);
                CHECK (std::abs (fit.gainDb) < 0.15);  // half-band passband ripple, 3 modules in series
                CHECK (fit.residual < 2.0e-4f);        // aligned to the sample: a 1-sample error would be ~0.07

                // And an impulse peaks exactly at the declared latency.
                engine->reset();
                const auto impulse = makeImpulse (1, latency + 4000, 1000, 0.5f);
                const auto impulseOut = runEngine (*engine, impulse, blockSizes (impulse.getNumSamples(), 512), p);
                CHECK (peakIndex (impulseOut) == 1000 + latency);
            }
        }
    }
}

TEST_CASE ("The latency does not change with quality, order, bypass, settings or offline rendering",
           "[engine][latency][phase0]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);
    const auto latency = processor.getLatencySamples();
    const auto input = makeNoise (2, 48000, 0.25f);

    juce::Random random (3);
    const auto output = runProcessor (processor, input, blockSizes (48000, 480), nullptr,
                                      [&] (int block, int)
                                      {
                                          if (block % 10 != 0)
                                              return;
                                          setParameter (processor, param::id::chainOrder, (float) random.nextInt (24));
                                          setParameter (processor, param::id::qualityRealtime, (float) random.nextInt (3));
                                          setParameter (processor, param::id::qualityOffline, (float) random.nextInt (4));
                                          setParameter (processor, param::id::eqEnabled, (float) random.nextInt (2));
                                          setParameter (processor, param::id::distortionEnabled, (float) random.nextInt (2));
                                          processor.setNonRealtime (random.nextBool());
                                          CHECK (processor.getLatencySamples() == latency);
                                      });

    CHECK (processor.getLatencySamples() == latency);
    CHECK (processor.getEngine().getLatencySamples() == latency);
    // With the placeholder modules every order and setting is still an exact delay.
    CHECK (countPassthroughMismatches (input, output, latency) == 0);
}

//==============================================================================
TEST_CASE ("Reordering crossfades without discontinuities", "[engine][reorder][phase0]")
{
    // Non-linear, stateful test modules so the order really changes the sound.
    std::array<TestModule::Settings, numModules> s {};
    s[0].gain = 4.0f;                // EQ slot: gain + low-pass (stateful)
    s[0].lowpassHz = 3000.0;
    s[1].clipLevel = 0.5f;           // Compressor slot: hard clip
    s[1].usesOversampling = true;
    s[2].gain = 0.5f;                // Saturation slot: gain with 2x oversampling
    s[2].usesOversampling = true;
    s[2].requestedOversampling = 1;
    s[3].coreLatency = 32;           // Distortion slot: lookahead-like delay

    const double sampleRate = 48000.0;
    const int numSamples = 96000;
    const int switchAt = 188 * 256; // a block boundary: the order is read once per block
    const auto p = plainParameters();

    const auto newOrder = chain::encode ({ ModuleId::compressor, ModuleId::eq, ModuleId::saturation, ModuleId::distortion });
    REQUIRE (newOrder != chain::defaultOrderIndex);

    const auto input = makeSine (2, numSamples, 220.0, sampleRate, 0.3f);
    const auto sizes = blockSizes (numSamples, 256);

    auto pNew = p;
    pNew.chainOrder = newOrder;

    auto oldEngine = makeEngine (testFactory (s), sampleRate, 256, 2, p);
    auto newEngine = makeEngine (testFactory (s), sampleRate, 256, 2, pNew);
    const auto oldOut = runEngine (*oldEngine, input, sizes, p);
    const auto newOut = runEngine (*newEngine, input, sizes, pNew);

    auto engine = makeEngine (testFactory (s), sampleRate, 256, 2, p);
    const auto preRoll = engine->getChain().getPreRollSamples();
    const auto out = runEngine (*engine, input, sizes, p, false,
                                [&] (int, int start, EngineParameters& q) { q.chainOrder = start >= switchAt ? newOrder : 0; });

    const auto crossfadeSamples = (int) (ModuleChain::crossfadeSeconds * sampleRate);
    const auto transitionEnd = switchAt + preRoll + crossfadeSamples + 1;

    // The orders really sound different, so an instant switch would jump.
    const auto steadyStep = juce::jmax (maxStep (oldOut, switchAt - 4800, switchAt), maxStep (newOut, transitionEnd, numSamples));
    const auto instantJump = maxDifference (oldOut, newOut, switchAt, switchAt + 2000);
    REQUIRE (instantJump > 10.0f * steadyStep);

    // Before the switch and during the silent pre-roll: exactly the old order.
    CHECK (maxDifference (out, oldOut, 0, switchAt + preRoll) == 0.0f);

    // During the crossfade: never steeper than the steady signals, always between both orders.
    CHECK (maxStep (out, switchAt - 1, transitionEnd + 1) <= steadyStep * 1.1f + 1.0e-4f);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = switchAt; i < transitionEnd; ++i)
        {
            const auto a = oldOut.getSample (ch, i), b = newOut.getSample (ch, i), y = out.getSample (ch, i);
            CHECK ((y >= juce::jmin (a, b) - 1.0e-4f && y <= juce::jmax (a, b) + 1.0e-4f));
        }

    // Afterwards: the new order.
    CHECK (maxDifference (out, newOut, transitionEnd, numSamples) < 1.0e-5f);
    CHECK (engine->getChain().getActiveOrder() == newOrder);
    CHECK (engine->getLatencySamples() == oldEngine->getLatencySamples());
}

TEST_CASE ("Fast successive reorders stay continuous and settle on the last order", "[engine][reorder][phase0]")
{
    std::array<TestModule::Settings, numModules> s {};
    s[0].gain = 3.0f;
    s[1].clipLevel = 0.4f;
    s[2].gain = 0.6f;
    s[3].lowpassHz = 2000.0;

    const double sampleRate = 44100.0;
    const int numSamples = 88200;
    const auto p = plainParameters();
    const auto input = makeSine (1, numSamples, 150.0, sampleRate, 0.3f);
    const auto sizes = blockSizes (numSamples, -1, 700, 11); // variable block sizes

    auto engine = makeEngine (testFactory (s), sampleRate, 512, 1, p);
    juce::Random random (5);
    int lastOrder = 0;
    const auto out = runEngine (*engine, input, sizes, p, false,
                                [&] (int block, int start, EngineParameters& q)
                                {
                                    if (start < 60000 && block % 3 == 0)
                                        lastOrder = random.nextInt (24);
                                    q.chainOrder = lastOrder;
                                });

    auto pLast = p;
    pLast.chainOrder = lastOrder;
    auto reference = makeEngine (testFactory (s), sampleRate, 512, 1, pLast);
    const auto refOut = runEngine (*reference, input, sizes, pLast);

    CHECK (allFinite (out));
    // Steepest natural slope of any order is bounded by the loudest one (gain 3 × 0.6 × sine).
    const auto naturalBound = (float) (twoPi * 150.0 / sampleRate) * 3.0f * 0.3f * 1.2f;
    CHECK (maxStep (out, 1, numSamples) <= naturalBound + 0.02f);
    CHECK (maxDifference (out, refOut, 80000, numSamples) < 1.0e-5f);
}

TEST_CASE ("Changing the order parameter of the plugin keeps the passthrough exact", "[engine][reorder][phase0]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 44100.0, 128, 2);
    const auto latency = processor.getLatencySamples();
    const auto input = makeNoise (2, 44100, 0.5f, 8);

    const auto output = runProcessor (processor, input, blockSizes (44100, 128), nullptr,
                                      [&] (int block, int)
                                      {
                                          if (block % 20 == 0)
                                              setParameter (processor, param::id::chainOrder, (float) ((block / 20) % 24));
                                      });

    CHECK (countPassthroughMismatches (input, output, latency) == 0);
}

//==============================================================================
TEST_CASE ("Module bypass and oversampling changes fade without clicks", "[engine][bypass][phase0]")
{
    auto s = neutralSettings();
    s[1].neutral = false;
    s[1].gain = 0.25f;
    s[1].usesOversampling = true;

    const double sampleRate = 48000.0;
    const int numSamples = 5 * 48000;
    const auto input = makeSine (2, numSamples, 330.0, sampleRate, 0.5f);
    auto p = plainParameters();

    auto engine = makeEngine (testFactory (s), sampleRate, 333, 2, p);
    const auto latency = engine->getLatencySamples();

    const auto out = runEngine (*engine, input, blockSizes (numSamples, 333), p, false,
                                [&] (int, int start, EngineParameters& q)
                                {
                                    q.moduleEnabled[1] = ! (start >= 48000 && start < 96000);  // bypass 1 s
                                    q.realtimeOversamplingIndex = start >= 144000 ? 2 : 1;      // quality change
                                });

    // Smoothness: never steeper than the unprocessed sine (the louder of the two states).
    const auto bound = (float) (twoPi * 330.0 / sampleRate) * 0.5f * 1.05f;
    CHECK (maxStep (out, latency + 4800, numSamples) <= bound);

    // Bypassed: exactly the delayed input.
    for (int i = 80000; i < 96000; ++i)
        REQUIRE (out.getSample (0, i) == input.getSample (0, i - latency));

    // Engaged again (after the change of quality too): input × 0.25, delayed.
    float error = 0.0f;
    for (int i = 200000; i < numSamples; ++i)
        error = juce::jmax (error, std::abs (out.getSample (1, i) - 0.25f * input.getSample (1, i - latency)));
    CHECK (error < 1.0e-3f);
    CHECK (engine->getChain().getModule (ModuleId::compressor).getActiveOversamplingIndex() == 2);
    CHECK (engine->getLatencySamples() == latency);
}

//==============================================================================
TEST_CASE ("Input and output gain", "[engine][global][phase0]")
{
    auto p = plainParameters();
    p.inputGainDb = 6.0f;
    p.outputGainDb = -12.0f;

    auto engine = makeEngine (createModule, 48000.0, 512, 2, p);
    const auto latency = engine->getLatencySamples();
    const auto input = makeNoise (2, 20000, 0.25f);
    const auto out = runEngine (*engine, input, blockSizes (20000, 512), p);

    const auto expectedGain = juce::Decibels::decibelsToGain (-6.0f);
    for (int i = latency; i < 20000; i += 37)
        CHECK (out.getSample (0, i) == Catch::Approx (input.getSample (0, i - latency) * expectedGain).margin (1.0e-6));
}

TEST_CASE ("Global mix blends the latency-aligned dry signal", "[engine][global][phase0]")
{
    auto s = neutralSettings();
    s[0].neutral = false; // EQ slot: gain 2, no oversampling, no latency of its own
    s[0].gain = 2.0f;

    for (const float mix : { 0.0f, 0.5f, 1.0f })
    {
        auto p = plainParameters();
        p.mix = mix;
        auto engine = makeEngine (testFactory (s), 48000.0, 512, 2, p);
        const auto latency = engine->getLatencySamples();
        const auto input = makeNoise (2, 30000, 0.2f);
        const auto out = runEngine (*engine, input, blockSizes (30000, 512), p);

        const auto expected = 1.0f + mix; // dry 1, wet 2
        float error = 0.0f;
        for (int i = latency + 2000; i < 30000; ++i)
            error = juce::jmax (error, std::abs (out.getSample (1, i) - expected * input.getSample (1, i - latency)));
        INFO ("mix " << mix);
        CHECK (error < 1.0e-6f);
    }
}

TEST_CASE ("Compare at equal loudness plays the original at the processed loudness", "[engine][global][phase0]")
{
    auto s = neutralSettings();
    s[0].neutral = false;
    s[0].gain = 2.0f; // processed is +6.02 dB louder

    auto p = plainParameters();
    p.compare = true;
    auto engine = makeEngine (testFactory (s), 48000.0, 512, 2, p);
    const auto latency = engine->getLatencySamples();

    const int numSamples = 4 * 48000;
    auto input = makeNoise (2, numSamples, 0.1f);
    input.clear (3 * 48000, 48000); // a silent last second: the match must hold

    const auto out = runEngine (*engine, input, blockSizes (numSamples, 512), p);

    CHECK (engine->getMatchGainDb() == Catch::Approx (6.0206).margin (0.05));

    // Output = original × matched gain, i.e. as loud as the processed signal.
    const auto outRms = rmsOf (out, 48000, 3 * 48000);
    const auto inRms = rmsOf (input, 48000 - latency, 3 * 48000 - latency);
    CHECK (juce::Decibels::gainToDecibels (outRms / inRms) == Catch::Approx (6.0206).margin (0.1));

    // Switching it off returns to the processed signal.
    p.compare = false;
    const auto offOut = runEngine (*engine, input, blockSizes (numSamples, 512), p);
    for (int i = 48000; i < 3 * 48000; i += 101)
        CHECK (offOut.getSample (0, i) == Catch::Approx (2.0f * input.getSample (0, i - latency)).margin (1.0e-6));
}

TEST_CASE ("Host bypass outputs the untouched, latency-aligned input", "[engine][global][phase0]")
{
    auto s = neutralSettings();
    s[2].neutral = false;
    s[2].gain = 3.0f;
    s[2].usesOversampling = true;

    auto p = plainParameters();
    p.inputGainDb = 12.0f;
    p.bypassed = true;
    auto engine = makeEngine (testFactory (s), 44100.0, 256, 2, p);
    const auto latency = engine->getLatencySamples();
    const auto input = makeNoise (2, 20000, 0.5f);
    const auto out = runEngine (*engine, input, blockSizes (20000, 256), p);

    CHECK (countPassthroughMismatches (input, out, latency) == 0);
}

//==============================================================================
TEST_CASE ("The safety clipper never exceeds its ceiling", "[engine][clipper][phase0]")
{
    for (const float ceilingDb : { -6.0f, -3.0f, -0.3f, 0.0f })
    {
        auto p = EngineParameters();
        p.inputGainDb = 24.0f;
        p.clipperCeilingDb = ceilingDb;
        auto engine = makeEngine (createModule, 96000.0, 512, 2, p);

        const auto input = makeNoise (2, 96000, 1.0f, 21);
        const auto out = runEngine (*engine, input, blockSizes (96000, -1, 2000), p);

        INFO ("ceiling " << ceilingDb << " dB");
        CHECK (peakOf (out) <= juce::Decibels::decibelsToGain (ceilingDb));
        CHECK (peakOf (out) > 0.9f * juce::Decibels::decibelsToGain (ceilingDb)); // it is reached, not squashed
    }
}

TEST_CASE ("The safety clipper is bit-exact below ceiling - 3 dB and can be switched off", "[engine][clipper][phase0]")
{
    SECTION ("Below the knee")
    {
        EngineParameters p; // clipper on, ceiling -0.3 dB → knee at -3.3 dBFS (0.684)
        auto engine = makeEngine (createModule, 48000.0, 512, 1, p);
        const auto latency = engine->getLatencySamples();
        const auto input = makeSine (1, 48000, 440.0, 48000.0, 0.68f);
        const auto out = runEngine (*engine, input, blockSizes (48000, 512), p);
        CHECK (countPassthroughMismatches (input, out, latency) == 0);
    }

    SECTION ("Switched off")
    {
        auto p = plainParameters();
        auto engine = makeEngine (createModule, 48000.0, 512, 1, p);
        const auto latency = engine->getLatencySamples();
        const auto input = makeSine (1, 48000, 440.0, 48000.0, 2.0f);
        const auto out = runEngine (*engine, input, blockSizes (48000, 512), p);
        CHECK (countPassthroughMismatches (input, out, latency) == 0);
        CHECK (peakOf (out) > 1.9f);
    }

    SECTION ("Switching it on and off is smooth")
    {
        auto p = EngineParameters();
        auto engine = makeEngine (createModule, 48000.0, 480, 1, p);
        const auto input = makeSine (1, 96000, 100.0, 48000.0, 1.5f);
        const auto out = runEngine (*engine, input, blockSizes (96000, 480), p, false,
                                    [] (int block, int, EngineParameters& q) { q.clipperEnabled = (block / 25) % 2 == 0; });
        const auto bound = (float) (twoPi * 100.0 / 48000.0) * 1.5f * 1.05f;
        CHECK (maxStep (out, 1, 96000) <= bound);
    }
}

//==============================================================================
TEST_CASE ("No NaN or Inf with extreme parameters and signals", "[engine][robustness][phase0]")
{
    std::vector<std::pair<const char*, juce::AudioBuffer<float>>> signals;
    signals.emplace_back ("silence", juce::AudioBuffer<float> (2, 20000));
    signals.back().second.clear();
    signals.emplace_back ("loud noise", makeNoise (2, 20000, 4.0f));
    signals.emplace_back ("huge impulses", makeImpulse (2, 20000, 5000, 1000.0f));
    signals.emplace_back ("full-scale square", makeSine (2, 20000, 50.0, 44100.0, 1.0f));
    for (int i = 0; i < 20000; ++i)
        for (int ch = 0; ch < 2; ++ch)
            signals.back().second.setSample (ch, i, signals.back().second.getSample (ch, i) >= 0.0f ? 1.0f : -1.0f);
    signals.emplace_back ("denormal-level noise", makeNoise (2, 20000, 1.0e-38f));
    signals.emplace_back ("DC", juce::AudioBuffer<float> (2, 20000));
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::fill (signals.back().second.getWritePointer (ch), 1.0f, 20000);

    for (const auto sampleRate : { 44100.0, 192000.0 })
    {
        for (auto& [name, signal] : signals)
        {
            INFO (name << " at " << sampleRate);
            PMelodiesProcessor processor;
            prepareProcessor (processor, sampleRate, 512, 2);

            juce::Random random (17);
            const auto out = runProcessor (processor, signal, blockSizes (signal.getNumSamples(), -1, 1500), &signal,
                                           [&] (int, int)
                                           {
                                               // Fuzz every parameter, including the extremes.
                                               for (auto* parameter : processor.getParameters())
                                               {
                                                   const auto r = random.nextInt (4);
                                                   const auto v = r == 0 ? 0.0f : r == 1 ? 1.0f : random.nextFloat();
                                                   parameter->setValueNotifyingHost (v);
                                               }
                                               processor.setNonRealtime (random.nextBool());
                                           });
            CHECK (allFinite (out));
        }
    }
}

TEST_CASE ("Every sample rate and block size works in mono and stereo, also with the oversampling path active",
           "[engine][robustness][phase0]")
{
    std::array<TestModule::Settings, numModules> s {};
    s[0].gain = 0.8f;
    s[1].usesOversampling = true;
    s[1].requestedOversampling = 2;
    s[2].usesOversampling = true;
    s[2].clipLevel = 0.3f;
    s[3].usesOversampling = true;
    s[3].lowpassHz = 5000.0;

    for (const auto sampleRate : allSampleRates)
        for (const int channels : { 1, 2 })
            for (const int pattern : { 1, 13, 1024, 5000, -1 })
            {
                INFO ("sample rate " << sampleRate << ", channels " << channels << ", pattern " << pattern);
                auto p = plainParameters();
                p.realtimeOversamplingIndex = 2;
                auto engine = makeEngine (testFactory (s), sampleRate, pattern > 0 ? pattern : 512, channels, p);
                const auto numSamples = (int) (sampleRate * 0.1);
                const auto input = makeNoise (channels, numSamples, 0.7f);
                const auto out = runEngine (*engine, input, blockSizes (numSamples, pattern, 6000), p);
                CHECK (allFinite (out));
                CHECK (peakOf (out, engine->getLatencySamples() + 1000) > 0.01f);
            }
}

//==============================================================================
TEST_CASE ("Meters report peak, RMS and a sticky clip flag", "[engine][meters][phase0]")
{
    auto p = plainParameters();
    auto engine = makeEngine (createModule, 48000.0, 512, 2, p);
    auto& meter = engine->getInputMeter();

    const auto sine = makeSine (2, 48000, 1000.0, 48000.0, 0.5f);
    runEngine (*engine, sine, blockSizes (48000, 512), p);

    CHECK (meter.takePeak (0) == Catch::Approx (0.5f).margin (1.0e-3));
    CHECK (meter.takePeak (0) == 0.0f); // taking resets
    CHECK (meter.getRms (1) == Catch::Approx (0.5 / std::sqrt (2.0)).epsilon (0.02));
    CHECK_FALSE (meter.clipped.load());

    const auto loud = makeSine (2, 4800, 1000.0, 48000.0, 1.2f);
    runEngine (*engine, loud, blockSizes (4800, 512), p);
    CHECK (meter.clipped.load());
    runEngine (*engine, sine, blockSizes (48000, 512), p);
    CHECK (meter.clipped.load()); // sticky until the interface resets it
    meter.resetClip();
    CHECK_FALSE (meter.clipped.load());
}
