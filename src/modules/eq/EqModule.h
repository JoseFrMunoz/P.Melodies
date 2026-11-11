// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#pragma once

#include "core/ModuleBase.h"
#include "dsp/eq/EqCharacter.h"
#include "dsp/eq/SvfCascade.h"
#include "modules/eq/EqParameters.h"

#include <array>

namespace pm::eq
{

/**
    One EQ band at run time (docs/dsp/eq.md).

    - Frequency, gain and Q glide to new values over 20 ms (frequency and Q in a logarithmic
      scale). The filter is redesigned every 16 samples while gliding, reusing the design method
      chosen for the target, so it stays cheap; the TPT sections take new coefficients without
      clicks.
    - Turning the band on or off (create, delete, bypass) fades it in or out over 20 ms.
    - Changing its shape or slope changes the filter structure: the new filter starts from rest
      and runs unheard for 10 ms (its start-up transient), then the output crossfades from the
      old filter to the new one over 20 ms.
    - Prepared to receive modulation later (§13, Volcano-style): the glide works on the target
      values, so a modulation offset can be added to them without touching the filter code.
*/
class Band
{
public:
    static constexpr double glideSeconds = 0.020;
    static constexpr double enableFadeSeconds = 0.020;
    static constexpr double shapeFadeSeconds = 0.020;
    static constexpr double shapeWarmUpSeconds = 0.010; // the new filter runs unheard first
    static constexpr int updateInterval = 16;

    void prepare (double sampleRate) noexcept;

    /** Jumps to the targets, clears the filter state. */
    void reset() noexcept;

    /** New target settings (once per block). */
    void setTarget (const BandParameters& parameters) noexcept;

    /** Processes a block in place (series with the other bands). */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** True while the band affects the output (on, or fading). */
    bool isRunning() const noexcept { return enable > 0.0f || enableTarget > 0.0f; }

    /** True while the band is fading in or out, or crossfading between shapes. */
    bool isFading() const noexcept { return ! juce::exactlyEqual (enable, enableTarget) || shapeFade < 1.0f; }

    /** Settings the filter is at right now (mid-glide). */
    dsp::eq::BandSettings currentSettings() const noexcept;

private:
    void designCurrent (bool chooseAgain, int rampSamples = 0) noexcept;
    void advanceGlide (int numSamples) noexcept;

    double sampleRate = 44100.0;
    dsp::eq::BandSettings target;
    float enableTarget = 0.0f;
    float enable = 0.0f;
    float enableStep = 1.0f;

    // Glide state, in the domain where it is linear.
    double logFrequency = 0.0, gainDb = 0.0, logQ = 0.0;
    double logFrequencyStep = 0.0, gainStep = 0.0, logQStep = 0.0;
    int glideRemaining = 0;
    int glideLength = 1;

    dsp::eq::DesignChoices choices;
    std::array<dsp::eq::SvfCascade, 2> cascades;
    int activeCascade = 0;
    float shapeFade = 1.0f; // weight of the active cascade while the previous one fades out
    float shapeFadeStep = 1.0f;
    int shapeWarmUp = 0;    // samples left before the crossfade starts
    int shapeWarmUpLength = 0;
    int samplesToUpdate = 0;
};

/**
    The EQ module (§4): up to eight bands in series, then the character, with solo of one band.
    Zero latency and no oversampling. When no band changes the sound and the character is Clean,
    the module is neutral and ModuleBase passes the input through bit-exactly. It only becomes
    neutral once its own fades have finished, so the output is already the input when ModuleBase
    stops computing it.
*/
class EqModule final : public ModuleBase
{
public:
    EqModule() noexcept;

    void setParameters (const ModuleParameters& parameters) noexcept override;

    static constexpr double soloFadeSeconds = 0.020;

protected:
    void prepareCore (double rate, int blockSize, int channels) override;
    void setCoreSampleRate (double sampleRate) noexcept override;
    void resetCore() noexcept override;
    void processCore (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept override;
    bool isCoreNeutral() const noexcept override;
    void copyCoreStateFrom (const ModuleBase& other) noexcept override;

private:
    void applyTargets() noexcept;
    void updateSolo (bool chooseAgain, int rampSamples) noexcept;

    Parameters pending;
    std::array<Band, numBands> bands;
    dsp::eq::EqCharacter character;

    // Solo: the soloed band's region, from the EQ input, crossfaded with the normal output.
    dsp::eq::SvfCascade soloFilter;
    dsp::eq::DesignChoices soloChoices;
    int soloBand = -1;
    dsp::eq::FilterType soloType = dsp::eq::FilterType::bell;
    float soloWeight = 0.0f;
    float soloStep = 1.0f;
    juce::AudioBuffer<float> soloInput;

    double sampleRate = 44100.0;
};

} // namespace pm::eq
