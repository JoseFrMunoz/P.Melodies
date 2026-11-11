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

juce::ParameterID pid (const juce::String& parameterId) { return { parameterId, versionHint }; }

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& parameterId, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float defaultValue,
                                                      const juce::String& unit, int decimals)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([decimals] (float v, int) { return format::number (v, decimals); })
                          .withValueFromStringFunction ([] (const juce::String& t) { return (float) format::parseNumber (t); });

    return std::make_unique<juce::AudioParameterFloat> (pid (parameterId), name, range, defaultValue, attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& parameterId, const juce::String& name, bool defaultValue,
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

juce::String eqBandId (int band, EqField field)
{
    static constexpr std::array<const char*, 7> suffixes { "On", "Type", "Freq", "Gain", "Q", "Slope", "Bypass" };
    return "eqB" + juce::String (band + 1) + suffixes[(size_t) field];
}

juce::StringArray eqTypeNames()
{
    return { "Campana", "Shelf grave", "Shelf agudo", "Corte de graves", "Corte de agudos", "Notch", "Pasa-banda", "Tilt shelf" };
}

juce::StringArray eqSlopeNames()
{
    juce::StringArray names;
    for (const auto slope : dsp::eq::slopesDbPerOctave)
        names.add (juce::String (slope) + " dB/oct");
    return names;
}

juce::StringArray eqCharacterNames() { return { "Clean", "Subtle", "Warm" }; }

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

    juce::StringArray orders;
    for (int i = 0; i < chain::numOrders; ++i)
        orders.add (chain::describe (i));

    auto modules = std::make_unique<juce::AudioProcessorParameterGroup> ("chain", "Cadena", " | ");
    modules->addChild (std::make_unique<juce::AudioParameterChoice> (pid (id::chainOrder), "Orden de la cadena", orders,
                                                                     chain::defaultOrderIndex),
                       makeBool (id::eqEnabled, "EQ encendido", true, "Encendido", "Apagado"),
                       makeBool (id::compressorEnabled, "Compresor encendido", true, "Encendido", "Apagado"),
                       makeBool (id::saturationEnabled, text ("Saturaci\xc3\xb3n encendida"), true, "Encendida", "Apagada"),
                       makeBool (id::distortionEnabled, text ("Distorsi\xc3\xb3n encendida"), true, "Encendida", "Apagada"));

    // EQ: eight band slots and the character (§4, §12). Frequency and Q use logarithmic ranges,
    // rounded to four significant digits so that saving and loading a project gives back exactly
    // the same values (the pow/log round trip alone changes the last decimals).
    const auto significant = [] (double x)
    {
        const auto magnitude = std::pow (10.0, std::floor (std::log10 (x)) - 3.0);
        return (float) (std::round (x / magnitude) * magnitude);
    };
    const auto logRange = [significant] (float start, float end)
    {
        return juce::NormalisableRange<float> (
            start, end,
            [significant] (float s, float e, float v) { return juce::jlimit (s, e, significant (s * std::pow ((double) e / s, (double) v))); },
            [] (float s, float e, float x) { return (float) (std::log ((double) juce::jlimit (s, e, x) / s) / std::log ((double) e / s)); },
            [significant] (float s, float e, float x) { return juce::jlimit (s, e, significant (x)); });
    };

    auto eqGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("eq", "EQ", " | ");
    eqGroup->addChild (std::make_unique<juce::AudioParameterChoice> (pid (id::eqCharacter), text ("EQ \xc2\xb7 Car\xc3\xa1" "cter"),
                                                                     eqCharacterNames(), 0));
    for (int b = 0; b < eq::numBands; ++b)
    {
        const auto prefix = text ("EQ \xc2\xb7 Banda ") + juce::String (b + 1) + text (" \xc2\xb7 ");
        const auto bandId = [b] (EqField field) { return eqBandId (b, field); };

        auto frequency = juce::AudioParameterFloatAttributes()
                             .withLabel ("Hz")
                             .withStringFromValueFunction ([] (float v, int) { return format::frequency (v); })
                             .withValueFromStringFunction ([] (const juce::String& t) { return (float) format::parseFrequency (t); });
        auto gain = juce::AudioParameterFloatAttributes()
                        .withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return format::number (v, 1); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return (float) format::parseNumber (t); });
        auto quality = juce::AudioParameterFloatAttributes()
                           .withStringFromValueFunction ([] (float v, int) { return format::number (v, 2); })
                           .withValueFromStringFunction ([] (const juce::String& t) { return (float) format::parseNumber (t); });

        eqGroup->addChild (
            makeBool (bandId (EqField::active), prefix + "Activa", false, "Activa", "Inactiva"),
            std::make_unique<juce::AudioParameterChoice> (pid (bandId (EqField::type)), prefix + "Tipo",
                                                          eqTypeNames(), 0),
            std::make_unique<juce::AudioParameterFloat> (pid (bandId (EqField::frequency)),
                                                         prefix + "Frecuencia", logRange ((float) dsp::eq::minFrequency, (float) dsp::eq::maxFrequency),
                                                         eqDefaultFrequencies[(size_t) b], frequency),
            std::make_unique<juce::AudioParameterFloat> (pid (bandId (EqField::gain)), prefix + "Ganancia",
                                                         juce::NormalisableRange<float> { -30.0f, 30.0f, 0.1f }, 0.0f, gain),
            std::make_unique<juce::AudioParameterFloat> (pid (bandId (EqField::q)), prefix + "Q",
                                                         logRange ((float) dsp::eq::minQ, (float) dsp::eq::maxQ), 1.0f, quality),
            std::make_unique<juce::AudioParameterChoice> (pid (bandId (EqField::slope)),
                                                          prefix + "Pendiente", eqSlopeNames(), dsp::eq::defaultSlopeIndex),
            makeBool (bandId (EqField::bypass), prefix + "Bypass", false, "Activado", "Desactivado"));
    }

    Apvts::ParameterLayout layout;
    layout.add (std::move (global), std::move (modules), std::move (eqGroup));
    layout.add (makeBool (id::bypass, "Bypass", false, "Activado", "Desactivado"));
    return layout;
}

ParameterReader::ParameterReader (Apvts& state)
{
    const auto get = [&state] (const juce::String& parameterId)
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
    chainOrder = get (id::chainOrder);
    for (int i = 0; i < numModules; ++i)
        moduleEnabled[(size_t) i] = get (moduleEnabledId (moduleAt (i)));
    bypass = get (id::bypass);

    for (int b = 0; b < eq::numBands; ++b)
    {
        auto& band = eqBands[(size_t) b];
        const auto field = [&] (EqField f) { return get (eqBandId (b, f)); };
        band.active = field (EqField::active);
        band.type = field (EqField::type);
        band.frequency = field (EqField::frequency);
        band.gain = field (EqField::gain);
        band.q = field (EqField::q);
        band.slope = field (EqField::slope);
        band.bypass = field (EqField::bypass);
    }
    eqCharacter = get (id::eqCharacter);
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
    p.chainOrder = choice (chainOrder, chain::numOrders);
    for (size_t i = 0; i < (size_t) numModules; ++i)
        p.moduleEnabled[i] = flag (moduleEnabled[i]);
    p.bypassed = flag (bypass);

    for (size_t b = 0; b < (size_t) eq::numBands; ++b)
    {
        const auto& source = eqBands[b];
        auto& band = p.modules.eq.bands[b];
        band.active = flag (source.active);
        band.bypassed = flag (source.bypass);
        band.settings.type = (dsp::eq::FilterType) choice (source.type, dsp::eq::numFilterTypes);
        band.settings.frequency = source.frequency->load();
        band.settings.gainDb = source.gain->load();
        band.settings.q = source.q->load();
        band.settings.slopeIndex = choice (source.slope, (int) dsp::eq::slopesDbPerOctave.size());
    }
    p.modules.eq.character = (eq::Character) choice (eqCharacter, eq::numCharacters);
    return p;
}

} // namespace pm::param
