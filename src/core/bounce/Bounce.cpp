// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Bounce.h"

#include "../OhpeCore.h"
#include "../engines/GrainEngine.h"
#include "../engines/SamplerEngine.h"
#include "../model/ParamRegistry.h"
#include "../tape/TapeMachine.h"

#include <cmath>

namespace ohpe::bounce
{
#if defined(_MSC_VER)
__declspec (noinline)
#else
__attribute__ ((noinline))
#endif
void registerParams (ParamRegistry& reg)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d, const char* group) { d.group = group; reg.add (std::move (d)); };
    add (choice (Core::Ids::bounceSrc, "Bounce Source", "Bounce-Quelle",
                 { "Edit Track", "Tape 1", "Tape 2", "Tape 3", "Tape 4", "Tape 5", "Tape 6", "Tape 7", "Tape 8", "Tape Mix", "Input" },
                 { "Bearbeitungsspur", "Tape 1", "Tape 2", "Tape 3", "Tape 4", "Tape 5", "Tape 6", "Tape 7", "Tape 8", "Tape-Mix", "Eingang" }, 0),
         "bounce");
    add (choice (Core::Ids::bounceTo, "Bounce Target", "Bounce-Ziel", { "Grain", "Sampler" }, { "Grain", "Sampler" }, 0), "bounce");
    add (toggle (Core::Ids::bounceLoop, "Endless Loop", "Endlosschleife", true), "bounce");
    add (choice (Core::Ids::bounceLen, "Input Length", "Eingangslänge", { "1 Bar", "2 Bars", "4 Bars", "8 Bars" },
                 { "1 Takt", "2 Takte", "4 Takte", "8 Takte" }, 2),
         "bounce");
    add (toggle (Core::Ids::bounceGo, "Bounce", "Bounce", false), "remote");
}

std::string sourceName (int source, int editTrack)
{
    if (source == SourceEditTrack) source = SourceTrack1 + std::clamp (editTrack, 0, TapeMachine::numTracks - 1);
    if (source >= SourceTrack1 && source < SourceMix) return "Tape " + std::to_string (source - SourceTrack1 + 1);
    if (source == SourceMix) return "Tape Mix";
    return "Input";
}

double lengthSeconds (int length, double bpm)
{
    const int bars = 1 << std::clamp (length, 0, (int) numLengths - 1);
    const double b = std::clamp (bpm, 20.0, 400.0);
    return std::clamp (bars * 4.0 * 60.0 / b, 0.5, maxSeconds);
}

Audio readTape (const TapeMachine& tape, int track, int64_t start, int64_t end)
{
    Audio a;
    a.rate = tape.sampleRate() > 0.0 ? tape.sampleRate() : 48000.0;
    if (track >= TapeMachine::numTracks) return a;
    if (end <= start)
    {
        start = 0;
        end = track < 0 ? tape.contentEndFrames() : tape.contentEndFrames (track);
    }
    start = std::max<int64_t> (0, start);
    end = std::min<int64_t> (end, start + (int64_t) (maxSeconds * a.rate));
    if (end <= start) return a;
    const auto n = (size_t) (end - start);
    a.l.assign (n, 0.0f);
    a.r.assign (n, 0.0f);
    constexpr int chunk = 4096;
    std::vector<float> l (chunk), r (chunk);
    for (int t = track < 0 ? 0 : track; t < (track < 0 ? TapeMachine::numTracks : track + 1); ++t)
    {
        if (track < 0 && tape.contentEndFrames (t) <= 0) continue;
        for (size_t pos = 0; pos < n; pos += chunk)
        {
            const int k = (int) std::min<size_t> (chunk, n - pos);
            tape.readCommitted (t, start + (int64_t) pos, k, l.data(), r.data());
            for (int i = 0; i < k; ++i)
            {
                a.l[pos + (size_t) i] += l[(size_t) i];
                a.r[pos + (size_t) i] += r[(size_t) i];
            }
        }
    }
    return a;
}

float peakOf (const Audio& a) noexcept
{
    float p = 0.0f;
    for (const auto* ch : { &a.l, &a.r })
        for (float x : *ch)
            if (std::isfinite (x)) p = std::max (p, std::abs (x));
    return p;
}

bool isSilent (const Audio& a) noexcept { return peakOf (a) < silenceLevel; }

float normalise (Audio& a) noexcept
{
    for (auto* ch : { &a.l, &a.r })
        for (float& x : *ch)
            if (! std::isfinite (x)) x = 0.0f;
    const float p = peakOf (a);
    if (p < silenceLevel) return 1.0f;
    const float g = std::min (maxGain, targetPeak / p);
    for (auto* ch : { &a.l, &a.r })
        for (float& x : *ch) x *= g;
    return g;
}

void trimSilence (Audio& a)
{
    const int64_t n = a.frames();
    auto loud = [&] (int64_t i) { return std::abs (a.l[(size_t) i]) >= silenceLevel || std::abs (a.r[(size_t) i]) >= silenceLevel; };
    int64_t first = 0, last = n - 1;
    while (first < n && ! loud (first)) ++first;
    while (last > first && ! loud (last)) --last;
    if (first >= n) { a.l.clear(); a.r.clear(); return; }
    first = std::max<int64_t> (0, first - (int64_t) (0.005 * a.rate));
    last = std::min<int64_t> (n - 1, last + (int64_t) (0.005 * a.rate));
    a.l = std::vector<float> (a.l.begin() + first, a.l.begin() + last + 1);
    a.r = std::vector<float> (a.r.begin() + first, a.r.begin() + last + 1);
}

void fadeEdges (Audio& a, double seconds) noexcept
{
    const int64_t n = a.frames();
    const int64_t f = std::min<int64_t> ((int64_t) (seconds * a.rate), n / 2);
    for (int64_t i = 0; i < f; ++i)
    {
        const float g = (float) i / (float) f;
        a.l[(size_t) i] *= g;
        a.r[(size_t) i] *= g;
        a.l[(size_t) (n - 1 - i)] *= g;
        a.r[(size_t) (n - 1 - i)] *= g;
    }
}

int64_t makeEndless (Audio& a)
{
    const int64_t n = a.frames();
    const int64_t x = std::min<int64_t> ((int64_t) (loopFadeSeconds * a.rate), n / 4);
    if (x < 2) return 0;
    const int64_t len = n - x;
    // the tail [len, n) continues the material past the loop's end; it fades out while the head fades in,
    // so the loop's last frame (len - 1) flows into its first frame (the tail's start) without a seam
    for (int64_t i = 0; i < x; ++i)
    {
        const double t = ((double) i + 0.5) / (double) x;
        const auto in = (float) std::sin (t * 1.5707963267948966), out = (float) std::cos (t * 1.5707963267948966);
        for (auto* ch : { &a.l, &a.r })
            (*ch)[(size_t) i] = (*ch)[(size_t) i] * in + (*ch)[(size_t) (len + i)] * out;
    }
    a.l.resize ((size_t) len);
    a.r.resize ((size_t) len);
    return x;
}

SamplePtr prepare (Audio a, const Options& o, const std::string& path, const std::string& name)
{
    if (a.l.size() != a.r.size()) a.r.resize (a.l.size(), 0.0f);
    for (auto* ch : { &a.l, &a.r })
        for (float& x : *ch)
            if (! std::isfinite (x)) x = 0.0f;
    if (isSilent (a) && ! o.normalise) return nullptr;
    // the seam cross-fade first (it sums two parts of the material), then the level
    const bool loop = o.endless && makeEndless (a) > 0;
    if (o.normalise) normalise (a);
    if (isSilent (a)) return nullptr;
    if (! loop) trimSilence (a);
    if (a.frames() < 64) return nullptr;
    int64_t loopStart = -1, loopEnd = -1;
    if (loop)
    {
        // guard frames: the loop's last frame before it, its first three frames after it
        const auto L = a.l.size();
        for (auto* ch : { &a.l, &a.r })
        {
            std::vector<float> g;
            g.reserve (L + 4);
            g.push_back ((*ch)[L - 1]);
            g.insert (g.end(), ch->begin(), ch->end());
            for (size_t i = 0; i < 3; ++i) g.push_back ((*ch)[i]);
            *ch = std::move (g);
        }
        loopStart = 1;
        loopEnd = 1 + (int64_t) L;
    }
    else
        fadeEdges (a);

    std::vector<std::vector<float>> ch;
    const bool mono = a.l == a.r;
    ch.push_back (std::move (a.l));
    if (! mono) ch.push_back (std::move (a.r));
    auto s = std::make_shared<SampleData> (std::move (ch), a.rate, path, name);
    if (loopStart >= 0)
    {
        s->fileLoopStart = loopStart;
        s->fileLoopEnd = loopEnd;
        s->endless = sample::hasEndlessLayout (loopStart, loopEnd, s->numFrames());
    }
    return s;
}

bool apply (Core& core, int part, Target target, SamplePtr s, const Setter& set)
{
    if (part < 0 || part >= Core::numParts || s == nullptr || ! set) return false;
    const auto p = "p" + std::to_string (part + 1) + ".";
    const bool endless = s->endless;
    if (target == TargetSampler)
    {
        auto* e = dynamic_cast<SamplerEngine*> (core.part (part).engine (EngineType::Sampler));
        if (e == nullptr) return false;
        set (p + "engine", (float) EngineType::Sampler);
        set (p + "sampler.mode", (float) sampler::ModeZones);
        set (p + "sampler.start", 0.0f);
        set (p + "sampler.reverse", 0.0f);
        set (p + "sampler.loop", endless ? (float) sampler::LoopForward : (float) sampler::LoopOff);
        e->setSample (std::move (s), 60); // C4 plays the bounce at its original pitch
    }
    else
    {
        auto* e = dynamic_cast<GrainEngine*> (core.part (part).engine (EngineType::Grain));
        if (e == nullptr) return false;
        set (p + "engine", (float) EngineType::Grain);
        set (p + "grain.source", (float) grain::SourceSample);
        e->setSample (std::move (s));
    }
    return true;
}
} // namespace ohpe::bounce
