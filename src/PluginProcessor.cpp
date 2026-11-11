// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Version.h"

namespace pm
{

PMelodiesProcessor::BusesProperties PMelodiesProcessor::makeBusesProperties()
{
    return BusesProperties()
        .withInput ("Entrada", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Salida", juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true);
}

PMelodiesProcessor::PMelodiesProcessor()
    : juce::AudioProcessor (makeBusesProperties()),
      state (*this, nullptr, stateType, param::createLayout()),
      reader (state)
{
    applyEqAnalyzer();
    state.state.setProperty (chainOrderProperty, chain::toText (chain::defaultOrder()), nullptr);
    state.state.addListener (this);
}

static_assert (std::atomic<std::uint64_t>::is_always_lock_free);

PMelodiesProcessor::~PMelodiesProcessor()
{
    state.state.removeListener (this);
}

EngineParameters PMelodiesProcessor::readEngineParameters() const noexcept
{
    auto p = reader.read();
    p.chainOrder = chain::unpack (packedChainOrder.load());
    p.modules.eq.soloBand = eqSoloBand.load();
    return p;
}

void PMelodiesProcessor::setChainOrder (const chain::Order& order)
{
    if (chain::isValid (order))
        state.state.setProperty (chainOrderProperty, chain::toText (order), nullptr); // syncs through the listener
}

void PMelodiesProcessor::syncChainOrder()
{
    packedChainOrder.store (chain::pack (chain::fromText (state.state.getProperty (chainOrderProperty).toString())));
}

void PMelodiesProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == state.state && property == chainOrderProperty)
        syncChainOrder();
}

void PMelodiesProcessor::valueTreeRedirected (juce::ValueTree&)
{
    syncChainOrder(); // replaceState(): a project or preset was loaded
}

const juce::String PMelodiesProcessor::getName() const
{
    return info::productName;
}

bool PMelodiesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.inputBuses[1];

        if (! sidechain.isDisabled() && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void PMelodiesProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = juce::jmax (1, getMainBusNumInputChannels());
    engine.prepare (sampleRate, samplesPerBlock, numChannels, readEngineParameters());
    setLatencySamples (engine.getLatencySamples());
}

void PMelodiesProcessor::reset()
{
    engine.reset();
}

void PMelodiesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);

    juce::AudioBuffer<float> sidechainBuffer;
    const juce::AudioBuffer<float>* sidechain = nullptr;

    // Only read the sidechain when the host really passed its channels.
    if (getBusCount (true) > 1 && buffer.getNumChannels() >= getTotalNumInputChannels())
    {
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            sidechainBuffer = getBusBuffer (buffer, true, 1);
            sidechain = &sidechainBuffer;
        }
    }

    engine.process (mainBuffer, sidechain, readEngineParameters(), isNonRealtime());

    // Any output channel beyond the main bus carries nothing.
    for (auto ch = getMainBusNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorParameter* PMelodiesProcessor::getBypassParameter() const
{
    return state.getParameter (param::id::bypass);
}

double PMelodiesProcessor::getLatencyMilliseconds() const noexcept
{
    const auto sampleRate = engine.getSampleRate();
    return sampleRate > 0.0 ? 1000.0 * engine.getLatencySamples() / sampleRate : 0.0;
}

ModuleId PMelodiesProcessor::getSelectedModule() const
{
    const int index = state.state.getProperty (selectedModuleProperty, 0);
    return moduleAt (juce::jlimit (0, numModules - 1, index));
}

void PMelodiesProcessor::setSelectedModule (ModuleId id)
{
    state.state.setProperty (selectedModuleProperty, toIndex (id), nullptr);
}

PMelodiesProcessor::EqAnalyzer PMelodiesProcessor::getEqAnalyzer() const
{
    const int value = state.state.getProperty (eqAnalyzerProperty, (int) EqAnalyzer::post);
    return (EqAnalyzer) juce::jlimit (0, 2, value);
}

void PMelodiesProcessor::setEqAnalyzer (EqAnalyzer mode)
{
    state.state.setProperty (eqAnalyzerProperty, (int) mode, nullptr);
    applyEqAnalyzer();
}

void PMelodiesProcessor::applyEqAnalyzer()
{
    using Point = dsp::AnalyzerTap::Point;
    const auto mode = getEqAnalyzer();
    engine.getEqAnalyzer().setPoint (mode == EqAnalyzer::pre ? Point::pre : mode == EqAnalyzer::post ? Point::post : Point::off);
}

int PMelodiesProcessor::getEqRangeDb() const
{
    // Saved as the scale in dB. Earlier phase 1 builds saved a flag: 0 (±12) or 1 (±30).
    const int value = state.state.getProperty (eqRangeProperty, 12);
    if (value == 1)
        return 30;
    for (const auto r : eqRangesDb)
        if (value == r)
            return r;
    return 12;
}

void PMelodiesProcessor::setEqRangeDb (int rangeDb)
{
    auto snapped = eqRangesDb.back();
    for (const auto r : eqRangesDb)
    {
        if (r >= rangeDb)
        {
            snapped = r;
            break;
        }
    }
    state.state.setProperty (eqRangeProperty, snapped, nullptr);
}

int PMelodiesProcessor::getEqRangeIndex() const
{
    const auto range = getEqRangeDb();
    for (size_t i = 0; i < eqRangesDb.size(); ++i)
        if (eqRangesDb[i] == range)
            return (int) i;
    return 1;
}

void PMelodiesProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty (stateVersionProperty, stateVersion, nullptr);
    tree.setProperty (pluginVersionProperty, info::versionString, nullptr);

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void PMelodiesProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (stateType.toString()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    migrateState (tree, *this);
    state.replaceState (tree);
    applyEqAnalyzer();
}

void PMelodiesProcessor::migrateState (juce::ValueTree& tree, const juce::AudioProcessor& processor)
{
    static const juce::Identifier paramType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };

    const int version = tree.getProperty (stateVersionProperty, 1);

    // 1 → 2: the chain order was the "chainOrder" parameter, a permutation index from 0 to 23.
    if (version < 2)
    {
        auto legacy = tree.getChildWithProperty (idProperty, "chainOrder");
        if (legacy.isValid())
        {
            const auto order = chain::fromLegacyIndex (juce::roundToInt ((double) legacy.getProperty (valueProperty, 0)));
            tree.setProperty (chainOrderProperty, chain::toText (order), nullptr);
            tree.removeChild (legacy, nullptr);
        }
    }

    // Normalise the order: unknown modules are dropped and new ones added (chain::fromText).
    tree.setProperty (chainOrderProperty, chain::toText (chain::fromText (tree.getProperty (chainOrderProperty).toString())), nullptr);

    // Parameters added after the state was saved get their default value instead of keeping
    // whatever the instance had before.
    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        if (ranged == nullptr)
            continue;

        if (! tree.getChildWithProperty (idProperty, ranged->getParameterID()).isValid())
        {
            juce::ValueTree child (paramType);
            child.setProperty (idProperty, ranged->getParameterID(), nullptr);
            child.setProperty (valueProperty, ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
            tree.appendChild (child, nullptr);
        }
    }

    tree.setProperty (stateVersionProperty, stateVersion, nullptr);
}

juce::AudioProcessorEditor* PMelodiesProcessor::createEditor()
{
    return new PMelodiesEditor (*this);
}

} // namespace pm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pm::PMelodiesProcessor();
}
