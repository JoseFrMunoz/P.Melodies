// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

/*
    Measurement tools for the DSP tests of every module. All the arithmetic is in double precision
    so that the tools are far more accurate than the float signals they measure. Each tool is
    itself checked against signals with a known answer in MeasurementTests.cpp.

    - Linear response: impulseResponse() + responseAt() give the exact complex response of a
      linear path at any frequency (EQ curves, crossovers, oversampling filters).
    - Sine fit: fitSine() measures the gain and phase of a path at one frequency, also for
      non-linear paths.
    - Harmonics and aliasing: analyseHarmonics() splits a periodic output into fundamental,
      harmonics and everything else (aliasing and noise). Use a test tone on an exact analysis
      bin (binFrequency()) so that harmonics and aliases fall on exact bins and nothing leaks.
    - Time behaviour: gainTraceDb() + timeToReach() measure attack, release and smoothing times.
    - Clicks: clickScore() is the peak of the signal after a steep high-pass, where a
      discontinuity shows up as a spike and a low test tone almost disappears.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <complex>
#include <functional>
#include <numbers>
#include <vector>

namespace pmtest::measure
{

using Complex = std::complex<double>;

inline double toDb (double gain) { return 20.0 * std::log10 (gain); }

//==============================================================================
// FFT (radix 2, double precision)

/** In-place forward FFT, X[k] = sum x[n] e^(-j 2 pi k n / N). The size must be a power of two. */
inline void fft (std::vector<Complex>& data)
{
    const auto n = data.size();
    jassert (n > 0 && (n & (n - 1)) == 0);

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (data[i], data[j]);
    }

    for (size_t length = 2; length <= n; length <<= 1)
    {
        const auto angle = -2.0 * std::numbers::pi / (double) length;
        for (size_t start = 0; start < n; start += length)
        {
            for (size_t k = 0; k < length / 2; ++k)
            {
                // Twiddle computed directly (not by repeated multiplication) to keep full accuracy.
                const Complex w (std::cos (angle * (double) k), std::sin (angle * (double) k));
                const auto even = data[start + k];
                const auto odd = data[start + k + length / 2] * w;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
            }
        }
    }
}

//==============================================================================
// Linear response

/** A processing path under test: takes an input buffer and returns the output buffer. */
using ProcessFunction = std::function<juce::AudioBuffer<float> (const juce::AudioBuffer<float>&)>;

/**
    Impulse response of channel `channel` of a path, `length` samples long, starting `latency`
    samples after the impulse (so a known latency can be removed). The impulse is scaled by
    `amplitude` (keep it small for paths that are only linear at low levels) and the result is
    divided by it again.
*/
inline std::vector<double> impulseResponse (const ProcessFunction& process, int length, int numChannels = 1,
                                            int channel = 0, int latency = 0, float amplitude = 1.0f)
{
    juce::AudioBuffer<float> input (numChannels, length + latency);
    input.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        input.setSample (ch, 0, amplitude);

    const auto output = process (input);
    jassert (output.getNumSamples() >= length + latency);

    std::vector<double> response ((size_t) length);
    for (int i = 0; i < length; ++i)
        response[(size_t) i] = (double) output.getSample (channel, i + latency) / (double) amplitude;
    return response;
}

/** H(f) = sum h[n] e^(-j 2 pi f n / fs), evaluated directly at any frequency (not just FFT bins). */
inline Complex responseAt (const std::vector<double>& impulse, double frequencyHz, double sampleRate)
{
    const auto omega = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
    Complex sum = 0.0;
    for (size_t n = 0; n < impulse.size(); ++n)
        sum += impulse[n] * std::polar (1.0, -omega * (double) n);
    return sum;
}

inline double magnitudeDbAt (const std::vector<double>& impulse, double frequencyHz, double sampleRate)
{
    return toDb (std::abs (responseAt (impulse, frequencyHz, sampleRate)));
}

//==============================================================================
// Sine fit

struct SineFit
{
    double amplitude = 0.0;
    double phase = 0.0; // radians: x[n] = dc + amplitude * sin (2 pi f n / fs + phase), n from `start`
    double dc = 0.0;
};

/**
    Least-squares fit of dc + a sin + b cos at a known frequency over [start, end). Exact for any
    window length (no need for whole periods), so it also measures gain and phase of non-linear
    paths at their fundamental.
*/
inline SineFit fitSine (const float* x, int start, int end, double frequencyHz, double sampleRate)
{
    const auto omega = 2.0 * std::numbers::pi * frequencyHz / sampleRate;

    // Normal equations for the basis {sin, cos, 1}.
    double m[3][3] {};
    double v[3] {};
    for (int i = start; i < end; ++i)
    {
        const auto t = omega * (double) (i - start);
        const double basis[3] { std::sin (t), std::cos (t), 1.0 };
        for (int r = 0; r < 3; ++r)
        {
            v[r] += basis[r] * (double) x[i];
            for (int c = 0; c < 3; ++c)
                m[r][c] += basis[r] * basis[c];
        }
    }

    // Gaussian elimination with partial pivoting.
    int order[3] { 0, 1, 2 };
    for (int col = 0; col < 3; ++col)
    {
        int pivot = col;
        for (int r = col + 1; r < 3; ++r)
            if (std::abs (m[order[r]][col]) > std::abs (m[order[pivot]][col]))
                pivot = r;
        std::swap (order[col], order[pivot]);
        for (int r = col + 1; r < 3; ++r)
        {
            const auto f = m[order[r]][col] / m[order[col]][col];
            for (int c = col; c < 3; ++c)
                m[order[r]][c] -= f * m[order[col]][c];
            v[order[r]] -= f * v[order[col]];
        }
    }
    double solution[3] {};
    for (int col = 2; col >= 0; --col)
    {
        auto s = v[order[col]];
        for (int c = col + 1; c < 3; ++c)
            s -= m[order[col]][c] * solution[c];
        solution[col] = s / m[order[col]][col];
    }

    SineFit fit;
    fit.amplitude = std::hypot (solution[0], solution[1]);
    fit.phase = std::atan2 (solution[1], solution[0]);
    fit.dc = solution[2];
    return fit;
}

//==============================================================================
// Harmonics and aliasing

/**
    Frequency of bin `bin` of an analysis of `fftSize` samples. A test tone at such a frequency
    has a whole number of periods in the analysis window: its harmonics and their aliases then
    land on exact bins and the analysis needs no window function.

    Pick an odd bin with a power-of-two size: then no alias of the first `bin` folds lands on a
    harmonic bin, so harmonics and aliasing are told apart exactly.
*/
inline double binFrequency (int bin, int fftSize, double sampleRate)
{
    return (double) bin * sampleRate / (double) fftSize;
}

struct HarmonicAnalysis
{
    double fundamental = 0.0;       // amplitude of the fundamental
    std::vector<double> harmonics;  // amplitudes of harmonics 2, 3, ... below Nyquist
    double thd = 0.0;               // sqrt (sum of harmonic powers) / fundamental
    double other = 0.0;             // everything else (aliasing + noise), RMS relative to the fundamental
    double dc = 0.0;

    double thdDb() const { return toDb (thd); }
    double otherDb() const { return toDb (other); }
};

/**
    Analyses x[0 .. fftSize) of a signal whose fundamental sits on bin `fundamentalBin` and that
    has settled (periodic in the window). Everything that is neither DC, the fundamental nor one of
    its harmonics below Nyquist counts as "other": aliasing, noise and intermodulation.
*/
inline HarmonicAnalysis analyseHarmonics (const float* x, int fftSize, int fundamentalBin)
{
    jassert (fundamentalBin > 0 && fundamentalBin < fftSize / 2);

    std::vector<Complex> spectrum ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        spectrum[(size_t) i] = (double) x[i];
    fft (spectrum);

    const auto half = fftSize / 2;
    const auto amplitudeOfBin = [&] (int k)
    {
        const auto scale = (k == 0 || k == half) ? 1.0 : 2.0;
        return scale * std::abs (spectrum[(size_t) k]) / (double) fftSize;
    };

    HarmonicAnalysis result;
    result.dc = amplitudeOfBin (0);
    result.fundamental = amplitudeOfBin (fundamentalBin);

    double harmonicPower = 0.0, otherPower = 0.0;
    for (int k = 1; k <= half; ++k)
    {
        const auto a = amplitudeOfBin (k);
        if (k == fundamentalBin)
            continue;
        if (k % fundamentalBin == 0)
        {
            result.harmonics.push_back (a);
            harmonicPower += a * a;
        }
        else
        {
            otherPower += a * a;
        }
    }

    result.thd = std::sqrt (harmonicPower) / result.fundamental;
    result.other = std::sqrt (otherPower) / result.fundamental;
    return result;
}

//==============================================================================
// Time behaviour

/**
    Gain in dB from input to output, sample by sample: 20 log10 |out[n + latency] / in[n]|.
    Use a constant (DC) or slowly varying input so that every sample is meaningful; samples where
    the input is (almost) zero repeat the previous value.
*/
inline std::vector<double> gainTraceDb (const float* input, const float* output, int numSamples, int latency = 0)
{
    std::vector<double> trace ((size_t) juce::jmax (0, numSamples - latency));
    double last = 0.0;
    for (size_t i = 0; i < trace.size(); ++i)
    {
        const auto in = std::abs ((double) input[i]);
        if (in > 1.0e-9)
            last = toDb (std::abs ((double) output[i + (size_t) latency]) / in);
        trace[i] = last;
    }
    return trace;
}

/**
    Number of samples from `stepIndex` until `trace` has covered `fraction` of the way from
    `initial` to `final` (rising or falling). Returns -1 if it never gets there.
    Example: fraction 1 - 1/e (63.2 %) gives the time constant of a one-pole smoother.
*/
inline int timeToReach (const std::vector<double>& trace, int stepIndex, double initial, double final, double fraction)
{
    const auto span = final - initial;
    jassert (std::abs (span) > 0.0);
    for (size_t i = (size_t) stepIndex; i < trace.size(); ++i)
        if ((trace[i] - initial) / span >= fraction)
            return (int) i - stepIndex;
    return -1;
}

//==============================================================================
// Clicks

/**
    Peak of |x| after a 4th-order Butterworth high-pass at `cutoffHz`, from `start` on (the first
    samples let the filter settle). At 48 kHz a jump of size d gives a score of about 0.4 d, a clean
    150 Hz tone at -6 dBFS about 5e-8 and a 20 ms linear fade on it about 6e-5 (measured in
    MeasurementTests.cpp). Use such a low tone: it is attenuated by 24 dB per octave below the
    cutoff, so what remains is the click.
*/
inline double clickScore (const float* x, int numSamples, double sampleRate, int start = 0, double cutoffHz = 8000.0)
{
    // Two RBJ high-pass biquads with the Q values of a 4th-order Butterworth.
    struct Biquad
    {
        double b0, b1, b2, a1, a2, z1 = 0.0, z2 = 0.0;

        double process (double in)
        {
            const auto out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }
    };

    const auto makeHighPass = [&] (double q)
    {
        const auto w = 2.0 * std::numbers::pi * cutoffHz / sampleRate;
        const auto alpha = std::sin (w) / (2.0 * q);
        const auto a0 = 1.0 + alpha;
        Biquad b {};
        b.b0 = (1.0 + std::cos (w)) / 2.0 / a0;
        b.b1 = -(1.0 + std::cos (w)) / a0;
        b.b2 = b.b0;
        b.a1 = -2.0 * std::cos (w) / a0;
        b.a2 = (1.0 - alpha) / a0;
        return b;
    };

    auto first = makeHighPass (0.54119610014619698); // 1 / (2 cos (pi / 8))
    auto second = makeHighPass (1.3065629648763766); // 1 / (2 cos (3 pi / 8))

    double peak = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const auto y = second.process (first.process ((double) x[i]));
        if (i >= start)
            peak = juce::jmax (peak, std::abs (y));
    }
    return peak;
}

} // namespace pmtest::measure
