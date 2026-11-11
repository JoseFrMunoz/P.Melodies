// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 1 tests of the EQ filter design: response against the analog prototype at several
// sample rates, no cramping near Nyquist, exact centre gains, notch depth, stability at the
// extremes and the realisation as TPT state-variable filters.

#include "Measurement.h"
#include "TestHelpers.h"

#include "dsp/eq/EqFilterDesign.h"
#include "dsp/eq/SvfCascade.h"

#include <catch2/catch_approx.hpp>
#include <numbers>
#include <catch2/catch_test_macros.hpp>

using namespace pm::dsp::eq;

namespace
{
constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };

/** Level in dB with a -40 dB floor: below it (stop bands, notch centres) only the shape matters. */
double floorDb (std::complex<double> h)
{
    return std::max (-40.0, 20.0 * std::log10 (std::max (std::abs (h), 1.0e-12)));
}

struct Deviation
{
    double belowQuarter = 0.0; // up to fs/4
    double upper = 0.0;        // from fs/4 to 0.45·fs
};

/** Largest dB deviation of the digital band from its analog prototype, from 10 Hz to 0.45·fs. */
Deviation deviation (const BandSettings& settings, double fs)
{
    const auto digital = designBand (settings, fs);
    auto limited = settings;
    limited.frequency = digital.frequency;
    const auto analog = makeAnalogPrototype (limited);

    Deviation d;
    for (double f = 10.0; f < 0.45 * fs; f *= 1.01)
    {
        const auto e = std::abs (floorDb (analog.response (f)) - floorDb (digital.response (f, fs)));
        (f < 0.25 * fs ? d.belowQuarter : d.upper) = std::max (f < 0.25 * fs ? d.belowQuarter : d.upper, e);
    }
    return d;
}

std::string describe (const BandSettings& s, double fs)
{
    return "type " + std::to_string ((int) s.type) + ", f " + std::to_string (s.frequency) + " Hz, gain "
           + std::to_string (s.gainDb) + " dB, Q " + std::to_string (s.q) + ", slope "
           + std::to_string (slopesDbPerOctave[(size_t) s.slopeIndex]) + ", fs " + std::to_string (fs);
}

/** Runs a band on an impulse through the real-time cascade and returns the impulse response. */
std::vector<double> runImpulse (const DigitalBand& band, int length)
{
    SvfCascade cascade;
    cascade.setCoefficients (band);
    std::vector<double> out ((size_t) length);
    for (int i = 0; i < length; ++i)
        out[(size_t) i] = cascade.processSample (i == 0 ? 1.0f : 0.0f, 0);
    return out;
}
} // namespace

TEST_CASE ("EQ: bells, shelves and tilt follow the analog response up to near Nyquist", "[eq][design][phase1]")
{
    // Typical bands (up to fs/8, the top octave of the band range at 44.1-48 kHz); measured
    // worst cases are about 0.26 dB below fs/4 and 0.65 dB above.
    for (const auto fs : sampleRates)
        for (const auto type : { FilterType::bell, FilterType::lowShelf, FilterType::highShelf, FilterType::tiltShelf })
            for (const auto f : { 30.0, 100.0, 1000.0, fs / 8.0 })
                for (const auto q : type == FilterType::bell ? std::vector<double> { 0.3, 1.0, 3.0, 10.0 } : std::vector<double> { 0.5, 1.0, 2.0 })
                    for (const auto gain : { -18.0, -6.0, 6.0, 18.0 })
                    {
                        const BandSettings s { type, f, type == FilterType::tiltShelf ? gain * 2.0 / 3.0 : gain, q };
                        const auto d = deviation (s, fs);
                        INFO (describe (s, fs));
                        CHECK (d.belowQuarter < 0.32);
                        CHECK (d.upper < 0.75);
                    }
}

TEST_CASE ("EQ: cuts, band pass and notch follow the analog response", "[eq][design][phase1]")
{
    for (const auto fs : sampleRates)
        for (const auto f : { 30.0, 100.0, 1000.0, fs / 8.0 })
        {
            for (int slope = 0; slope < (int) slopesDbPerOctave.size(); ++slope)
                for (const auto q : { 0.5, 1.0, 2.0 })
                {
                    const BandSettings low { FilterType::lowCut, f, 0.0, q, slope };
                    const auto dl = deviation (low, fs);
                    INFO (describe (low, fs));
                    CHECK (dl.belowQuarter < 0.45);
                    CHECK (dl.upper < 0.45);

                    // Above the cut-off of a high cut, the analog keeps falling while any digital
                    // filter flattens towards Nyquist: about 1.6 dB more there, 30-40 dB down.
                    const BandSettings high { FilterType::highCut, f, 0.0, q, slope };
                    const auto dh = deviation (high, fs);
                    INFO (describe (high, fs));
                    CHECK (dh.belowQuarter < 1.2);
                    CHECK (dh.upper < 1.9);
                }

            for (const auto q : { 0.3, 1.0, 3.0, 10.0 })
            {
                const BandSettings bandPass { FilterType::bandPass, f, 0.0, q };
                const auto d = deviation (bandPass, fs);
                INFO (describe (bandPass, fs));
                CHECK (d.belowQuarter < 1.0);
                CHECK (d.upper < 1.1);

                // Notch: exactly zero at its frequency, and close everywhere else (linear scale,
                // since dB exaggerate tiny differences next to the zero).
                const BandSettings notch { FilterType::notch, f, 0.0, q };
                const auto digital = designBand (notch, fs);
                const auto analog = makeAnalogPrototype (notch);
                INFO (describe (notch, fs));
                CHECK (std::abs (digital.response (f, fs)) < 1.0e-3);
                double worst = 0.0;
                for (double probe = 10.0; probe < 0.45 * fs; probe *= 1.01)
                    worst = std::max (worst, std::abs (std::abs (analog.response (probe)) - std::abs (digital.response (probe, fs))));
                CHECK (worst < 0.04);
            }
        }
}

TEST_CASE ("EQ: near Nyquist the curve does not cramp like a plain bilinear EQ", "[eq][design][phase1]")
{
    // A +12 dB bell at 10 kHz, Q 1, at 44.1 kHz: the textbook bilinear design (RBJ cookbook) has
    // to reach 0 dB at Nyquist, so its bell narrows and drops several dB at 16-20 kHz. Ours does not.
    const double fs = 44100.0, f0 = 10000.0, gainDb = 12.0, q = 1.0;
    const BandSettings s { FilterType::bell, f0, gainDb, q };
    const auto ours = designBand (s, fs);
    const auto analog = makeAnalogPrototype (s);

    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = 2.0 * std::numbers::pi * f0 / fs;
    const auto alpha = std::sin (w0) / (2.0 * q);
    const std::array<double, 3> b { 1.0 + alpha * A, -2.0 * std::cos (w0), 1.0 - alpha * A };
    const std::array<double, 3> a { 1.0 + alpha / A, -2.0 * std::cos (w0), 1.0 - alpha / A };
    const auto bilinear = [&] (double f)
    {
        const auto z1 = std::polar (1.0, -2.0 * std::numbers::pi * f / fs);
        return (b[0] + b[1] * z1 + b[2] * z1 * z1) / (a[0] + a[1] * z1 + a[2] * z1 * z1);
    };

    for (const auto f : { 14000.0, 16000.0, 18000.0, 20000.0 })
    {
        const auto target = floorDb (analog.response (f));
        INFO (f << " Hz: analog " << target << " dB, ours " << floorDb (ours.response (f, fs)) << " dB, bilinear "
                << floorDb (bilinear (f)) << " dB");
        CHECK (std::abs (floorDb (ours.response (f, fs)) - target) < 0.6);
        CHECK (std::abs (floorDb (bilinear (f)) - target) > 2.0);
    }
}

TEST_CASE ("EQ: gains are exact where they are defined", "[eq][design][phase1]")
{
    for (const auto fs : sampleRates)
        for (const auto f : { 50.0, 1000.0, 8000.0 })
            for (const auto gain : { -24.0, -9.0, 9.0, 24.0 })
            {
                INFO ("fs " << fs << ", f " << f << ", gain " << gain);

                // A bell reaches its gain at its frequency; shelves are halfway at theirs.
                CHECK (floorDb (designBand ({ FilterType::bell, f, gain, 1.0 }, fs).response (f, fs)) == Catch::Approx (gain).margin (0.05));
                CHECK (floorDb (designBand ({ FilterType::lowShelf, f, gain, 1.0 }, fs).response (f, fs)) == Catch::Approx (gain / 2.0).margin (0.1));
                CHECK (floorDb (designBand ({ FilterType::highShelf, f, gain, 1.0 }, fs).response (f, fs)) == Catch::Approx (gain / 2.0).margin (0.1));

                // Shelves reach the full gain at their end (DC is matched exactly); the tilt goes
                // from -G to +G through 0 dB at its frequency.
                CHECK (floorDb (designBand ({ FilterType::lowShelf, f, gain, 1.0 }, fs).response (0.0, fs)) == Catch::Approx (gain).margin (1.0e-6));
                const auto tilt = designBand ({ FilterType::tiltShelf, f, gain / 2.0, 1.0 }, fs);
                CHECK (floorDb (tilt.response (f, fs)) == Catch::Approx (0.0).margin (0.1));
                CHECK (floorDb (tilt.response (0.0, fs)) == Catch::Approx (-gain / 2.0).margin (1.0e-6));
            }

    // With the gain at 0 dB, the gain-based shapes are a straight line.
    for (const auto type : { FilterType::bell, FilterType::lowShelf, FilterType::highShelf, FilterType::tiltShelf })
    {
        const auto flat = designBand ({ type, 2000.0, 0.0, 2.0 }, 48000.0);
        for (double f = 10.0; f < 24000.0; f *= 1.1)
            CHECK (std::abs (flat.response (f, 48000.0)) == Catch::Approx (1.0).margin (1.0e-9));
    }
}

TEST_CASE ("EQ: cut slopes from 6 to 96 dB/oct", "[eq][design][phase1]")
{
    for (int slope = 0; slope < (int) slopesDbPerOctave.size(); ++slope)
    {
        const auto dbPerOctave = (double) slopesDbPerOctave[(size_t) slope];
        const BandSettings lowSettings { FilterType::lowCut, 4000.0, 0.0, 1.0, slope };
        const BandSettings highSettings { FilterType::highCut, 250.0, 0.0, 1.0, slope };
        const auto low = designBand (lowSettings, 48000.0);
        const auto high = designBand (highSettings, 48000.0);
        const auto lowAnalog = makeAnalogPrototype (lowSettings);
        const auto highAnalog = makeAnalogPrototype (highSettings);

        // Two octaves into the stop band, the drop per octave approaches n·6 dB, like the analog.
        const auto drop = [] (auto&& response, double a, double b) { return 20.0 * std::log10 (std::abs (response (a)) / std::abs (response (b))); };
        const auto lowDrop = drop ([&] (double f) { return low.response (f, 48000.0); }, 1000.0, 500.0);
        const auto highDrop = drop ([&] (double f) { return high.response (f, 48000.0); }, 1000.0, 2000.0);
        const auto lowAnalogDrop = drop ([&] (double f) { return lowAnalog.response (f); }, 1000.0, 500.0);
        const auto highAnalogDrop = drop ([&] (double f) { return highAnalog.response (f); }, 1000.0, 2000.0);
        INFO (dbPerOctave << " dB/oct: low cut " << lowDrop << " (analog " << lowAnalogDrop << "), high cut " << highDrop
                          << " (analog " << highAnalogDrop << ")");
        CHECK (lowDrop == Catch::Approx (lowAnalogDrop).margin (0.05 * dbPerOctave));
        CHECK (highDrop == Catch::Approx (highAnalogDrop).margin (0.05 * dbPerOctave));
        CHECK (lowAnalogDrop == Catch::Approx (dbPerOctave).margin (0.1 * dbPerOctave));
        CHECK (highAnalogDrop == Catch::Approx (dbPerOctave).margin (0.1 * dbPerOctave));

        // At the cut-off, Q 1 is the maximally flat response: -3 dB.
        CHECK (floorDb (low.response (4000.0, 48000.0)) == Catch::Approx (-3.01).margin (0.15));
        CHECK (floorDb (high.response (250.0, 48000.0)) == Catch::Approx (-3.01).margin (0.15));
    }
}

TEST_CASE ("EQ: every design is stable, even at the extreme settings", "[eq][design][phase1]")
{
    for (const auto fs : sampleRates)
        for (int type = 0; type < numFilterTypes; ++type)
            for (const auto f : { minFrequency, 1000.0, 20000.0, maxFrequency })
                for (const auto q : { minQ, 1.0, maxQ })
                    for (const auto gain : { -maxGainDb, 0.0, maxGainDb })
                        for (int slope = 0; slope < (usesSlope ((FilterType) type) ? (int) slopesDbPerOctave.size() : 1); ++slope)
                        {
                            const BandSettings s { (FilterType) type, f, gain, q, usesSlope ((FilterType) type) ? slope : defaultSlopeIndex };
                            const auto band = designBand (s, fs);
                            INFO (describe (s, fs));
                            REQUIRE (band.frequency <= frequencyLimit (fs));

                            // Every section's poles are strictly inside the unit circle: for a TPT
                            // SVF that means g > 0 and k > 0 (a one-pole needs g > 0).
                            bool stable = true;
                            for (int i = 0; i < band.numSections; ++i)
                            {
                                const auto& section = band.sections[(size_t) i];
                                const auto a0 = 1.0 + section.g * section.k + section.g * section.g;
                                const auto a1 = 2.0 * (section.g * section.g - 1.0) / a0;
                                const auto a2 = (1.0 - section.g * section.k + section.g * section.g) / a0;
                                stable = stable && section.g > 0.0 && std::isfinite (section.g)
                                         && std::isfinite (section.m0) && std::isfinite (section.m1) && std::isfinite (section.m2)
                                         && (section.firstOrder || (section.k > 0.0 && std::abs (a2) < 1.0 && std::abs (a1) < 1.0 + a2));
                            }
                            CHECK (stable);

                            // And the real-time cascade stays finite.
                            const auto ir = runImpulse (band, 2048);
                            CHECK (std::all_of (ir.begin(), ir.end(), [] (double x) { return std::isfinite (x); }));
                        }
}

TEST_CASE ("EQ: the real-time SVF cascade produces the designed response", "[eq][design][phase1]")
{
    const std::array<BandSettings, 6> cases { BandSettings { FilterType::bell, 1000.0, 9.0, 2.0 },
                                              BandSettings { FilterType::lowShelf, 40.0, -12.0, 1.0 },
                                              BandSettings { FilterType::highShelf, 12000.0, 6.0, 1.0 },
                                              BandSettings { FilterType::lowCut, 80.0, 0.0, 1.5, 7 },
                                              BandSettings { FilterType::highCut, 9000.0, 0.0, 1.0, 2 },
                                              BandSettings { FilterType::notch, 3000.0, 0.0, 4.0 } };
    for (const auto fs : { 48000.0, 192000.0 })
        for (const auto& s : cases)
        {
            const auto band = designBand (s, fs);
            const auto ir = runImpulse (band, 1 << 17);
            INFO (describe (s, fs));
            for (const auto f : { 30.0, 200.0, 1000.0, 5000.0, 15000.0 })
            {
                const auto expected = std::abs (band.response (f, fs));
                const auto measured = std::abs (pmtest::measure::responseAt (ir, f, fs));
                if (expected > 1.0e-3)
                    CHECK (20.0 * std::log10 (measured / expected) == Catch::Approx (0.0).margin (0.01));
                else
                    CHECK (measured < 2.0e-3);
            }
        }
}

TEST_CASE ("EQ: a band can keep its design methods while it glides", "[eq][design][phase1]")
{
    // The full choice runs when a target changes; the intermediate steps of a glide reuse it.
    for (const auto type : { FilterType::bell, FilterType::highShelf, FilterType::lowCut, FilterType::notch })
    {
        DesignChoices choices;
        const BandSettings start { type, 1000.0, 9.0, 1.0, 3 };
        designBand (start, 48000.0, &choices);

        for (double f = 1000.0; f < 1200.0; f *= 1.02)
        {
            auto glide = start;
            glide.frequency = f;
            const auto reused = designBand (glide, 48000.0, choices);
            const auto fresh = designBand (glide, 48000.0);
            for (const auto probe : { 100.0, 1000.0, 5000.0 })
                CHECK (floorDb (reused.response (probe, 48000.0)) == Catch::Approx (floorDb (fresh.response (probe, 48000.0))).margin (0.3));
        }
    }
}
