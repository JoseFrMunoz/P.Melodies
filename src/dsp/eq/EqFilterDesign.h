// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <array>
#include <cmath>
#include <complex>

/**
    Filter design for the EQ bands (docs/dsp/eq.md).

    Every band starts from an analog prototype (what an ideal analog EQ would do) and becomes a
    cascade of first- and second-order digital sections whose magnitude matches the prototype at
    DC, at the band frequency and at Nyquist, so the curve does not "cramp" near the top of the
    spectrum. Each section is realised as a TPT state-variable filter with an output mix, which
    stays stable and smooth while its frequency, gain and Q are automated quickly.

    Everything here is pure maths in double precision: no allocation, safe on the audio thread.
*/
namespace pm::dsp::eq
{

/** Band shapes. The numeric values are stored in presets: never reorder them. */
enum class FilterType : int
{
    bell = 0,
    lowShelf = 1,
    highShelf = 2,
    lowCut = 3,
    highCut = 4,
    notch = 5,
    bandPass = 6,
    tiltShelf = 7
};

inline constexpr int numFilterTypes = 8;

/** Slopes of the low and high cuts, in dB/octave. The index is stored in presets. */
inline constexpr std::array<int, 8> slopesDbPerOctave { 6, 12, 18, 24, 36, 48, 72, 96 };
inline constexpr int defaultSlopeIndex = 1; // 12 dB/oct

inline constexpr double minFrequency = 10.0;
inline constexpr double maxFrequency = 30000.0;
inline constexpr double maxGainDb = 30.0;
inline constexpr double minQ = 0.025;
inline constexpr double maxQ = 40.0;

/** Highest band frequency at a sample rate: just below Nyquist (spec §4). */
constexpr double frequencyLimit (double sampleRate) noexcept { return 0.475 * sampleRate; }

constexpr bool usesGain (FilterType t) noexcept
{
    return t == FilterType::bell || t == FilterType::lowShelf || t == FilterType::highShelf || t == FilterType::tiltShelf;
}

constexpr bool usesSlope (FilterType t) noexcept { return t == FilterType::lowCut || t == FilterType::highCut; }

/** A 6 dB/oct cut is a single first-order section: it has no Q. */
constexpr bool usesQ (FilterType t, int slopeIndex) noexcept
{
    return ! (usesSlope (t) && slopesDbPerOctave[(size_t) slopeIndex] == 6);
}

struct BandSettings
{
    FilterType type = FilterType::bell;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slopeIndex = defaultSlopeIndex;
};

/** Order 16 (96 dB/oct) needs eight second-order sections. */
inline constexpr int maxSections = 8;

/**
    One analog section, with s normalised so that s = j is the band frequency:
    H(s) = (n0 + n1 s + n2 s²) / (d0 + d1 s + d2 s²). First-order sections have n2 = d2 = 0.
*/
struct AnalogSection
{
    double n0 = 1.0, n1 = 0.0, n2 = 0.0;
    double d0 = 1.0, d1 = 0.0, d2 = 0.0;
    double fitSpread = 2.0; // the digital fit also matches the magnitude at the band frequency × and ÷ this

    bool isFirstOrder() const noexcept { return std::fpclassify (d2) == FP_ZERO; }
    std::complex<double> response (std::complex<double> s) const noexcept;
};

struct AnalogPrototype
{
    std::array<AnalogSection, maxSections> sections {};
    int numSections = 0;
    double gain = 1.0;         // overall linear gain (used by the tilt shelf)
    double frequency = 1000.0; // band frequency in Hz

    std::complex<double> response (double hz) const noexcept;
};

/** The ideal analog response of a band, as an analog EQ would have it. */
AnalogPrototype makeAnalogPrototype (const BandSettings& settings) noexcept;

/**
    A digital section realised as a TPT state-variable filter (Simper's notation: v0 is the input,
    v1 the band-pass state output and v2 the low-pass one) plus an output mix:
        y = m0·v0 + m1·v1 + m2·v2
    A first-order section is a TPT one-pole: y = m0·x + m2·lowpass.
*/
struct SvfSection
{
    double g = 0.0;
    double k = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;
    bool firstOrder = false;

    std::complex<double> response (double omega) const noexcept; // omega in radians per sample
};

/** How a digital section was obtained from its analog prototype (docs/dsp/eq.md). */
enum class SectionMethod : unsigned char
{
    automatic,    // try the candidates and keep the closest to the analog response
    firstOrder,   // one-pole, exact at DC and Nyquist
    fivePoint,    // poles and zeros fitted at DC, Nyquist and three points around the band
    leastSquares, // weighted least-squares magnitude fit, exact at DC and Nyquist
    threePoint,   // impulse-invariant poles, exact at DC, the band frequency and Nyquist
    inverted,     // inverse of the three-point match of the reciprocal section (deep cuts)
    highPassFit,  // double zero at DC, poles fitted around the cut-off and at Nyquist
    highPassMatch, // double zero at DC, impulse-invariant poles, exact at the cut-off
    notchFit       // zeros exactly at the notch frequency, poles fitted at DC and the notch edges
};

/** The method chosen for each section, so a gliding band can keep it (cheaper than choosing again). */
struct DesignChoices
{
    std::array<SectionMethod, maxSections> methods {};
};

struct DigitalBand
{
    std::array<SvfSection, maxSections> sections {};
    int numSections = 0;
    double frequency = 1000.0; // after limiting to the sample rate

    /** Exact response of the digital filter at a frequency in Hz. */
    std::complex<double> response (double hz, double sampleRate) const noexcept;
};

/**
    Designs a band for a sample rate, keeping for each section the candidate design closest to
    the analog prototype. The frequency is limited to frequencyLimit(sampleRate). When `chosen`
    is given, it receives the method used for each section.
*/
DigitalBand designBand (const BandSettings& settings, double sampleRate, DesignChoices* chosen = nullptr) noexcept;

/**
    Same, with the methods already chosen (for the intermediate steps of a parameter glide).
    A section whose method has no solution at these settings falls back to choosing again.
*/
DigitalBand designBand (const BandSettings& settings, double sampleRate, const DesignChoices& methods) noexcept;

/** Q of the k-th (0-based) second-order section of an order-n Butterworth filter, highest first. */
double butterworthQ (int order, int section) noexcept;

} // namespace pm::dsp::eq
