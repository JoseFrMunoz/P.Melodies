// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "dsp/eq/EqFilterDesign.h"

#include <algorithm>
#include <cmath>

namespace pm::dsp::eq
{

namespace
{
using Complex = std::complex<double>;

bool isZero (double x) noexcept { return std::fpclassify (x) == FP_ZERO; }

constexpr double pi = 3.14159265358979323846;
constexpr double inverseSqrt2 = 0.70710678118654752440;

//==============================================================================
// Analog prototypes

/** Ratio between the band frequency and the edges of a peak of quality factor q. */
double halfGainSpread (double q) noexcept
{
    const auto half = 0.5 / q;
    return half + std::sqrt (1.0 + half * half);
}

/** Shelves: Q 1 (the default) gives the smooth, overshoot-free shape. */
double shelfQ (double q) noexcept { return q * inverseSqrt2; }

AnalogSection secondOrder (double n0, double n1, double n2, double d0, double d1, double d2, double spread) noexcept
{
    return { n0, n1, n2, d0, d1, d2, spread };
}

void addCutSections (AnalogPrototype& p, bool lowCut, int order, double q) noexcept
{
    if (order % 2 == 1)
        p.sections[(size_t) p.numSections++] = lowCut ? AnalogSection { 0.0, 1.0, 0.0, 1.0, 1.0, 0.0 }
                                                      : AnalogSection { 1.0, 0.0, 0.0, 1.0, 1.0, 0.0 };

    for (int i = 0; i < order / 2; ++i)
    {
        // The Q knob sets the resonance: 1 is the maximally flat (Butterworth) response, and it
        // scales the section that already has the highest Q.
        const auto sectionQ = butterworthQ (order, i) * (i == 0 ? q : 1.0);
        const auto spread = halfGainSpread (sectionQ);
        p.sections[(size_t) p.numSections++] = lowCut ? secondOrder (0.0, 0.0, 1.0, 1.0, 1.0 / sectionQ, 1.0, spread)
                                                      : secondOrder (1.0, 0.0, 0.0, 1.0, 1.0 / sectionQ, 1.0, spread);
    }
}

//==============================================================================
// Digital building blocks

/**
    |p0 + p1 z⁻¹ + p2 z⁻²|² = P0·phi0 + P1·phi1 + P2·phi2 with phi0 = cos²(w/2), phi1 = sin²(w/2),
    phi2 = sin²(w), P0 = (p0 + p1 + p2)², P1 = (p0 - p1 + p2)² and P2 = -4·p0·p2 (Vicanek 2016).
*/
struct PhiBasis
{
    double phi0 = 1.0, phi1 = 0.0, phi2 = 0.0;

    PhiBasis() = default;

    explicit PhiBasis (double omega) noexcept
    {
        const auto s = std::sin (0.5 * omega);
        phi1 = s * s;
        phi0 = 1.0 - phi1;
        phi2 = 4.0 * phi0 * phi1;
    }
};

/** Digital denominator 1 + a1 z⁻¹ + a2 z⁻², kept as A = 1 + a1 + a2, B = 1 - a1 + a2 and C = 1 - a2. */
struct Poles
{
    double A = 0.0, B = 0.0, C = 0.0;

    double a2() const noexcept { return 1.0 - C; }

    double squaredMagnitude (const PhiBasis& at) const noexcept
    {
        return A * A * at.phi0 + B * B * at.phi1 - 4.0 * a2() * at.phi2;
    }

    bool isStable() const noexcept { return A > 0.0 && B > 0.0 && C > 0.0 && C < 2.0; }
};

/**
    Poles of d2 s² + d1 s + d0 (normalised s) mapped by impulse invariance, z = exp(p·w0). A, B
    and C are computed without cancellation so that very low frequencies stay accurate.
*/
Poles impulseInvariantPoles (const AnalogSection& a, double w0) noexcept
{
    Poles poles;
    const auto discriminant = a.d1 * a.d1 - 4.0 * a.d2 * a.d0;
    if (discriminant < 0.0)
    {
        const auto sigma = a.d1 / (2.0 * a.d2) * w0;
        const auto theta = std::sqrt (-discriminant) / (2.0 * a.d2) * w0;
        const auto r = std::exp (-sigma);
        const auto oneMinusR = -std::expm1 (-sigma);
        const auto sinHalf = std::sin (0.5 * theta);
        poles.A = oneMinusR * oneMinusR + 4.0 * r * sinHalf * sinHalf;
        poles.B = (1.0 + r) * (1.0 + r) - 4.0 * r * sinHalf * sinHalf;
        poles.C = -std::expm1 (-2.0 * sigma);
    }
    else
    {
        const auto root = std::sqrt (discriminant);
        const auto p1 = (-a.d1 + root) / (2.0 * a.d2) * w0;
        const auto p2 = (-a.d1 - root) / (2.0 * a.d2) * w0;
        poles.A = std::expm1 (p1) * std::expm1 (p2);
        poles.B = (1.0 + std::exp (p1)) * (1.0 + std::exp (p2));
        poles.C = -std::expm1 (p1 + p2);
    }
    return poles;
}

/**
    The TPT SVF with these poles whose output equals the biquad (b0 + b1 z⁻¹ + b2 z⁻²) / (1 + a1 z⁻¹ + a2 z⁻²),
    given b0, the DC gain and the Nyquist gain (which fix b1 and b2). The SVF low-pass output is 1
    at DC and 0 at Nyquist, the high-pass one the opposite, so their mixes are the DC and Nyquist
    gains; the band-pass mix follows from b0.
*/
SvfSection makeSvf (const Poles& poles, double b0, double dcGain, double nyquistGain) noexcept
{
    SvfSection s;
    s.g = std::sqrt (poles.A / poles.B);
    s.k = 2.0 * poles.C / (poles.B * s.g);
    const auto a0 = 1.0 + s.g * s.k + s.g * s.g; // = 4 / B
    const auto mixBand = (a0 * b0 - nyquistGain - dcGain * s.g * s.g) / s.g;

    // y = high·mH + band·mB + low·mL, with high = v0 - k·v1 - v2.
    s.m0 = nyquistGain;
    s.m1 = mixBand - s.k * nyquistGain;
    s.m2 = dcGain - nyquistGain;
    return s;
}

bool isUsable (const SvfSection& s) noexcept
{
    return std::isfinite (s.g) && std::isfinite (s.k) && s.g > 0.0 && (s.firstOrder || s.k > 0.0)
           && std::isfinite (s.m0) && std::isfinite (s.m1) && std::isfinite (s.m2);
}

/** Solves a 3×3 linear system (augmented matrix) by Gaussian elimination with partial pivoting. */
bool solve3 (std::array<std::array<double, 4>, 3> m, std::array<double, 3>& x) noexcept
{
    for (size_t c = 0; c < 3; ++c)
    {
        auto pivot = c;
        for (auto r = c + 1; r < 3; ++r)
            if (std::abs (m[r][c]) > std::abs (m[pivot][c]))
                pivot = r;
        if (! (std::abs (m[pivot][c]) > 1.0e-300))
            return false;
        std::swap (m[c], m[pivot]);
        for (auto r = c + 1; r < 3; ++r)
        {
            const auto f = m[r][c] / m[c][c];
            for (auto k = c; k < 4; ++k)
                m[r][k] -= f * m[c][k];
        }
    }
    for (int r = 2; r >= 0; --r)
    {
        const auto row = (size_t) r;
        auto v = m[row][3];
        for (auto k = row + 1; k < 3; ++k)
            v -= m[row][k] * x[k];
        x[row] = v / m[row][row];
    }
    return std::isfinite (x[0]) && std::isfinite (x[1]) && std::isfinite (x[2]);
}

/**
    Turns the squared-magnitude forms |N|² = P·phi and |D|² = Q·phi (scaled so that Q1 = 1;
    P0 = T0·Q0 and P1 = TN follow from the DC and Nyquist gains) back into a stable, minimum-phase
    section. With u² the scale, 1 + a1 + a2 = u·sqrt(Q0), 1 - a1 + a2 = u and a2 = -u²·Q2/4.
*/
bool factor (double Q0, double P2, double Q2, double dc, double nyquist, SvfSection& result) noexcept
{
    if (! (Q0 > 0.0) || ! std::isfinite (P2) || ! std::isfinite (Q2))
        return false;

    const auto sq0 = std::sqrt (Q0);
    double u = 0.0;
    if (std::abs (Q2) < 1.0e-12)
        u = 2.0 / (sq0 + 1.0);
    else
    {
        const auto disc = (sq0 + 1.0) * (sq0 + 1.0) + 4.0 * Q2;
        if (disc < 0.0)
            return false;
        u = (-(sq0 + 1.0) + std::sqrt (disc)) / Q2;
    }

    Poles poles;
    poles.A = u * sq0;
    poles.B = u;
    poles.C = 1.0 + 0.25 * u * u * Q2;
    if (! (u > 0.0) || ! poles.isStable())
        return false;

    // Numerator: b0 + b1 + b2 = u·sqrt(T0·Q0), b0 - b1 + b2 = u·sqrt(TN), b0·b2 = -u²·P2/4.
    const auto w = 0.5 * u * (dc * sq0 + nyquist);
    const auto product = -0.25 * u * u * P2;
    const auto disc = w * w - 4.0 * product;
    if (disc < -1.0e-9 * w * w)
        return false;
    const auto b0 = 0.5 * (w + std::sqrt (std::max (0.0, disc)));

    result = makeSvf (poles, b0, dc, nyquist);
    return isUsable (result);
}

//==============================================================================
// Design methods for one section

struct Context
{
    const AnalogSection& a;
    double w0;       // band frequency, radians per sample
    double nyquistS; // Nyquist on the normalised s axis

    double gainAt (double omega) const noexcept { return std::abs (a.response ({ 0.0, omega / w0 })); }
    double dc() const noexcept { return std::abs (a.n0 / a.d0); }
    double nyquist() const noexcept { return std::abs (a.response ({ 0.0, nyquistS })); }
};

bool firstOrder (const Context& c, SvfSection& s) noexcept
{
    // Pole at s = -d0/d1 (normalised), mapped by impulse invariance: z = exp(p·w0). The TPT
    // one-pole with that pole has (1 - g) / (1 + g) = exp(p·w0). Its low-pass output is 1 at DC
    // and 0 at Nyquist and the input passes as it is, so mixing them with the analog gains
    // matches the magnitude at both ends.
    const auto pole = -c.a.d0 / c.a.d1;
    s = {};
    s.firstOrder = true;
    s.g = std::tanh (-0.5 * pole * c.w0);
    s.m0 = c.nyquist();
    s.m2 = c.a.n0 / c.a.d0 - s.m0;
    return isUsable (s);
}

/** Three-point match (Vicanek 2016): impulse-invariant poles, minimum-phase numerator. */
struct Match
{
    Poles poles;
    double b0 = 0.0, b2 = 0.0;
    double dc = 0.0, nyquist = 0.0;
    double margin = 0.0; // (b0 - b2)² / (b0 + b2)²; negative: no real numerator exists
};

Match matchThreePoints (const AnalogSection& a, double w0, double wMatch, double nyquistS) noexcept
{
    const Context c { a, w0, nyquistS };
    Match m;
    m.poles = impulseInvariantPoles (a, w0);
    m.dc = c.dc();
    m.nyquist = c.nyquist();
    const auto target = c.gainAt (wMatch);

    const PhiBasis at (wMatch);
    const auto p0 = m.dc * m.poles.A;      // b0 + b1 + b2
    const auto p1 = m.nyquist * m.poles.B; // b0 - b1 + b2
    const auto P2 = (target * target * m.poles.squaredMagnitude (at) - p0 * p0 * at.phi0 - p1 * p1 * at.phi1) / at.phi2;

    // b0 + b2 = w and b0·b2 = -P2/4; b0 is the larger root, so the zeros are inside the circle.
    // A notch puts them exactly on it (margin 0), which rounding can make slightly negative.
    const auto w = 0.5 * (p0 + p1);
    m.margin = (w * w + P2) / (w * w);
    const auto root = std::sqrt (std::max (0.0, w * w + P2));
    m.b0 = 0.5 * (w + root);
    m.b2 = 0.5 * (w - root);
    return m;
}

constexpr double roundingMargin = -1.0e-9;

bool threePoint (const Context& c, SvfSection& s) noexcept
{
    const auto m = matchThreePoints (c.a, c.w0, c.w0, c.nyquistS);
    if (m.margin < roundingMargin)
        return false;
    s = makeSvf (m.poles, m.b0, m.dc, m.nyquist);
    return isUsable (s);
}

/**
    Sections with gain at both ends (bells, shelves): a deep dip has no three-point solution with
    impulse-invariant poles, but its mirror-image peak does. The analog dip is exactly the
    reciprocal of that peak, so the inverse of its match (zeros become poles) matches the dip at
    the same three points.
*/
bool inverted (const Context& c, SvfSection& s) noexcept
{
    if (isZero (c.a.n0) || isZero (c.a.n2) || ! (c.gainAt (c.w0) > 0.0))
        return false;

    const AnalogSection reciprocal { c.a.d0, c.a.d1, c.a.d2, c.a.n0, c.a.n1, c.a.n2, c.a.fitSpread };
    const auto m = matchThreePoints (reciprocal, c.w0, c.w0, c.nyquistS);
    if (m.margin < 1.0e-9) // the zeros must be strictly inside the circle to become poles
        return false;

    Poles poles;
    poles.A = m.dc * m.poles.A / m.b0;      // (b0 + b1 + b2) / b0
    poles.B = m.nyquist * m.poles.B / m.b0; // (b0 - b1 + b2) / b0
    poles.C = (m.b0 - m.b2) / m.b0;         // 1 - b2 / b0
    if (! poles.isStable())
        return false;
    s = makeSvf (poles, 1.0 / m.b0, 1.0 / m.dc, 1.0 / m.nyquist);
    return isUsable (s);
}

/**
    Five-point fit: poles and zeros together, so that |H|² equals the analog response at DC, at
    Nyquist, at the band frequency and at the edges of its peak. Each point gives one linear
    equation in Q0, P2 and Q2 (see factor()).
*/
bool fivePoint (const Context& c, SvfSection& s) noexcept
{
    const auto dc = c.dc(), nyquist = c.nyquist();
    const auto T0 = dc * dc, TN = nyquist * nyquist;
    const std::array<double, 3> points { c.w0 / c.a.fitSpread, c.w0,
                                         std::min (c.w0 * c.a.fitSpread, c.w0 + 0.6 * (pi - c.w0)) };

    std::array<std::array<double, 4>, 3> m {};
    for (size_t i = 0; i < 3; ++i)
    {
        const PhiBasis at (points[i]);
        const auto h = c.gainAt (points[i]);
        const auto T = h * h;
        // P·phi = T·Q·phi with P0 = T0·Q0, P1 = TN, Q1 = 1:
        // Q0·phi0·(T0 - T) + P2·phi2 - Q2·T·phi2 = (T - TN)·phi1
        m[i] = { at.phi0 * (T0 - T), at.phi2, -T * at.phi2, (T - TN) * at.phi1 };
    }

    std::array<double, 3> x {};
    return solve3 (m, x) && factor (x[0], x[1], x[2], dc, nyquist, s);
}

/**
    Weighted least-squares magnitude fit over ten frequencies from below the band to just under
    Nyquist, plus the band frequency with a much larger weight, exact at DC and Nyquist. The equation error P·phi - T·Q·phi is divided by T·|D|², so
    it becomes the relative error of |H|²; |D|² comes from the previous pass (Steiglitz-McBride
    style, two passes) starting from the impulse-invariant poles.
*/
bool leastSquares (const Context& c, SvfSection& s) noexcept
{
    constexpr int numPoints = 11; // ten spread points plus the band frequency, weighted heavily
    constexpr double bandWeight = 100.0;
    const auto dc = c.dc(), nyquist = c.nyquist();
    const auto T0 = dc * dc, TN = nyquist * nyquist;

    std::array<PhiBasis, numPoints> basis {};
    std::array<double, numPoints> target {};
    const auto low = std::log (c.w0 / (2.0 * c.a.fitSpread));
    const auto high = std::log (0.95 * pi);
    for (size_t i = 0; i < (size_t) numPoints; ++i)
    {
        const auto omega = i + 1 == (size_t) numPoints ? c.w0 : std::exp (low + (high - low) * (double) i / (numPoints - 2));
        const auto h = c.gainAt (omega);
        target[i] = std::max (h * h, 1.0e-8);
        basis[i] = PhiBasis (omega);
    }

    const auto start = impulseInvariantPoles (c.a, c.w0);
    auto Q0 = (start.A * start.A) / (start.B * start.B);
    auto Q2 = -4.0 * start.a2() / (start.B * start.B);
    double P2 = 0.0;

    for (int pass = 0; pass < 2; ++pass)
    {
        std::array<std::array<double, 4>, 3> normal {};
        for (size_t i = 0; i < (size_t) numPoints; ++i)
        {
            const auto& at = basis[i];
            const auto d = Q0 * at.phi0 + at.phi1 + Q2 * at.phi2;
            const auto weight = (i + 1 == (size_t) numPoints ? bandWeight : 1.0) / (target[i] * std::max (d, 1.0e-30));
            const std::array<double, 3> row { at.phi0 * (T0 - target[i]), at.phi2, -target[i] * at.phi2 };
            const auto rhs = (target[i] - TN) * at.phi1;
            for (size_t r = 0; r < 3; ++r)
            {
                for (size_t k = 0; k < 3; ++k)
                    normal[r][k] += weight * weight * row[r] * row[k];
                normal[r][3] += weight * weight * row[r] * rhs;
            }
        }
        std::array<double, 3> x {};
        if (! solve3 (normal, x) || ! (x[0] > 0.0))
            return false;
        Q0 = x[0];
        P2 = x[1];
        Q2 = x[2];
    }
    return factor (Q0, P2, Q2, dc, nyquist, s);
}

/**
    High-pass sections (low cuts) keep a double zero at DC, so the stop band falls at exactly
    12 dB/oct. Fit: |H|² = 16·c²·phi1² / (Q·phi) equal to the analog below the cut-off, at it and
    at Nyquist, which is linear in c², Q0 and Q2 (Q1 = 1).
*/
bool highPassFit (const Context& c, SvfSection& s) noexcept
{
    const std::array<double, 3> points { c.w0 / c.a.fitSpread, c.w0, pi };
    std::array<std::array<double, 4>, 3> m {};
    for (size_t i = 0; i < 3; ++i)
    {
        const PhiBasis at (points[i]);
        const auto h = i == 2 ? c.nyquist() : c.gainAt (points[i]);
        const auto T = h * h;
        m[i] = { 16.0 * at.phi1 * at.phi1, -T * at.phi0, -T * at.phi2, T * at.phi1 };
    }

    std::array<double, 3> x {};
    if (! solve3 (m, x) || ! (x[0] > 0.0) || ! (x[1] > 0.0))
        return false;

    const auto c2 = x[0], Q0 = x[1], Q2 = x[2];
    const auto sq0 = std::sqrt (Q0);
    const auto disc = (sq0 + 1.0) * (sq0 + 1.0) + 4.0 * Q2;
    if (disc < 0.0)
        return false;
    const auto u = std::abs (Q2) < 1.0e-12 ? 2.0 / (sq0 + 1.0) : (-(sq0 + 1.0) + std::sqrt (disc)) / Q2;

    Poles poles;
    poles.A = u * sq0;
    poles.B = u;
    poles.C = 1.0 + 0.25 * u * u * Q2;
    if (! (u > 0.0) || ! poles.isStable())
        return false;

    const auto gain = u * std::sqrt (c2); // numerator gain·(1 - z⁻¹)²
    s = makeSvf (poles, gain, 0.0, 4.0 * gain / poles.B);
    return isUsable (s);
}

/** High-pass with impulse-invariant poles and the gain matched at the cut-off frequency. */
bool highPassMatch (const Context& c, SvfSection& s) noexcept
{
    const auto poles = impulseInvariantPoles (c.a, c.w0);
    const PhiBasis at (c.w0);
    const auto gain = c.gainAt (c.w0) * std::sqrt (poles.squaredMagnitude (at)) / (4.0 * at.phi1);
    s = makeSvf (poles, gain, 0.0, 4.0 * gain / poles.B);
    return isUsable (s);
}

/**
    Notches: the zeros sit exactly on the unit circle at the notch frequency, N = c·(1 - 2·cos(w0)·z⁻¹ + z⁻²),
    whose squared magnitude is (2·cos(w) - 2·cos(w0))². The poles and c are fitted so that the
    magnitude equals the analog one at DC and at both edges of the notch: linear in c², Q0, Q2.
*/
bool notchFit (const Context& c, SvfSection& s) noexcept
{
    const auto cosW0 = std::cos (c.w0);
    const auto zeros = [cosW0] (double w) { const auto d = 2.0 * std::cos (w) - 2.0 * cosW0; return d * d; };

    const std::array<double, 3> points { 0.0, c.w0 / c.a.fitSpread, std::min (c.w0 * c.a.fitSpread, c.w0 + 0.6 * (pi - c.w0)) };
    std::array<std::array<double, 4>, 3> m {};
    for (size_t i = 0; i < 3; ++i)
    {
        const PhiBasis at (points[i]);
        const auto h = c.gainAt (points[i]);
        const auto T = h * h;
        // c²·|N0|² = T·(Q0·phi0 + phi1 + Q2·phi2)
        m[i] = { zeros (points[i]), -T * at.phi0, -T * at.phi2, T * at.phi1 };
    }

    std::array<double, 3> x {};
    if (! solve3 (m, x) || ! (x[0] > 0.0) || ! (x[1] > 0.0))
        return false;

    const auto Q0 = x[1], Q2 = x[2];
    const auto sq0 = std::sqrt (Q0);
    const auto disc = (sq0 + 1.0) * (sq0 + 1.0) + 4.0 * Q2;
    if (disc < 0.0)
        return false;
    const auto u = std::abs (Q2) < 1.0e-12 ? 2.0 / (sq0 + 1.0) : (-(sq0 + 1.0) + std::sqrt (disc)) / Q2;

    Poles poles;
    poles.A = u * sq0;
    poles.B = u;
    poles.C = 1.0 + 0.25 * u * u * Q2;
    if (! (u > 0.0) || ! poles.isStable())
        return false;

    const auto gain = u * std::sqrt (x[0]);
    s = makeSvf (poles, gain, gain * (2.0 - 2.0 * cosW0) / poles.A, gain * (2.0 + 2.0 * cosW0) / poles.B);
    return isUsable (s);
}

/** Last resort: the three-point match with its third point moved down until it has a solution. */
SvfSection lowered (const Context& c) noexcept
{
    auto wMatch = c.w0;
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        wMatch *= 0.7;
        const auto m = matchThreePoints (c.a, c.w0, wMatch, c.nyquistS);
        if (m.margin >= roundingMargin)
            return makeSvf (m.poles, m.b0, m.dc, m.nyquist);
    }
    const auto m = matchThreePoints (c.a, c.w0, c.w0, c.nyquistS);
    return makeSvf (m.poles, m.b0, m.dc, m.nyquist);
}

bool designWith (SectionMethod method, const Context& c, SvfSection& s) noexcept
{
    switch (method)
    {
        case SectionMethod::firstOrder:    return firstOrder (c, s);
        case SectionMethod::fivePoint:     return fivePoint (c, s);
        case SectionMethod::leastSquares:  return leastSquares (c, s);
        case SectionMethod::threePoint:    return threePoint (c, s);
        case SectionMethod::inverted:      return inverted (c, s);
        case SectionMethod::highPassFit:   return highPassFit (c, s);
        case SectionMethod::highPassMatch: return highPassMatch (c, s);
        case SectionMethod::notchFit:      return notchFit (c, s);
        case SectionMethod::automatic:     break;
    }
    return false;
}

/**
    Largest deviation, in dB, between a digital section and its analog prototype at probe
    frequencies from two octaves below the band to just under Nyquist, plus the band frequency
    and the edges of its peak. Levels below -40 dB (stop bands, notch centres) count as -40 dB.
*/
double probeError (const SvfSection& digital, const Context& c) noexcept
{
    constexpr int numSpread = 9;
    std::array<double, numSpread + 3> probes {};
    const auto low = std::log (0.25 * c.w0);
    const auto high = std::log (0.97 * pi);
    for (size_t i = 0; i < (size_t) numSpread; ++i)
        probes[i] = std::exp (low + (high - low) * (double) i / (numSpread - 1));
    probes[numSpread] = c.w0;
    probes[numSpread + 1] = c.w0 / c.a.fitSpread;
    probes[numSpread + 2] = std::min (c.w0 * c.a.fitSpread, 0.97 * pi);

    const auto floorDb = [] (double magnitude) { return std::max (-40.0, 20.0 * std::log10 (std::max (magnitude, 1.0e-12))); };
    double worst = 0.0;
    for (const auto w : probes)
        worst = std::max (worst, std::abs (floorDb (c.gainAt (w)) - floorDb (std::abs (digital.response (w)))));
    return std::isfinite (worst) ? worst : 1.0e9;
}

/** Tries the candidate methods for this kind of section and keeps the closest to the analog. */
SvfSection chooseSection (const Context& c, SectionMethod& chosen) noexcept
{
    SvfSection result;
    if (c.a.isFirstOrder())
    {
        chosen = SectionMethod::firstOrder;
        firstOrder (c, result);
        return result;
    }

    const auto isHighPass = isZero (c.a.n0) && isZero (c.a.n1);
    const auto isNotch = ! isHighPass && c.gainAt (c.w0) < 1.0e-12;
    static constexpr std::array<SectionMethod, 1> notchCandidates { SectionMethod::notchFit };
    static constexpr std::array<SectionMethod, 2> highPassCandidates { SectionMethod::highPassFit, SectionMethod::highPassMatch };
    static constexpr std::array<SectionMethod, 4> generalCandidates { SectionMethod::fivePoint, SectionMethod::leastSquares,
                                                                      SectionMethod::threePoint, SectionMethod::inverted };

    auto bestError = 1.0e300;
    const auto tryAll = [&] (const auto& candidates)
    {
        for (const auto method : candidates)
        {
            SvfSection candidate;
            if (! designWith (method, c, candidate))
                continue;
            if (const auto e = probeError (candidate, c); e < bestError)
            {
                bestError = e;
                result = candidate;
                chosen = method;
            }
        }
    };

    if (isNotch)
        tryAll (notchCandidates);
    else if (isHighPass)
        tryAll (highPassCandidates);
    else
        tryAll (generalCandidates);

    if (bestError < 1.0e300)
        return result;

    chosen = SectionMethod::automatic;
    return lowered (c);
}
} // namespace

//==============================================================================
Complex AnalogSection::response (Complex s) const noexcept
{
    return (n0 + s * (n1 + s * n2)) / (d0 + s * (d1 + s * d2));
}

Complex AnalogPrototype::response (double hz) const noexcept
{
    const Complex s { 0.0, hz / frequency };
    Complex h { gain, 0.0 };
    for (int i = 0; i < numSections; ++i)
        h *= sections[(size_t) i].response (s);
    return h;
}

double butterworthQ (int order, int section) noexcept
{
    return 1.0 / (2.0 * std::sin (pi * (2.0 * section + 1.0) / (2.0 * order)));
}

AnalogPrototype makeAnalogPrototype (const BandSettings& settings) noexcept
{
    AnalogPrototype p;
    p.frequency = settings.frequency;

    const auto q = std::clamp (settings.q, minQ, maxQ);
    const auto gainDb = std::clamp (settings.gainDb, -maxGainDb, maxGainDb);
    const auto A = std::pow (10.0, gainDb / 40.0); // square root of the linear gain

    switch (settings.type)
    {
        case FilterType::bell:
            p.sections[0] = secondOrder (1.0, A / q, 1.0, 1.0, 1.0 / (A * q), 1.0, halfGainSpread (q));
            p.numSections = 1;
            break;

        case FilterType::lowShelf:
        {
            const auto sqrtA = std::sqrt (A);
            const auto qs = shelfQ (q);
            p.sections[0] = secondOrder (A * A, A * sqrtA / qs, A, 1.0, sqrtA / qs, A, halfGainSpread (qs));
            p.numSections = 1;
            break;
        }

        case FilterType::highShelf:
        {
            const auto sqrtA = std::sqrt (A);
            const auto qs = shelfQ (q);
            p.sections[0] = secondOrder (A, A * sqrtA / qs, A * A, A, sqrtA / qs, 1.0, halfGainSpread (qs));
            p.numSections = 1;
            break;
        }

        case FilterType::tiltShelf:
        {
            // A high shelf of twice the gain, lowered by the gain: lows at -G, highs at +G and
            // 0 dB at the band frequency.
            const auto At = std::pow (10.0, gainDb / 20.0);
            const auto sqrtA = std::sqrt (At);
            const auto qs = shelfQ (q);
            p.sections[0] = secondOrder (At, At * sqrtA / qs, At * At, At, sqrtA / qs, 1.0, halfGainSpread (qs));
            p.numSections = 1;
            p.gain = std::pow (10.0, -gainDb / 20.0);
            break;
        }

        case FilterType::notch:
            p.sections[0] = secondOrder (1.0, 0.0, 1.0, 1.0, 1.0 / q, 1.0, halfGainSpread (q));
            p.numSections = 1;
            break;

        case FilterType::bandPass:
            p.sections[0] = secondOrder (0.0, 1.0 / q, 0.0, 1.0, 1.0 / q, 1.0, halfGainSpread (q));
            p.numSections = 1;
            break;

        case FilterType::lowCut:
        case FilterType::highCut:
        {
            const auto slopeIndex = std::clamp (settings.slopeIndex, 0, (int) slopesDbPerOctave.size() - 1);
            const auto order = slopesDbPerOctave[(size_t) slopeIndex] / 6;
            addCutSections (p, settings.type == FilterType::lowCut, order, q);
            break;
        }
    }

    return p;
}

//==============================================================================
Complex SvfSection::response (double omega) const noexcept
{
    const auto z1 = std::polar (1.0, -omega); // z⁻¹

    if (firstOrder)
    {
        const auto low = g * (1.0 + z1) / ((1.0 + g) - (1.0 - g) * z1);
        return m0 + m2 * low;
    }

    const auto denominator = (1.0 + g * k + g * g) + 2.0 * (g * g - 1.0) * z1 + (1.0 - g * k + g * g) * z1 * z1;
    const auto band = g * (1.0 - z1 * z1) / denominator;
    const auto low = g * g * (1.0 + z1) * (1.0 + z1) / denominator;
    return m0 + m1 * band + m2 * low;
}

Complex DigitalBand::response (double hz, double sampleRate) const noexcept
{
    const auto omega = 2.0 * pi * hz / sampleRate;
    Complex h { 1.0, 0.0 };
    for (int i = 0; i < numSections; ++i)
        h *= sections[(size_t) i].response (omega);
    return h;
}

namespace
{
DigitalBand design (const BandSettings& settings, double sampleRate, const DesignChoices* methods,
                    DesignChoices* chosen) noexcept
{
    auto limited = settings;
    limited.frequency = std::clamp (settings.frequency, minFrequency, std::min (maxFrequency, frequencyLimit (sampleRate)));

    const auto prototype = makeAnalogPrototype (limited);
    const auto w0 = 2.0 * pi * limited.frequency / sampleRate;
    const auto nyquistS = 0.5 * sampleRate / limited.frequency;

    DigitalBand band;
    band.frequency = limited.frequency;
    band.numSections = prototype.numSections;
    for (size_t i = 0; i < (size_t) prototype.numSections; ++i)
    {
        const Context c { prototype.sections[i], w0, nyquistS };
        auto& digital = band.sections[i];
        auto method = methods != nullptr ? methods->methods[i] : SectionMethod::automatic;
        if (method == SectionMethod::automatic || ! designWith (method, c, digital))
            digital = chooseSection (c, method);
        if (chosen != nullptr)
            chosen->methods[i] = method;
    }

    // The overall gain (tilt shelf) goes into the first section's mix.
    if (band.numSections > 0 && (prototype.gain < 1.0 || prototype.gain > 1.0))
    {
        auto& first = band.sections[0];
        first.m0 *= prototype.gain;
        first.m1 *= prototype.gain;
        first.m2 *= prototype.gain;
    }

    return band;
}
} // namespace

DigitalBand designBand (const BandSettings& settings, double sampleRate, DesignChoices* chosen) noexcept
{
    return design (settings, sampleRate, nullptr, chosen);
}

DigitalBand designBand (const BandSettings& settings, double sampleRate, const DesignChoices& methods) noexcept
{
    return design (settings, sampleRate, &methods, nullptr);
}

} // namespace pm::dsp::eq
