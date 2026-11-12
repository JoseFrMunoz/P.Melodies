// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "TestHelpers.h"

#include "core/Parameters.h"
#include "core/ValueFormat.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace pm;
using namespace pmtest;

namespace
{
juce::RangedAudioParameter& get (PMelodiesProcessor& p, const char* id)
{
    auto* parameter = p.getState().getParameter (id);
    REQUIRE (parameter != nullptr);
    return *parameter;
}

float defaultOf (juce::RangedAudioParameter& p) { return p.convertFrom0to1 (p.getDefaultValue()); }
} // namespace

TEST_CASE ("Every parameter is automatable, uniquely identified and readable", "[parameters][phase0]")
{
    PMelodiesProcessor processor;
    std::set<juce::String> ids;

    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        REQUIRE (ranged != nullptr);
        INFO (ranged->getParameterID());

        CHECK (ranged->isAutomatable());
        CHECK (ranged->getName (100).isNotEmpty());
        CHECK (ids.insert (ranged->getParameterID()).second);
        CHECK (ranged->getVersionHint() == param::versionHint);
    }

    // 13 global and chain parameters (phase 0; the chain order is not a parameter, §12)
    // + the EQ: character and 7 per band (phase 1) + the compressor's 25 (phase 2).
    CHECK (ids.size() == 13 + 1 + 7 * eq::numBands + 25);
    CHECK (param::compressorIds().size() == 25);
    for (const auto& id : param::compressorIds())
        CHECK (ids.contains (id));
    CHECK_FALSE (ids.contains ("chainOrder"));
    CHECK (processor.getBypassParameter() == processor.getState().getParameter (param::id::bypass));
}

TEST_CASE ("Global parameter ranges and defaults follow the specification", "[parameters][phase0]")
{
    PMelodiesProcessor processor;

    for (const auto* id : { param::id::inputGain, param::id::outputGain })
    {
        auto& p = get (processor, id);
        CHECK (p.getNormalisableRange().start == -24.0f);
        CHECK (p.getNormalisableRange().end == 24.0f);
        CHECK (defaultOf (p) == Catch::Approx (0.0f));
        CHECK (p.getLabel() == "dB");
    }

    auto& mix = get (processor, param::id::mix);
    CHECK (mix.getNormalisableRange().start == 0.0f);
    CHECK (mix.getNormalisableRange().end == 100.0f);
    CHECK (defaultOf (mix) == Catch::Approx (100.0f));

    auto& ceiling = get (processor, param::id::clipperCeiling);
    CHECK (ceiling.getNormalisableRange().start == -6.0f);
    CHECK (ceiling.getNormalisableRange().end == 0.0f);
    CHECK (defaultOf (ceiling) == Catch::Approx (-0.3f).margin (1.0e-4));

    CHECK (get (processor, param::id::clipperEnabled).getDefaultValue() == 1.0f);
    CHECK (get (processor, param::id::compare).getDefaultValue() == 0.0f);
    CHECK (get (processor, param::id::bypass).getDefaultValue() == 0.0f);

    CHECK (get (processor, param::id::qualityRealtime).getCurrentValueAsText() == "2x");
    CHECK (get (processor, param::id::qualityOffline).getCurrentValueAsText() == "8x");

    for (int i = 0; i < numModules; ++i)
        CHECK (get (processor, param::moduleEnabledId (moduleAt (i))).getDefaultValue() == 1.0f);
}

TEST_CASE ("Default parameters give the neutral engine settings", "[parameters][phase0]")
{
    PMelodiesProcessor processor;
    const param::ParameterReader reader (processor.getState());
    const auto p = reader.read();

    CHECK (p.inputGainDb == 0.0f);
    CHECK (p.outputGainDb == 0.0f);
    CHECK (p.mix == 1.0f);
    CHECK_FALSE (p.compare);
    CHECK (p.clipperEnabled);
    CHECK (p.realtimeOversamplingIndex == 1); // 2x
    CHECK (p.offlineOversamplingIndex == 3);  // 8x
    CHECK (p.chainOrder == chain::defaultOrder());
    CHECK (processor.getChainOrder() == chain::defaultOrder());
    CHECK_FALSE (p.bypassed);
}

TEST_CASE ("Values are shown and parsed the Spanish way", "[parameters][format][phase0]")
{
    CHECK (format::number (-1.5, 1) == "-1,5");
    CHECK (format::number (-0.04, 1) == "0,0"); // no "-0,0"
    CHECK (format::decibels (-3.25, 1, true) == format::unicodeMinus() + "3,3 dB");
    CHECK (format::percent (99.6) == "100 %");
    CHECK (format::parseNumber ("-3,5 dB") == Catch::Approx (-3.5));
    CHECK (format::parseNumber (format::unicodeMinus() + "12,25") == Catch::Approx (-12.25));
    CHECK (format::parseNumber ("7.5") == Catch::Approx (7.5));

    PMelodiesProcessor processor;
    auto& input = get (processor, param::id::inputGain);
    CHECK (input.getText (input.convertTo0to1 (-1.5f), 100) == "-1,5");
    CHECK (input.convertFrom0to1 (input.getValueForText ("-7,3")) == Catch::Approx (-7.3f).margin (1.0e-4));
    CHECK (get (processor, param::id::clipperEnabled).getCurrentValueAsText() == "Activado");
}

TEST_CASE ("Quality and clipper settings are kept out of presets", "[parameters][phase0]")
{
    CHECK (param::isExcludedFromPresets (param::id::qualityRealtime));
    CHECK (param::isExcludedFromPresets (param::id::qualityOffline));
    CHECK (param::isExcludedFromPresets (param::id::clipperEnabled));
    CHECK (param::isExcludedFromPresets (param::id::clipperCeiling));
    CHECK_FALSE (param::isExcludedFromPresets (param::id::mix));
}

TEST_CASE ("Bus layouts: mono and stereo main bus, optional mono or stereo sidechain", "[parameters][buses][phase0]")
{
    PMelodiesProcessor processor;
    using Set = juce::AudioChannelSet;

    const auto layout = [] (Set in, Set out, Set sidechain)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.inputBuses.add (sidechain);
        l.outputBuses.add (out);
        return l;
    };

    CHECK (processor.checkBusesLayoutSupported (layout (Set::stereo(), Set::stereo(), Set::stereo())));
    CHECK (processor.checkBusesLayoutSupported (layout (Set::mono(), Set::mono(), Set::stereo())));
    CHECK (processor.checkBusesLayoutSupported (layout (Set::stereo(), Set::stereo(), Set::disabled())));
    CHECK (processor.checkBusesLayoutSupported (layout (Set::mono(), Set::mono(), Set::mono())));
    CHECK_FALSE (processor.checkBusesLayoutSupported (layout (Set::mono(), Set::stereo(), Set::stereo())));
    CHECK_FALSE (processor.checkBusesLayoutSupported (layout (Set::create5point1(), Set::create5point1(), Set::stereo())));
    CHECK (processor.getBusCount (true) == 2);
    CHECK (processor.getBus (true, 1)->getName() == "Sidechain");
}
