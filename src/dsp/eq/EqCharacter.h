// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

namespace pm::dsp::eq
{

/**
    Subtle and Warm character of the EQ (§4, decisions in §12): a gentle, level-dependent
    saturation applied after the filters, without oversampling so the EQ keeps zero latency.

    For each mode the signal goes through a pre-emphasis, a low-order polynomial with soft limits
    and the inverse de-emphasis, but only the distortion residual is added to the input:
        y = x + DC-block(DE(sat(d) - d)),  d = PE(x)
    so the clean path is exact and Clean costs nothing. Low-order polynomials generate only the
    2nd and 3rd harmonics at normal levels, which keeps the aliasing far below audibility.

    - Subtle (transformer): odd harmonics (cubic), with the lows pushed +6 dB into the curve so
      they colour more. About 0.1 % THD for a 1 kHz tone at -12 dBFS.
    - Warm (tube): mainly the 2nd harmonic (asymmetric curve) plus a little 3rd. About 0.5 %.
    - Both: the top end is lowered 6 dB before the curve (and restored after), so high
      frequencies distort less and alias less. The colour grows with the level.

    Switching modes crossfades the residuals over 20 ms.
*/
class EqCharacter
{
public:
    enum class Mode
    {
        clean,
        subtle,
        warm
    };

    static constexpr double fadeSeconds = 0.020;
    static constexpr int maxChannels = 2;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        fadeStep = (float) (1.0 / (fadeSeconds * sampleRate));
        voices[0].prepare (sampleRate, true);
        voices[1].prepare (sampleRate, false);
        reset();
    }

    void reset() noexcept
    {
        for (auto& v : voices)
        {
            v.reset();
            v.weight = v.target;
        }
    }

    void setMode (Mode newMode) noexcept
    {
        voices[0].target = newMode == Mode::subtle ? 1.0f : 0.0f;
        voices[1].target = newMode == Mode::warm ? 1.0f : 0.0f;
    }

    /** Jumps to the current mode without a crossfade (after a reset or a state copy). */
    void skipFade() noexcept
    {
        for (auto& v : voices)
            v.weight = v.target;
    }

    /** True when the output is exactly the input (Clean, and no fade under way). */
    bool isClean() const noexcept
    {
        for (const auto& v : voices)
            if (v.weight > 0.0f || v.target > 0.0f)
                return false;
        return true;
    }

    /** Processes one block in place. Channels are processed with their own filter states. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (auto& v : voices)
        {
            if (v.weight <= 0.0f && v.target <= 0.0f)
                continue;

            const auto startWeight = v.weight;
            for (int ch = 0; ch < juce::jmin (numChannels, maxChannels); ++ch)
            {
                auto* x = channels[ch];
                auto weight = startWeight;
                auto& s = v.state[(size_t) ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    weight = v.target > weight ? juce::jmin (v.target, weight + fadeStep) : juce::jmax (v.target, weight - fadeStep);
                    x[i] += weight * v.residual (x[i], s);
                }
                v.weight = weight;
            }

            if (v.weight <= 0.0f && v.target <= 0.0f)
                v.reset(); // silent from now on: start from rest next time
        }
    }

private:
    /** First-order filter in direct form (fixed coefficients, so no modulation concerns). */
    struct FirstOrder
    {
        double b0 = 1.0, b1 = 0.0, a1 = 0.0;

        /** Shelf with gain `dcGain` below `hz` and 1 above (or the opposite for highShelf), bilinear. */
        static FirstOrder shelf (double hz, double gain, bool highShelf, double sampleRate) noexcept
        {
            // Analog: low shelf (s + g·w)/(s + w), high shelf (g·s + w)/(s + w), with w = 2·pi·hz.
            const auto k = std::tan (juce::MathConstants<double>::pi * hz / sampleRate); // prewarped w·T/2
            const auto num0 = highShelf ? gain : 1.0;  // coefficient of s
            const auto num1 = highShelf ? 1.0 : gain;  // coefficient of w
            const auto norm = 1.0 / (1.0 + k);
            FirstOrder f;
            f.b0 = (num0 + num1 * k) * norm;
            f.b1 = (num1 * k - num0) * norm;
            f.a1 = (k - 1.0) * norm;
            return f;
        }

        FirstOrder inverse() const noexcept
        {
            FirstOrder f;
            f.b0 = 1.0 / b0;
            f.b1 = a1 / b0;
            f.a1 = b1 / b0;
            return f;
        }

        float process (float x, float& z) const noexcept
        {
            // Transposed direct form II: y = b0·x + z; z = b1·x - a1·y.
            const auto y = (float) b0 * x + z;
            z = (float) b1 * x - (float) a1 * y;
            return y;
        }
    };

    struct ChannelState
    {
        float lowPre = 0.0f, highPre = 0.0f, lowDe = 0.0f, highDe = 0.0f, dc = 0.0f;
    };

    struct Voice
    {
        FirstOrder lowEmphasis, highEmphasis, lowDeEmphasis, highDeEmphasis, dcBlock;
        double c2 = 0.0, c3 = 0.0;   // polynomial u + c2·u² - c3·u³
        double lower = -1.0, upper = 1.0; // where its slope reaches 0: beyond, it holds its value
        double lowerValue = 0.0, upperValue = 0.0;
        std::array<ChannelState, maxChannels> state {};
        float weight = 0.0f, target = 0.0f;

        void prepare (double sampleRate, bool subtle) noexcept
        {
            lowEmphasis = FirstOrder::shelf (150.0, subtle ? 2.0 : 1.0, false, sampleRate); // +6 dB lows (Subtle)
            highEmphasis = FirstOrder::shelf (6000.0, 0.5, true, sampleRate);              // -6 dB highs
            lowDeEmphasis = lowEmphasis.inverse();
            highDeEmphasis = highEmphasis.inverse();
            dcBlock = FirstOrder::shelf (5.0, 0.0, false, sampleRate); // high-pass: gain 0 below 5 Hz

            c2 = subtle ? 0.0 : 0.040;
            c3 = subtle ? 0.064 : 0.032;

            // Slope 1 + 2·c2·u - 3·c3·u² = 0 at the curve's limits.
            const auto root = std::sqrt (4.0 * c2 * c2 + 12.0 * c3);
            upper = (2.0 * c2 + root) / (6.0 * c3);
            lower = (2.0 * c2 - root) / (6.0 * c3);
            upperValue = curve (upper);
            lowerValue = curve (lower);
        }

        double curve (double u) const noexcept { return u + c2 * u * u - c3 * u * u * u; }

        void reset() noexcept { state.fill ({}); }

        float residual (float x, ChannelState& s) const noexcept
        {
            const auto d = highEmphasis.process (lowEmphasis.process (x, s.lowPre), s.highPre);
            const auto u = (double) d;
            const auto shaped = u >= upper ? upperValue : (u <= lower ? lowerValue : curve (u));
            const auto r = (float) (shaped - u);
            return dcBlock.process (lowDeEmphasis.process (highDeEmphasis.process (r, s.highDe), s.lowDe), s.dc);
        }
    };

    double sampleRate = 44100.0;
    float fadeStep = 1.0f;
    std::array<Voice, 2> voices {}; // 0: Subtle, 1: Warm
};

} // namespace pm::dsp::eq
