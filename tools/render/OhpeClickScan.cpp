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
#include "render/ClickDetect.h"

#include <algorithm>
#include <chrono>
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

using namespace clickscan;

namespace
{
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
    bool limiter = true, arp = false;
    double lead = 0.0, gap = 0.0;
    std::string filter;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--dir") dir = next();
        else if (a == "--rate") rate = std::stod (next());
        else if (a == "--filter") filter = next();
        else if (a == "--nolimit") limiter = false;
        else if (a == "--arp") arp = true;          // part presets keep their arpeggiator
        else if (a == "--lead") lead = std::stod (next()); // seconds of silence before the material (sleep / wake)
        else if (a == "--gap") gap = std::stod (next());   // seconds of silence before the last chord (sleep / wake)
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
        render::Audio reference; // the first block size's output: the others are compared with it (block-size invariance)
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
                opt.arp = arp;
                preset::applyPart (p, core->params(), 0, opt);
                usertable::partFromJson (*core, 0, p.userTables ? *p.userTables : std::string(),
                                         [] (const std::string&, const std::string&) { return usertable::UserTablePtr(); });
                core->params().set ("tempo", 100.0f);
                core->params().set ("mix.master.limit", limiter ? 1.0f : 0.0f);
                core->prepareEffects();
                events = partMaterial (rate, lead, gap);
                total = (int64_t) ((12.0 + lead + gap) * rate);
            }
            std::vector<int64_t> eventTimes, boundaries;
            for (const auto& e : events) eventTimes.push_back (e.at);
            const auto audio = run (*core, events, total, block, boundaries);
            // project presets: their sequencer notes are unknown here - every click is "free" (compared between builds)
            const auto r = detect (audio, eventTimes, boundaries, core->latencySamples());
            double dev = 0.0; // largest difference to the first block size's render
            if (reference.size() == 0) reference = audio;
            else
                for (size_t i = 0; i < std::min (reference.size(), audio.size()); ++i)
                    dev = std::max (dev, (double) std::max (std::abs (audio.l[i] - reference.l[i]), std::abs (audio.r[i] - reference.r[i])));
            if (! wavDir.empty()) writeWav (wavDir / (file.stem().string() + "_b" + std::to_string (block) + ".wav"), audio);
            std::printf ("{\"file\":\"%s\",\"kind\":\"%s\",\"engine\":\"%s\",\"block\":%d,\"free\":%d,\"event\":%d,\"holds\":%d,"
                         "\"boundary\":%d,\"nonfinite\":%d,\"worst\":%.1f,\"peak\":%.3f,\"dev\":%.2e,\"load\":%.3f,\"mean\":%.4f,\"times\":[",
                         file.stem().string().c_str(), project ? "project" : "part", p.meta.engine.c_str(), block, r.freeClicks,
                         r.eventClicks, r.holds, r.onBoundary, r.nonFinite, r.worstRatio, r.peak, dev, worstLoad, sumUs / (1.0e6 * (double) total / rate));
            for (size_t i = 0; i < r.freeTimes.size(); ++i) std::printf ("%s%.4f", i ? "," : "", r.freeTimes[i]);
            std::printf ("]}\n");
            std::fflush (stdout);
        }
    }
    return 0;
}
