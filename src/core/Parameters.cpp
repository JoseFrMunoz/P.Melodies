// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 JoseFrMunoz
// This file is part of P.Melodies. See LICENSE for the full license text.

#include "core/Parameters.h"

#include "core/ValueFormat.h"

namespace pm::param
{

namespace
{
using Apvts = juce::AudioProcessorValueTreeState;

juce::ParameterID pid (const char* parameterId) { return { parameterId, versionHint }; }

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* parameterId, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float defaultValue,
                                                      const juce::String& unit, int decimals)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([decimals] (float v, int) { return format::number (v, decimals); })
                          .withValueFromStringFunction ([] (const juce::String& t) { return (float) format::parseNumber (t); });

    return std::make_unique<juce::AudioParameterFloat> (pid (parameterId), name, range, defaultValue, attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* parameterId, const juce::String& name, bool defaultValue,
                                                    const juce::String& onText, const juce::String& offText)
{
    auto attributes = juce::AudioParameterBoolAttributes()
                          .withStringFromValueFunction ([onText, offText] (bool v, int) { return v ? onText : offText; })
                          .withValueFromStringFunction ([onText] (const juce::String& t)
                                                        {
                                                            const auto s = t.trim();
                                                            return s.equalsIgnoreCase (onText) || s == "1"
                                                                   || s.equalsIgnoreCase ("true");
                                                        });

    return std::make_unique<juce::AudioParameterBool> (pid (parameterId), name, defaultValue, attributes);
}

juce::String text (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }
} // namespace

const char* moduleEnabledId (ModuleId moduleId) noexcept
{
    switch (moduleId)
    {
        case ModuleId::eq:         return id::eqEnabled;
        case ModuleId::compressor: return id::compressorEnabled;
        case ModuleId::saturation: return id::saturationEnabled;
        case ModuleId::distortion: return id::distortionEnabled;
    }
    return id::eqEnabled;
}

bool isExcludedFromPresets (const juce::String& parameterId)
{
    return parameterId == id::qualityRealtime || parameterId == id::qualityOffline
           || parameterId == id::clipperEnabled || parameterId == id::clipperCeiling;
}

juce::StringArray realtimeQualityChoices() { return { "Off", "2x", "4x" }; }
juce::StringArray offlineQualityChoices() { return { "2x", "4x", "8x", "16x" }; }

Apvts::ParameterLayout createLayout()
{
    const juce::NormalisableRange<float> gainRange { -24.0f, 24.0f, 0.1f };

    auto global = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", " | ");
    global->addChild (makeFloat (id::inputGain, "Ganancia de entrada", gainRange, 0.0f, "dB", 1),
                      makeFloat (id::outputGain, "Ganancia de salida", gainRange, 0.0f, "dB", 1),
                      makeFloat (id::mix, "Mix global", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", 0),
                      makeBool (id::compare, "Comparar a igual volumen", false, "Activado", "Desactivado"),
                      makeBool (id::clipperEnabled, "Clipper de seguridad", true, "Activado", "Desactivado"),
                      makeFloat (id::clipperCeiling, "Techo del clipper",
                                 { AudioEngine::minCeilingDb, AudioEngine::maxCeilingDb, 0.1f }, -0.3f, "dB", 1),
                      std::make_unique<juce::AudioParameterChoice> (pid (id::qualityRealtime), "Calidad al reproducir",
                                                                    realtimeQualityChoices(), defaultRealtimeQualityChoice),
                      std::make_unique<juce::AudioParameterChoice> (pid (id::qualityOffline), "Calidad al exportar",
                                                                    offlineQualityChoices(), defaultOfflineQualityChoice));

    // The chain order is not a parameter: it is saved in the state (§12, 2026-11-11).
    auto modules = std::make_unique<juce::AudioProcessorParameterGroup> ("chain", "Cadena", " | ");
    modules->addChild (makeBool (id::eqEnabled, "EQ encendido", true, "Encendido", "Apagado"),
                       makeBool (id::compressorEnabled, "Compresor encendido", true, "Encendido", "Apagado"),
                       makeBool (id::saturationEnabled, text ("Saturaci\xc3\xb3n encendida"), true, "Encendida", "Apagada"),
                       makeBool (id::distortionEnabled, text ("Distorsi\xc3\xb3n encendida"), true, "Encendida", "Apagada"));

    Apvts::ParameterLayout layout;
    layout.add (std::move (global), std::move (modules));
    layout.add (makeBool (id::bypass, "Bypass", false, "Activado", "Desactivado"));
    return layout;
}

ParameterReader::ParameterReader (Apvts& state)
{
    const auto get = [&state] (const char* parameterId)
    {
        auto* value = state.getRawParameterValue (parameterId);
        jassert (value != nullptr);
        return value;
    };

    inputGain = get (id::inputGain);
    outputGain = get (id::outputGain);
    mix = get (id::mix);
    compare = get (id::compare);
    clipperEnabled = get (id::clipperEnabled);
    clipperCeiling = get (id::clipperCeiling);
    qualityRealtime = get (id::qualityRealtime);
    qualityOffline = get (id::qualityOffline);
    for (int i = 0; i < numModules; ++i)
        moduleEnabled[(size_t) i] = get (moduleEnabledId (moduleAt (i)));
    bypass = get (id::bypass);
}

EngineParameters ParameterReader::read() const noexcept
{
    const auto choice = [] (const std::atomic<float>* v, int numChoices)
    { return juce::jlimit (0, numChoices - 1, juce::roundToInt (v->load())); };
    const auto flag = [] (const std::atomic<float>* v) { return v->load() >= 0.5f; };

    EngineParameters p;
    p.inputGainDb = inputGain->load();
    p.outputGainDb = outputGain->load();
    p.mix = mix->load() / 100.0f;
    p.compare = flag (compare);
    p.clipperEnabled = flag (clipperEnabled);
    p.clipperCeilingDb = clipperCeiling->load();
    p.realtimeOversamplingIndex = realtimeQualityFactorIndex[(size_t) choice (qualityRealtime, (int) realtimeQualityFactorIndex.size())];
    p.offlineOversamplingIndex = offlineQualityFactorIndex[(size_t) choice (qualityOffline, (int) offlineQualityFactorIndex.size())];
    for (size_t i = 0; i < (size_t) numModules; ++i)
        p.moduleEnabled[i] = flag (moduleEnabled[i]);
    p.bypassed = flag (bypass);
    return p;
}

} // namespace pm::param
