// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

// The measurement tools are checked against signals whose answer is known exactly, so that the
// module tests that rely on them (phases 1-4) can trust what they measure.

#include "Measurement.h"
#include "dsp/OversamplingEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

using namespace pmtest::measure;

namespace
{
constexpr double pi = std::numbers::pi;

/** One-pole low-pass y[n] = (1 - a) x[n] + a y[n - 1], applied to channel 0. */
juce::AudioBuffer<float> onePole (const juce::AudioBuffer<float>& input, double a)
{
    auto output = input;
    double y = 0.0;
    for (int i = 0; i < output.getNumSamples(); ++i)
    {
        y = (1.0 - a) * input.getSample (0, i) + a * y;
        output.setSample (0, i, (float) y);
    }
    return output;
}

/** Sine on an exact analysis bin, `extra` samples longer than the analysis window. */
juce::AudioBuffer<float> binSine (int fftSize, int bin, int extra, float amplitude)
{
    juce::AudioBuffer<float> buffer (1, fftSize + extra);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (0, i, amplitude * (float) std::sin (2.0 * pi * bin * (double) i / fftSize));
    return buffer;
}

/** Drives a sine into tanh at the given oversampling factor; returns the analysed part. */
HarmonicAnalysis tanhAliasing (int factorIndex, int fftSize, int bin)
{
    const int warmUp = 8192;
    auto signal = binSine (fftSize, bin, warmUp, 0.5f);

    pm::dsp::OversamplingEngine oversampling;
    oversampling.prepare (1, 512);

    for (int start = 0; start < signal.getNumSamples(); start += 512)
    {
        const auto n = juce::jmin (512, signal.getNumSamples() - start);
        auto block = juce::dsp::AudioBlock<float> (signal).getSubBlock ((size_t) start, (size_t) n);
        oversampling.process (factorIndex, block,
                              [] (juce::dsp::AudioBlock<float> highRate)
                              {
                                  for (size_t ch = 0; ch < highRate.getNumChannels(); ++ch)
                                  {
                                      auto* x = highRate.getChannelPointer (ch);
                                      for (size_t i = 0; i < highRate.getNumSamples(); ++i)
                                          x[i] = std::tanh (4.0f * x[i]);
                                  }
                              });
    }

    return analyseHarmonics (signal.getReadPointer (0, warmUp), fftSize, bin);
}
} // namespace

TEST_CASE ("Measurement: the FFT matches a direct DFT", "[measure][phase0]")
{
    const size_t n = 256;
    std::vector<Complex> data (n);
    juce::Random random (3);
    for (auto& x : data)
        x = Complex (random.nextDouble() - 0.5, random.nextDouble() - 0.5);

    auto transformed = data;
    fft (transformed);

    double worst = 0.0;
    for (size_t k = 0; k < n; ++k)
    {
        Complex direct = 0.0;
        for (size_t i = 0; i < n; ++i)
            direct += data[i] * std::polar (1.0, -2.0 * pi * (double) (k * i) / (double) n);
        worst = std::max (worst, std::abs (direct - transformed[k]));
    }
    CHECK (worst < 1.0e-9);
}

TEST_CASE ("Measurement: impulse response and complex response of known linear paths", "[measure][phase0]")
{
    const double fs = 48000.0;

    SECTION ("A pure delay with gain: flat magnitude, linear phase, removable latency")
    {
        const int delay = 7;
        const ProcessFunction delayed = [delay] (const juce::AudioBuffer<float>& in)
        {
            juce::AudioBuffer<float> out (in.getNumChannels(), in.getNumSamples());
            out.clear();
            for (int ch = 0; ch < in.getNumChannels(); ++ch)
                out.addFrom (ch, delay, in, ch, 0, in.getNumSamples() - delay, 0.5f);
            return out;
        };

        const auto h = impulseResponse (delayed, 64, 2, 1);
        for (const double f : { 20.0, 1000.0, 15000.0, 23000.0 })
        {
            const auto response = responseAt (h, f, fs);
            CHECK (toDb (std::abs (response)) == Catch::Approx (-6.0206).margin (1.0e-4));
            CHECK (std::remainder (std::arg (response) + 2.0 * pi * f * delay / fs, 2.0 * pi) == Catch::Approx (0.0).margin (1.0e-9));
        }

        const auto aligned = impulseResponse (delayed, 64, 2, 1, delay);
        CHECK (std::arg (responseAt (aligned, 5000.0, fs)) == Catch::Approx (0.0).margin (1.0e-9));
    }

    SECTION ("A one-pole low-pass follows its analytic response")
    {
        const auto a = std::exp (-2.0 * pi * 1000.0 / fs);
        const ProcessFunction filter = [a] (const juce::AudioBuffer<float>& in) { return onePole (in, a); };
        const auto h = impulseResponse (filter, 8192);

        for (const double f : { 20.0, 100.0, 1000.0, 5000.0, 20000.0 })
        {
            const auto omega = 2.0 * pi * f / fs;
            const auto expected = (1.0 - a) / (1.0 - a * std::polar (1.0, -omega));
            const auto measured = responseAt (h, f, fs);
            INFO ("frequency " << f);
            CHECK (toDb (std::abs (measured)) == Catch::Approx (toDb (std::abs (expected))).margin (1.0e-4));
            CHECK (std::arg (measured) == Catch::Approx (std::arg (expected)).margin (1.0e-5));
            CHECK (magnitudeDbAt (h, f, fs) == Catch::Approx (toDb (std::abs (measured))));
        }
    }
}

TEST_CASE ("Measurement: sine fit recovers amplitude, phase and DC on any window", "[measure][phase0]")
{
    const double fs = 48000.0, f = 997.3, amplitude = 0.3, phase = 0.7, dc = 0.01;
    std::vector<float> x (6000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = (float) (dc + amplitude * std::sin (2.0 * pi * f * (double) i / fs + phase));

    // 4800 samples are 99.73 periods: no whole number of periods needed.
    const auto fit = fitSine (x.data(), 0, 4800, f, fs);
    CHECK (fit.amplitude == Catch::Approx (amplitude).margin (1.0e-6));
    CHECK (fit.phase == Catch::Approx (phase).margin (1.0e-5));
    CHECK (fit.dc == Catch::Approx (dc).margin (1.0e-6));

    // The phase is measured at the first sample of the window.
    const auto later = fitSine (x.data(), 1000, 6000, f, fs);
    CHECK (std::remainder (later.phase - (phase + 2.0 * pi * f * 1000.0 / fs), 2.0 * pi) == Catch::Approx (0.0).margin (1.0e-5));
}

TEST_CASE ("Measurement: harmonic analysis separates fundamental, harmonics and the rest", "[measure][phase0]")
{
    const int fftSize = 65536, bin = 1365; // odd bin: about 1 kHz at 48 kHz

    SECTION ("A synthetic signal with known content")
    {
        std::vector<float> x ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
        {
            const auto t = 2.0 * pi * (double) i / fftSize;
            x[(size_t) i] = (float) (0.02 + 0.5 * std::sin (bin * t) + 0.05 * std::sin (2 * bin * t + 0.3)
                                     + 0.005 * std::sin (3 * bin * t + 1.0) + 0.0005 * std::sin (1000 * t));
        }

        const auto analysis = analyseHarmonics (x.data(), fftSize, bin);
        CHECK (analysis.fundamental == Catch::Approx (0.5).margin (1.0e-6));
        REQUIRE (analysis.harmonics.size() >= 2);
        CHECK (analysis.harmonics[0] == Catch::Approx (0.05).margin (1.0e-6));
        CHECK (analysis.harmonics[1] == Catch::Approx (0.005).margin (1.0e-6));
        CHECK (analysis.thd == Catch::Approx (std::sqrt (0.05 * 0.05 + 0.005 * 0.005) / 0.5).margin (1.0e-5));
        CHECK (analysis.otherDb() == Catch::Approx (-60.0).margin (0.01));
        CHECK (analysis.dc == Catch::Approx (0.02).margin (1.0e-6));
    }

    SECTION ("A clean float sine shows nothing else")
    {
        const auto sine = binSine (fftSize, bin, 0, 0.5f);
        const auto analysis = analyseHarmonics (sine.getReadPointer (0), fftSize, bin);
        CHECK (analysis.fundamental == Catch::Approx (0.5).margin (1.0e-6));
        CHECK (analysis.thdDb() < -120.0);
        CHECK (analysis.otherDb() < -120.0);
    }

    SECTION ("Saturation without oversampling aliases; oversampling removes most of it")
    {
        const int highBin = 9557; // about 7 kHz at 48 kHz: its 5th harmonic is above Nyquist
        const auto plain = tanhAliasing (0, fftSize, highBin);
        const auto oversampled = tanhAliasing (3, fftSize, highBin);

        INFO ("aliasing at 1x " << plain.otherDb() << " dB, at 8x " << oversampled.otherDb() << " dB, THD 1x "
                                << plain.thdDb() << " dB, 8x " << oversampled.thdDb() << " dB");
        CHECK (plain.otherDb() > -40.0);
        CHECK (oversampled.otherDb() < plain.otherDb() - 30.0);
        // The harmonic below Nyquist (3rd, 21 kHz) is still there: oversampling removes aliasing, not distortion.
        CHECK (oversampled.thdDb() == Catch::Approx (plain.thdDb()).margin (3.0));
    }
}

TEST_CASE ("Measurement: gain trace and settling time", "[measure][phase0]")
{
    SECTION ("A one-pole step response reaches 63.2 % after one time constant and 90 % after 2.3")
    {
        const double tau = 480.0; // 10 ms at 48 kHz
        const auto a = std::exp (-1.0 / tau);
        std::vector<double> trace (5000, 0.0);
        double y = 0.0;
        for (size_t i = 1000; i < trace.size(); ++i)
        {
            y = (1.0 - a) * 1.0 + a * y;
            trace[i] = y;
        }

        CHECK (std::abs (timeToReach (trace, 1000, 0.0, 1.0, 0.632) + 1 - tau) <= 1.0);
        CHECK (std::abs (timeToReach (trace, 1000, 0.0, 1.0, 0.9) + 1 - tau * std::log (10.0)) <= 1.0);
        CHECK (timeToReach (trace, 1000, 0.0, 1.0, 1.1) == -1);

        // Falling direction.
        for (auto& v : trace)
            v = 1.0 - v;
        CHECK (std::abs (timeToReach (trace, 1000, 1.0, 0.0, 0.632) + 1 - tau) <= 1.0);
    }

    SECTION ("The gain trace compares output with input, latency included")
    {
        const int latency = 5, n = 2000;
        std::vector<float> input ((size_t) n), output ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
            input[(size_t) i] = 0.1f + 0.1f * (float) i / (float) n;
        for (int i = latency; i < n; ++i)
            output[(size_t) i] = 0.5f * input[(size_t) (i - latency)];

        const auto aligned = gainTraceDb (input.data(), output.data(), n, latency);
        REQUIRE (aligned.size() == (size_t) (n - latency));
        for (const auto g : aligned)
            REQUIRE (g == Catch::Approx (-6.0206).margin (1.0e-4));

        const auto misaligned = gainTraceDb (input.data(), output.data(), n, 0);
        CHECK (misaligned[100] != Catch::Approx (-6.0206).margin (1.0e-4));
    }
}

TEST_CASE ("Measurement: the click score finds discontinuities and ignores a low tone", "[measure][phase0]")
{
    const double fs = 48000.0;
    const int n = 48000, start = 4800;
    const int peak = n / 2 + 80; // a positive peak of the 150 Hz tone (a quarter period after a zero crossing)

    const auto tone = [&] (const std::function<float (int)>& gain, float offsetAfterMiddle = 0.0f)
    {
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] = gain (i) * 0.5f * (float) std::sin (2.0 * pi * 150.0 * i / fs)
                            + (i >= n / 2 ? offsetAfterMiddle : 0.0f);
        return x;
    };

    const auto clean = tone ([] (int) { return 1.0f; });
    const auto smallStep = tone ([] (int) { return 1.0f; }, 0.01f);
    const auto smoothFade = tone ([&] (int i) { return (float) juce::jlimit (0.5, 1.0, 1.0 - 0.5 * (i - peak) / (0.02 * fs)); });
    const auto abruptFade = tone ([&] (int i) { return i < peak ? 1.0f : 0.5f; });

    const auto scoreClean = clickScore (clean.data(), n, fs, start);
    const auto scoreStep = clickScore (smallStep.data(), n, fs, start);
    const auto scoreSmooth = clickScore (smoothFade.data(), n, fs, start);
    const auto scoreAbrupt = clickScore (abruptFade.data(), n, fs, start);

    INFO ("clean " << scoreClean << ", step 0.01 " << scoreStep << ", 20 ms fade " << scoreSmooth << ", abrupt "
                   << scoreAbrupt);
    CHECK (scoreClean < 1.0e-5);
    CHECK (scoreStep > 2.0e-3);
    CHECK (scoreSmooth < 1.0e-4);
    CHECK (scoreAbrupt > 1.0e-2);
}
