// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace ohpe
{
/** How a parameter value is shown and quantised. */
enum class ParamKind
{
    Continuous, ///< float range
    Integer,    ///< whole numbers between min and max
    Toggle,     ///< 0 / 1
    Choice      ///< index into `choices` (the list may only grow at the end)
};

enum class ParamUnit
{
    None,
    Percent,
    Decibel,
    Hertz,
    Seconds,
    Semitones,
    Cents,
    Bpm,
    Milliseconds ///< 0.7 (rack effects): plain value in ms
};

/**
    Static description of one automatable / MIDI-mappable parameter.

    The `id` is the parameter's permanent identity: host automation, MIDI maps, presets and
    states all refer to it. IDs are append-only (see docs/ARCHITECTURE.md "Parameter rules"):
    never rename, renumber, remove or reorder an existing ID. New parameters go at the end.
*/
struct ParamDescriptor
{
    std::string id;     ///< stable string ID, e.g. "p1.analog.cutoff"
    std::string nameEn; ///< English display name
    std::string nameDe; ///< German display name
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float skew = 1.0f; ///< JUCE-compatible skew: norm = ((v - min) / (max - min)) ^ skew
    ParamKind kind = ParamKind::Continuous;
    ParamUnit unit = ParamUnit::None;
    std::vector<std::string> choices;   ///< for Choice (English labels)
    std::vector<std::string> choicesDe; ///< for Choice (German labels, same order)
    std::string group;                  ///< UI / host group, e.g. "part1/analog"

    float clamp (float v) const noexcept
    {
        if (! (v >= minValue)) v = minValue; // also catches NaN
        if (v > maxValue) v = maxValue;
        if (kind != ParamKind::Continuous) v = std::round (v);
        return v;
    }

    /** plain value -> 0..1 (same mapping as juce::NormalisableRange with this skew). */
    float toNormalised (float plain) const noexcept
    {
        const float range = maxValue - minValue;
        if (range <= 0.0f) return 0.0f;
        float p = (clamp (plain) - minValue) / range;
        if (skew != 1.0f && p > 0.0f) p = std::pow (p, skew);
        return p;
    }

    /** 0..1 -> plain value (snapped for discrete kinds). */
    float fromNormalised (float norm) const noexcept
    {
        if (! (norm >= 0.0f)) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        if (skew != 1.0f && norm > 0.0f) norm = std::exp (std::log (norm) / skew);
        return clamp (minValue + (maxValue - minValue) * norm);
    }

    bool isDiscrete() const noexcept { return kind != ParamKind::Continuous; }
};

/** Builder helpers keep the parameter tables short and readable. */
namespace pd
{
inline ParamDescriptor continuous (std::string id, std::string en, std::string de, float mn, float mx, float def,
                                   ParamUnit unit = ParamUnit::None, float skew = 1.0f)
{
    ParamDescriptor d;
    d.id = std::move (id); d.nameEn = std::move (en); d.nameDe = std::move (de);
    d.minValue = mn; d.maxValue = mx; d.defaultValue = def; d.unit = unit; d.skew = skew;
    return d;
}

inline ParamDescriptor integer (std::string id, std::string en, std::string de, int mn, int mx, int def,
                                ParamUnit unit = ParamUnit::None)
{
    auto d = continuous (std::move (id), std::move (en), std::move (de), (float) mn, (float) mx, (float) def, unit);
    d.kind = ParamKind::Integer;
    return d;
}

inline ParamDescriptor toggle (std::string id, std::string en, std::string de, bool def)
{
    auto d = continuous (std::move (id), std::move (en), std::move (de), 0.0f, 1.0f, def ? 1.0f : 0.0f);
    d.kind = ParamKind::Toggle;
    return d;
}

inline ParamDescriptor choice (std::string id, std::string en, std::string de, std::vector<std::string> choicesEn,
                               std::vector<std::string> choicesDe, int def)
{
    auto d = continuous (std::move (id), std::move (en), std::move (de), 0.0f, (float) choicesEn.size() - 1.0f, (float) def);
    d.kind = ParamKind::Choice;
    d.choices = std::move (choicesEn);
    d.choicesDe = std::move (choicesDe);
    return d;
}
} // namespace pd
} // namespace ohpe
