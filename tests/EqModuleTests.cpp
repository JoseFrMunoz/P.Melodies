// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 1 tests of the EQ module: bands in series, zero latency, the bit-exact neutral case,
// click-free automation and shape changes, stability, solo, bypass, the character modes and
// the EQ parameters.

#include "AllocationTracker.h"
#include "Measurement.h"
#include "TestHelpers.h"

#include "core/Parameters.h"
#include "modules/eq/EqModule.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;
using dsp::eq::FilterType;

namespace
{
/** An EqModule on its own, processed in blocks like the chain does. */
struct EqRig
{
    explicit EqRig (double rate = 48000.0, int block = 512, int channels = 2) : sampleRate (rate), blockSize (block)
    {
        module.prepare ({ rate, (juce::uint32) block, (juce::uint32) channels });
    }

    void set (const eq::Parameters& p)
    {
        parameters.eq = p;
        module.setParameters (parameters);
    }

    /** Processes `input` in blocks; `beforeBlock` may change the parameters (automation). */
    juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& input, const std::function<void (int start)>& beforeBlock = {})
    {
        juce::AudioBuffer<float> output (input);
        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            if (beforeBlock)
                beforeBlock (start);
            const auto n = juce::jmin (blockSize, output.getNumSamples() - start);
            auto block = juce::dsp::AudioBlock<float> (output).getSubBlock ((size_t) start, (size_t) n);
            module.process (block, nullptr);
        }
        return output;
    }

    /** Lets the module engage and the glides settle (half a second of silence). */
    void settle()
    {
        juce::AudioBuffer<float> silence (2, (int) (0.5 * sampleRate));
        silence.clear();
        run (silence);
    }

    eq::EqModule module;
    ModuleParameters parameters;
    double sampleRate;
    int blockSize;
};

eq::BandParameters band (FilterType type, double hz, double gainDb, double q = 1.0, int slopeIndex = dsp::eq::defaultSlopeIndex)
{
    eq::BandParameters b;
    b.active = true;
    b.settings = { type, hz, gainDb, q, slopeIndex };
    return b;
}

/** Amplitude of a sine at `hz` in the last `length` samples of channel 0 (least-squares fit). */
double amplitudeAt (const juce::AudioBuffer<float>& x, double hz, double fs, int length)
{
    const auto end = x.getNumSamples();
    return measure::fitSine (x.getReadPointer (0), end - length, end, hz, fs).amplitude;
}

double gainDbAt (EqRig& rig, double hz)
{
    const auto fs = rig.sampleRate;
    const auto input = makeSine (2, (int) (0.4 * fs), hz, fs, 0.25f);
    const auto output = rig.run (input);
    return 20.0 * std::log10 (amplitudeAt (output, hz, fs, (int) (0.1 * fs)) / 0.25);
}

/** Coherent tone on an odd FFT bin, run through the rig, analysed after it settles. */
measure::HarmonicAnalysis analyse (EqRig& rig, double approxHz, double amplitude, int fftSize = 1 << 16)
{
    const auto fs = rig.sampleRate;
    auto bin = juce::roundToInt (approxHz * fftSize / fs);
    if (bin % 2 == 0)
        ++bin;
    const auto hz = measure::binFrequency (bin, fftSize, fs);
    const auto settleSamples = (int) (0.3 * fs);
    const auto input = makeSine (2, settleSamples + fftSize, hz, fs, (float) amplitude);
    const auto output = rig.run (input);
    return measure::analyseHarmonics (output.getReadPointer (0) + settleSamples, fftSize, bin);
}
} // namespace

TEST_CASE ("EQ module: zero latency, bands in series give the designed response", "[eq][module][phase1]")
{
    for (const auto fs : { 44100.0, 96000.0 })
    {
        EqRig rig (fs);
        CHECK (rig.module.getLatencySamples() == 0);

        eq::Parameters p;
        p.bands[0] = band (FilterType::bell, 200.0, 6.0, 1.0);
        p.bands[2] = band (FilterType::highShelf, 6000.0, -4.0, 1.0);
        p.bands[3] = band (FilterType::lowCut, 40.0, 0.0, 1.0, 3);
        p.bands[5] = band (FilterType::notch, 3000.0, 0.0, 8.0);
        p.bands[7] = band (FilterType::bell, 9000.0, 3.0, 2.0);
        p.bands[7].bypassed = true; // bypassed: not part of the response
        rig.set (p);
        rig.settle();

        const auto impulse = makeImpulse (2, 1 << 17, 0, 1.0f);
        const auto output = rig.run (impulse);
        std::vector<double> ir ((size_t) output.getNumSamples());
        for (int i = 0; i < output.getNumSamples(); ++i)
            ir[(size_t) i] = output.getSample (1, i);

        for (const auto hz : { 30.0, 100.0, 200.0, 1000.0, 2900.0, 6000.0, 15000.0 })
        {
            std::complex<double> expected { 1.0, 0.0 };
            for (const auto b : { 0, 2, 3, 5 })
                expected *= dsp::eq::designBand (p.bands[(size_t) b].settings, fs).response (hz, fs);
            INFO (fs << " Hz, at " << hz << " Hz");
            CHECK (measure::toDb (std::abs (measure::responseAt (ir, hz, fs))) == Catch::Approx (measure::toDb (std::abs (expected))).margin (0.02));
        }
    }
}

TEST_CASE ("EQ module: with Clean and every gain at 0 dB the output is the input, bit for bit", "[eq][module][phase1]")
{
    EqRig rig;
    eq::Parameters p;
    for (int b = 0; b < eq::numBands; ++b)
        p.bands[(size_t) b] = band (b % 2 == 0 ? FilterType::bell : FilterType::tiltShelf, 60.0 * (b + 1), 0.0, 0.5 + b);
    p.bands[1].settings.type = FilterType::lowShelf;
    p.bands[3].bypassed = true;
    p.bands[3].settings = { FilterType::lowCut, 200.0, 0.0, 1.0, 5 }; // bypassed: does not count
    rig.set (p);

    const auto input = makeNoise (2, 48000, 0.5f);
    const auto output = rig.run (input);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < input.getNumSamples(); ++i)
            REQUIRE (juce::exactlyEqual (output.getSample (ch, i), input.getSample (ch, i)));
}

TEST_CASE ("EQ module: automating frequency, gain and Q does not click", "[eq][module][phase1]")
{
    const double fs = 48000.0;
    const auto seconds = 2.5;
    const auto input = makeSine (2, (int) (seconds * fs), 150.0, fs, 0.5f); // a low tone: the click detector ignores it

    SECTION ("frequency sweep of a +12 dB bell from 50 Hz to 12 kHz, new value every block")
    {
        EqRig rig (fs);
        eq::Parameters p;
        p.bands[0] = band (FilterType::bell, 50.0, 12.0, 1.0);
        rig.set (p);
        const auto output = rig.run (input, [&] (int start)
                                     {
                                         const auto t = juce::jlimit (0.0, 1.0, start / (2.0 * fs));
                                         p.bands[0].settings.frequency = 50.0 * std::pow (12000.0 / 50.0, t);
                                         rig.set (p);
                                     });
        const auto score = measure::clickScore (output.getReadPointer (0), output.getNumSamples(), fs, (int) (0.05 * fs));
        INFO ("click score " << score);
        CHECK (score < 2.0e-4);
    }

    SECTION ("gain jumps of 48 dB at the tone's frequency, and Q jumps")
    {
        // A 48 dB jump in 20 ms on a loud tone makes some high-frequency content by itself. The
        // reference is the same tone with the ideal gain glide (linear in dB over 20 ms) applied
        // directly: the EQ must not add clicks beyond it.
        const auto period = (int) (0.25 * fs);
        const auto targetAt = [&] (int start) { return (start / period) % 2 == 0 ? -24.0 : 24.0; };

        EqRig rig (fs);
        eq::Parameters p;
        p.bands[0] = band (FilterType::bell, 150.0, -24.0, 1.0);
        rig.set (p);
        rig.settle();
        const auto output = rig.run (input, [&] (int start)
                                     {
                                         p.bands[0].settings.gainDb = targetAt (start);
                                         p.bands[0].settings.q = (start / period) % 3 == 0 ? 0.3 : 6.0;
                                         rig.set (p);
                                     });

        juce::AudioBuffer<float> reference (1, input.getNumSamples());
        const auto glide = (int) (eq::Band::glideSeconds * fs);
        double gain = -24.0, from = -24.0, to = -24.0;
        int since = glide;
        for (int i = 0; i < input.getNumSamples(); ++i)
        {
            if (i % rig.blockSize == 0 && ! juce::exactlyEqual (targetAt (i), to))
            {
                from = gain;
                to = targetAt (i);
                since = 0;
            }
            gain = since < glide ? from + (to - from) * (since + 1) / glide : to;
            ++since;
            reference.setSample (0, i, input.getSample (0, i) * (float) std::pow (10.0, gain / 20.0));
        }

        CHECK (allFinite (output));
        const auto start = (int) (0.05 * fs);
        const auto score = measure::clickScore (output.getReadPointer (0), output.getNumSamples(), fs, start);
        const auto ideal = measure::clickScore (reference.getReadPointer (0), reference.getNumSamples(), fs, start);
        INFO ("click score " << score << ", ideal glide " << ideal);
        CHECK (score < 1.5 * ideal + 1.0e-4);
    }
}

TEST_CASE ("EQ module: changing shape, slope, bypass or deleting a band crossfades without clicks", "[eq][module][phase1]")
{
    const double fs = 48000.0;
    EqRig rig (fs);
    const auto input = makeSine (2, (int) (3.0 * fs), 150.0, fs, 0.5f);

    eq::Parameters p;
    p.bands[0] = band (FilterType::bell, 160.0, 9.0, 2.0);
    rig.set (p);

    const auto output = rig.run (input, [&] (int start)
                                 {
                                     const auto step = start / (int) (0.2 * fs);
                                     auto& b = p.bands[0];
                                     b.active = step != 9;     // deleted, then created again
                                     b.bypassed = step == 12;  // bypassed for a moment
                                     static constexpr std::array<FilterType, 5> shapes { FilterType::bell, FilterType::notch, FilterType::lowCut,
                                                                                         FilterType::highShelf, FilterType::bandPass };
                                     b.settings.type = shapes[(size_t) (step % 5)];
                                     b.settings.slopeIndex = step % 8;
                                     rig.set (p);
                                 });

    const auto score = measure::clickScore (output.getReadPointer (0), output.getNumSamples(), fs, (int) (0.05 * fs));
    INFO ("click score " << score);
    CHECK (score < 1.0e-3);
}

TEST_CASE ("EQ module: extreme settings stay finite and bounded", "[eq][module][phase1]")
{
    for (const auto fs : { 44100.0, 192000.0 })
    {
        EqRig rig (fs);
        eq::Parameters p;
        p.bands[0] = band (FilterType::bell, 20000.0, 30.0, 40.0);
        p.bands[1] = band (FilterType::bell, 10.0, -30.0, 0.025);
        p.bands[2] = band (FilterType::lowCut, 30000.0, 0.0, 40.0, 7);
        p.bands[3] = band (FilterType::highCut, 10.0, 0.0, 0.025, 7);
        p.bands[4] = band (FilterType::tiltShelf, 30000.0, 30.0, 40.0);
        p.bands[5] = band (FilterType::lowShelf, 10.0, 30.0, 40.0);
        p.character = eq::Character::warm;
        rig.set (p);

        juce::Random random (3);
        const auto output = rig.run (makeNoise (2, (int) fs, 0.5f), [&] (int)
                                     {
                                         // Random jumps every block, like wild automation.
                                         for (auto& b : p.bands)
                                         {
                                             b.settings.frequency = dsp::eq::minFrequency * std::pow (3000.0, random.nextDouble());
                                             b.settings.q = dsp::eq::minQ * std::pow (1600.0, random.nextDouble());
                                             b.settings.gainDb = -30.0 + 60.0 * random.nextDouble();
                                         }
                                         rig.set (p);
                                     });
        INFO (fs);
        CHECK (allFinite (output));
        CHECK (peakOf (output) < 1.0e4f);
    }
}

TEST_CASE ("EQ module: solo plays the band's region, bypass removes the band", "[eq][module][phase1]")
{
    const double fs = 48000.0;
    EqRig rig (fs);

    eq::Parameters p;
    p.bands[0] = band (FilterType::bell, 1000.0, 6.0, 2.0);
    p.bands[1] = band (FilterType::lowCut, 100.0, 0.0, 1.0, 3);
    rig.set (p);
    rig.settle();
    CHECK (gainDbAt (rig, 1000.0) == Catch::Approx (6.0).margin (0.1));

    // Solo of the bell: a band pass at 1 kHz from the EQ input, without the bell's own gain.
    p.soloBand = 0;
    rig.set (p);
    CHECK (gainDbAt (rig, 1000.0) == Catch::Approx (0.0).margin (0.1));
    CHECK (gainDbAt (rig, 100.0) < -20.0);

    // Solo of the low cut: what it removes (the lows), not what it keeps.
    p.soloBand = 1;
    rig.set (p);
    CHECK (gainDbAt (rig, 30.0) > -1.0);
    CHECK (gainDbAt (rig, 1000.0) < -40.0);

    // Back to normal, then the bell bypassed: 1 kHz at 0 dB.
    p.soloBand = -1;
    rig.set (p);
    CHECK (gainDbAt (rig, 1000.0) == Catch::Approx (6.0).margin (0.1));
    p.bands[0].bypassed = true;
    rig.set (p);
    CHECK (gainDbAt (rig, 1000.0) == Catch::Approx (0.0).margin (0.05));

    // Solo of a band that does not exist does nothing.
    p.soloBand = 4;
    rig.set (p);
    CHECK (gainDbAt (rig, 1000.0) == Catch::Approx (0.0).margin (0.05));
}

TEST_CASE ("EQ module: Subtle and Warm colour gently, grow with level and barely alias", "[eq][module][character][phase1]")
{
    const double fs = 48000.0;
    const auto rigWith = [fs] (eq::Character character)
    {
        auto rig = std::make_unique<EqRig> (fs);
        eq::Parameters p;
        p.character = character;
        rig->set (p);
        rig->settle();
        return rig;
    };

    const auto minus12 = std::pow (10.0, -12.0 / 20.0);

    SECTION ("Subtle: about 0.1 % at 1 kHz, odd harmonics, more in the lows")
    {
        auto rig = rigWith (eq::Character::subtle);
        const auto a = analyse (*rig, 1000.0, minus12);
        INFO ("THD " << a.thdDb() << " dB, 2nd " << measure::toDb (a.harmonics[0] / a.fundamental) << ", 3rd "
                     << measure::toDb (a.harmonics[1] / a.fundamental));
        CHECK (a.thdDb() == Catch::Approx (-60.0).margin (3.0));
        CHECK (a.harmonics[1] > 30.0 * a.harmonics[0]); // the 3rd dominates

        const auto low = analyse (*rig, 60.0, minus12);
        INFO ("THD at 60 Hz " << low.thdDb() << " dB");
        CHECK (low.thdDb() > a.thdDb() + 6.0);
    }

    SECTION ("Warm: about 0.5 % at 1 kHz, mostly the 2nd harmonic")
    {
        auto rig = rigWith (eq::Character::warm);
        const auto a = analyse (*rig, 1000.0, minus12);
        INFO ("THD " << a.thdDb() << " dB, 2nd " << measure::toDb (a.harmonics[0] / a.fundamental) << ", 3rd "
                     << measure::toDb (a.harmonics[1] / a.fundamental));
        CHECK (a.thdDb() == Catch::Approx (-46.0).margin (3.0));
        CHECK (a.harmonics[0] > 3.0 * a.harmonics[1]);
    }

    for (const auto character : { eq::Character::subtle, eq::Character::warm })
    {
        auto rig = rigWith (character);
        INFO ("character " << (int) character);

        // More level, more colour.
        const auto quiet = analyse (*rig, 1000.0, std::pow (10.0, -24.0 / 20.0));
        const auto loud = analyse (*rig, 1000.0, std::pow (10.0, -6.0 / 20.0));
        CHECK (loud.thdDb() > quiet.thdDb() + 6.0);

        // A loud 10 kHz tone at -18 dBFS: aliasing 75 dB below it (under -93 dBFS), far below
        // audibility. Real high-frequency content is much quieter, and the aliasing falls 2 dB
        // for each dB less (3rd order).
        const auto high = analyse (*rig, 10000.0, std::pow (10.0, -18.0 / 20.0));
        INFO ("aliasing at 10 kHz " << high.otherDb() << " dB");
        CHECK (high.otherDb() < -75.0);

        // Even at +6 dBFS the output stays bounded.
        const auto hot = rig->run (makeSine (2, 48000, 100.0, fs, 2.0f));
        CHECK (allFinite (hot));
        CHECK (peakOf (hot) < 3.0f);
    }
}

TEST_CASE ("EQ module: switching the character crossfades without clicks", "[eq][module][character][phase1]")
{
    const double fs = 48000.0;
    EqRig rig (fs);
    eq::Parameters p;
    p.bands[0] = band (FilterType::bell, 150.0, 3.0); // keeps the module engaged throughout
    rig.set (p);
    const auto output = rig.run (makeSine (2, (int) (2.0 * fs), 150.0, fs, 0.5f), [&] (int start)
                                 {
                                     p.character = (eq::Character) ((start / (int) (0.3 * fs)) % 3);
                                     rig.set (p);
                                 });
    const auto score = measure::clickScore (output.getReadPointer (0), output.getNumSamples(), fs, (int) (0.05 * fs));
    INFO ("click score " << score);
    CHECK (score < 1.0e-3);
}

TEST_CASE ("EQ in the plugin: no allocation with every band, solo and character, under automation", "[eq][realtime][phase1]")
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, 48000.0, 512, 2);
    setParameter (processor, param::id::eqCharacter, 2.0f);
    for (int b = 0; b < eq::numBands; ++b)
    {
        setParameter (processor, param::eqBandId (b, param::EqField::active), 1.0f);
        setParameter (processor, param::eqBandId (b, param::EqField::type), (float) b);
        setParameter (processor, param::eqBandId (b, param::EqField::gain), 6.0f);
        setParameter (processor, param::eqBandId (b, param::EqField::slope), 7.0f);
    }
    processor.setEqSoloBand (2);

    juce::AudioBuffer<float> buffer (4, 512);
    juce::MidiBuffer midi;
    juce::Random random (5);
    long long allocations = 0;
    for (int block = 0; block < 200; ++block)
    {
        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.3f * (2.0f * random.nextFloat() - 1.0f));

        // Automation between blocks (the host thread in real use).
        setParameter (processor, param::eqBandId (block % eq::numBands, param::EqField::frequency), 20.0f + 15000.0f * random.nextFloat());
        if (block == 100)
            processor.setEqSoloBand (-1);

        ScopedAllocationCounter counter;
        processor.processBlock (buffer, midi);
        allocations += counter.getCount();
    }
    CHECK (allocations == 0);
    CHECK (allFinite (buffer));
}

TEST_CASE ("EQ parameters: names, ranges, defaults and what the engine reads", "[eq][parameters][phase1]")
{
    PMelodiesProcessor processor;
    auto& state = processor.getState();

    for (int b = 0; b < eq::numBands; ++b)
    {
        INFO ("band " << b + 1);
        auto* frequency = state.getParameter (param::eqBandId (b, param::EqField::frequency));
        auto* gain = state.getParameter (param::eqBandId (b, param::EqField::gain));
        auto* q = state.getParameter (param::eqBandId (b, param::EqField::q));
        auto* type = state.getParameter (param::eqBandId (b, param::EqField::type));
        auto* slope = state.getParameter (param::eqBandId (b, param::EqField::slope));
        auto* active = state.getParameter (param::eqBandId (b, param::EqField::active));
        REQUIRE (frequency != nullptr);
        REQUIRE (gain != nullptr);
        REQUIRE (q != nullptr);
        REQUIRE (type != nullptr);
        REQUIRE (slope != nullptr);
        REQUIRE (active != nullptr);

        CHECK (frequency->getName (100) == juce::String (juce::CharPointer_UTF8 ("EQ \xc2\xb7 Banda ")) + juce::String (b + 1)
                                               + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 Frecuencia")));
        CHECK (frequency->convertFrom0to1 (0.0f) == Catch::Approx (10.0f));
        CHECK (frequency->convertFrom0to1 (1.0f) == Catch::Approx (30000.0f));
        CHECK (frequency->convertFrom0to1 (0.5f) == Catch::Approx (std::sqrt (10.0f * 30000.0f)).epsilon (0.001)); // logarithmic
        CHECK (frequency->convertFrom0to1 (frequency->getDefaultValue()) == Catch::Approx (param::eqDefaultFrequencies[(size_t) b]));
        CHECK (gain->convertFrom0to1 (0.0f) == Catch::Approx (-30.0f));
        CHECK (gain->convertFrom0to1 (1.0f) == Catch::Approx (30.0f));
        CHECK (q->convertFrom0to1 (0.0f) == Catch::Approx (0.025f));
        CHECK (q->convertFrom0to1 (1.0f) == Catch::Approx (40.0f));
        CHECK (q->convertFrom0to1 (q->getDefaultValue()) == Catch::Approx (1.0f));
        CHECK (active->getValue() == 0.0f); // no band until one is created
        CHECK (slope->getCurrentValueAsText() == "12 dB/oct");
        CHECK (type->getCurrentValueAsText() == "Campana");
    }

    // Values as text, Spanish style, and typed back.
    auto* frequency = state.getParameter (param::eqBandId (0, param::EqField::frequency));
    CHECK (frequency->getText (frequency->convertTo0to1 (1250.0f), 20) == "1,25 kHz");
    CHECK (frequency->getText (frequency->convertTo0to1 (400.0f), 20) == "400 Hz");
    CHECK (frequency->convertFrom0to1 (frequency->getValueForText ("2k")) == Catch::Approx (2000.0f));
    CHECK (frequency->convertFrom0to1 (frequency->getValueForText ("2,5 kHz")) == Catch::Approx (2500.0f));

    // The engine reads them.
    setParameter (processor, param::eqBandId (2, param::EqField::active), 1.0f);
    setParameter (processor, param::eqBandId (2, param::EqField::type), 3.0f);
    setParameter (processor, param::eqBandId (2, param::EqField::frequency), 80.0f);
    setParameter (processor, param::eqBandId (2, param::EqField::gain), -4.5f);
    setParameter (processor, param::eqBandId (2, param::EqField::q), 2.0f);
    setParameter (processor, param::eqBandId (2, param::EqField::slope), 5.0f);
    setParameter (processor, param::eqBandId (2, param::EqField::bypass), 1.0f);
    setParameter (processor, param::id::eqCharacter, 1.0f);
    const auto read = param::ParameterReader (state).read().modules.eq;
    const auto& b = read.bands[2];
    CHECK (b.active);
    CHECK (b.bypassed);
    CHECK (b.settings.type == FilterType::lowCut);
    CHECK (b.settings.frequency == Catch::Approx (80.0));
    CHECK (b.settings.gainDb == Catch::Approx (-4.5));
    CHECK (b.settings.q == Catch::Approx (2.0));
    CHECK (dsp::eq::slopesDbPerOctave[(size_t) b.settings.slopeIndex] == 48);
    CHECK (read.character == eq::Character::subtle);
    CHECK_FALSE (read.bands[0].active);
}


