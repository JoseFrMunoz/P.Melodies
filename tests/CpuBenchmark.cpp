// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.
//
// CPU measurement (hidden test, run with: Tests "[.cpu]"). It reports how much of one core the
// plugin needs to keep up in real time; it does not fail on a number yet. CI runs it on Windows
// and adds the table to the run summary, so the cost of every phase is visible as it grows. In
// phase 6 it becomes a check against the documented budget.
//
// If PM_CPU_REPORT names a file, the table is appended to it in Markdown.

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace pm;
using namespace pmtest;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr double secondsOfAudio = 5.0;
constexpr int runs = 3;

struct CpuResult
{
    std::string name;
    double percentOfRealTime = 0.0;
    double microsecondsPerBlock = 0.0;
};

/** Times processOneBlock() over secondsOfAudio of audio: best of `runs`, after a warm-up second. */
template <typename ProcessOneBlock>
CpuResult measure (std::string name, ProcessOneBlock&& processOneBlock)
{
    const auto numBlocks = (int) (secondsOfAudio * sampleRate / blockSize);
    for (int i = 0; i < (int) (sampleRate / blockSize); ++i)
        processOneBlock();

    double best = 1.0e30;
    for (int run = 0; run < runs; ++run)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < numBlocks; ++i)
            processOneBlock();
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        best = std::min (best, elapsed.count());
    }

    CpuResult result;
    result.name = std::move (name);
    result.percentOfRealTime = 100.0 * best / (numBlocks * blockSize / sampleRate);
    result.microsecondsPerBlock = 1.0e6 * best / numBlocks;
    return result;
}

/** Fills the first `channels` channels of `io` with fresh noise from `source` (cheap, but not free). */
void refill (juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>& source, int channels)
{
    for (int ch = 0; ch < channels; ++ch)
        io.copyFrom (ch, 0, source, ch % source.getNumChannels(), 0, blockSize);
}

CpuResult measurePlugin (const std::string& name, bool offline)
{
    PMelodiesProcessor processor;
    prepareProcessor (processor, sampleRate, blockSize, 2);
    processor.setNonRealtime (offline);

    const auto noise = makeNoise (2, blockSize, 0.5f);
    juce::AudioBuffer<float> io (4, blockSize); // main stereo + stereo sidechain
    juce::MidiBuffer midi;
    return measure (name, [&]
    {
        refill (io, noise, 4);
        processor.processBlock (io, midi);
    });
}

/** Four active test modules: what the oversampling infrastructure costs before any real DSP. */
CpuResult measureOversampledChain (const std::string& name, int factorIndex)
{
    TestModule::Settings active;
    active.gain = 0.9f;
    active.clipLevel = 0.8f;
    active.lowpassHz = 8000.0;
    active.usesOversampling = true;
    std::array<TestModule::Settings, numModules> settings { active, active, active, active };

    EngineParameters parameters;
    parameters.offlineOversamplingIndex = factorIndex;

    AudioEngine engine (testFactory (settings));
    engine.prepare (sampleRate, blockSize, 2, parameters);

    const auto noise = makeNoise (2, blockSize, 0.5f);
    juce::AudioBuffer<float> io (2, blockSize);
    return measure (name, [&]
    {
        refill (io, noise, 2);
        engine.process (io, nullptr, parameters, true);
    });
}

std::string formatTable (const std::vector<CpuResult>& results)
{
    std::ostringstream out;
    out << "### CPU: " << juce::SystemStats::getCpuModel().toStdString() << " ("
        << juce::SystemStats::getNumCpus() << " threads)\n\n"
        << "One instance, stereo, " << (int) sampleRate << " Hz, " << blockSize
        << "-sample blocks. Best of " << runs << " runs; shared CI machines vary from run to run.\n\n"
        << "| Case | % of one core in real time | µs per block |\n"
        << "|---|---:|---:|\n"
        << std::fixed;
    for (const auto& r : results)
        out << "| " << r.name << " | " << std::setprecision (2) << r.percentOfRealTime << " | "
            << std::setprecision (1) << r.microsecondsPerBlock << " |\n";
    return out.str();
}
} // namespace

TEST_CASE ("CPU use of the plugin and of the oversampling infrastructure", "[.cpu]")
{
    std::vector<CpuResult> results;
    results.push_back (measurePlugin ("Plugin, default settings, playing", false));
    results.push_back (measurePlugin ("Plugin, default settings, exporting", true));
    results.push_back (measureOversampledChain ("4 active modules at 2x (default playing quality)", 1));
    results.push_back (measureOversampledChain ("4 active modules at 8x (default export quality)", 3));
    results.push_back (measureOversampledChain ("4 active modules at 16x (maximum quality)", 4));

    const auto table = formatTable (results);
    std::cout << table << std::endl;

    if (const auto* path = std::getenv ("PM_CPU_REPORT"); path != nullptr && *path != '\0')
    {
        std::ofstream file (path, std::ios::app);
        file << table << "\n";
    }

    for (const auto& r : results)
    {
        INFO (r.name);
        CHECK (r.percentOfRealTime > 0.0);
        CHECK (std::isfinite (r.percentOfRealTime));
    }
}
