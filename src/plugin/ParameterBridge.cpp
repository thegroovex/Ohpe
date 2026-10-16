// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ParameterBridge.h"

#include "Localisation.h"

namespace ohpe
{
namespace
{
/** Fixed decimals; juce::String (v, 0) would print every significant digit instead of none. */
juce::String fixed (float v, int places)
{
    return places <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, places);
}
} // namespace

juce::String formatValue (const ParamDescriptor& d, float v)
{
    switch (d.kind)
    {
        case ParamKind::Toggle: return v >= 0.5f ? i18n::tr ("On") : i18n::tr ("Off");
        case ParamKind::Choice:
        {
            const int i = juce::jlimit (0, (int) d.choices.size() - 1, (int) std::lround (v));
            return d.choices.empty() ? juce::String (i) : i18n::pick (d.choices[(size_t) i], d.choicesDe[(size_t) i]);
        }
        case ParamKind::Integer:
        {
            const int i = (int) std::lround (v);
            if (d.unit == ParamUnit::Semitones) return (i > 0 ? "+" : "") + juce::String (i) + " st";
            return juce::String (i);
        }
        case ParamKind::Continuous: break;
    }
    switch (d.unit)
    {
        case ParamUnit::Hertz: return v >= 1000.0f ? fixed (v / 1000.0f, 2) + " kHz" : fixed (v, v < 10.0f ? 2 : 0) + " Hz";
        case ParamUnit::Decibel: return v <= d.minValue + 0.05f && d.minValue <= -59.0f ? "-inf dB" : fixed (v, 1) + " dB";
        case ParamUnit::Seconds: return v < 1.0f ? fixed (v * 1000.0f, v < 0.1f ? 1 : 0) + " ms" : fixed (v, 2) + " s";
        case ParamUnit::Cents: return fixed (v, 0) + " ct";
        case ParamUnit::Bpm: return fixed (v, 1) + " BPM";
        case ParamUnit::Milliseconds: return v >= 1000.0f ? fixed (v / 1000.0f, 2) + " s" : fixed (v, v < 10.0f ? 1 : 0) + " ms";
        case ParamUnit::Percent: return fixed (v * 100.0f, 0) + " %";
        case ParamUnit::Semitones: return fixed (v, 1) + " st";
        case ParamUnit::Degrees: return fixed (v, 0) + juce::String::fromUTF8 ("°");
        case ParamUnit::Metres: return fixed (v, v < 10.0f ? 1 : 0) + " m";
        case ParamUnit::None: break;
    }
    if (d.minValue >= 0.0f && d.maxValue <= 1.0f) return fixed (v * 100.0f, 0) + " %";
    return fixed (v, 2);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const ParamRegistry& reg)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        const juce::ParameterID pid { juce::String (d.id), 1 };
        const auto name = juce::String::fromUTF8 (d.nameEn.c_str());
        const ParamDescriptor desc = d;
        switch (d.kind)
        {
            case ParamKind::Toggle:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, d.defaultValue >= 0.5f));
                break;
            case ParamKind::Choice:
            {
                juce::StringArray choices;
                for (const auto& c : d.choices) choices.add (juce::String::fromUTF8 (c.c_str()));
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, (int) d.defaultValue));
                break;
            }
            case ParamKind::Integer:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    pid, name, (int) d.minValue, (int) d.maxValue, (int) d.defaultValue,
                    juce::AudioParameterIntAttributes().withStringFromValueFunction ([desc] (int v, int) { return formatValue (desc, (float) v); })));
                break;
            case ParamKind::Continuous:
            {
                juce::NormalisableRange<float> range (d.minValue, d.maxValue, 0.0f, d.skew);
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, name, range, d.defaultValue,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction ([desc] (float v, int) { return formatValue (desc, v); })));
                break;
            }
        }
    }
    return layout;
}
} // namespace ohpe
