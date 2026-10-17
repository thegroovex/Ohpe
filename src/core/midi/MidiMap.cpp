// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiMap.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <thread>

namespace ohpe
{
namespace
{
const char* modeName (MidiMapMode m)
{
    switch (m)
    {
        case MidiMapMode::Toggle: return "toggle";
        case MidiMapMode::Relative: return "relative";
        default: return "absolute";
    }
}
MidiMapMode modeFrom (const std::string& s)
{
    if (s == "toggle") return MidiMapMode::Toggle;
    if (s == "relative") return MidiMapMode::Relative;
    return MidiMapMode::Absolute;
}
} // namespace

// ---------------------------------------------------------------- MidiMap
void MidiMap::add (const MidiMapping& m)
{
    list.erase (std::remove_if (list.begin(), list.end(), [&] (const MidiMapping& o) { return o.paramId == m.paramId && o.sameSource (m); }),
                list.end());
    list.push_back (m);
}

bool MidiMap::replace (size_t index, const MidiMapping& m)
{
    if (index >= list.size() || m.paramId.empty()) return false;
    auto c = m;
    c.channel = std::clamp (c.channel, 0, 16);
    c.number = std::clamp (c.number, 0, 127);
    c.minNorm = std::clamp (c.minNorm, 0.0f, 1.0f);
    c.maxNorm = std::clamp (c.maxNorm, 0.0f, 1.0f);
    list[index] = c;
    // the edited mapping may now duplicate another one (same source, same parameter): keep the edited one
    for (size_t i = 0; i < list.size(); ++i)
        if (i != index && list[i].paramId == c.paramId && list[i].sameSource (c))
        {
            list.erase (list.begin() + (std::ptrdiff_t) i);
            break;
        }
    return true;
}

bool MidiMap::remove (size_t index)
{
    if (index >= list.size()) return false;
    list.erase (list.begin() + (std::ptrdiff_t) index);
    return true;
}

int MidiMap::clearParam (const std::string& id)
{
    const auto before = list.size();
    list.erase (std::remove_if (list.begin(), list.end(), [&] (const MidiMapping& o) { return o.paramId == id; }), list.end());
    return (int) (before - list.size());
}

std::vector<MidiMapping> MidiMap::forParam (const std::string& id) const
{
    std::vector<MidiMapping> r;
    for (const auto& m : list)
        if (m.paramId == id) r.push_back (m);
    return r;
}

bool MidiMap::isMapped (const std::string& id) const
{
    return std::any_of (list.begin(), list.end(), [&] (const MidiMapping& m) { return m.paramId == id; });
}

std::string MidiMap::toJson() const
{
    nlohmann::json j;
    j["format"] = "ohpe-midimap";
    j["version"] = 1;
    auto arr = nlohmann::json::array();
    for (const auto& m : list)
        arr.push_back ({ { "param", m.paramId }, { "type", m.type == MidiSourceType::Note ? "note" : "cc" }, { "channel", m.channel },
                         { "number", m.number }, { "min", m.minNorm }, { "max", m.maxNorm }, { "mode", modeName (m.mode) } });
    j["mappings"] = arr;
    return j.dump (2);
}

bool MidiMap::fromJson (const std::string& text)
{
    const auto j = nlohmann::json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || j.value ("format", "") != "ohpe-midimap" || ! j.contains ("mappings")
        || ! j["mappings"].is_array())
        return false;
    std::vector<MidiMapping> parsed;
    for (const auto& jm : j["mappings"])
    {
        if (! jm.is_object()) continue;
        MidiMapping m;
        m.paramId = jm.value ("param", "");
        if (m.paramId.empty()) continue;
        m.type = jm.value ("type", "cc") == "note" ? MidiSourceType::Note : MidiSourceType::CC;
        m.channel = std::clamp (jm.value ("channel", 0), 0, 16);
        m.number = std::clamp (jm.value ("number", 0), 0, 127);
        m.minNorm = std::clamp (jm.value ("min", 0.0f), 0.0f, 1.0f);
        m.maxNorm = std::clamp (jm.value ("max", 1.0f), 0.0f, 1.0f);
        m.mode = modeFrom (jm.value ("mode", "absolute"));
        parsed.push_back (m);
    }
    list = std::move (parsed);
    return true;
}

// ---------------------------------------------------------------- MidiMapper
void MidiMapper::publish (const MidiMap& map)
{
    const int target = 1 - active.load();
    while (reading.load() == target)
        std::this_thread::yield();
    auto& t = tables[(size_t) target];
    t.count = 0;
    for (const auto& m : map.mappings())
    {
        const int idx = reg.indexOf (m.paramId);
        if (idx < 0 || t.count >= maxEntries) continue; // unknown parameter (other version): kept in the map, not active
        t.entries[(size_t) t.count++] = { idx, m.type, m.channel, m.number, m.minNorm, m.maxNorm, m.mode };
    }
    active.store (target);
}

bool MidiMapper::handleMidi (const MidiEvent& e) noexcept
{
    const bool isCc = e.isController();
    const bool isNote = e.isNoteOn() || e.isNoteOff();
    if (! isCc && ! isNote) return false;

    // learn: the first control (CC or note-on) after arming is assigned
    const int target = learnTarget.load (std::memory_order_relaxed);
    if (target >= 0 && (isCc || e.isNoteOn()))
    {
        int expected = target;
        if (learnTarget.compare_exchange_strong (expected, -1))
            learned.push ({ target, isCc ? MidiSourceType::CC : MidiSourceType::Note, e.channel(), e.data1 });
        return true;
    }

    int idx = active.load();
    reading.store (idx);
    while (active.load() != idx) { idx = active.load(); reading.store (idx); }
    const auto& t = tables[(size_t) idx];

    bool consumed = false;
    const auto type = isCc ? MidiSourceType::CC : MidiSourceType::Note;
    for (int i = 0; i < t.count; ++i)
    {
        const auto& m = t.entries[(size_t) i];
        if (m.type != type || m.number != e.data1 || (m.channel != 0 && m.channel != e.channel())) continue;
        consumed = true;
        float norm = reg.getNormalised (m.paramIndex);
        const float v = (float) e.data2 / 127.0f;
        switch (m.mode)
        {
            case MidiMapMode::Absolute:
                if (type == MidiSourceType::Note) norm = e.isNoteOn() ? m.maxNorm : m.minNorm;
                else norm = m.minNorm + (m.maxNorm - m.minNorm) * v;
                break;
            case MidiMapMode::Toggle:
                if ((type == MidiSourceType::Note && e.isNoteOn()) || (type == MidiSourceType::CC && e.data2 >= 64))
                    norm = norm >= 0.5f * (m.minNorm + m.maxNorm) ? m.minNorm : m.maxNorm;
                else continue;
                break;
            case MidiMapMode::Relative:
            {
                if (type != MidiSourceType::CC) continue;
                const int delta = e.data2 < 64 ? e.data2 : e.data2 - 128; // two's complement
                norm = std::clamp (norm + (float) delta / 127.0f, std::min (m.minNorm, m.maxNorm), std::max (m.minNorm, m.maxNorm));
                break;
            }
        }
        reg.setNormalised (m.paramIndex, norm);
        changes.push ({ m.paramIndex, reg.getNormalised (m.paramIndex) });
    }
    reading.store (-1);
    return consumed;
}

bool MidiMapper::applyPendingLearns (MidiMap& map)
{
    bool changed = false;
    LearnResult r;
    while (learned.pop (r))
    {
        if (r.paramIndex < 0 || r.paramIndex >= reg.size()) continue;
        MidiMapping m;
        m.paramId = reg.descriptor (r.paramIndex).id;
        m.type = r.type;
        m.channel = r.channel;
        m.number = r.number;
        const auto& d = reg.descriptor (r.paramIndex);
        m.mode = (r.type == MidiSourceType::Note && (d.kind == ParamKind::Toggle)) ? MidiMapMode::Toggle : MidiMapMode::Absolute;
        // learning replaces the parameter's previous mappings (one gesture = one control)
        map.clearParam (m.paramId);
        map.add (m);
        changed = true;
    }
    if (changed) publish (map);
    return changed;
}
} // namespace ohpe
