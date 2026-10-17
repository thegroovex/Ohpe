// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 0.4 (package F): offline rendering of presets for the factory-sound quality loop and the tests.
// Header-only, JUCE-free: used by tools/render/OhpeRender.cpp (core-only command-line tool) and by
// the plug-in tests (every embedded factory preset, with the embedded factory samples).

#include "OhpeCore.h"
#include "preset/Preset.h"
#include "wavetable/UserTableState.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ohpe::render
{
/** One note of a test phrase: start and length in beats. */
struct Note
{
    double beat, length;
    int note, velocity;
};

struct Phrase
{
    std::vector<Note> notes;
    double beats = 8.0;   ///< length of the phrase
    double tail = 3.0;    ///< seconds rendered after the phrase (release, effects)
};

/** The preset's main category: its first category tag ("Keys" ...), "Keys" if it has none. */
inline std::string primaryCategory (const preset::Meta& m)
{
    for (const auto& t : m.tags)
        if (preset::isCategory (t)) return t;
    return "Keys";
}

/** A typical phrase for a category (C minor, 100 BPM): bass line, chords, lead line, groove ... */
inline Phrase phraseFor (const std::string& category)
{
    Phrase p;
    using V = std::vector<int>;
    auto chord = [&] (double beat, double len, const V& notes, int vel) {
        for (size_t i = 0; i < notes.size(); ++i) p.notes.push_back ({ beat, len, notes[i], vel - (int) i * 3 });
    };
    const V cm7 { 48, 55, 58, 63, 67 }, abmaj7 { 44, 51, 55, 60, 63 }, fm7 { 41, 48, 56, 60, 63 }, g { 43, 50, 55, 59, 62 };
    if (category == "Bass")
    {
        const Note line[] = { { 0, 0.45, 36, 112 }, { 0.5, 0.4, 36, 88 }, { 1, 0.22, 48, 100 }, { 1.5, 0.45, 36, 92 },
                              { 2, 0.45, 39, 106 }, { 2.5, 0.45, 41, 94 }, { 3, 0.9, 43, 110 }, { 4, 0.45, 32, 112 },
                              { 4.5, 0.4, 32, 88 }, { 5, 0.22, 44, 100 }, { 5.5, 0.45, 32, 92 }, { 6, 0.45, 34, 106 },
                              { 6.5, 0.45, 38, 94 }, { 7, 0.95, 31, 110 } };
        p.notes.assign (std::begin (line), std::end (line));
        p.tail = 1.5;
    }
    else if (category == "Pads" || category == "Textures")
    {
        chord (0, 3.9, V (cm7.begin() + 1, cm7.end()), 96);
        chord (4, 3.9, V (abmaj7.begin() + 1, abmaj7.end()), 92);
        p.notes.push_back ({ 0, 3.9, 36, 90 });
        p.notes.push_back ({ 4, 3.9, 32, 88 });
        p.tail = 5.0;
    }
    else if (category == "Leads")
    {
        const Note line[] = { { 0, 0.55, 67, 108 }, { 0.5, 0.55, 70, 96 }, { 1, 1.05, 72, 112 }, { 2, 0.55, 70, 98 },
                              { 2.5, 0.55, 67, 94 }, { 3, 1.0, 65, 104 }, { 4, 0.8, 63, 108 }, { 4.75, 0.3, 65, 90 },
                              { 5, 1.5, 67, 112 }, { 6.5, 0.55, 62, 96 }, { 7, 1.0, 60, 104 } };
        p.notes.assign (std::begin (line), std::end (line));
        p.tail = 2.5;
    }
    else if (category == "Plucks" || category == "Arp")
    {
        const bool arp = category == "Arp";
        const V up1 { 60, 63, 67, 70, 72, 70, 67, 63 }, up2 { 56, 60, 63, 67, 68, 67, 63, 60 };
        for (int i = 0; i < 32; ++i)
        {
            const auto& seq = i < 16 ? up1 : up2;
            const int n = seq[(size_t) (i % 8)] + (arp && (i / 8) % 2 == 1 ? 12 : 0);
            p.notes.push_back ({ i * 0.25, arp ? 0.14 : 0.22, n, (i % 4 == 0) ? 112 : 88 + (i * 7) % 16 });
        }
        p.tail = 3.0;
    }
    else if (category == "Drums")
    {
        for (int s = 0; s < 32; ++s)
        {
            const double b = s * 0.25;
            if (s % 8 == 0 || s == 22) p.notes.push_back ({ b, 0.2, 36, 118 });
            if (s % 8 == 4) p.notes.push_back ({ b, 0.2, 50, 110 });
            if (s % 2 == 1) p.notes.push_back ({ b, 0.1, 72, s % 4 == 3 ? 96 : 70 });
            if (s == 14 || s == 30) p.notes.push_back ({ b, 0.2, 43, 100 });
        }
        p.tail = 2.0;
    }
    else if (category == "FX")
    {
        p.notes = { { 0, 2.0, 60, 110 }, { 2.5, 1.5, 72, 100 }, { 4.5, 3.0, 48, 120 } };
        p.tail = 4.0;
    }
    else if (category == "Vocal")
    {
        const Note line[] = { { 0, 1.9, 60, 100 }, { 2, 1.4, 63, 104 }, { 3.5, 0.5, 65, 96 }, { 4, 1.9, 67, 108 },
                              { 6, 1.9, 63, 100 } };
        p.notes.assign (std::begin (line), std::end (line));
        chord (0, 3.9, V { 48, 55 }, 84);
        chord (4, 3.9, V { 44, 51 }, 84);
        p.tail = 3.0;
    }
    else // Keys
    {
        chord (0, 1.45, cm7, 100);
        chord (1.5, 0.45, V (cm7.begin() + 1, cm7.end()), 84);
        chord (2, 1.9, abmaj7, 96);
        chord (4, 1.45, fm7, 100);
        chord (5.5, 0.45, V (fm7.begin() + 1, fm7.end()), 84);
        chord (6, 1.9, g, 104);
        p.tail = 3.0;
    }
    return p;
}

struct Audio
{
    std::vector<float> l, r;
    double sampleRate = 48000.0;
    size_t size() const noexcept { return l.size(); }
};

/** Renders `numSamples` from the core in blocks, with the given MIDI events (absolute sample time). */
inline Audio renderCore (Core& core, std::vector<std::pair<int64_t, MidiEvent>> events, int64_t numSamples, int block = 256)
{
    Audio a;
    a.sampleRate = core.sampleRate();
    a.l.assign ((size_t) numSamples, 0.0f);
    a.r.assign ((size_t) numSamples, 0.0f);
    std::stable_sort (events.begin(), events.end(), [] (const auto& x, const auto& y) { return x.first < y.first; });
    std::vector<MidiEvent> blockEvents;
    size_t next = 0;
    for (int64_t pos = 0; pos < numSamples; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, numSamples - pos);
        blockEvents.clear();
        while (next < events.size() && events[next].first < pos + n)
        {
            auto e = events[next].second;
            e.sampleOffset = (int) std::max<int64_t> (0, events[next].first - pos);
            blockEvents.push_back (e);
            ++next;
        }
        OutputBuffers out;
        out.left[0] = a.l.data() + pos;
        out.right[0] = a.r.data() + pos;
        Transport tr;
        core.process (out, n, blockEvents.empty() ? nullptr : blockEvents.data(), (int) blockEvents.size(), tr);
    }
    return a;
}

/**
    Loads a part preset into part `partIndex` of a freshly prepared core and plays the phrase of
    its category on the part's MIDI channel. `beatsLimit` shortens the phrase (tests). The master
    limiter is switched off, so peaks show what the preset really does.
*/
/** A held note / chord for listening to movement: bass C2, lead G4, a C minor chord otherwise, held for `beats`. */
inline Phrase holdPhrase (const std::string& category, double beats)
{
    Phrase p;
    p.beats = beats;
    p.tail = 2.0;
    if (category == "Bass") p.notes = { { 0, beats, 36, 110 } };
    else if (category == "Leads") p.notes = { { 0, beats, 67, 110 } };
    else if (category == "Drums") p.notes = { { 0, beats, 36, 110 } };
    else
        for (int n : { 48, 55, 63, 67 }) p.notes.push_back ({ 0, beats, n, 100 });
    return p;
}

inline Audio renderPart (Core& core, const preset::Preset& p, int partIndex = 0, double bpm = 100.0, double beatsLimit = 1.0e9,
                         double tailLimit = 1.0e9, double holdSeconds = 0.0, const std::vector<int>& holdNotes = {},
                         int holdVelocity = 0, int modWheel = -1)
{
    preset::PartOptions opt;
    opt.arp = false; // measure the sound itself: the phrase plays the notes, not the arpeggiator
    preset::applyPart (p, core.params(), partIndex, opt);
    // p44: a preset's own user wavetable (embedded copy) plays too, as the plug-in's preset manager loads it
    usertable::partFromJson (core, partIndex, p.userTables ? *p.userTables : std::string(),
                             [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
    core.params().set ("tempo", (float) bpm);
    core.params().set ("mix.master.limit", 0.0f); // measure the preset itself, not the master limiter
    core.prepareEffects(); // rack slots 2-6 prepare their types lazily (the plugin does this on the message thread)
    auto phrase = holdSeconds > 0.0 ? holdPhrase (primaryCategory (p.meta), holdSeconds * bpm / 60.0)
                                    : phraseFor (primaryCategory (p.meta));
    if (holdSeconds > 0.0 && ! holdNotes.empty()) // the held note / chord of a click check (other pitches, velocities)
    {
        const int vel = holdVelocity > 0 ? holdVelocity : phrase.notes.front().velocity;
        phrase.notes.clear();
        for (int n : holdNotes) phrase.notes.push_back ({ 0, phrase.beats, n, vel });
    }
    const double sr = core.sampleRate();
    const double spb = 60.0 / bpm * sr;
    const int ch = partIndex + 1;
    std::vector<std::pair<int64_t, MidiEvent>> ev;
    if (modWheel >= 0) ev.push_back ({ 0, MidiEvent::controller (ch, 1, std::clamp (modWheel, 0, 127), 0) }); // p29: wheel position
    const double beats = std::min (phrase.beats, beatsLimit);
    for (const auto& n : phrase.notes)
    {
        if (n.beat >= beats) continue;
        const auto on = (int64_t) std::llround (n.beat * spb);
        const auto off = (int64_t) std::llround (std::min (n.beat + n.length, beats) * spb);
        ev.push_back ({ on, MidiEvent::noteOn (ch, n.note, n.velocity) });
        ev.push_back ({ std::max (on + 1, off), MidiEvent::noteOff (ch, n.note) });
    }
    const auto total = (int64_t) std::llround (beats * spb + std::min (phrase.tail, tailLimit) * sr);
    return renderCore (core, std::move (ev), total);
}

/** Loads a project preset and lets its sequencers run for `seconds` (limiter off: measurement). */
inline Audio renderProject (Core& core, const preset::Preset& p, double seconds, bool limiter = true, int modWheel = -1)
{
    preset::applyProject (p, core);
    if (! limiter) core.params().set ("mix.master.limit", 0.0f);
    core.prepareEffects(); // lazy rack slots, as the plugin's message thread does after a preset load
    core.params().set ("seq.run", 1.0f);
    std::vector<std::pair<int64_t, MidiEvent>> ev; // p29: the wheel on every MIDI channel (every part)
    if (modWheel >= 0)
        for (int ch = 1; ch <= 16; ++ch) ev.push_back ({ 0, MidiEvent::controller (ch, 1, std::clamp (modWheel, 0, 127), 0) });
    return renderCore (core, std::move (ev), (int64_t) std::llround (seconds * core.sampleRate()));
}

struct Metrics
{
    double rmsDb = -200.0;        ///< whole render
    double activeRmsDb = -200.0;  ///< mean power of the 50 ms windows within 30 dB of the loudest (level match)
    double peakDb = -200.0;
    double dc = 0.0;              ///< mean of the signal (both channels)
    double tailDb = -200.0;       ///< peak of the last 250 ms
    double hfDb = -200.0;         ///< energy above ~16 kHz relative to the total
    int nonFinite = 0;
    int denormals = 0;
};

inline double toDb (double x) { return x > 1.0e-10 ? 20.0 * std::log10 (x) : -200.0; }

inline Metrics measure (const Audio& a)
{
    Metrics m;
    const size_t n = a.size();
    if (n == 0) return m;
    double sum = 0.0, sumSq = 0.0, peak = 0.0;
    for (size_t i = 0; i < n; ++i)
        for (float x : { a.l[i], a.r[i] })
        {
            if (! std::isfinite (x)) { ++m.nonFinite; continue; }
            if (x != 0.0f && std::fpclassify (x) == FP_SUBNORMAL) ++m.denormals;
            sum += x;
            sumSq += (double) x * x;
            peak = std::max (peak, (double) std::abs (x));
        }
    m.dc = sum / (2.0 * (double) n);
    m.rmsDb = toDb (std::sqrt (sumSq / (2.0 * (double) n)));
    m.peakDb = toDb (peak);

    const size_t win = (size_t) (0.05 * a.sampleRate);
    std::vector<double> powers;
    for (size_t s = 0; s + win <= n; s += win)
    {
        double e = 0.0;
        for (size_t i = s; i < s + win; ++i)
            if (std::isfinite (a.l[i]) && std::isfinite (a.r[i])) e += (double) a.l[i] * a.l[i] + (double) a.r[i] * a.r[i];
        powers.push_back (e / (2.0 * (double) win));
    }
    if (! powers.empty())
    {
        const double loudest = *std::max_element (powers.begin(), powers.end());
        double e = 0.0;
        int count = 0;
        for (double p : powers)
            if (p > loudest * 1.0e-3) { e += p; ++count; }
        if (count > 0) m.activeRmsDb = 10.0 * std::log10 (std::max (1.0e-20, e / count));
    }

    const size_t tail = std::min (n, (size_t) (0.25 * a.sampleRate));
    double tp = 0.0;
    for (size_t i = n - tail; i < n; ++i) tp = std::max ({ tp, (double) std::abs (a.l[i]), (double) std::abs (a.r[i]) });
    m.tailDb = toDb (tp);

    // energy above ~16 kHz: 4th-order high-pass (two RBJ biquads) on the mid signal
    const double w0 = 2.0 * 3.14159265358979 * 16000.0 / a.sampleRate;
    const double cs = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.7071);
    const double b0 = (1.0 + cs) / 2.0, b1 = -(1.0 + cs), b2 = b0, a0 = 1.0 + alpha, a1 = -2.0 * cs, a2 = 1.0 - alpha;
    double z[2][4] = {};
    double hf = 0.0, all = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        double x = 0.5 * ((double) a.l[i] + (double) a.r[i]);
        if (! std::isfinite (x)) continue;
        all += x * x;
        for (auto& s : z)
        {
            const double y = (b0 * x + b1 * s[0] + b2 * s[1] - a1 * s[2] - a2 * s[3]) / a0;
            s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
            x = y;
        }
        hf += x * x;
    }
    m.hfDb = all > 0.0 ? 10.0 * std::log10 (std::max (1.0e-20, hf / all)) : -200.0;
    return m;
}
} // namespace ohpe::render
