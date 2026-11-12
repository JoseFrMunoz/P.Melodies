// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 2 tests of the compressor module: constant latency and lookahead, the static curve
// measured on audio for every style, range, mix, makeup and auto-gain, the external sidechain
// and its EQ, listen, stereo link, automation without clicks, the meters and real-time safety.

#include "AllocationTracker.h"
#include "Measurement.h"
#include "TestHelpers.h"

#include "core/Parameters.h"
#include "modules/comp/CompressorModule.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;
using dsp::comp::Style;

namespace
{
/** A CompressorModule on its own, processed in blocks like the chain does. */
struct CompRig
{
    explicit CompRig (double rate = 48000.0, int block = 256, int channels = 2) : sampleRate (rate), blockSize (block)
    {
        module.prepare ({ rate, (juce::uint32) block, (juce::uint32) channels });
        meter.prepare();
        module.setMeter (&meter);
        module.setAnalyzerListening (true); // the heard module: it feeds the meter
    }

    comp::Parameters& p() noexcept { return parameters.comp; }

    juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>* sidechain = nullptr,
                                  const std::function<void (int startSample)>& beforeBlock = {})
    {
        juce::AudioBuffer<float> output (input);
        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, output.getNumSamples() - start);
            if (beforeBlock)
                beforeBlock (start);
            module.setParameters (parameters);
            auto block = juce::dsp::AudioBlock<float> (output).getSubBlock ((size_t) start, (size_t) n);
            if (sidechain != nullptr)
            {
                auto sc = juce::dsp::AudioBlock<const float> (*sidechain).getSubBlock ((size_t) start, (size_t) n);
                module.process (block, &sc);
            }
            else
            {
                module.process (block, nullptr);
            }
        }
        return output;
    }

    int latency() const noexcept { return module.getLatencySamples(); }

    comp::CompressorModule module;
    ModuleParameters parameters;
    dsp::comp::CompressorMeter meter;
    double sampleRate;
    int blockSize;
};

/** Typical settings that compress: threshold −20 dB, 4:1, hard knee, fast attack. */
void compressing (comp::Parameters& p, Style style = Style::clean)
{
    p.style = style;
    p.thresholdDb = -20.0f;
    p.ratio = 4.0f;
    p.kneeDb = 0.0f;
    p.attackMs = 1.0f;
    p.releaseMs = 50.0f;
}

juce::AudioBuffer<float> tone (double hz, double levelDb, double seconds, double fs, int channels = 2)
{
    return makeSine (channels, (int) (seconds * fs), hz, fs, (float) std::pow (10.0, levelDb / 20.0));
}

/** Level (dB) of a tone at `hz` over the last `seconds` of a channel. */
double toneLevelDb (const juce::AudioBuffer<float>& x, double hz, double fs, int channel = 0, double seconds = 0.2)
{
    const auto end = x.getNumSamples();
    const auto start = end - (int) (seconds * fs);
    return measure::toDb (measure::fitSine (x.getReadPointer (channel), start, end, hz, fs).amplitude);
}
} // namespace

//==============================================================================
TEST_CASE ("Compressor module: latency is 20 ms plus the oversampling, whatever the settings", "[comp][module][phase2]")
{
    for (const auto fs : { 44100.0, 48000.0, 96000.0 })
    {
        CompRig rig (fs);
        dsp::OversamplingEngine reference;
        reference.prepare (2, 256);
        const auto expected = (int) std::lround (0.020 * fs) + reference.getMaxLatencySamples();
        CHECK (rig.latency() == expected);

        // An engaged compressor at 1:1 is a pure delay of exactly that latency, at any lookahead
        // and oversampling.
        for (const auto lookahead : { 0.0f, 7.5f, 20.0f })
            for (const auto oversampling : { 0, 1, 2 })
            {
                CompRig engaged (fs);
                engaged.p().thresholdDb = -30.0f;
                engaged.p().ratio = 1.0f;
                engaged.p().lookaheadMs = lookahead;
                engaged.p().oversamplingIndex = oversampling;
                juce::AudioBuffer<float> silence (2, (int) (0.2 * fs));
                silence.clear();
                engaged.run (silence); // engage (pre-roll and fade)
                REQUIRE (engaged.module.isWetPathActive());
                REQUIRE (engaged.module.getActiveOversamplingIndex() == oversampling);

                const auto output = engaged.run (makeImpulse (2, (int) (0.2 * fs), 100, 0.5f));
                INFO (fs << " Hz, lookahead " << lookahead << " ms, oversampling " << oversampling);
                CHECK (peakIndex (output) == 100 + expected);
                CHECK (output.getSample (0, 100 + expected) == Catch::Approx (0.5f).margin (0.02));
                CHECK (engaged.latency() == expected);
            }
    }
}

TEST_CASE ("Compressor module: the default settings leave the sound untouched, bit for bit", "[comp][module][phase2]")
{
    CompRig rig;
    const auto input = makeNoise (2, 48000, 0.5f);
    const auto output = rig.run (input);
    CHECK_FALSE (rig.module.isWetPathActive());

    const auto latency = rig.latency();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = latency; i < input.getNumSamples(); ++i)
            REQUIRE (output.getSample (ch, i) == input.getSample (ch, i - latency));
}

TEST_CASE ("Compressor module: steady tones follow the static curve in every style", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    for (int s = 0; s < dsp::comp::numStyles; ++s)
        for (const auto oversampling : { 0, 1 })
            for (const auto levelDb : { -40.0, -24.0, -12.0, 0.0 })
            {
                CompRig rig (fs);
                compressing (rig.p(), (Style) s);
                rig.p().thresholdDb = -24.0f;
                rig.p().ratio = 3.0f;
                rig.p().kneeDb = 6.0f;
                rig.p().oversamplingIndex = oversampling;
                const auto output = rig.run (tone (1000.0, levelDb, 1.5, fs));

                const dsp::comp::StaticCurve curve (rig.p().curve());
                INFO ("style " << s << ", oversampling " << oversampling << ", level " << levelDb);
                CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (curve.outputDb (levelDb)).margin (0.25));
            }
}

TEST_CASE ("Compressor module: range limits the reduction", "[comp][module][phase2]")
{
    CompRig rig;
    compressing (rig.p());
    rig.p().thresholdDb = -40.0f;
    rig.p().ratio = (float) dsp::comp::infiniteRatio;
    rig.p().rangeDb = 6.0f;
    const auto output = rig.run (tone (1000.0, 0.0, 1.0, 48000.0));
    CHECK (toneLevelDb (output, 1000.0, 48000.0) == Catch::Approx (-6.0).margin (0.1));
}

TEST_CASE ("Compressor module: mix 0 % is the input and 50 % is parallel compression", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().thresholdDb = -40.0f;
        rig.p().ratio = (float) dsp::comp::infiniteRatio;
        rig.p().makeupDb = 12.0f;
        rig.p().mix = 0.0f;
        const auto input = makeNoise (2, 48000, 0.7f);
        const auto output = rig.run (input);
        const auto latency = rig.latency();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = latency; i < input.getNumSamples(); ++i)
                REQUIRE (output.getSample (ch, i) == input.getSample (ch, i - latency));
    }
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().mix = 0.5f;
        const auto output = rig.run (tone (1000.0, 0.0, 1.0, fs));
        // Dry and compressed are in phase: 0.5 · 1 + 0.5 · 10^(−15/20).
        const auto expected = measure::toDb (0.5 + 0.5 * std::pow (10.0, -15.0 / 20.0));
        CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (expected).margin (0.1));
    }
}

TEST_CASE ("Compressor module: makeup and auto-gain", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().makeupDb = 6.0f;
        const auto output = rig.run (tone (1000.0, -30.0, 1.0, fs)); // below the threshold
        CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (-24.0).margin (0.05));
    }
    for (const auto makeup : { 0.0f, -3.0f })
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().autoGain = true;
        rig.p().makeupDb = makeup;
        // Halfway between the threshold (−20) and 0 dBFS the level is kept; the makeup adds on top.
        const auto output = rig.run (tone (1000.0, -10.0, 1.0, fs));
        CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (-10.0 + makeup).margin (0.1));
    }
}

TEST_CASE ("Compressor module: stereo is linked", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    CompRig rig (fs);
    compressing (rig.p());
    auto input = tone (1000.0, 0.0, 1.0, fs);
    input.applyGain (1, 0, input.getNumSamples(), (float) std::pow (10.0, -30.0 / 20.0));
    const auto output = rig.run (input);
    CHECK (toneLevelDb (output, 1000.0, fs, 0) == Catch::Approx (-15.0).margin (0.1));
    CHECK (toneLevelDb (output, 1000.0, fs, 1) == Catch::Approx (-45.0).margin (0.1)); // same −15 dB
}

TEST_CASE ("Compressor module: the lookahead reacts before the transient arrives", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    const auto firstPeakDb = [fs] (float lookaheadMs)
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().lookaheadMs = lookaheadMs;
        rig.p().highPass.enabled = false; // its start-up ring would add a little to the first peak
        auto input = tone (1000.0, -60.0, 0.6, fs);
        const auto step = (int) (0.3 * fs);
        const auto loud = tone (1000.0, 0.0, 0.3, fs);
        for (int ch = 0; ch < 2; ++ch)
            input.copyFrom (ch, step, loud, ch, step, input.getNumSamples() - step);
        const auto output = rig.run (input);
        // Loudest sample in the first millisecond of the transient at the output.
        const auto at = step + rig.latency();
        return measure::toDb (peakOf (output, at, at + (int) (0.001 * fs)));
    };

    CHECK (firstPeakDb (0.0f) > -6.0);                                 // the start gets through (1 ms attack)
    CHECK (firstPeakDb (5.0f) == Catch::Approx (-15.0).margin (0.2)); // five attack times ahead: compressed
}

//==============================================================================
TEST_CASE ("Compressor module: the external sidechain drives the gain", "[comp][module][sidechain][phase2]")
{
    const double fs = 48000.0;
    for (const auto oversampling : { 0, 2 })
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().externalSidechain = true;
        rig.p().oversamplingIndex = oversampling;
        const auto main = tone (1000.0, -30.0, 1.0, fs);  // below the threshold on its own
        const auto key = tone (300.0, 0.0, 1.0, fs, 1);    // a mono kick-like key
        const auto output = rig.run (main, &key);
        INFO ("oversampling " << oversampling);
        CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (-45.0).margin (0.3));
        CHECK_FALSE (rig.meter.sidechainMissing.load());
    }

    // External selected but nothing arrives: no compression, and the interface is told.
    CompRig rig (fs);
    compressing (rig.p());
    rig.p().externalSidechain = true;
    const auto output = rig.run (tone (1000.0, 0.0, 1.0, fs));
    CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (0.0).margin (0.05));
    CHECK (rig.meter.sidechainMissing.load());

    // Back to the internal sidechain: compresses, and the warning goes away.
    rig.p().externalSidechain = false;
    const auto internal = rig.run (tone (1000.0, 0.0, 1.0, fs));
    CHECK (toneLevelDb (internal, 1000.0, fs) == Catch::Approx (-15.0).margin (0.1));
    CHECK_FALSE (rig.meter.sidechainMissing.load());
}

TEST_CASE ("Compressor module: the sidechain EQ shapes what the detector hears", "[comp][module][sidechain][phase2]")
{
    const double fs = 48000.0;
    const auto levelWith = [fs] (double hz, double levelDb, const std::function<void (comp::Parameters&)>& set)
    {
        CompRig rig (fs);
        compressing (rig.p());
        set (rig.p());
        return toneLevelDb (rig.run (tone (hz, levelDb, 1.0, fs)), hz, fs);
    };

    // High-pass at 1 kHz, 48 dB/oct: a 100 Hz tone no longer triggers the compressor.
    const auto highPass = [] (comp::Parameters& p) { p.highPass = { true, 1000.0f, 0.0f, 1.0f, 5 }; };
    CHECK (levelWith (100.0, 0.0, highPass) == Catch::Approx (0.0).margin (0.05));
    CHECK (levelWith (100.0, 0.0, [] (comp::Parameters& p) { p.highPass.enabled = false; }) == Catch::Approx (-15.0).margin (0.1));

    // Low-pass at 1 kHz, 48 dB/oct: a 10 kHz tone no longer triggers it.
    const auto lowPass = [] (comp::Parameters& p) { p.lowPass = { true, 1000.0f, 0.0f, 1.0f, 5 }; };
    CHECK (levelWith (10000.0, 0.0, lowPass) == Catch::Approx (0.0).margin (0.05));

    // Bell +12 dB at 1 kHz: a −30 dB tone is heard at −18 dB, 2 dB over the threshold.
    const auto bell = [] (comp::Parameters& p) { p.bell = { true, 1000.0f, 12.0f, 1.0f, 0 }; };
    CHECK (levelWith (1000.0, -30.0, bell) == Catch::Approx (-30.0 - 1.5).margin (0.1));
}

TEST_CASE ("Compressor module: listen plays the filtered sidechain", "[comp][module][sidechain][phase2]")
{
    const double fs = 48000.0;
    const auto listenLevel = [fs] (double hz, bool external)
    {
        CompRig rig (fs);
        compressing (rig.p());
        rig.p().listen = true;
        rig.p().highPass = { true, 1000.0f, 0.0f, 1.0f, 7 }; // 96 dB/oct
        rig.p().externalSidechain = external;
        const auto key = tone (hz, -6.0, 1.0, fs);
        const auto main = tone (hz * 1.5, -3.0, 1.0, fs);
        const auto output = rig.run (external ? main : key, external ? &key : nullptr);
        return std::pair { toneLevelDb (output, hz, fs), toneLevelDb (output, hz * 1.5, fs) };
    };

    CHECK (listenLevel (5000.0, false).first == Catch::Approx (-6.0).margin (0.1)); // passes the high-pass
    CHECK (listenLevel (100.0, false).first < -80.0);                              // removed by it

    // With the external sidechain, what is heard is the key, not the main signal.
    const auto [key, main] = listenLevel (5000.0, true);
    CHECK (key == Catch::Approx (-6.0).margin (0.1));
    CHECK (main < -80.0);
}

//==============================================================================
TEST_CASE ("Compressor module: automating the threshold and switching listen do not click", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    CompRig rig (fs);
    compressing (rig.p());
    rig.p().attackMs = 5.0f;
    rig.p().thresholdDb = -1.0f;
    const auto input = tone (150.0, -6.0, 2.0, fs);

    const auto output = rig.run (input, nullptr,
                                 [&] (int start)
                                 {
                                     const auto t = (double) start / fs;
                                     // Sweep down to −40 dB and back, then listen on and off.
                                     rig.p().thresholdDb = (float) (-1.0 - 39.0 * std::sin (juce::MathConstants<double>::pi * juce::jlimit (0.0, 1.0, t)));
                                     rig.p().listen = t > 1.2 && t < 1.6;
                                     rig.p().mix = t > 1.7 ? 0.3f : 1.0f;
                                 });

    const auto score = measure::clickScore (output.getReadPointer (0), output.getNumSamples(), fs, (int) (0.1 * fs));
    CHECK (score < 2.0e-3);
}

TEST_CASE ("Compressor module: the meter reports the gain reduction and the level points", "[comp][module][phase2]")
{
    const double fs = 48000.0;
    CompRig rig (fs);
    compressing (rig.p());
    rig.run (tone (1000.0, 0.0, 1.0, fs));

    CHECK (rig.meter.gainDb.load() == Catch::Approx (-15.0).margin (0.1));
    CHECK (rig.meter.levelDb.load() == Catch::Approx (0.0).margin (0.1));

    // One point every 5 ms.
    auto& fifo = rig.meter.getPoints();
    CHECK (std::abs (fifo.getNumReady() - 200) <= 2);
    std::vector<dsp::comp::MeterPoint> points ((size_t) fifo.getNumReady());
    fifo.pop (points.data(), (int) points.size());
    const auto& last = points.back();
    CHECK (last.inputDb == Catch::Approx (0.0).margin (0.1));
    CHECK (last.outputDb == Catch::Approx (-15.0).margin (0.15));
    CHECK (last.gainDb == Catch::Approx (-15.0).margin (0.1));

    // While resting (threshold back to 0 dB) the graph keeps scrolling with no reduction.
    rig.p().thresholdDb = 0.0f;
    rig.run (makeSine (2, (int) (3.0 * fs), 1000.0, fs, 0.5f));
    CHECK_FALSE (rig.module.isWetPathActive());
    CHECK (rig.meter.gainDb.load() == 0.0f);
    CHECK (rig.meter.levelDb.load() == Catch::Approx (measure::toDb (0.5)).margin (0.1));
}

TEST_CASE ("Compressor module: processing never allocates and stays finite", "[comp][module][realtime][phase2]")
{
    const double fs = 96000.0;
    CompRig rig (fs, 512);
    const auto key = makeNoise (2, 512, 1.0f, 5);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::Random random (9);

    for (int round = 0; round < 80; ++round)
    {
        auto& p = rig.p();
        p.style = (Style) (round % dsp::comp::numStyles);
        p.thresholdDb = (float) -(round % 61);
        p.ratio = round % 7 == 0 ? (float) dsp::comp::infiniteRatio : 1.0f + (float) (round % 20);
        p.kneeDb = (float) (round * 7 % 73);
        p.rangeDb = (float) (round * 3 % 61);
        p.attackMs = round % 5 == 0 ? (float) dsp::comp::minAttackMs : 30.0f;
        p.releaseMs = round % 3 == 0 ? (float) dsp::comp::minReleaseMs : 900.0f;
        p.autoRelease = round % 2 == 0;
        p.lookaheadMs = (float) (round % 21);
        p.holdMs = (float) (round * 13 % 501);
        p.makeupDb = (float) (round % 49 - 12);
        p.autoGain = round % 4 == 0;
        p.mix = (float) (round % 11) / 10.0f;
        p.externalSidechain = round % 6 == 0;
        p.listen = round % 9 == 0;
        p.oversamplingIndex = round % 3;
        p.highPass = { round % 2 == 0, 20.0f + (float) (round * 25), 0.0f, 1.0f, round % 8 };
        p.lowPass = { round % 3 != 0, 1000.0f + (float) (round * 200), 0.0f, 1.0f, (round + 3) % 8 };
        p.bell = { true, 100.0f * (float) (1 + round % 50), (float) (round % 49 - 24), 0.1f + (float) (round % 10), 0 };
        rig.module.setParameters (rig.parameters);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, (round % 10 == 0 ? 8.0f : 1.0f) * (2.0f * random.nextFloat() - 1.0f));

        const auto n = 1 + random.nextInt (511);
        auto block = juce::dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) n);
        auto sc = juce::dsp::AudioBlock<const float> (key).getSubBlock (0, (size_t) n);

        long long allocations = 0;
        {
            ScopedAllocationCounter counter;
            rig.module.process (block, round % 4 == 0 ? nullptr : &sc);
            allocations = counter.getCount();
        }
        INFO ("round " << round);
        CHECK (allocations == 0);
        CHECK (allFinite (buffer));
    }
}

//==============================================================================
TEST_CASE ("Compressor parameters: ranges, defaults and texts follow the specification", "[comp][parameters][phase2]")
{
    PMelodiesProcessor processor;
    const auto get = [&] (const char* id) -> juce::RangedAudioParameter&
    { return *processor.getState().getParameter (id); };
    const auto defaultOf = [] (juce::RangedAudioParameter& p) { return p.convertFrom0to1 (p.getDefaultValue()); };

    CHECK (defaultOf (get (param::id::compThreshold)) == Catch::Approx (0.0f));
    CHECK (get (param::id::compThreshold).getNormalisableRange().start == -60.0f);
    CHECK (defaultOf (get (param::id::compRatio)) == Catch::Approx (4.0f));
    CHECK (defaultOf (get (param::id::compKnee)) == Catch::Approx (0.0f));
    CHECK (get (param::id::compKnee).getNormalisableRange().end == 72.0f);
    CHECK (defaultOf (get (param::id::compRange)) == Catch::Approx (60.0f));
    CHECK (defaultOf (get (param::id::compAttack)) == Catch::Approx (10.0f));
    CHECK (get (param::id::compAttack).getNormalisableRange().start == Catch::Approx (0.005f));
    CHECK (get (param::id::compAttack).getNormalisableRange().end == 250.0f);
    CHECK (defaultOf (get (param::id::compRelease)) == Catch::Approx (120.0f));
    CHECK (get (param::id::compRelease).getNormalisableRange().end == 2500.0f);
    CHECK (get (param::id::compLookahead).getNormalisableRange().end == 20.0f);
    CHECK (get (param::id::compHold).getNormalisableRange().end == 500.0f);
    CHECK (get (param::id::compMakeup).getNormalisableRange().start == -12.0f);
    CHECK (get (param::id::compMakeup).getNormalisableRange().end == 36.0f);
    CHECK (defaultOf (get (param::id::compMix)) == Catch::Approx (100.0f));
    CHECK (defaultOf (get (param::id::compScHpFreq)) == Catch::Approx (80.0f));
    CHECK (get (param::id::compScHpSlope).getCurrentValueAsText() == "24 dB/oct");
    CHECK (get (param::id::compScHpFreq).getNormalisableRange().end == 2000.0f);
    CHECK (get (param::id::compScLpFreq).getNormalisableRange().start == 1000.0f);
    CHECK (get (param::id::compStyle).getCurrentValueAsText() == "Clean");
    CHECK (get (param::id::compOversampling).getCurrentValueAsText() == "Off");
    CHECK (get (param::id::compSidechain).getCurrentValueAsText() == "Interno");

    // Ratio: 4:1 near the middle of the knob, 100:1, then ∞:1 at the end; texts both ways.
    auto& ratio = get (param::id::compRatio);
    CHECK (ratio.convertTo0to1 (4.0f) == Catch::Approx (0.5f).margin (0.02));
    CHECK (ratio.convertFrom0to1 (1.0f) == (float) dsp::comp::infiniteRatio);
    CHECK (ratio.convertFrom0to1 (0.96f) == 100.0f);
    CHECK (ratio.convertFrom0to1 (0.0f) == 1.0f);
    CHECK (ratio.getText (ratio.convertTo0to1 (4.0f), 10) == "4:1");
    CHECK (ratio.getText (ratio.convertTo0to1 (1.5f), 10) == "1,5:1");
    CHECK (ratio.getText (1.0f, 10) == juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e:1")));
    CHECK (ratio.convertFrom0to1 (ratio.getValueForText ("inf")) == (float) dsp::comp::infiniteRatio);
    CHECK (ratio.convertFrom0to1 (ratio.getValueForText ("8:1")) == Catch::Approx (8.0f).margin (0.01));
    for (float r = 1.0f; r <= 100.0f; r *= 1.37f)
    {
        const auto snapped = ratio.convertFrom0to1 (ratio.convertTo0to1 (r));
        CHECK (ratio.convertFrom0to1 (ratio.convertTo0to1 (snapped)) == snapped); // projects reload exactly
    }

    // Times.
    auto& attack = get (param::id::compAttack);
    CHECK (attack.getText (attack.convertTo0to1 (0.005f), 10) == "0,005");
    CHECK (attack.convertFrom0to1 (attack.getValueForText ("1,5")) == Catch::Approx (1.5f));
}

TEST_CASE ("Compressor in the plugin: the host's sidechain bus drives it and listen is not a parameter", "[comp][sidechain][phase2]")
{
    const double fs = 48000.0;
    PMelodiesProcessor processor;
    prepareProcessor (processor, fs, 512, 2, 2);
    setParameter (processor, param::id::compThreshold, -20.0f);
    setParameter (processor, param::id::compAttack, 1.0f);
    setParameter (processor, param::id::compSidechain, 1.0f);

    const auto main = tone (1000.0, -30.0, 1.5, fs);
    const auto key = tone (300.0, 0.0, 1.5, fs);
    const auto sizes = blockSizes (main.getNumSamples(), 512, 512);
    const auto output = runProcessor (processor, main, sizes, &key);
    CHECK (toneLevelDb (output, 1000.0, fs) == Catch::Approx (-45.0).margin (0.3));

    processor.setCompressorListen (true);
    CHECK (processor.isCompressorListening());
    for (const auto& id : param::compressorIds())
        CHECK_FALSE (id.containsIgnoreCase ("listen"));
}
