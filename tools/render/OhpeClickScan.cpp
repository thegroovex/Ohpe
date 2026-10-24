// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// ohpe_clickscan (p85): plays every factory preset with realistic material at several host block sizes and looks
// for clicks / crackle. Core-only (built-in sample fallbacks). One JSON object per preset and block size on stdout.
//
//   ohpe_clickscan [--dir resources/presets/factory] [--blocks 64,128,256,512,var] [--rate 48000] [--filter TEXT]
//                  [--nolimit] [--wav DIR] [--seconds S]
//
// Material (part presets, part 1, master limiter on as heard): a held single note with release, a held 4-note chord
// with release, a legato line, three velocities, a loud chord. Project presets run their sequencers.
// Detector: second difference (steps and kinks) in 16-sample windows against the median of the windows around it
// (+-13 ms); a window more than 8x (18 dB) above its surroundings and above an absolute floor is a click. Clicks within
// -1..+8 ms of a MIDI event count as "at event" (attacks / releases), all others as "free" (crackle). Exactly repeated
// samples (>= 24 in a row inside a signal above -60 dBFS) count as "holds" (dropouts).

#include "OhpeCore.h"
#include "preset/Preset.h"
#include "render/PresetRender.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ohpe;

namespace
{
struct Ev
{
    int64_t at;
    MidiEvent e;
};

std::vector<Ev> partMaterial (double sr)
{
    std::vector<Ev> ev;
    auto s = [&] (double t) { return (int64_t) std::llround (t * sr); };
    auto note = [&] (double on, double off, int n, int v) {
        ev.push_back ({ s (on), MidiEvent::noteOn (1, n, v) });
        ev.push_back ({ s (off), MidiEvent::noteOff (1, n) });
    };
    note (0.00, 1.20, 60, 100);
    for (int n : { 48, 55, 63, 67 }) note (1.80, 3.60, n, 90);
    note (4.20, 4.55, 60, 96); note (4.50, 4.85, 62, 96); note (4.80, 5.15, 63, 96); note (5.10, 5.45, 65, 96); note (5.40, 6.20, 67, 96);
    note (6.60, 6.85, 72, 30); note (7.00, 7.25, 72, 70); note (7.40, 7.65, 72, 127);
    for (int n : { 53, 60, 65, 68 }) note (7.90, 9.40, n, 110);
    std::stable_sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    return ev;
}

struct Rng
{
    uint32_t s = 12345u;
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
};

/** Renders with a fixed block size (block > 0) or the variable host pattern (block == 0). */
render::Audio run (Core& core, const std::vector<Ev>& events, int64_t total, int block, std::vector<int64_t>& boundaries)
{
    render::Audio a;
    a.sampleRate = core.sampleRate();
    a.l.assign ((size_t) total, 0.0f);
    a.r.assign ((size_t) total, 0.0f);
    std::vector<MidiEvent> be;
    size_t next = 0;
    Rng rng;
    for (int64_t pos = 0; pos < total;)
    {
        int b = block;
        if (b <= 0)
        {
            const uint32_t r = rng.next() % 100;
            b = r < 55 ? 512 : r < 75 ? 16 + (int) (rng.next() % 497) : r < 90 ? 128 * (1 + (int) (rng.next() % 4)) : 1 + (int) (rng.next() % 31);
        }
        const int n = (int) std::min<int64_t> (b, total - pos);
        be.clear();
        while (next < events.size() && events[next].at < pos + n)
        {
            auto e = events[next].e;
            e.sampleOffset = (int) std::max<int64_t> (0, events[next].at - pos);
            be.push_back (e);
            ++next;
        }
        OutputBuffers out;
        out.left[0] = a.l.data() + pos;
        out.right[0] = a.r.data() + pos;
        Transport tr;
        core.process (out, n, be.empty() ? nullptr : be.data(), (int) be.size(), tr);
        pos += n;
        boundaries.push_back (pos);
    }
    return a;
}

struct Result
{
    int freeClicks = 0, eventClicks = 0, holds = 0, onBoundary = 0, nonFinite = 0;
    double worstRatio = 0.0, firstFree = -1.0, peak = 0.0;
    std::vector<double> freeTimes;
};

Result detect (const render::Audio& a, const std::vector<int64_t>& eventTimes, const std::vector<int64_t>& boundaries, int latency)
{
    Result r;
    constexpr int W = 16, R = 40;
    const size_t n = a.size();
    const size_t nw = n / W;
    std::vector<char> nearEvent (nw, 0), nearBoundary (nw, 0);
    for (auto t : eventTimes)
    {
        const int64_t from = t + latency - (int64_t) (0.001 * a.sampleRate), to = t + latency + (int64_t) (0.008 * a.sampleRate);
        for (int64_t k = std::max<int64_t> (0, from / W); k <= to / W && k < (int64_t) nw; ++k) nearEvent[(size_t) k] = 1;
    }
    for (auto b : boundaries)
        for (int64_t k = (b - 2) / W; k <= (b + 2) / W; ++k)
            if (k >= 0 && k < (int64_t) nw) nearBoundary[(size_t) k] = 1;
    for (const auto* ch : { &a.l, &a.r })
    {
        const auto& x = *ch;
        std::vector<float> p (nw, 0.0f);
        for (size_t i = 2; i < n; ++i)
        {
            if (! std::isfinite (x[i])) { ++r.nonFinite; continue; }
            r.peak = std::max (r.peak, (double) std::abs (x[i]));
            const float h = std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]);
            auto& w = p[i / W < nw ? i / W : nw - 1];
            w = std::max (w, h);
        }
        std::vector<float> tmp;
        for (size_t k = 0; k < nw; ++k)
        {
            tmp.clear();
            for (int d = -R; d <= R; ++d)
            {
                if (std::abs (d) <= 2) continue;
                const int64_t j = (int64_t) k + d;
                if (j >= 0 && j < (int64_t) nw) tmp.push_back (p[(size_t) j]);
            }
            if (tmp.empty()) continue;
            std::nth_element (tmp.begin(), tmp.begin() + (long) tmp.size() / 2, tmp.end());
            const float bg = tmp[tmp.size() / 2];
            if (p[k] > 8.0f * bg && p[k] > 2.0e-3f)
            {
                const double ratio = p[k] / std::max (bg, 1.0e-7f);
                if (nearEvent[k]) { ++r.eventClicks; continue; }
                ++r.freeClicks;
                if (nearBoundary[k]) ++r.onBoundary;
                r.worstRatio = std::max (r.worstRatio, ratio);
                if (r.freeTimes.size() < 8) r.freeTimes.push_back ((double) (k * W) / a.sampleRate);
            }
        }
        // repeated samples inside a signal (dropouts / held buffers)
        int run = 0;
        for (size_t i = 1; i < n; ++i)
        {
            if (x[i] == x[i - 1] && x[i] != 0.0f) ++run;
            else
            {
                if (run >= 24) ++r.holds;
                run = 0;
            }
        }
    }
    if (! r.freeTimes.empty()) r.firstFree = r.freeTimes.front();
    return r;
}

void writeWav (const fs::path& path, const render::Audio& a)
{
    std::ofstream f (path, std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t n = (uint32_t) a.size();
    f.write ("RIFF", 4); w32 (36 + n * 8); f.write ("WAVEfmt ", 8); w32 (16); w16 (3); w16 (2);
    w32 ((uint32_t) a.sampleRate); w32 ((uint32_t) a.sampleRate * 8); w16 (8); w16 (32); f.write ("data", 4); w32 (n * 8);
    for (uint32_t i = 0; i < n; ++i) { f.write ((const char*) &a.l[i], 4); f.write ((const char*) &a.r[i], 4); }
}
} // namespace

int main (int argc, char** argv)
{
    fs::path dir = "resources/presets/factory", wavDir;
    std::vector<int> blocks { 64, 128, 256, 512, 0 };
    double rate = 48000.0, projectSeconds = 10.0;
    bool limiter = true;
    std::string filter;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--dir") dir = next();
        else if (a == "--rate") rate = std::stod (next());
        else if (a == "--filter") filter = next();
        else if (a == "--nolimit") limiter = false;
        else if (a == "--wav") wavDir = next();
        else if (a == "--seconds") projectSeconds = std::stod (next());
        else if (a == "--blocks")
        {
            blocks.clear();
            std::stringstream ss (next());
            for (std::string t; std::getline (ss, t, ',');) blocks.push_back (t == "var" ? 0 : std::stoi (t));
        }
    }
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator (dir))
        if (e.path().extension() == ".ohpepreset" && (filter.empty() || e.path().filename().string().find (filter) != std::string::npos))
            files.push_back (e.path());
    std::sort (files.begin(), files.end());
    if (! wavDir.empty()) fs::create_directories (wavDir);

    for (const auto& file : files)
    {
        std::ifstream f (file);
        std::stringstream ss;
        ss << f.rdbuf();
        preset::Preset p;
        if (! preset::fromJson (ss.str(), p)) { std::cerr << file << ": cannot read\n"; continue; }
        const bool project = p.meta.kind == preset::Kind::Project;
        for (int block : blocks)
        {
            auto core = std::make_unique<Core>();
            core->prepare (rate, 512);
            std::vector<Ev> events;
            int64_t total;
            if (project)
            {
                preset::applyProject (p, *core);
                if (! limiter) core->params().set ("mix.master.limit", 0.0f);
                core->prepareEffects();
                core->params().set ("seq.run", 1.0f);
                total = (int64_t) (projectSeconds * rate);
            }
            else
            {
                preset::PartOptions opt;
                opt.arp = false;
                preset::applyPart (p, core->params(), 0, opt);
                usertable::partFromJson (*core, 0, p.userTables ? *p.userTables : std::string(),
                                         [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
                core->params().set ("tempo", 100.0f);
                core->params().set ("mix.master.limit", limiter ? 1.0f : 0.0f);
                core->prepareEffects();
                events = partMaterial (rate);
                total = (int64_t) (12.0 * rate);
            }
            std::vector<int64_t> eventTimes, boundaries;
            for (const auto& e : events) eventTimes.push_back (e.at);
            const auto audio = run (*core, events, total, block, boundaries);
            // project presets: their sequencer notes are unknown here - every click is "free" (compared between builds)
            const auto r = detect (audio, eventTimes, boundaries, core->latencySamples());
            if (! wavDir.empty()) writeWav (wavDir / (file.stem().string() + "_b" + std::to_string (block) + ".wav"), audio);
            std::printf ("{\"file\":\"%s\",\"kind\":\"%s\",\"engine\":\"%s\",\"block\":%d,\"free\":%d,\"event\":%d,\"holds\":%d,"
                         "\"boundary\":%d,\"nonfinite\":%d,\"worst\":%.1f,\"peak\":%.3f,\"times\":[",
                         file.stem().string().c_str(), project ? "project" : "part", p.meta.engine.c_str(), block, r.freeClicks,
                         r.eventClicks, r.holds, r.onBoundary, r.nonFinite, r.worstRatio, r.peak);
            for (size_t i = 0; i < r.freeTimes.size(); ++i) std::printf ("%s%.4f", i ? "," : "", r.freeTimes[i]);
            std::printf ("]}\n");
            std::fflush (stdout);
        }
    }
    return 0;
}
