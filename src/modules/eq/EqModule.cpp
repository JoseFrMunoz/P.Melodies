// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "modules/eq/EqModule.h"

#include <cmath>

namespace pm::eq
{

namespace
{
using dsp::eq::BandSettings;

double clampFrequency (double hz, double sampleRate) noexcept
{
    return juce::jlimit (dsp::eq::minFrequency, juce::jmin (dsp::eq::maxFrequency, dsp::eq::frequencyLimit (sampleRate)), hz);
}

bool sameStructure (const BandSettings& a, const BandSettings& b) noexcept
{
    return a.type == b.type && (! dsp::eq::usesSlope (a.type) || a.slopeIndex == b.slopeIndex);
}

float ramp (float value, float target, float step) noexcept
{
    return target > value ? juce::jmin (target, value + step) : juce::jmax (target, value - step);
}
} // namespace

//==============================================================================
void Band::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    enableStep = (float) (1.0 / (enableFadeSeconds * sampleRate));
    shapeFadeStep = (float) (1.0 / (shapeFadeSeconds * sampleRate));
    shapeWarmUpLength = juce::roundToInt (shapeWarmUpSeconds * sampleRate);
    glideLength = juce::jmax (1, juce::roundToInt (glideSeconds * sampleRate));
    reset();
}

void Band::reset() noexcept
{
    logFrequency = std::log (clampFrequency (target.frequency, sampleRate));
    gainDb = target.gainDb;
    logQ = std::log (target.q);
    glideRemaining = 0;
    enable = enableTarget;
    shapeFade = 1.0f;
    for (auto& c : cascades)
        c.reset();
    designCurrent (true);
    samplesToUpdate = 0;
}

BandSettings Band::currentSettings() const noexcept
{
    auto s = target;
    s.frequency = std::exp (logFrequency);
    s.gainDb = gainDb;
    s.q = std::exp (logQ);
    return s;
}

void Band::designCurrent (bool chooseAgain, int rampSamples) noexcept
{
    const auto settings = currentSettings();
    const auto band = chooseAgain ? dsp::eq::designBand (settings, sampleRate, &choices)
                                  : dsp::eq::designBand (settings, sampleRate, choices);
    cascades[(size_t) activeCascade].setCoefficients (band, rampSamples);
}

void Band::setTarget (const BandParameters& parameters) noexcept
{
    auto next = parameters.settings;
    next.frequency = clampFrequency (next.frequency, sampleRate);
    next.gainDb = juce::jlimit (-dsp::eq::maxGainDb, dsp::eq::maxGainDb, next.gainDb);
    next.q = juce::jlimit (dsp::eq::minQ, dsp::eq::maxQ, next.q);
    const auto nextEnable = parameters.active && ! parameters.bypassed ? 1.0f : 0.0f;

    const auto structureChanged = ! sameStructure (next, target);
    const auto valuesChanged = ! juce::exactlyEqual (next.frequency, target.frequency)
                               || ! juce::exactlyEqual (next.gainDb, target.gainDb) || ! juce::exactlyEqual (next.q, target.q);
    const auto wasSilent = ! isRunning();

    target = next;
    enableTarget = nextEnable;

    if (wasSilent)
    {
        // Nothing to glide from: start at the targets, from rest, and fade in.
        if (enableTarget > 0.0f)
        {
            const auto fadingIn = enableTarget;
            enableTarget = 0.0f; // so reset() keeps enable at 0
            reset();
            enableTarget = fadingIn;
        }
        return;
    }

    if (structureChanged)
    {
        // New filter structure: the old filter keeps running with its last coefficients while
        // the new one, started at the targets, fades in.
        activeCascade = 1 - activeCascade;
        cascades[(size_t) activeCascade].reset();
        logFrequency = std::log (target.frequency);
        gainDb = target.gainDb;
        logQ = std::log (target.q);
        glideRemaining = 0;
        designCurrent (true);
        shapeFade = 0.0f;
        shapeWarmUp = shapeWarmUpLength;
        return;
    }

    if (valuesChanged)
    {
        // Choose the design methods for the target, then glide there reusing them.
        logFrequencyStep = (std::log (target.frequency) - logFrequency) / glideLength;
        gainStep = (target.gainDb - gainDb) / glideLength;
        logQStep = (std::log (target.q) - logQ) / glideLength;
        glideRemaining = glideLength;
        dsp::eq::designBand (target, sampleRate, &choices);
    }
}

void Band::advanceGlide (int numSamples) noexcept
{
    const auto steps = juce::jmin (numSamples, glideRemaining);
    glideRemaining -= steps;
    if (glideRemaining == 0)
    {
        logFrequency = std::log (target.frequency);
        gainDb = target.gainDb;
        logQ = std::log (target.q);
    }
    else
    {
        logFrequency += logFrequencyStep * steps;
        gainDb += gainStep * steps;
        logQ += logQStep * steps;
    }
}

void Band::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! isRunning())
        return;

    const auto channelCount = juce::jmin (numChannels, dsp::eq::SvfCascade::maxChannels);
    for (int offset = 0; offset < numSamples;)
    {
        if (samplesToUpdate <= 0)
        {
            // While gliding, design the filter a step ahead and ramp its coefficients there.
            if (glideRemaining > 0)
            {
                advanceGlide (updateInterval);
                designCurrent (false, updateInterval);
            }
            samplesToUpdate = updateInterval;
        }

        const auto length = juce::jmin (numSamples - offset, samplesToUpdate);
        auto& active = cascades[(size_t) activeCascade];
        auto& previous = cascades[(size_t) (1 - activeCascade)];
        const auto crossfading = shapeFade < 1.0f;

        for (int i = offset; i < offset + length; ++i)
        {
            std::array<float, dsp::eq::SvfCascade::maxChannels> input {}, output {}, old {};
            for (int ch = 0; ch < channelCount; ++ch)
                input[(size_t) ch] = output[(size_t) ch] = old[(size_t) ch] = channels[ch][i];

            active.processFrame (output.data(), channelCount);
            if (crossfading)
            {
                previous.processFrame (old.data(), channelCount);
                if (shapeWarmUp > 0)
                    --shapeWarmUp;
                else
                    shapeFade = ramp (shapeFade, 1.0f, shapeFadeStep);
            }
            enable = ramp (enable, enableTarget, enableStep);

            for (int ch = 0; ch < channelCount; ++ch)
            {
                const auto c = (size_t) ch;
                const auto y = crossfading ? old[c] + shapeFade * (output[c] - old[c]) : output[c];
                channels[ch][i] = input[c] + enable * (y - input[c]);
            }
        }

        if (crossfading && shapeFade >= 1.0f)
            previous.reset();

        samplesToUpdate -= length;
        offset += length;

        if (! isRunning())
        {
            for (auto& c : cascades)
                c.reset();
            return;
        }
    }
}

//==============================================================================
EqModule::EqModule() noexcept : ModuleBase (ModuleId::eq, false)
{
}

void EqModule::setParameters (const ModuleParameters& parameters) noexcept
{
    pending = parameters.eq;
}

bool EqModule::isCoreNeutral() const noexcept
{
    if (! pending.isNeutral() || ! character.isClean() || soloWeight > 0.0f)
        return false;
    for (const auto& band : bands)
        if (band.isFading())
            return false;
    return true;
}

void EqModule::prepareCore (double rate, int blockSize, int channels)
{
    soloInput.setSize (channels, blockSize, false, false, true);
    setCoreSampleRate (rate);
}

void EqModule::setCoreSampleRate (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    soloStep = (float) (1.0 / (soloFadeSeconds * sampleRate));
    character.prepare (sampleRate);
    for (auto& band : bands)
        band.prepare (sampleRate);
    resetCore();
}

void EqModule::resetCore() noexcept
{
    for (size_t b = 0; b < bands.size(); ++b)
    {
        bands[b].setTarget (pending.bands[b]);
        bands[b].reset();
    }
    character.setMode ((dsp::eq::EqCharacter::Mode) pending.character);
    character.reset();
    soloFilter.reset();
    soloBand = -1;
    soloWeight = 0.0f;
    updateSolo (true, 0);
    soloWeight = soloBand >= 0 ? 1.0f : 0.0f;
}

void EqModule::copyCoreStateFrom (const ModuleBase& other) noexcept
{
    if (auto* o = dynamic_cast<const EqModule*> (&other))
    {
        bands = o->bands;
        character = o->character;
        soloFilter = o->soloFilter;
        soloChoices = o->soloChoices;
        soloBand = o->soloBand;
        soloType = o->soloType;
        soloWeight = o->soloWeight;
    }
}

void EqModule::applyTargets() noexcept
{
    for (size_t b = 0; b < bands.size(); ++b)
        bands[b].setTarget (pending.bands[b]);
    character.setMode ((dsp::eq::EqCharacter::Mode) pending.character);
}

void EqModule::updateSolo (bool chooseAgain, int rampSamples) noexcept
{
    const auto requested = pending.soloBand >= 0 && pending.soloBand < numBands && pending.bands[(size_t) pending.soloBand].active
                               ? pending.soloBand
                               : -1;
    if (requested < 0)
        return; // keep the last solo filter while it fades out

    const auto settings = soloSettings (bands[(size_t) requested].currentSettings());
    const auto changed = requested != soloBand || settings.type != soloType;
    if (changed && soloBand >= 0 && requested != soloBand)
        soloFilter.reset();
    soloBand = requested;
    soloType = settings.type;

    const auto band = chooseAgain || changed ? dsp::eq::designBand (settings, sampleRate, &soloChoices)
                                             : dsp::eq::designBand (settings, sampleRate, soloChoices);
    soloFilter.setCoefficients (band, changed ? 0 : rampSamples);
}

void EqModule::processCore (juce::dsp::AudioBlock<float> block, const SidechainBlock*) noexcept
{
    applyTargets();

    const auto numChannels = juce::jmin ((int) block.getNumChannels(), soloInput.getNumChannels());
    const auto numSamples = juce::jmin ((int) block.getNumSamples(), soloInput.getNumSamples());

    std::array<float*, dsp::eq::SvfCascade::maxChannels> channels {};
    for (int ch = 0; ch < juce::jmin (numChannels, (int) channels.size()); ++ch)
        channels[(size_t) ch] = block.getChannelPointer ((size_t) ch);
    const auto channelCount = juce::jmin (numChannels, (int) channels.size());

    // Solo needs the EQ input.
    const auto soloTarget = pending.soloBand >= 0 && pending.soloBand < numBands && pending.bands[(size_t) pending.soloBand].active ? 1.0f : 0.0f;
    const auto soloRunning = soloTarget > 0.0f || soloWeight > 0.0f;
    if (soloRunning)
    {
        for (int ch = 0; ch < channelCount; ++ch)
            juce::FloatVectorOperations::copy (soloInput.getWritePointer (ch), channels[(size_t) ch], numSamples);
        updateSolo (false, numSamples);
    }

    for (auto& band : bands)
        band.process (channels.data(), channelCount, numSamples);

    if (! character.isClean())
        character.process (channels.data(), channelCount, numSamples);

    if (soloRunning)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            std::array<float, dsp::eq::SvfCascade::maxChannels> frame {};
            for (int ch = 0; ch < channelCount; ++ch)
                frame[(size_t) ch] = soloInput.getSample (ch, i);
            soloFilter.processFrame (frame.data(), channelCount);
            soloWeight = ramp (soloWeight, soloTarget, soloStep);
            for (int ch = 0; ch < channelCount; ++ch)
            {
                auto& out = channels[(size_t) ch][i];
                out += soloWeight * (frame[(size_t) ch] - out);
            }
        }
        if (soloWeight <= 0.0f && soloTarget <= 0.0f)
        {
            soloFilter.reset();
            soloBand = -1;
        }
    }
}

} // namespace pm::eq
