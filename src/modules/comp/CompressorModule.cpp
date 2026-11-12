// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "modules/comp/CompressorModule.h"

#include "core/ModuleParameters.h"

#include <cmath>

namespace pm::comp
{

namespace
{
bool same (float a, float b) noexcept { return ! (a < b || a > b); }

bool sameFilter (const SidechainFilter& a, const SidechainFilter& b) noexcept
{
    return a.enabled == b.enabled && same (a.frequency, b.frequency) && same (a.gainDb, b.gainDb) && same (a.q, b.q)
           && a.slopeIndex == b.slopeIndex;
}

float toDb (float x) noexcept { return x > 1.0e-6f ? 20.0f * std::log10 (x) : -120.0f; }

constexpr double dbToNepers = 0.11512925464970229; // ln (10) / 20
} // namespace

CompressorModule::CompressorModule() noexcept : ModuleBase (ModuleId::compressor, true, true) {}

void CompressorModule::setParameters (const ModuleParameters& parameters) noexcept
{
    pending = parameters.comp;
}

void CompressorModule::prepareCore (double baseRate, int blockSize, int channels)
{
    baseSampleRate = baseRate;
    numChannels = juce::jlimit (1, dsp::eq::SvfCascade::maxChannels, channels);
    maxLookaheadSamples = (int) std::lround (dsp::comp::maxLookaheadMs * 0.001 * baseRate);
    settleSamples = (int) std::lround (0.010 * baseRate);

    const auto maxFactor = dsp::OversamplingEngine::factorForIndex (dsp::OversamplingEngine::maxFactorIndex);
    dynamics.prepare (baseRate * maxFactor);
    audioDelay.prepare (numChannels, maxLookaheadSamples * maxFactor);
    sidechainDelay.prepare (2, maxLookaheadSamples * maxFactor);
    sidechainBuffer.setSize (2, blockSize * maxFactor, false, true, false);
    gainTrace.assign ((size_t) blockSize, 0.0f);
    gainTraceLength = 0;
    pointLength = juce::jmax (1, (int) std::lround (dsp::comp::CompressorMeter::pointSeconds * baseRate));

    setCoreSampleRate (baseRate);
}

void CompressorModule::setCoreSampleRate (double sampleRate) noexcept
{
    coreRate = sampleRate;
    factor = juce::jmax (1, (int) std::lround (sampleRate / baseSampleRate));
    audioDelay.setDelay (juce::jmin (audioDelay.getMaximumDelay(), maxLookaheadSamples * factor));
    dynamics.setSampleRate (sampleRate);

    for (auto* s : { &makeup, &mix, &listen })
        s->reset (sampleRate, glideSeconds);

    for (auto* f : { &highPass, &lowPass, &bell })
        f->designed = false;

    applySettings (true);
}

void CompressorModule::resetCore() noexcept
{
    audioDelay.reset();
    sidechainDelay.reset();
    for (auto* f : { &highPass, &lowPass, &bell })
    {
        f->cascade.reset();
        f->active = false;
        f->designed = false;
    }

    applySettings (true);
    dynamics.reset();
    gainTraceLength = 0;
}

void CompressorModule::updateFilter (FilterState& filter, const SidechainFilter& wanted, dsp::eq::FilterType type, bool jump) noexcept
{
    const auto isBell = type == dsp::eq::FilterType::bell;

    // At its top position (20 kHz) the low-pass is open: no filter at all, so the default really
    // has no effect (a cut that close to Nyquist would leave a small bump below it).
    const auto open = type == dsp::eq::FilterType::highCut && wanted.frequency >= sidechainLowPassMax;

    if (open || ! wanted.isActive (isBell))
    {
        filter.active = false;
        return;
    }

    if (filter.active && filter.designed && ! jump && sameFilter (wanted, filter.applied))
        return;

    dsp::eq::BandSettings settings;
    settings.type = type;
    settings.frequency = wanted.frequency;
    settings.gainDb = isBell ? wanted.gainDb : 0.0;
    settings.q = isBell ? wanted.q : 1.0;
    settings.slopeIndex = isBell ? dsp::eq::defaultSlopeIndex
                                 : juce::jlimit (0, (int) dsp::eq::slopesDbPerOctave.size() - 1, wanted.slopeIndex);

    const auto glide = filter.active && filter.designed && ! jump;
    if (! glide)
        filter.cascade.reset();

    filter.cascade.setCoefficients (dsp::eq::designBand (settings, coreRate),
                                    glide ? (int) std::lround (0.0015 * coreRate) : 0);
    filter.applied = wanted;
    filter.active = true;
    filter.designed = true;
}

void CompressorModule::applySettings (bool jump) noexcept
{
    dsp::comp::Dynamics::Settings s;
    s.curve = pending.curve();
    s.attackMs = pending.attackMs;
    s.releaseMs = pending.releaseMs;
    s.holdMs = pending.holdMs;
    s.autoRelease = pending.autoRelease;
    dynamics.setSettings (s);

    // The detector sees the signal `lookahead` earlier than the (constantly delayed) audio.
    const auto lookahead = (int) std::lround (juce::jlimit (0.0, dsp::comp::maxLookaheadMs, (double) pending.lookaheadMs)
                                              * 0.001 * baseSampleRate);
    sidechainDelay.setDelay (juce::jlimit (0, sidechainDelay.getMaximumDelay(), (maxLookaheadSamples - lookahead) * factor));

    const auto setTarget = [jump] (LinearSmoother& smoother, float value)
    {
        if (jump)
            smoother.setCurrentAndTargetValue (value);
        else
            smoother.setTargetValue (value);
    };

    setTarget (makeup, (float) pending.totalMakeupDb());
    setTarget (mix, juce::jlimit (0.0f, 1.0f, pending.mix));
    setTarget (listen, pending.listen ? 1.0f : 0.0f);

    updateFilter (highPass, pending.highPass, dsp::eq::FilterType::lowCut, jump);
    updateFilter (lowPass, pending.lowPass, dsp::eq::FilterType::highCut, jump);
    updateFilter (bell, pending.bell, dsp::eq::FilterType::bell, jump);
}

void CompressorModule::processCore (juce::dsp::AudioBlock<float> block, const SidechainBlock* sidechain) noexcept
{
    applySettings (false);

    const auto numSamples = block.getNumSamples();
    const auto channels = juce::jmin (block.getNumChannels(), (size_t) numChannels);
    if (numSamples == 0 || channels == 0 || (int) numSamples > sidechainBuffer.getNumSamples())
        return;

    // 1. Detector signal, always on two channels (a mono source is copied to both).
    auto* sc0 = sidechainBuffer.getWritePointer (0);
    auto* sc1 = sidechainBuffer.getWritePointer (1);

    if (pending.externalSidechain)
    {
        if (sidechain != nullptr && sidechain->getNumChannels() > 0 && sidechain->getNumSamples() >= numSamples)
        {
            const auto last = sidechain->getNumChannels() - 1;
            std::copy_n (sidechain->getChannelPointer (0), numSamples, sc0);
            std::copy_n (sidechain->getChannelPointer (juce::jmin ((size_t) 1, last)), numSamples, sc1);
        }
        else
        {
            std::fill_n (sc0, numSamples, 0.0f);
            std::fill_n (sc1, numSamples, 0.0f);
        }
    }
    else
    {
        std::copy_n (block.getChannelPointer (0), numSamples, sc0);
        std::copy_n (block.getChannelPointer (channels > 1 ? 1 : 0), numSamples, sc1);
    }

    // 2. Sidechain EQ.
    if (highPass.active || lowPass.active || bell.active)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            float frame[2] { sc0[i], sc1[i] };
            if (highPass.active)
                highPass.cascade.processFrame (frame, 2);
            if (lowPass.active)
                lowPass.cascade.processFrame (frame, 2);
            if (bell.active)
                bell.cascade.processFrame (frame, 2);
            sc0[i] = frame[0];
            sc1[i] = frame[1];
        }
    }

    // 3. Delays: the audio by the maximum lookahead, the detector by what is left.
    auto scBlock = juce::dsp::AudioBlock<float> (sidechainBuffer).getSubBlock (0, numSamples);
    sidechainDelay.process (scBlock);
    auto audio = block.getSubsetChannelBlock (0, channels);
    audioDelay.process (audio);

    // 4. Gain, makeup, mix and listen.
    gainTraceLength = juce::jmin ((int) gainTrace.size(), (int) numSamples / factor);
    std::fill_n (gainTrace.begin(), gainTraceLength, 0.0f);

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float frame[2] { sc0[i], sc1[i] };
        const auto g = dynamics.process (frame, 2);

        const auto traceIndex = (int) i / factor;
        if (traceIndex < gainTraceLength)
            gainTrace[(size_t) traceIndex] = juce::jmin (gainTrace[(size_t) traceIndex], (float) g);

        const auto linear = (float) std::exp ((g + (double) makeup.getNextValue()) * dbToNepers);
        const auto m = mix.getNextValue();
        const auto l = listen.getNextValue();

        for (size_t ch = 0; ch < channels; ++ch)
        {
            auto& y = audio.getChannelPointer (ch)[i];
            const auto dry = y;
            y = dry + m * (dry * linear - dry);
            if (l > 0.0f)
            {
                const auto heard = ch == 0 ? sc0[i] : sc1[i];
                y += l * (heard - y);
            }
        }
    }

    lastLevelDb = (float) dynamics.getLevelDb();
}

bool CompressorModule::isCoreNeutral() const noexcept
{
    if (pending.listen || listen.isSmoothing() || listen.getCurrentValue() > 0.0f)
        return false;

    // Mix at 0 %: the output is the delayed input, whatever the gain does.
    if (! (pending.mix > 0.0f) && ! mix.isSmoothing() && ! (mix.getCurrentValue() > 0.0f))
        return true;

    // Otherwise only once the gain has come back and the makeup has settled at 0 dB.
    return pending.isNeutral() && ! makeup.isSmoothing() && ! (std::abs (makeup.getCurrentValue()) > 0.0f)
           && dynamics.getGainDb() > -1.0e-4;
}

void CompressorModule::copyCoreStateFrom (const ModuleBase& other) noexcept
{
    const auto* o = dynamic_cast<const CompressorModule*> (&other);
    if (o == nullptr)
        return;

    pending = o->pending;
    dynamics.copyStateFrom (o->dynamics);
    highPass = o->highPass;
    lowPass = o->lowPass;
    bell = o->bell;
    audioDelay.copyStateFrom (o->audioDelay);
    sidechainDelay.copyStateFrom (o->sidechainDelay);
    makeup = o->makeup;
    mix = o->mix;
    listen = o->listen;
    lastLevelDb = o->lastLevelDb;
    gainTraceLength = 0;
}

void CompressorModule::observe (juce::dsp::AudioBlock<const float> alignedInput, juce::dsp::AudioBlock<const float> output,
                                const SidechainBlock* sidechain, bool wetComputed) noexcept
{
    if (meter == nullptr)
        return;

    const auto numSamples = output.getNumSamples();
    const auto channels = juce::jmin (output.getNumChannels(), alignedInput.getNumChannels());

    // External sidechain selected but nothing arrives: tell the interface (it does not compress).
    if (pending.externalSidechain)
    {
        auto peak = 0.0f;
        if (sidechain != nullptr)
            for (size_t ch = 0; ch < sidechain->getNumChannels(); ++ch)
                for (size_t i = 0; i < juce::jmin (numSamples, sidechain->getNumSamples()); ++i)
                    peak = juce::jmax (peak, std::abs (sidechain->getSample ((int) ch, (int) i)));

        silentSamples = peak < silenceLevel ? juce::jmin (silentSamples + (int) numSamples, 1 << 30) : 0;
        meter->sidechainMissing.store (silentSamples >= (int) (silentSidechainSeconds * baseSampleRate));
    }
    else
    {
        silentSamples = 0;
        meter->sidechainMissing.store (false);
    }

    const auto useTrace = wetComputed && gainTraceLength >= (int) numSamples;
    auto points = std::array<dsp::comp::MeterPoint, 64> {};
    int numPoints = 0;

    for (size_t i = 0; i < numSamples; ++i)
    {
        auto in = 0.0f, out = 0.0f;
        for (size_t ch = 0; ch < channels; ++ch)
        {
            in = juce::jmax (in, std::abs (alignedInput.getSample ((int) ch, (int) i)));
            out = juce::jmax (out, std::abs (output.getSample ((int) ch, (int) i)));
        }

        pointInput = juce::jmax (pointInput, in);
        pointOutput = juce::jmax (pointOutput, out);
        pointGain = juce::jmin (pointGain, useTrace ? gainTrace[i] : 0.0f);

        if (++pointCount >= pointLength)
        {
            points[(size_t) numPoints++] = { toDb (pointInput), toDb (pointOutput), pointGain };
            if (! useTrace)
                lastLevelDb = toDb (pointInput);
            pointCount = 0;
            pointInput = pointOutput = pointGain = 0.0f;

            if (numPoints == (int) points.size())
            {
                meter->getPoints().push (points.data(), numPoints);
                numPoints = 0;
            }
        }
    }

    if (numPoints > 0)
        meter->getPoints().push (points.data(), numPoints);

    meter->gainDb.store (useTrace && numSamples > 0 ? gainTrace[numSamples - 1] : 0.0f);
    meter->levelDb.store (lastLevelDb);
}

} // namespace pm::comp
