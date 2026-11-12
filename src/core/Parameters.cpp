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

/** Rounded to four significant digits: saving and loading a project gives back exactly the same value. */
float significant (double x)
{
    if (! (x > 0.0))
        return (float) x;
    const auto magnitude = std::pow (10.0, std::floor (std::log10 (x)) - 3.0);
    return (float) (std::round (x / magnitude) * magnitude);
}

/** Logarithmic range (frequencies, Q, times), with values rounded by significant(). */
juce::NormalisableRange<float> logRange (float start, float end)
{
    return juce::NormalisableRange<float> (
        start, end,
        [] (float s, float e, float v) { return juce::jlimit (s, e, significant (s * std::pow ((double) e / s, (double) v))); },
        [] (float s, float e, float x) { return (float) (std::log ((double) juce::jlimit (s, e, x) / s) / std::log ((double) e / s)); },
        [] (float s, float e, float x) { return juce::jlimit (s, e, significant (x)); });
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& parameterId, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float defaultValue,
                                                      const juce::String& unit,
                                                      std::function<juce::String (float)> toText,
                                                      std::function<float (const juce::String&)> fromText)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([toText] (float v, int) { return toText (v); })
                          .withValueFromStringFunction ([fromText] (const juce::String& t) { return fromText (t); });

    return std::make_unique<juce::AudioParameterFloat> (pid (parameterId), name, range, defaultValue, attributes);
}

float parseNumberText (const juce::String& t) { return (float) format::parseNumber (t); }
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

juce::StringArray compStyleNames() { return { "Clean", "Classic", "Opto", "Vocal", "Mastering", "Bus", "Punch", "Pumping" }; }
juce::StringArray compSidechainNames() { return { "Interno", "Externo" }; }
juce::StringArray compOversamplingNames() { return { "Off", "2x", "4x" }; }

juce::StringArray compressorIds()
{
    return { id::compStyle,     id::compThreshold, id::compRatio,       id::compKnee,         id::compRange,
             id::compAttack,    id::compRelease,   id::compAutoRelease, id::compLookahead,    id::compHold,
             id::compMakeup,    id::compAutoGain,  id::compMix,         id::compSidechain,    id::compOversampling,
             id::compScHpOn,    id::compScHpFreq,  id::compScHpSlope,   id::compScLpOn,       id::compScLpFreq,
             id::compScLpSlope, id::compScBellOn,  id::compScBellFreq,  id::compScBellGain,   id::compScBellQ };
}

juce::NormalisableRange<float> compRatioRange()
{
    // Normalised 0 … 0.96: 1:1 … 100:1, logarithmic with 4:1 near the middle; above 0.98: ∞:1.
    static constexpr double finiteEnd = 0.96;
    static constexpr double infiniteStart = 0.98;
    static constexpr double skew = 1.84;
    const auto finiteMax = (float) dsp::comp::maxFiniteRatio;
    const auto infinite = (float) dsp::comp::infiniteRatio;

    const auto snap = [finiteMax, infinite] (float x)
    {
        if (x >= 0.5f * (finiteMax + infinite))
            return infinite;
        if (x >= finiteMax)
            return finiteMax;
        const auto decimals = x < 10.0f ? 100.0f : 10.0f;
        return juce::jmax (1.0f, std::round (x * decimals) / decimals);
    };

    return juce::NormalisableRange<float> (
        1.0f, infinite,
        [snap, finiteMax, infinite] (float, float, float v)
        {
            if (v > infiniteStart)
                return infinite;
            if (v >= finiteEnd)
                return finiteMax;
            return snap ((float) std::pow ((double) finiteMax, std::pow (juce::jmax (0.0, (double) v / finiteEnd), skew)));
        },
        [finiteMax, infinite] (float, float, float x)
        {
            if (x >= infinite)
                return 1.0f;
            if (x >= finiteMax)
                return (float) finiteEnd;
            const auto position = std::log (juce::jmax (1.0, (double) x)) / std::log ((double) finiteMax);
            return (float) (finiteEnd * std::pow (position, 1.0 / skew));
        },
        [snap] (float, float, float x) { return snap (x); });
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

    // EQ: eight band slots and the character (§4, §12). Frequency and Q use logarithmic ranges
    // (logRange(): four significant digits, so projects reload exactly).

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

    // Compressor (§5, §12): neutral defaults, so it does nothing until the threshold goes down.
    auto compGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("comp", "Compresor", " | ");
    {
        const auto prefix = text ("Compresor \xc2\xb7 ");
        const auto sidechain = prefix + text ("Sidechain \xc2\xb7 ");
        const auto decibels = [] (float v) { return format::number (v, 1); };
        const auto parseTime = [] (const juce::String& t) { return (float) format::parseMilliseconds (t); };
        const auto hertz = [] (float v) { return format::frequency (v); };
        const auto parseHertz = [] (const juce::String& t) { return (float) format::parseFrequency (t); };
        const auto plain = [] (float v) { return format::compact (v, 2); };

        compGroup->addChild (
            std::make_unique<juce::AudioParameterChoice> (pid (id::compStyle), prefix + "Estilo", compStyleNames(), 0),
            makeFloat (id::compThreshold, prefix + "Threshold",
                       { (float) dsp::comp::minThresholdDb, (float) dsp::comp::maxThresholdDb, 0.1f }, 0.0f, "dB",
                       decibels, parseNumberText),
            makeFloat (id::compRatio, prefix + "Ratio", compRatioRange(), 4.0f, "",
                       [] (float v) { return format::ratio (v, dsp::comp::infiniteRatio); },
                       [] (const juce::String& t) { return (float) format::parseRatio (t, dsp::comp::infiniteRatio); }),
            makeFloat (id::compKnee, prefix + "Knee", { 0.0f, (float) dsp::comp::maxKneeDb, 0.1f }, 0.0f, "dB",
                       decibels, parseNumberText),
            makeFloat (id::compRange, prefix + "Range", { 0.0f, (float) dsp::comp::maxRangeDb, 0.1f }, 60.0f, "dB",
                       decibels, parseNumberText),
            makeFloat (id::compAttack, prefix + "Attack",
                       logRange ((float) dsp::comp::minAttackMs, (float) dsp::comp::maxAttackMs), 10.0f, "ms",
                       [] (float v) { return format::compact (v, v < 0.1f ? 3 : v < 1.0f ? 2 : v < 10.0f ? 1 : 0); },
                       parseTime),
            makeFloat (id::compRelease, prefix + "Release",
                       logRange ((float) dsp::comp::minReleaseMs, (float) dsp::comp::maxReleaseMs), 120.0f, "ms",
                       [] (float v) { return format::compact (v, 0); }, parseTime),
            makeBool (id::compAutoRelease, prefix + "Auto-release", false, "Activado", "Desactivado"),
            makeFloat (id::compLookahead, prefix + "Lookahead", { 0.0f, (float) dsp::comp::maxLookaheadMs, 0.01f }, 0.0f,
                       "ms", [] (float v) { return format::compact (v, 2); }, parseTime),
            makeFloat (id::compHold, prefix + "Hold", { 0.0f, (float) dsp::comp::maxHoldMs, 0.1f, 0.35f }, 0.0f, "ms",
                       [] (float v) { return format::compact (v, v < 10.0f ? 1 : 0); }, parseTime),
            makeFloat (id::compMakeup, prefix + "Makeup",
                       { (float) dsp::comp::minMakeupDb, (float) dsp::comp::maxMakeupDb, 0.1f }, 0.0f, "dB",
                       decibels, parseNumberText),
            makeBool (id::compAutoGain, prefix + "Auto-gain", false, "Activado", "Desactivado"),
            makeFloat (id::compMix, prefix + "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%",
                       [] (float v) { return format::number (v, 0); }, parseNumberText),
            std::make_unique<juce::AudioParameterChoice> (pid (id::compSidechain), prefix + "Sidechain", compSidechainNames(), 0),
            std::make_unique<juce::AudioParameterChoice> (pid (id::compOversampling), prefix + "Oversampling",
                                                          compOversamplingNames(), 0),
            makeBool (id::compScHpOn, sidechain + "Pasa-altos", true, "Activado", "Desactivado"),
            makeFloat (id::compScHpFreq, sidechain + text ("Pasa-altos \xc2\xb7 Frecuencia"),
                       logRange (comp::sidechainHighPassMin, comp::sidechainHighPassMax), 80.0f, "Hz", hertz, parseHertz),
            std::make_unique<juce::AudioParameterChoice> (pid (id::compScHpSlope), sidechain + text ("Pasa-altos \xc2\xb7 Pendiente"),
                                                          eqSlopeNames(), 3),
            makeBool (id::compScLpOn, sidechain + "Pasa-bajos", true, "Activado", "Desactivado"),
            makeFloat (id::compScLpFreq, sidechain + text ("Pasa-bajos \xc2\xb7 Frecuencia"),
                       logRange (comp::sidechainLowPassMin, comp::sidechainLowPassMax), 20000.0f, "Hz", hertz, parseHertz),
            std::make_unique<juce::AudioParameterChoice> (pid (id::compScLpSlope), sidechain + text ("Pasa-bajos \xc2\xb7 Pendiente"),
                                                          eqSlopeNames(), 1),
            makeBool (id::compScBellOn, sidechain + "Campana", true, "Activada", "Desactivada"),
            makeFloat (id::compScBellFreq, sidechain + text ("Campana \xc2\xb7 Frecuencia"),
                       logRange (comp::sidechainBellMin, comp::sidechainBellMax), 1000.0f, "Hz", hertz, parseHertz),
            makeFloat (id::compScBellGain, sidechain + text ("Campana \xc2\xb7 Ganancia"),
                       { -comp::sidechainBellMaxGainDb, comp::sidechainBellMaxGainDb, 0.1f }, 0.0f, "dB", decibels,
                       parseNumberText),
            makeFloat (id::compScBellQ, sidechain + text ("Campana \xc2\xb7 Q"),
                       logRange (comp::sidechainBellMinQ, comp::sidechainBellMaxQ), 1.0f, "", plain, parseNumberText));
    }

    Apvts::ParameterLayout layout;
    layout.add (std::move (global), std::move (modules), std::move (eqGroup), std::move (compGroup));
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

    comp.style = get (id::compStyle);
    comp.threshold = get (id::compThreshold);
    comp.ratio = get (id::compRatio);
    comp.knee = get (id::compKnee);
    comp.range = get (id::compRange);
    comp.attack = get (id::compAttack);
    comp.release = get (id::compRelease);
    comp.autoRelease = get (id::compAutoRelease);
    comp.lookahead = get (id::compLookahead);
    comp.hold = get (id::compHold);
    comp.makeup = get (id::compMakeup);
    comp.autoGain = get (id::compAutoGain);
    comp.mix = get (id::compMix);
    comp.sidechain = get (id::compSidechain);
    comp.oversampling = get (id::compOversampling);
    comp.highPass = { get (id::compScHpOn), get (id::compScHpFreq), get (id::compScHpSlope), nullptr, nullptr };
    comp.lowPass = { get (id::compScLpOn), get (id::compScLpFreq), get (id::compScLpSlope), nullptr, nullptr };
    comp.bell = { get (id::compScBellOn), get (id::compScBellFreq), nullptr, get (id::compScBellGain), get (id::compScBellQ) };
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

    auto& c = p.modules.comp;
    const auto numSlopes = (int) dsp::eq::slopesDbPerOctave.size();
    c.style = (dsp::comp::Style) choice (comp.style, dsp::comp::numStyles);
    c.thresholdDb = comp.threshold->load();
    c.ratio = comp.ratio->load();
    c.kneeDb = comp.knee->load();
    c.rangeDb = comp.range->load();
    c.attackMs = comp.attack->load();
    c.releaseMs = comp.release->load();
    c.autoRelease = flag (comp.autoRelease);
    c.lookaheadMs = comp.lookahead->load();
    c.holdMs = comp.hold->load();
    c.makeupDb = comp.makeup->load();
    c.autoGain = flag (comp.autoGain);
    c.mix = comp.mix->load() / 100.0f;
    c.externalSidechain = choice (comp.sidechain, 2) == 1;
    c.oversamplingIndex = choice (comp.oversampling, 3);
    c.highPass = { flag (comp.highPass.on), comp.highPass.frequency->load(), 0.0f, 1.0f, choice (comp.highPass.slope, numSlopes) };
    c.lowPass = { flag (comp.lowPass.on), comp.lowPass.frequency->load(), 0.0f, 1.0f, choice (comp.lowPass.slope, numSlopes) };
    c.bell = { flag (comp.bell.on), comp.bell.frequency->load(), comp.bell.gain->load(), comp.bell.q->load(), 0 };
    return p;
}

} // namespace pm::param
