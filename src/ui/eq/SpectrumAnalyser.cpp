// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "ui/eq/SpectrumAnalyser.h"

#include <cmath>

namespace pm::ui
{

void SpectrumAnalyser::prepare (double newSampleRate)
{
    if (juce::exactlyEqual (newSampleRate, sampleRate) && fft != nullptr)
        return;

    sampleRate = newSampleRate;
    fftOrder = sampleRate > 100000.0 ? 14 : (sampleRate > 50000.0 ? 13 : 12);
    fftSize = 1 << fftOrder;
    hop = fftSize / 4;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.assign ((size_t) fftSize, 0.0f);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    history.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    levels.assign ((size_t) fftSize / 2 + 1, floorDb);
    scratch.assign ((size_t) fftSize, 0.0f);
    writePosition = 0;
    samplesSinceLast = 0;
    filled = 0;
}

bool SpectrumAnalyser::update (dsp::AnalyzerTap& tap)
{
    prepare (tap.getSampleRate());
    bool changed = false;
    for (;;)
    {
        const auto n = tap.pop (scratch.data(), (int) scratch.size());
        if (n <= 0)
            break;
        changed = push (scratch.data(), n) || changed;
    }
    return changed;
}

bool SpectrumAnalyser::push (const float* samples, int numSamples)
{
    if (fft == nullptr)
        prepare (48000.0);

    bool computed = false;
    for (int i = 0; i < numSamples; ++i)
    {
        history[(size_t) writePosition] = samples[i];
        writePosition = (writePosition + 1) % fftSize;
        filled = juce::jmin (fftSize, filled + 1);
        if (++samplesSinceLast >= hop && filled == fftSize)
        {
            samplesSinceLast = 0;
            compute();
            computed = true;
        }
    }
    return computed;
}

void SpectrumAnalyser::clear()
{
    std::fill (levels.begin(), levels.end(), floorDb);
    std::fill (history.begin(), history.end(), 0.0f);
    filled = 0;
}

void SpectrumAnalyser::compute()
{
    // Oldest sample first, windowed.
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = history[(size_t) ((writePosition + i) % fftSize)] * window[(size_t) i];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (fftData.data(), true);

    // A full-scale sine gives |X| = N/4 with a Hann window: scale to dBFS of a sine.
    const auto scale = 4.0f / (float) fftSize;
    constexpr float attack = 0.6f, release = 0.12f; // per new spectrum (about 47 per second at 48 kHz)
    for (size_t k = 1; k < levels.size(); ++k)
    {
        const auto hz = (double) k * sampleRate / fftSize;
        const auto tilt = (float) (tiltDbPerOctave * std::log2 (hz / 1000.0));
        const auto db = 20.0f * std::log10 (std::max (fftData[k] * scale, 1.0e-9f)) + tilt;
        auto& level = levels[k];
        level += (db > level ? attack : release) * (db - level);
        level = std::max (level, floorDb);
    }
    levels[0] = levels[1];
}

float SpectrumAnalyser::levelAt (double hz) const noexcept
{
    if (levels.empty() || sampleRate <= 0.0)
        return floorDb;
    const auto position = juce::jlimit (0.0, (double) levels.size() - 1.001, hz * fftSize / sampleRate);
    const auto k = (size_t) position;
    const auto t = (float) (position - (double) k);
    return levels[k] + t * (levels[k + 1] - levels[k]);
}

} // namespace pm::ui
