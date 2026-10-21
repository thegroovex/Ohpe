// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HarmonyBrain.h"
#include "../model/FormatVersions.h"

#include "../seq/ScaleChord.h"

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace ohpe
{
using nlohmann::json;

namespace harmony
{
namespace
{
constexpr uint16_t bits (std::initializer_list<int> pcs)
{
    uint16_t m = 0;
    for (int p : pcs) m = (uint16_t) (m | (1u << p));
    return m;
}
int popcount (uint16_t m) noexcept
{
    int n = 0;
    for (; m != 0; m = (uint16_t) (m & (m - 1))) ++n;
    return n;
}
uint16_t rotate (uint16_t mask, int root) noexcept // relative to root -> absolute pitch classes
{
    root = ((root % 12) + 12) % 12;
    return (uint16_t) (((mask << root) | (mask >> (12 - root))) & 0xfff);
}
} // namespace

uint16_t chordShape (int type) noexcept
{
    static constexpr uint16_t shapes[numChordTypes] = {
        bits ({ 0, 4, 7 }), bits ({ 0, 3, 7 }), bits ({ 0, 3, 6 }), bits ({ 0, 4, 8 }), bits ({ 0, 2, 7 }),
        bits ({ 0, 5, 7 }), bits ({ 0, 4, 7, 11 }), bits ({ 0, 3, 7, 10 }), bits ({ 0, 4, 7, 10 }), bits ({ 0, 7 })
    };
    return shapes[std::clamp (type, 0, numChordTypes - 1)];
}

const char* chordSuffix (int type) noexcept
{
    static const char* s[numChordTypes] = { "", "m", "dim", "+", "sus2", "sus4", "maj7", "m7", "7", "5" };
    return s[std::clamp (type, 0, numChordTypes - 1)];
}

std::vector<std::string> followNamesEn() { return { "Off", "Scale", "Chord", "Root" }; }
std::vector<std::string> followNamesDe() { return { "Aus", "Skala", "Akkord", "Grundton" }; }
std::vector<std::string> layoutNamesEn() { return { "Fourths", "Thirds", "Whole Tones", "Sequential" }; }
std::vector<std::string> layoutNamesDe() { return { "Quarten", "Terzen", "Ganztöne", "Sequenziell" }; }

bool detectChord (uint16_t pcs, int bassPc, Chord& out) noexcept
{
    pcs &= 0xfff;
    if (popcount (pcs) < 2) return false;
    float best = 0.0f;
    bool found = false;
    for (int root = 0; root < 12; ++root)
    {
        if (((pcs >> root) & 1u) == 0) continue; // the root is played
        for (int t = 0; t < numChordTypes; ++t)
        {
            const uint16_t shape = rotate (chordShape (t), root);
            const int matched = popcount ((uint16_t) (pcs & shape));
            if (matched < 2) continue;
            const int missing = popcount ((uint16_t) (shape & ~pcs & 0xfff));
            const int extra = popcount ((uint16_t) (pcs & ~shape & 0xfff));
            float score = 2.0f * (float) matched - 1.0f * (float) missing - 1.5f * (float) extra - 0.01f * (float) t;
            if (root == bassPc) score += 0.5f;
            if (! found || score > best)
            {
                best = score;
                out = { root, t };
                found = true;
            }
        }
    }
    return found && best > 0.0f;
}

float keyScore (const std::array<float, 12>& w, int root, int scale) noexcept
{
    const uint16_t m = rotate (scales::mask (scale), root);
    float in = 0.0f, out = 0.0f;
    for (int pc = 0; pc < 12; ++pc) (((m >> pc) & 1u) != 0 ? in : out) += w[(size_t) pc];
    const int third = (root + (scale == scales::Minor ? 3 : 4)) % 12;
    return in - 1.5f * out + 0.6f * w[(size_t) root] + 0.3f * w[(size_t) ((root + 7) % 12)] + 0.3f * w[(size_t) third];
}

void detectKey (const std::array<float, 12>& w, int prevRoot, int prevScale, int& root, int& scale) noexcept
{
    float total = 0.0f;
    for (float v : w) total += v;
    root = prevRoot;
    scale = prevScale;
    if (total <= 1.0e-4f) return;
    float best = -1.0e9f;
    int bestRoot = prevRoot, bestScale = prevScale;
    for (int s : { (int) scales::Major, (int) scales::Minor })
        for (int r = 0; r < 12; ++r)
            if (const float sc = keyScore (w, r, s); sc > best + 1.0e-6f) { best = sc; bestRoot = r; bestScale = s; }
    const bool prevValid = prevRoot >= 0 && prevRoot < 12 && (prevScale == scales::Major || prevScale == scales::Minor);
    if (! prevValid || best > keyScore (w, prevRoot, prevScale) + 0.05f * total + 1.0e-4f)
    {
        root = bestRoot;
        scale = bestScale;
    }
}

GridStep layoutSteps (int layout) noexcept
{
    switch (layout)
    {
        case LayoutThirds: return { 1, 4 };
        case LayoutWhole: return { 2, 5 };
        case LayoutSequential: return { 1, 8 }; // p59: continuous rows of eight (the pads' width)
        default: return { 1, 5 };
    }
}

GridStep layoutDegrees (int layout) noexcept
{
    switch (layout)
    {
        case LayoutThirds: return { 1, 2 };     // a row up = a third (two scale steps)
        case LayoutSequential: return { 1, 8 }; // continuous rows: eight scale steps per row
        default: return { 1, 3 };               // Fourths (and Whole Tones: every scale step is about a whole tone)
    }
}

int gridNote (int layout, int col, int row, int base, int keyRoot, int keyScale, bool inKey, bool fixed, int originRoot) noexcept
{
    layout = std::clamp (layout, 0, (int) numLayouts - 1);
    const int from = originRoot >= 0 ? originRoot : keyRoot;
    const int root = ((from % 12) + 12) % 12;
    if (! inKey || keyScale == scales::Chromatic)
    {
        const auto st = layoutSteps (layout);
        const int origin = base + (fixed ? 0 : root); // not fixed: the bottom-left cell is the key's root
        return std::clamp (origin + col * st.col + row * st.row, 0, 127);
    }
    // In Key: every step is a scale step; the bottom-left cell is the root (not fixed) or the first key note from C
    int n = std::clamp (base + (fixed ? 0 : root), 0, 127);
    while (n < 127 && ! scales::inScale (n, keyRoot, keyScale)) ++n;
    const auto st = layoutDegrees (layout);
    return std::clamp (scales::degreeAbove (n, col * st.col + row * st.row, keyRoot, keyScale), 0, 127);
}

GridOptions gridOptions (const ParamRegistry& reg) noexcept
{
    GridOptions o;
    o.layout = std::clamp ((int) std::lround (reg.get ("harmony.kb.layout")), 0, (int) numLayouts - 1);
    o.inKey = reg.get ("harmony.kb.inkey") >= 0.5f;
    o.fixed = reg.get ("harmony.kb.fixed") >= 0.5f;
    return o;
}

bool migrateLayout (std::map<std::string, float>& params)
{
    if (params.count ("harmony.kb.inkey") != 0) return false; // written by p59 or later
    const auto it = params.find ("harmony.kb.layout");
    if (it == params.end() || (int) std::lround (it->second) != legacyLayoutInKey) return false;
    it->second = (float) LayoutFourths;
    params["harmony.kb.inkey"] = 1.0f;
    return true;
}
} // namespace harmony

// ------------------------------------------------------------------------------------------------ brain
HarmonyBrain::HarmonyBrain()
{
    for (auto& h : pubHistory) h.store (-1, std::memory_order_relaxed);
    for (auto& w : pubWeight) w.store (0.0f, std::memory_order_relaxed);
}

void HarmonyBrain::registerParams (ParamRegistry& r, int parts)
{
    using namespace pd;
    auto addGlobal = [&] (ParamDescriptor d) { d.group = "global/harmony"; r.add (std::move (d)); };
    addGlobal (integer ("harmony.memory", "Harmony Memory", "Harmonie-Gedächtnis", 1, 32, 8));
    addGlobal (toggle ("harmony.hold", "Harmony Hold", "Harmonie halten", false));
    addGlobal (toggle ("harmony.take", "Harmony Take Key", "Harmonie Tonart übernehmen", false));
    addGlobal (toggle ("harmony.clear", "Harmony Clear", "Harmonie vergessen", false));
    addGlobal (choice ("harmony.kb.layout", "Grid Layout", "Raster-Anordnung", harmony::layoutNamesEn(), harmony::layoutNamesDe(),
                       harmony::LayoutFourths));
    for (int p = 1; p <= parts; ++p)
    {
        const auto n = std::to_string (p);
        // global IDs (not "pN.*"): the follow mode is the part's role in the song - not its sound (presets, breeding)
        auto d = choice ("harmony.p" + n + ".follow", "Part " + n + " Harmony Follow", "Part " + n + " Harmonie folgen",
                         harmony::followNamesEn(), harmony::followNamesDe(), harmony::FollowOff);
        d.group = "global/harmony";
        r.add (std::move (d));
    }
}

void HarmonyBrain::registerLate59GridParams (ParamRegistry& r)
{
    using namespace pd;
    auto inKey = toggle ("harmony.kb.inkey", "Grid In Key", "Raster nur Tonart", false);
    inKey.group = "global/harmony";
    r.add (std::move (inKey));
    auto fixed = toggle ("harmony.kb.fixed", "Grid Fixed C", "Raster C fest", false);
    fixed.group = "global/harmony";
    r.add (std::move (fixed));
}

void HarmonyBrain::bind (const ParamRegistry& r)
{
    for (int p = 0; p < numParts; ++p) idx[(size_t) p].follow = r.indexOf ("harmony.p" + std::to_string (p + 1) + ".follow");
    idxMemory = r.indexOf ("harmony.memory");
    idxHold = r.indexOf ("harmony.hold");
    idxClear = r.indexOf ("harmony.clear");
}

void HarmonyBrain::reset() noexcept
{
    for (auto& h : held) h.fill (0);
    pubHeld.store (0, std::memory_order_relaxed);
    changePulse = 0.0f;
    tension = 0.0f;
    stableSamples = 0;
    current = committed;
}

void HarmonyBrain::clearMemory() noexcept
{
    profile.fill (0.0f);
    for (auto& w : pubWeight) w.store (0.0f, std::memory_order_relaxed);
    for (auto& h : pubHistory) h.store (-1, std::memory_order_relaxed);
    current = committed = {};
    stableSamples = 0;
    changePulse = 0.0f;
    pubChord.store (-1, std::memory_order_relaxed);
    pubHeard.store (false, std::memory_order_relaxed);
    lastGlobalRoot = lastGlobalScale = -1; // the key falls back to the global key at the next block
}

void HarmonyBrain::beginBlock (const ParamSnapshot& snap, double bpm, int numSamples, int globalRoot, int globalScale) noexcept
{
    for (int p = 0; p < numParts; ++p)
        follow[(size_t) p] = idx[(size_t) p].follow >= 0 ? std::clamp (snap.getInt (idx[(size_t) p].follow), 0, (int) harmony::numFollow - 1) : 0;
    hold = idxHold >= 0 && snap.getBool (idxHold);
    const bool clear = idxClear >= 0 && snap.getBool (idxClear);
    if ((clear && ! wasClear) || clearRequest.exchange (false, std::memory_order_acq_rel)) clearMemory();
    wasClear = clear;
    if (loaded.exchange (false, std::memory_order_acq_rel)) // fromJson: adopt the loaded memory
    {
        for (int pc = 0; pc < 12; ++pc) profile[(size_t) pc] = pubWeight[(size_t) pc].load (std::memory_order_relaxed);
        const int c = pubChord.load (std::memory_order_relaxed);
        committed = current = c >= 0 ? harmony::Chord { c / 16, c % 16 } : harmony::Chord {};
        stableSamples = 0;
        lastGlobalRoot = globalRoot;
        lastGlobalScale = globalScale;
    }

    const double secs = (double) std::max (0, numSamples) / sr;
    // the pitch-class memory fades (half-life in bars) and gathers the held notes
    if (! hold)
    {
        const double beat = 60.0 / (bpm > 1.0 ? bpm : 120.0);
        const int bars = idxMemory >= 0 ? std::clamp (snap.getInt (idxMemory), 1, 32) : 8;
        const float fade = (float) std::pow (0.5, secs / (beat * 4.0 * (double) bars));
        const uint16_t h = pubHeld.load (std::memory_order_relaxed);
        for (int pc = 0; pc < 12; ++pc)
        {
            float v = profile[(size_t) pc] * fade + (((h >> pc) & 1u) != 0 ? (float) secs * 2.0f : 0.0f);
            if (! (v > 1.0e-6f)) v = 0.0f; // no denormals, no NaN
            profile[(size_t) pc] = std::min (v, 1000.0f);
            pubWeight[(size_t) pc].store (profile[(size_t) pc], std::memory_order_relaxed);
        }
        detectKey (globalRoot, globalScale);
    }
    else if (! heard() && (globalRoot != lastGlobalRoot || globalScale != lastGlobalScale))
        detectKey (globalRoot, globalScale);

    // a chord commits (history, change pulse) once it stood for 40 ms (the notes of a played chord arrive one by one)
    if (current != committed && current.root >= 0)
    {
        stableSamples += numSamples;
        if ((double) stableSamples >= 0.04 * sr) commit (current);
    }
    else
        stableSamples = 0;

    changePulse *= (float) std::exp (-secs / 0.3);
    if (changePulse < 1.0e-5f) changePulse = 0.0f;
    // tension: held pitch classes outside the key
    const uint16_t h = pubHeld.load (std::memory_order_relaxed);
    float t = 0.0f;
    if (h != 0)
    {
        const int r = keyRoot(), s = keyScale();
        int outside = 0, n = 0;
        for (int pc = 0; pc < 12; ++pc)
            if (((h >> pc) & 1u) != 0) { ++n; outside += scales::inScale (pc, r, s) ? 0 : 1; }
        t = (float) outside / (float) std::max (1, n);
    }
    const float k = 1.0f - (float) std::exp (-secs / 0.1);
    tension += (t - tension) * k;
    if (tension < 1.0e-5f) tension = 0.0f;
}

void HarmonyBrain::detectKey (int globalRoot, int globalScale) noexcept
{
    if (! heard())
    {
        lastGlobalRoot = globalRoot;
        lastGlobalScale = globalScale;
        pubKeyRoot.store (std::clamp (globalRoot, 0, 11), std::memory_order_relaxed);
        pubKeyScale.store (std::clamp (globalScale, 0, (int) scales::numScales - 1), std::memory_order_relaxed);
        return;
    }
    int r = 0, s = 0;
    harmony::detectKey (profile, keyRoot(), keyScale(), r, s);
    pubKeyRoot.store (r, std::memory_order_relaxed);
    pubKeyScale.store (s, std::memory_order_relaxed);
}

void HarmonyBrain::detectChord (int minPitchClasses) noexcept
{
    uint16_t pcs = 0;
    int bass = -1;
    for (int p = 0; p < numParts; ++p)
    {
        if (! listensTo (p)) continue;
        for (int n = 0; n < 128; ++n)
            if (held[(size_t) p][(size_t) n] > 0)
            {
                pcs = (uint16_t) (pcs | (1u << (n % 12)));
                if (bass < 0 || n < bass) bass = n;
            }
    }
    pubHeld.store (pcs, std::memory_order_relaxed);
    int count = 0;
    for (int pc = 0; pc < 12; ++pc) count += (pcs >> pc) & 1;
    if (hold || count < minPitchClasses) return;
    harmony::Chord c;
    if (harmony::detectChord (pcs, bass >= 0 ? bass % 12 : -1, c)) current = c;
}

void HarmonyBrain::commit (const harmony::Chord& c) noexcept
{
    committed = c;
    stableSamples = 0;
    changePulse = 1.0f;
    for (int i = historySize - 1; i > 0; --i)
        pubHistory[(size_t) i].store (pubHistory[(size_t) i - 1].load (std::memory_order_relaxed), std::memory_order_relaxed);
    pubHistory[0].store (c.root * 16 + c.type, std::memory_order_relaxed);
    pubChord.store (c.root * 16 + c.type, std::memory_order_relaxed);
}

void HarmonyBrain::noteOn (int part, int note, int velocity) noexcept
{
    if (part < 0 || part >= numParts || note < 0 || note > 127) return;
    auto& c = held[(size_t) part][(size_t) note];
    if (c < 255) ++c;
    if (! listensTo (part)) return;
    if (! hold)
    {
        profile[(size_t) (note % 12)] += 0.5f + (float) std::clamp (velocity, 0, 127) / 254.0f;
        pubHeard.store (true, std::memory_order_relaxed);
    }
    detectChord (2);
}

void HarmonyBrain::noteOff (int part, int note) noexcept
{
    if (part < 0 || part >= numParts || note < 0 || note > 127) return;
    auto& c = held[(size_t) part][(size_t) note];
    if (c > 0) --c;
    detectChord (3); // releasing one note of a triad keeps the chord
}

bool HarmonyBrain::followSet (int part, int& root, uint16_t& mask) const noexcept
{
    const int f = followMode (part);
    if (f == harmony::FollowOff) return false;
    const int kr = keyRoot(), ks = keyScale();
    if (f == harmony::FollowScale)
    {
        root = kr;
        mask = scales::mask (ks);
        return true;
    }
    harmony::Chord c = current;
    if (c.root < 0) // nothing heard: the key's tonic triad
    {
        std::array<int, 5> notes {};
        const int n = scales::build (60 + kr, scales::Triad, kr, ks, notes);
        uint16_t m = 0;
        for (int i = 0; i < n; ++i) m = (uint16_t) (m | (1u << (((notes[(size_t) i] - 60 - kr) % 12 + 12) % 12)));
        root = kr;
        mask = f == harmony::FollowRoot ? (uint16_t) 1 : m;
        return true;
    }
    root = c.root;
    mask = f == harmony::FollowRoot ? (uint16_t) 1 : harmony::chordShape (c.type);
    return true;
}

float HarmonyBrain::sourceValue (int source) const noexcept
{
    switch (source)
    {
        case 0: { const int c = pubChord.load (std::memory_order_relaxed); return c >= 0 ? (float) (c / 16) / 11.0f : 0.0f; }
        case 1: return changePulse;
        case 2: return std::clamp (tension, 0.0f, 1.0f);
        default: return 0.0f;
    }
}

harmony::Chord HarmonyBrain::chord() const noexcept
{
    const int c = pubChord.load (std::memory_order_relaxed);
    return c >= 0 ? harmony::Chord { c / 16, c % 16 } : harmony::Chord {};
}

harmony::Chord HarmonyBrain::history (int index) const noexcept
{
    if (index < 0 || index >= historySize) return {};
    const int c = pubHistory[(size_t) index].load (std::memory_order_relaxed);
    return c >= 0 ? harmony::Chord { c / 16, c % 16 } : harmony::Chord {};
}

int HarmonyBrain::historyCount() const noexcept
{
    int n = 0;
    while (n < historySize && pubHistory[(size_t) n].load (std::memory_order_relaxed) >= 0) ++n;
    return n;
}

float HarmonyBrain::weight (int pc) const noexcept
{
    if (pc < 0 || pc >= 12) return 0.0f;
    float mx = 0.0f;
    for (const auto& w : pubWeight) mx = std::max (mx, w.load (std::memory_order_relaxed));
    return mx > 1.0e-6f ? std::clamp (pubWeight[(size_t) pc].load (std::memory_order_relaxed) / mx, 0.0f, 1.0f) : 0.0f;
}

// ------------------------------------------------------------------------------------------------ state
std::string HarmonyBrain::toJson() const
{
    json hist = json::array();
    for (int i = 0; i < historyCount(); ++i)
    {
        const auto c = history (i);
        hist.push_back ({ c.root, c.type });
    }
    json prof = json::array();
    for (const auto& w : pubWeight) prof.push_back (w.load (std::memory_order_relaxed));
    const auto c = chord();
    return json ({ { "version", formats::harmony }, { "key", { keyRoot(), keyScale() } }, { "chord", { c.root, c.type } }, { "history", hist },
                   { "profile", prof } })
        .dump();
}

bool HarmonyBrain::fromJson (const std::string& text)
{
    const auto j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object()) return false;
    auto pair = [&] (const char* key, int& a, int& b) {
        if (auto it = j.find (key); it != j.end() && it->is_array() && it->size() == 2 && (*it)[0].is_number_integer() && (*it)[1].is_number_integer())
        {
            a = (*it)[0].get<int>();
            b = (*it)[1].get<int>();
            return true;
        }
        return false;
    };
    int kr = 0, ks = scales::Major, cr = -1, ct = 0;
    pair ("key", kr, ks);
    pair ("chord", cr, ct);
    pubKeyRoot.store (std::clamp (kr, 0, 11), std::memory_order_relaxed);
    pubKeyScale.store (std::clamp (ks, 0, (int) scales::numScales - 1), std::memory_order_relaxed);
    const bool chordOk = cr >= 0 && cr < 12 && ct >= 0 && ct < harmony::numChordTypes;
    pubChord.store (chordOk ? cr * 16 + ct : -1, std::memory_order_relaxed);
    for (auto& h : pubHistory) h.store (-1, std::memory_order_relaxed);
    if (auto it = j.find ("history"); it != j.end() && it->is_array())
    {
        int n = 0;
        for (const auto& e : *it)
            if (n < historySize && e.is_array() && e.size() == 2 && e[0].is_number_integer() && e[1].is_number_integer())
            {
                const int r = e[0].get<int>(), t = e[1].get<int>();
                if (r >= 0 && r < 12 && t >= 0 && t < harmony::numChordTypes) pubHistory[(size_t) n++].store (r * 16 + t, std::memory_order_relaxed);
            }
    }
    bool any = chordOk;
    for (auto& w : pubWeight) w.store (0.0f, std::memory_order_relaxed);
    if (auto it = j.find ("profile"); it != j.end() && it->is_array() && it->size() == 12)
        for (int pc = 0; pc < 12; ++pc)
            if ((*it)[(size_t) pc].is_number())
            {
                const float v = (*it)[(size_t) pc].get<float>();
                const float w = std::isfinite (v) ? std::clamp (v, 0.0f, 1000.0f) : 0.0f;
                pubWeight[(size_t) pc].store (w, std::memory_order_relaxed);
                any |= w > 0.0f;
            }
    pubHeard.store (any, std::memory_order_relaxed);
    loaded.store (true, std::memory_order_release);
    return true;
}
} // namespace ohpe
