// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// Phase 2 tests of the compressor DSP: static curve for every ratio and knee, range, the styles'
// curves, the feedback inversion, auto-gain, and the time behaviour measured with steps (attack,
// release, hold and the styles' program-dependent behaviour).

#include "Measurement.h"
#include "TestHelpers.h"

#include "dsp/comp/CompressorCurve.h"
#include "dsp/comp/CompressorDynamics.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

using namespace pm::dsp::comp;

namespace
{
constexpr std::array<double, 7> ratios { 1.0, 1.5, 2.0, 4.0, 10.0, 100.0, infiniteRatio };
constexpr std::array<double, 6> knees { 0.0, 1.0, 6.0, 12.0, 24.0, 72.0 };
constexpr std::array<double, 4> thresholds { -40.0, -20.0, -6.0, 0.0 };

/** Giannoulis, Massberg and Reiss (2012), eq. 4, written independently, then the range limit. */
double referenceGain (double x, double threshold, double ratio, double knee, double range)
{
    const auto slope = ratio >= infiniteRatio ? -1.0 : 1.0 / ratio - 1.0;
    double gain = 0.0;
    if (2.0 * (x - threshold) < -knee)
        gain = 0.0;
    else if (knee > 0.0 && 2.0 * std::abs (x - threshold) <= knee)
        gain = slope * std::pow (x - threshold + knee / 2.0, 2.0) / (2.0 * knee);
    else
        gain = slope * (x - threshold);
    return std::max (gain, -range);
}

/** Runs a mono amplitude envelope through the detector and returns the gain trace (dB). */
std::vector<double> runDynamics (Dynamics& dynamics, const std::vector<float>& signal)
{
    std::vector<double> gains (signal.size());
    for (size_t i = 0; i < signal.size(); ++i)
    {
        auto x = signal[i];
        gains[i] = dynamics.process (&x, 1);
    }
    return gains;
}

/** Constant level (dB) for `seconds`, as a DC signal: the level detector sees it exactly. */
void appendLevel (std::vector<float>& signal, double levelDb, double seconds, double sampleRate)
{
    const auto value = (float) std::pow (10.0, levelDb / 20.0);
    signal.insert (signal.end(), (size_t) std::lround (seconds * sampleRate), value);
}

Dynamics makeDynamics (double sampleRate, const Dynamics::Settings& settings)
{
    Dynamics d;
    d.prepare (sampleRate);
    d.setSampleRate (sampleRate);
    d.setSettings (settings);
    d.reset();
    return d;
}

Dynamics::Settings basicSettings (Style style = Style::clean)
{
    Dynamics::Settings s;
    s.curve.thresholdDb = -20.0;
    s.curve.ratio = 4.0;
    s.curve.kneeDb = 0.0;
    s.curve.rangeDb = 60.0;
    s.curve.style = style;
    s.attackMs = 10.0;
    s.releaseMs = 100.0;
    return s;
}

/** Samples from `start` until the trace has recovered `fraction` of the way from `from` to 0 dB. */
int recoveryTime (const std::vector<double>& gains, size_t start, double from, double fraction)
{
    return pmtest::measure::timeToReach (gains, (int) start, from, 0.0, fraction);
}
} // namespace

//==============================================================================
TEST_CASE ("Compressor curve: every ratio and knee follows the soft-knee formula", "[comp][curve]")
{
    for (const auto ratio : ratios)
        for (const auto knee : knees)
            for (const auto threshold : thresholds)
                for (const auto range : { 60.0, 12.0 })
                {
                    CurveSettings settings { threshold, ratio, knee, range, Style::clean };
                    const StaticCurve curve (settings);

                    double previousOutput = -1.0e9;
                    for (double x = -100.0; x <= 20.0; x += 0.125)
                    {
                        const auto gain = curve.gainDb (x);
                        INFO ("ratio " << ratio << " knee " << knee << " threshold " << threshold << " range " << range << " level " << x);
                        REQUIRE (gain == Catch::Approx (referenceGain (x, threshold, ratio, knee, range)).margin (1.0e-9));
                        REQUIRE (gain <= 0.0);
                        REQUIRE (gain >= -range - 1.0e-12);

                        // The output never goes down when the input goes up.
                        REQUIRE (curve.outputDb (x) >= previousOutput - 1.0e-9);
                        previousOutput = curve.outputDb (x);
                    }
                }
}

TEST_CASE ("Compressor curve: the slope is continuous through the knee", "[comp][curve]")
{
    for (const auto ratio : ratios)
        for (const auto knee : { 1.0, 6.0, 24.0, 72.0 })
        {
            const StaticCurve curve (CurveSettings { -30.0, ratio, knee, 60.0, Style::clean });
            for (const auto edge : { -30.0 - knee / 2.0, -30.0 + knee / 2.0 })
                REQUIRE (curve.slopeAt (edge - 1.0e-6) == Catch::Approx (curve.slopeAt (edge + 1.0e-6)).margin (1.0e-5));

            // slopeAt() is the derivative of gainDb().
            for (double x = -80.0; x <= 10.0; x += 0.37)
            {
                const auto h = 1.0e-4;
                const auto numeric = (curve.gainDb (x + h) - curve.gainDb (x - h)) / (2.0 * h);
                REQUIRE (numeric == Catch::Approx (curve.slopeAt (x)).margin (1.0e-3));
            }
        }
}

TEST_CASE ("Compressor curve: ratio 1:1 and range 0 do nothing; infinite ratio stops at the threshold", "[comp][curve]")
{
    const StaticCurve unity (CurveSettings { -40.0, 1.0, 12.0, 60.0, Style::clean });
    const StaticCurve noRange (CurveSettings { -40.0, 10.0, 12.0, 0.0, Style::clean });
    const StaticCurve brickwall (CurveSettings { -12.0, infiniteRatio, 0.0, 60.0, Style::clean });

    for (double x = -90.0; x <= 10.0; x += 1.0)
    {
        REQUIRE (unity.gainDb (x) == 0.0);
        REQUIRE (noRange.gainDb (x) == 0.0);
        if (x >= -12.0)
            REQUIRE (brickwall.outputDb (x) == Catch::Approx (-12.0).margin (1.0e-12));
    }
}

TEST_CASE ("Compressor curve: Opto widens the knee and Vocal sets its own progressive ratio", "[comp][curve]")
{
    const StaticCurve opto (CurveSettings { -20.0, 4.0, 6.0, 60.0, Style::opto });
    REQUIRE (opto.kneeDb() == Catch::Approx (6.0 + traitsOf (Style::opto).extraKneeDb));
    REQUIRE (opto.gainDb (-20.0 - opto.kneeDb() / 2.0) == 0.0);
    REQUIRE (opto.gainDb (-40.0) == 0.0);

    // Vocal ignores the ratio and knee knobs.
    const StaticCurve vocalA (CurveSettings { -20.0, 2.0, 0.0, 60.0, Style::vocal });
    const StaticCurve vocalB (CurveSettings { -20.0, 50.0, 40.0, 60.0, Style::vocal });
    REQUIRE (vocalA.isAutomatic());
    for (double x = -60.0; x <= 20.0; x += 0.5)
        REQUIRE (vocalA.gainDb (x) == vocalB.gainDb (x));

    // The further over the threshold, the higher the local ratio (more negative slope), up to 20:1.
    const auto localRatio = [&] (double over) { return 1.0 / (1.0 + vocalA.slopeAt (-20.0 + over)); };
    REQUIRE (localRatio (6.0) < localRatio (12.0));
    REQUIRE (localRatio (12.0) < localRatio (18.0));
    REQUIRE (localRatio (40.0) == Catch::Approx (20.0).epsilon (1.0e-6));
    REQUIRE (localRatio (5.0 + 1.0e-6) == Catch::Approx (1.5).epsilon (1.0e-3));

    // Continuous and the output keeps rising.
    double previous = -1.0e9;
    for (double x = -60.0; x <= 40.0; x += 0.01)
    {
        REQUIRE (vocalA.outputDb (x) >= previous - 1.0e-9);
        REQUIRE (std::abs (vocalA.outputDb (x) - previous) < 0.02 + 1.0e9 * (previous < -1.0e8));
        previous = vocalA.outputDb (x);
    }
}

TEST_CASE ("Compressor curve: the feedback styles keep the same static curve", "[comp][curve]")
{
    for (const auto beta : { 0.25, 0.5, 0.9 })
        for (const auto ratio : ratios)
            for (const auto knee : { 0.0, 6.0, 30.0, 72.0 })
                for (const auto range : { 60.0, 9.0, 0.0 })
                {
                    const StaticCurve curve (CurveSettings { -24.0, ratio, knee, range, Style::clean });
                    for (double x = -100.0; x <= 20.0; x += 0.25)
                    {
                        const auto g = curve.gainDb (x);
                        const auto u = x + beta * g;
                        double gain = 0.0, derivative = 0.0;
                        curve.feedbackGain (u, beta, gain, derivative);
                        INFO ("beta " << beta << " ratio " << ratio << " knee " << knee << " range " << range << " level " << x);
                        REQUIRE (gain == Catch::Approx (g).margin (1.0e-7));

                        double g1 = 0.0, g2 = 0.0, d = 0.0;
                        curve.feedbackGain (u - 1.0e-5, beta, g1, d);
                        curve.feedbackGain (u + 1.0e-5, beta, g2, d);
                        if (std::abs ((g2 - g1) / 2.0e-5 - derivative) > 1.0e-3)
                        {
                            // Only at a corner (hard knee or range): the derivative is one of the two sides.
                            REQUIRE ((knee == 0.0 || range < 60.0));
                        }
                    }
                }
}

TEST_CASE ("Compressor: auto-gain keeps levels halfway to the threshold", "[comp][curve]")
{
    REQUIRE (autoGainDb (CurveSettings { -20.0, 4.0, 0.0, 60.0, Style::clean }) == Catch::Approx (7.5));
    REQUIRE (autoGainDb (CurveSettings { 0.0, 4.0, 0.0, 60.0, Style::clean }) == Catch::Approx (0.0));
    REQUIRE (autoGainDb (CurveSettings { -40.0, infiniteRatio, 0.0, 60.0, Style::clean }) == Catch::Approx (20.0));
    REQUIRE (autoGainDb (CurveSettings { -40.0, infiniteRatio, 0.0, 6.0, Style::clean }) == Catch::Approx (6.0));

    for (const auto style : { Style::clean, Style::opto, Style::vocal })
        for (const auto threshold : { -48.0, -20.0, -3.0 })
        {
            const CurveSettings settings { threshold, 6.0, 12.0, 60.0, style };
            const StaticCurve curve (settings);
            const auto half = threshold / 2.0;
            REQUIRE (curve.outputDb (half) + autoGainDb (settings) == Catch::Approx (half).margin (1.0e-9));
        }
}

//==============================================================================
TEST_CASE ("Compressor detector: the sliding maximum matches a brute-force maximum", "[comp][detector]")
{
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> dist (0.0f, 1.0f);

    for (const auto length : { 1, 2, 7, 100 })
    {
        SlidingMaximum window;
        window.prepare (128);
        window.setLength (length);

        std::vector<float> history;
        for (int i = 0; i < 2000; ++i)
        {
            // Long runs of falling values are the hard case for the queue.
            const auto value = (i / 300) % 2 == 0 ? dist (rng) : 1.0f - (float) (i % 300) / 300.0f;
            history.push_back (value);
            const auto from = history.size() > (size_t) length ? history.size() - (size_t) length : 0;
            const auto expected = *std::max_element (history.begin() + (long) from, history.end());
            REQUIRE (window.push (value) == expected);
        }
    }
}

TEST_CASE ("Compressor detector: steady tones get the static curve in every style", "[comp][detector]")
{
    const double fs = 48000.0;

    for (int s = 0; s < numStyles; ++s)
        for (const auto amplitudeDb : { 0.0, -12.0, -30.0 })
        {
            auto settings = basicSettings ((Style) s);
            settings.curve.kneeDb = 6.0;
            settings.attackMs = 1.0;
            settings.releaseMs = 50.0;
            auto d = makeDynamics (fs, settings);

            const auto amplitude = std::pow (10.0, amplitudeDb / 20.0);
            double gain = 0.0;
            for (int i = 0; i < (int) (3.0 * fs); ++i)
            {
                auto x = (float) (amplitude * std::sin (pmtest::twoPi * 1000.0 * i / fs));
                gain = d.process (&x, 1);
            }

            const StaticCurve curve (settings.curve);
            INFO ("style " << s << " level " << amplitudeDb);
            REQUIRE (gain == Catch::Approx (curve.gainDb (amplitudeDb)).margin (0.15));
        }
}

TEST_CASE ("Compressor detector: Clean attack and release times match the knobs", "[comp][detector][timing]")
{
    for (const auto fs : { 44100.0, 48000.0, 96000.0 })
        for (const auto attackMs : { 0.1, 1.0, 10.0, 100.0 })
            for (const auto releaseMs : { 10.0, 100.0, 1000.0 })
            {
                auto settings = basicSettings();
                settings.attackMs = attackMs;
                settings.releaseMs = releaseMs;
                auto d = makeDynamics (fs, settings);

                std::vector<float> signal;
                appendLevel (signal, -40.0, 0.2, fs);
                const auto stepUp = signal.size();
                appendLevel (signal, 0.0, 1.0 + 8.0 * attackMs * 0.001, fs);
                const auto stepDown = signal.size();
                appendLevel (signal, -40.0, 8.0 * releaseMs * 0.001 + 0.1, fs);

                const auto gains = runDynamics (d, signal);
                const auto fraction = 1.0 - std::exp (-1.0);
                INFO ("fs " << fs << " attack " << attackMs << " release " << releaseMs);

                REQUIRE (gains[stepDown - 1] == Catch::Approx (-15.0).margin (1.0e-3));

                const auto attack = pmtest::measure::timeToReach (gains, (int) stepUp, 0.0, -15.0, fraction);
                const auto expectedAttack = attackMs * 0.001 * fs;
                REQUIRE (std::abs (attack - expectedAttack) <= std::max (1.5, 0.02 * expectedAttack));

                // The peaks are held over the detector window before the release starts.
                const auto window = std::lround (Dynamics::peakWindowMs * 0.001 * fs);
                const auto release = recoveryTime (gains, stepDown, -15.0, fraction) - window;
                const auto expectedRelease = releaseMs * 0.001 * fs;
                REQUIRE (std::abs (release - expectedRelease) <= std::max (2.0, 0.02 * expectedRelease));
            }
}

TEST_CASE ("Compressor detector: hold delays the release", "[comp][detector][timing]")
{
    const double fs = 48000.0;
    for (const auto holdMs : { 0.0, 50.0, 500.0 })
    {
        auto settings = basicSettings();
        settings.holdMs = holdMs;
        auto d = makeDynamics (fs, settings);

        std::vector<float> signal;
        appendLevel (signal, 0.0, 0.5, fs);
        const auto stepDown = signal.size();
        appendLevel (signal, -40.0, 1.5, fs);
        const auto gains = runDynamics (d, signal);

        const auto window = std::lround (Dynamics::peakWindowMs * 0.001 * fs);
        const auto hold = std::lround (holdMs * 0.001 * fs);
        // Nothing moves during window + hold, then the usual release.
        REQUIRE (gains[stepDown + (size_t) (window + hold) - 2] == Catch::Approx (-15.0).margin (1.0e-6));
        const auto release = recoveryTime (gains, stepDown, -15.0, 1.0 - std::exp (-1.0)) - window - hold;
        REQUIRE (std::abs (release - 0.1 * fs) <= 0.02 * 0.1 * fs);
    }
}

TEST_CASE ("Compressor detector: range limits the reduction while it moves", "[comp][detector]")
{
    auto settings = basicSettings();
    settings.curve.ratio = infiniteRatio;
    settings.curve.rangeDb = 6.0;
    settings.attackMs = 0.005;
    auto d = makeDynamics (48000.0, settings);

    std::vector<float> signal;
    appendLevel (signal, 0.0, 0.3, 48000.0);
    appendLevel (signal, -50.0, 0.3, 48000.0);
    appendLevel (signal, 6.0, 0.3, 48000.0);
    for (const auto g : runDynamics (d, signal))
        REQUIRE (g >= -6.0 - 1.0e-9);
}

TEST_CASE ("Compressor detector: settings glide without jumps", "[comp][detector]")
{
    const double fs = 48000.0;
    auto settings = basicSettings();
    settings.attackMs = 0.005; // the gain follows the curve almost at once
    settings.curve.thresholdDb = -10.0;
    auto d = makeDynamics (fs, settings);

    auto x = 1.0f;
    for (int i = 0; i < (int) fs / 2; ++i)
        d.process (&x, 1);
    REQUIRE (d.getGainDb() == Catch::Approx (-7.5).margin (1.0e-3));

    settings.curve.thresholdDb = -30.0;
    settings.curve.ratio = infiniteRatio;
    d.setSettings (settings);

    double previous = d.getGainDb();
    double largestStep = 0.0;
    for (int i = 0; i < (int) fs / 10; ++i)
    {
        const auto g = d.process (&x, 1);
        largestStep = std::max (largestStep, std::abs (g - previous));
        previous = g;
    }
    REQUIRE (previous == Catch::Approx (-30.0).margin (1.0e-3));
    // 22.5 dB over the 20 ms glide (960 samples): about 0.023 dB per sample.
    REQUIRE (largestStep < 0.05);
}

//==============================================================================
// Styles: program-dependent behaviour

TEST_CASE ("Compressor styles: Punch lets the start of a transient through", "[comp][styles]")
{
    const double fs = 48000.0;
    std::vector<float> signal;
    appendLevel (signal, -40.0, 0.2, fs);
    const auto step = signal.size();
    appendLevel (signal, 0.0, 0.3, fs);

    auto clean = makeDynamics (fs, basicSettings (Style::clean));
    auto punch = makeDynamics (fs, basicSettings (Style::punch));
    const auto a = runDynamics (clean, signal);
    const auto b = runDynamics (punch, signal);

    const auto tenth = step + (size_t) (0.001 * fs); // a tenth of the 10 ms attack
    REQUIRE (b[tenth] > 0.3 * a[tenth]); // gain closer to 0 dB: much less reduction
    REQUIRE (std::abs (b[tenth]) < 0.3 * std::abs (a[tenth]));

    // Still about the attack time to 63 %.
    const auto t = pmtest::measure::timeToReach (b, (int) step, 0.0, -15.0, 1.0 - std::exp (-1.0));
    REQUIRE (std::abs (t - 0.010 * fs) < 0.2 * 0.010 * fs);
}

TEST_CASE ("Compressor styles: the slow release stage remembers long compression", "[comp][styles]")
{
    const double fs = 48000.0;

    for (const auto style : { Style::classic, Style::opto, Style::vocal, Style::mastering, Style::bus })
    {
        const auto recovery = [&] (double compressedSeconds)
        {
            auto settings = basicSettings (style);
            settings.attackMs = 1.0;
            auto d = makeDynamics (fs, settings);
            std::vector<float> signal;
            appendLevel (signal, -40.0, 0.1, fs);
            appendLevel (signal, 0.0, compressedSeconds, fs);
            const auto stepDown = signal.size();
            appendLevel (signal, -40.0, 6.0, fs);
            const auto gains = runDynamics (d, signal);
            const auto depth = gains[stepDown - 1];
            return recoveryTime (gains, stepDown, depth, 0.9);
        };

        const auto afterHit = recovery (0.03);
        const auto afterLong = recovery (3.0);
        INFO ("style " << (int) style << " after a hit " << afterHit << ", after long compression " << afterLong);
        REQUIRE (afterHit > 0);
        REQUIRE (afterLong > 2 * afterHit);
    }

    // Clean has no memory: the release does not depend on the history.
    const auto cleanRecovery = [&] (double compressedSeconds)
    {
        auto settings = basicSettings (Style::clean);
        settings.attackMs = 1.0;
        auto d = makeDynamics (fs, settings);
        std::vector<float> signal;
        appendLevel (signal, 0.0, compressedSeconds, fs);
        const auto stepDown = signal.size();
        appendLevel (signal, -40.0, 2.0, fs);
        const auto gains = runDynamics (d, signal);
        return recoveryTime (gains, stepDown, gains[stepDown - 1], 0.9);
    };
    REQUIRE (std::abs (cleanRecovery (0.1) - cleanRecovery (3.0)) <= 2);
}

TEST_CASE ("Compressor styles: auto-release adds the memory to Clean", "[comp][styles]")
{
    const double fs = 48000.0;
    const auto recovery = [&] (double compressedSeconds)
    {
        auto settings = basicSettings (Style::clean);
        settings.attackMs = 1.0;
        settings.autoRelease = true;
        auto d = makeDynamics (fs, settings);
        std::vector<float> signal;
        appendLevel (signal, -40.0, 0.1, fs);
        appendLevel (signal, 0.0, compressedSeconds, fs);
        const auto stepDown = signal.size();
        appendLevel (signal, -40.0, 6.0, fs);
        const auto gains = runDynamics (d, signal);
        return recoveryTime (gains, stepDown, gains[stepDown - 1], 0.9);
    };
    REQUIRE (recovery (3.0) > 2 * recovery (0.03));
}

TEST_CASE ("Compressor styles: Pumping releases slowly at first and fast at the end", "[comp][styles]")
{
    const double fs = 48000.0;
    const auto release = [&] (Style style)
    {
        auto d = makeDynamics (fs, basicSettings (style));
        std::vector<float> signal;
        appendLevel (signal, 0.0, 0.5, fs);
        const auto stepDown = signal.size();
        appendLevel (signal, -40.0, 1.5, fs);
        auto gains = runDynamics (d, signal);
        const auto window = (size_t) std::lround (Dynamics::peakWindowMs * 0.001 * fs);
        return std::vector<double> (gains.begin() + (long) (stepDown + window), gains.end());
    };

    const auto clean = release (Style::clean);
    const auto pumping = release (Style::pumping);
    const auto at = [&] (const std::vector<double>& g, double releases) { return g[(size_t) (releases * 0.1 * fs)]; };

    // After 0.3 release times Pumping has come back less, but it ends first.
    REQUIRE (at (pumping, 0.3) < at (clean, 0.3) - 1.0);
    const auto clean95 = recoveryTime (clean, 0, -15.0, 0.95);
    const auto pumping95 = recoveryTime (pumping, 0, -15.0, 0.95);
    REQUIRE (pumping95 < 0.85 * clean95);
}

TEST_CASE ("Compressor styles: feedback reacts faster the more it compresses", "[comp][styles]")
{
    const double fs = 48000.0;
    const auto attackTime = [&] (Style style)
    {
        auto settings = basicSettings (style);
        settings.curve.ratio = 10.0;
        auto d = makeDynamics (fs, settings);
        std::vector<float> signal;
        appendLevel (signal, -40.0, 0.2, fs);
        const auto step = signal.size();
        appendLevel (signal, 0.0, 0.5, fs);
        const auto gains = runDynamics (d, signal);
        return pmtest::measure::timeToReach (gains, (int) step, 0.0, gains.back(), 1.0 - std::exp (-1.0));
    };

    REQUIRE (attackTime (Style::classic) < 0.7 * attackTime (Style::clean));
}

TEST_CASE ("Compressor detector: silence, huge levels and extreme settings stay finite", "[comp][detector]")
{
    for (int s = 0; s < numStyles; ++s)
        for (const auto ratio : { 1.0, infiniteRatio })
            for (const auto knee : { 0.0, 72.0 })
            {
                Dynamics::Settings settings;
                settings.curve = { -60.0, ratio, knee, 60.0, (Style) s };
                settings.attackMs = minAttackMs;
                settings.releaseMs = minReleaseMs;
                settings.holdMs = maxHoldMs;
                settings.autoRelease = true;
                auto d = makeDynamics (192000.0, settings);

                for (const auto value : { 0.0f, 1.0e-30f, 1.0e6f, -1.0e6f, 0.0f, 1.0f })
                    for (int i = 0; i < 2000; ++i)
                    {
                        float frame[2] { value, -value };
                        const auto g = d.process (frame, 2);
                        REQUIRE (std::isfinite (g));
                        REQUIRE (g <= 1.0e-12);
                        REQUIRE (g >= -60.0 - 1.0e-9);
                    }
            }
}
