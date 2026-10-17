// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Metronome.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
void Metronome::registerParams (ParamRegistry& reg)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d) { d.group = "metronome"; reg.add (std::move (d)); };
    add (toggle (Ids::on, "Metronome", "Metronom", false));
    add (continuous (Ids::level, "Click Level", "Klick-Lautstärke", 0.0f, 1.0f, 0.6f, ParamUnit::Percent));
    add (toggle (Ids::accent, "Click Accent", "Klick-Betonung", true));
    add (choice (Ids::countIn, "Count-In", "Vorzähler", { "Off", "1 Bar", "2 Bars" }, { "Aus", "1 Takt", "2 Takte" }, 0));
    add (toggle (Ids::plugin, "Click in Plug-in", "Klick im Plug-in", false));
    auto tap = toggle (Ids::tap, "Tap Tempo", "Tempo tippen", false);
    tap.group = "remote";
    reg.add (std::move (tap));
}

void Metronome::bind (const ParamRegistry& reg)
{
    idxOn = reg.indexOf (Ids::on);
    idxLevel = reg.indexOf (Ids::level);
    idxAccent = reg.indexOf (Ids::accent);
    idxCountIn = reg.indexOf (Ids::countIn);
    idxPlugin = reg.indexOf (Ids::plugin);
}

void Metronome::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void Metronome::reset() noexcept
{
    phase = 0.0;
    amp = noiseAmp = noiseLp = attack = 0.0f;
    pendingCount = 0;
}

void Metronome::pull (const ParamSnapshot& s) noexcept
{
    if (idxOn >= 0) on = s.getBool (idxOn);
    if (idxLevel >= 0) level = std::clamp (s[idxLevel], 0.0f, 1.0f);
    if (idxAccent >= 0) accent = s.getBool (idxAccent);
    if (idxCountIn >= 0) countIn = std::clamp (s.getInt (idxCountIn), 0, 2);
    if (idxPlugin >= 0) plugin = s.getBool (idxPlugin);
}

void Metronome::trigger (int offset, bool downbeat) noexcept
{
    if (pendingCount >= (int) pending.size()) return;
    pending[(size_t) pendingCount++] = { std::max (0, offset), downbeat };
}

void Metronome::startClick (bool downbeat) noexcept
{
    const bool acc = downbeat && accent;
    // a woody "tick": 1.6 kHz (accent 2.4 kHz) falling from 1.5x within ~2 ms, ~25 ms decay; a short noise burst
    freqTarget = acc ? 2400.0f : 1600.0f;
    freq = freqTarget * 1.5f;
    freqCoef = (float) std::exp (-1.0 / (0.002 * sr));
    ampDecay = (float) std::exp (-1.0 / ((acc ? 0.032 : 0.024) * sr));
    noiseDecay = (float) std::exp (-1.0 / (0.0025 * sr));
    attackStep = (float) (1.0 / std::max (1.0, 0.0004 * sr)); // 0.4 ms rise: no click-in-the-click
    attack = 0.0f;
    amp = 1.0f;
    noiseAmp = 0.35f;
    phase = 0.0;
    const float g = level * level * 0.5f; // level 1 = -6 dBFS peak
    gain = acc ? g : g * 0.7f;
}

void Metronome::render (float* left, float* right, int numSamples) noexcept
{
    int next = 0;
    // triggers arrive in time order (the core schedules them in order); keep it robust anyway
    std::sort (pending.begin(), pending.begin() + pendingCount, [] (const Pending& a, const Pending& b) { return a.offset < b.offset; });
    const double twoPi = 6.283185307179586;
    for (int i = 0; i < numSamples; ++i)
    {
        while (next < pendingCount && pending[(size_t) next].offset <= i) startClick (pending[(size_t) next++].downbeat);
        if (amp < 1.0e-5f && noiseAmp < 1.0e-5f) continue;
        freq = freqTarget + (freq - freqTarget) * freqCoef;
        phase += twoPi * (double) freq / sr;
        if (phase > twoPi) phase -= twoPi;
        noiseState = noiseState * 1664525u + 1013904223u;
        const float white = (float) (int32_t) noiseState * (1.0f / 2147483648.0f);
        noiseLp += 0.45f * (white - noiseLp); // a soft, not hissy, transient
        attack = std::min (1.0f, attack + attackStep);
        const float s = gain * attack * (amp * (float) std::sin (phase) + noiseAmp * (white - noiseLp));
        amp *= ampDecay;
        noiseAmp *= noiseDecay;
        if (amp < 1.0e-5f) amp = 0.0f;
        if (noiseAmp < 1.0e-5f) noiseAmp = 0.0f;
        if (left != nullptr) left[i] += s;
        if (right != nullptr) right[i] += s;
    }
    // a trigger past the block end (should not happen) starts at the next block's first sample
    int keep = 0;
    for (int k = next; k < pendingCount; ++k)
        pending[(size_t) keep++] = { std::max (0, pending[(size_t) k].offset - numSamples), pending[(size_t) k].downbeat };
    pendingCount = keep;
}

// ---------------------------------------------------------------- tap tempo
double TapTempo::tap (double seconds) noexcept
{
    if (last >= 0.0 && (seconds - last > 2.0 || seconds <= last)) { count = 0; numIntervals = 0; next = 0; }
    if (count > 0)
    {
        intervals[(size_t) next] = seconds - last;
        next = (next + 1) % (int) intervals.size();
        numIntervals = std::min (numIntervals + 1, (int) intervals.size());
    }
    last = seconds;
    ++count;
    if (numIntervals == 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < numIntervals; ++i) sum += intervals[(size_t) i];
    const double avg = sum / numIntervals;
    return avg > 0.0 ? std::clamp (60.0 / avg, 20.0, 300.0) : 0.0;
}
} // namespace ohpe
