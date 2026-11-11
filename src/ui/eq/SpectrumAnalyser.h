// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "dsp/AnalyzerTap.h"

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

namespace pm::ui
{

/**
    Spectrum of the EQ's input or output for the display (§4), computed on the interface thread
    from the samples the audio thread leaves in an AnalyzerTap.

    - FFT of 4096 points at 44.1-48 kHz (8192 at 88.2-96 kHz, 16384 above, so the resolution in
      Hz stays the same), Hann window, a new spectrum every quarter of a window.
    - Smooth averaging: levels rise quickly (attack) and fall slowly (release).
    - Display tilt of 4.5 dB/oct around 1 kHz, so a typical mix looks roughly flat.
*/
class SpectrumAnalyser
{
public:
    static constexpr double tiltDbPerOctave = 4.5;
    static constexpr float floorDb = -120.0f;

    void prepare (double sampleRate);

    /** Takes what the audio thread produced. Returns true when the spectrum changed. */
    bool update (dsp::AnalyzerTap& tap);

    /** Feeds samples directly (tests). Returns true when a new spectrum was computed. */
    bool push (const float* samples, int numSamples);

    /** Lets the levels fall away (analyser switched off). */
    void clear();

    /** Smoothed, tilted level in dB at a frequency (linear interpolation between bins). */
    float levelAt (double hz) const noexcept;

    double getSampleRate() const noexcept { return sampleRate; }
    int getFftSize() const noexcept { return fftSize; }

private:
    void compute();

    double sampleRate = 0.0;
    int fftOrder = 12;
    int fftSize = 4096;
    int hop = 1024;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, history, fftData, levels, scratch;
    int writePosition = 0;
    int samplesSinceLast = 0;
    int filled = 0;
};

} // namespace pm::ui
