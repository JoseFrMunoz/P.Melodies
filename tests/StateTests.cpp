// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "TestHelpers.h"

#include "core/Parameters.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace pm;
using namespace pmtest;

namespace
{
void randomise (PMelodiesProcessor& processor, int seed)
{
    juce::Random random (seed);
    for (auto* parameter : processor.getParameters())
        parameter->setValueNotifyingHost (random.nextFloat());
}

void checkSameParameters (PMelodiesProcessor& a, PMelodiesProcessor& b)
{
    const auto& pa = a.getParameters();
    const auto& pb = b.getParameters();
    REQUIRE (pa.size() == pb.size());

    for (int i = 0; i < pa.size(); ++i)
    {
        auto* ra = dynamic_cast<juce::RangedAudioParameter*> (pa[i]);
        auto* rb = dynamic_cast<juce::RangedAudioParameter*> (pb[i]);
        REQUIRE (ra != nullptr);
        REQUIRE (rb != nullptr);
        INFO (ra->getParameterID());
        CHECK (ra->getParameterID() == rb->getParameterID());
        CHECK (ra->getCurrentValueAsText() == rb->getCurrentValueAsText());
        // Saved values are snapped to each parameter's step (bools to on/off, floats to 0.1).
        CHECK (ra->convertFrom0to1 (ra->getValue()) == Catch::Approx (rb->convertFrom0to1 (rb->getValue())).margin (1.0e-5));
    }
}
} // namespace

TEST_CASE ("Saved state restores every parameter, the chain order and the selected module", "[state][phase0]")
{
    for (int seed = 1; seed <= 20; ++seed)
    {
        PMelodiesProcessor original;
        randomise (original, seed);
        original.setSelectedModule (moduleAt (seed % numModules));

        juce::MemoryBlock saved;
        original.getStateInformation (saved);

        PMelodiesProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());

        INFO ("seed " << seed);
        checkSameParameters (original, restored);
        CHECK (restored.getSelectedModule() == moduleAt (seed % numModules));

        // Saving the restored instance gives the same document again.
        juce::MemoryBlock savedAgain;
        restored.getStateInformation (savedAgain);
        CHECK (savedAgain == saved);
    }
}

TEST_CASE ("Restored state is what the audio engine uses", "[state][phase0]")
{
    PMelodiesProcessor original;
    setParameter (original, param::id::outputGain, -6.0f);
    setParameter (original, param::id::chainOrder, 13.0f);
    setParameter (original, param::id::eqEnabled, 0.0f);

    juce::MemoryBlock saved;
    original.getStateInformation (saved);

    PMelodiesProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    prepareProcessor (restored, 48000.0, 512, 2);

    CHECK (restored.getEngine().getChain().getActiveOrder() == 13);
    CHECK (restored.getEngine().getChain().getModule (ModuleId::eq).isBypassed());

    const auto latency = restored.getLatencySamples();
    const auto input = makeNoise (2, 10000, 0.25f);
    const auto out = runProcessor (restored, input, blockSizes (10000, 512));
    const auto expected = juce::Decibels::decibelsToGain (-6.0f);
    CHECK (std::abs (out.getSample (0, 5000) - expected * input.getSample (0, 5000 - latency)) < 1.0e-6f);
}

TEST_CASE ("State saved by an older version gets defaults for parameters it did not know", "[state][phase0]")
{
    PMelodiesProcessor original;
    setParameter (original, param::id::inputGain, 5.0f);
    setParameter (original, param::id::clipperCeiling, -2.0f);

    juce::MemoryBlock saved;
    original.getStateInformation (saved);

    // Simulate an older document: drop one parameter and the version properties.
    auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
    REQUIRE (xml != nullptr);
    auto tree = juce::ValueTree::fromXml (*xml);
    tree.removeChild (tree.getChildWithProperty ("id", param::id::clipperCeiling), nullptr);
    tree.removeProperty (PMelodiesProcessor::stateVersionProperty, nullptr);
    juce::MemoryBlock oldDocument;
    juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), oldDocument);

    PMelodiesProcessor restored;
    setParameter (restored, param::id::clipperCeiling, -5.0f); // must not survive the load
    restored.setStateInformation (oldDocument.getData(), (int) oldDocument.getSize());

    auto* ceiling = restored.getState().getParameter (param::id::clipperCeiling);
    CHECK (ceiling->convertFrom0to1 (ceiling->getValue()) == Catch::Approx (-0.3f).margin (1.0e-4));
    auto* input = restored.getState().getParameter (param::id::inputGain);
    CHECK (input->convertFrom0to1 (input->getValue()) == Catch::Approx (5.0f).margin (1.0e-4));
    CHECK ((int) restored.getState().state.getProperty (PMelodiesProcessor::stateVersionProperty)
           == PMelodiesProcessor::stateVersion);
}

TEST_CASE ("Invalid state data is ignored", "[state][phase0]")
{
    PMelodiesProcessor processor;
    setParameter (processor, param::id::outputGain, 3.0f);

    const char garbage[] = "this is not a P.Melodies state";
    processor.setStateInformation (garbage, (int) sizeof (garbage));

    juce::XmlElement foreign ("SomeOtherPlugin");
    juce::MemoryBlock foreignData;
    juce::AudioProcessor::copyXmlToBinary (foreign, foreignData);
    processor.setStateInformation (foreignData.getData(), (int) foreignData.getSize());

    auto* out = processor.getState().getParameter (param::id::outputGain);
    CHECK (out->convertFrom0to1 (out->getValue()) == Catch::Approx (3.0f).margin (1.0e-4));
}
