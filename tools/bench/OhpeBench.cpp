// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// ohpe_bench: how much of one CPU core the instrument needs for real time.
//
// Renders typical and worst-case patches with the core alone (no plug-in, no UI) and prints, per scenario, the
// average and the slowest block as a percentage of one core (100 % = exactly real time on this machine).
// Lower is better. The numbers depend on the machine and on what else runs on it: compare runs on the same
// machine only. CI prints them for information; nothing gates on timing.
//
//   ohpe_bench                  all scenarios, 48 kHz, block 256, 2 s each (best of 3 runs)
//   ohpe_bench --quick          shorter runs (CI)
//   ohpe_bench --block 64       another block size
//   ohpe_bench --rate 96000     another sample rate
//   ohpe_bench --seconds 5      longer runs
//   ohpe_bench --filter fm      only scenarios whose name contains "fm"
//   ohpe_bench --csv            machine-readable output (name;avg;peak)
//   ohpe_bench --list           scenario names only
//
// Exit code 0, or 2 when a scenario produced a non-finite or runaway output (a correctness check, not a timing one).

#include "OhpeCore.h"
#include "engines/EngineFactory.h"
#include "fx/FxRack.h"
#include "fx/InsertSlot.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace ohpe;
using Clock = std::chrono::steady_clock;

namespace
{
struct Options
{
    double rate = 48000.0;
    int block = 256;
    double seconds = 2.0;
    int repeats = 3;
    std::string filter;
    bool csv = false, list = false;
};

struct Scenario
{
    std::string name, description;
    std::function<void (Core&)> setup; ///< parameters, corners ... (after prepare)
    int notes = 0;                     ///< chord notes held on channel 1 (every part listening there plays them)
};

void setParam (Core& core, const std::string& id, float v)
{
    if (! core.params().set (id, v)) { std::fprintf (stderr, "ohpe_bench: unknown parameter %s\n", id.c_str()); std::exit (1); }
}

std::string partPrefix (int p) { return "p" + std::to_string (p + 1) + "."; }

/** Every part on `engine` with `voices` voices on MIDI channel 1. */
void parts (Core& core, EngineType engine, int voices, int numParts = Core::numParts)
{
    for (int p = 0; p < numParts; ++p)
    {
        const auto pp = partPrefix (p);
        setParam (core, pp + "engine", (float) engine);
        setParam (core, pp + "voices", (float) voices);
        setParam (core, pp + "midich", 1.0f);
    }
    for (int p = numParts; p < Core::numParts; ++p) setParam (core, partPrefix (p) + "midich", (float) (p + 2)); // nobody plays them
}

/** The first `count` rack slots of part `p`: a typical chain (modulation, drive, filter, delay, reverb ...). */
void rack (Core& core, int p, int count)
{
    static const int types[FxRack::numPartSlots] = { InsertSlot::Chorus, InsertSlot::Reverb, InsertSlot::FilterDrive,
                                                     InsertSlot::Phaser, InsertSlot::Echo,   InsertSlot::Hall };
    for (int k = 0; k < count && k < FxRack::numPartSlots; ++k)
        setParam (core, FxRack::partSlotPrefix (partPrefix (p), k) + "type", (float) types[k]);
}

/** A layer engine under every part (the next engine type, so it never equals the main one). */
void layer (Core& core, EngineType main)
{
    const int lt = ((int) main + 1) % numEngineTypes;
    for (int p = 0; p < Core::numParts; ++p) setParam (core, partPrefix (p) + "layer.engine", (float) (lt + 1)); // 0 = off
}

/** Four corners on four different engines per part, pad in the middle: four lanes (four Part instances) per part. */
void morphEverywhere (Core& core)
{
    const EngineType corners[4] = { EngineType::Wavetable, EngineType::Analog, EngineType::FM, EngineType::Pluck };
    for (int p = 0; p < Core::numParts; ++p)
    {
        const auto pp = partPrefix (p);
        for (int c = 0; c < 4; ++c)
        {
            setParam (core, pp + "engine", (float) corners[c]);
            core.morph().captureCorner (p, c, std::string (1, (char) ('A' + c)));
        }
        setParam (core, pp + "morph.on", 1.0f);
        setParam (core, pp + "morph.x", 0.5f);
        setParam (core, pp + "morph.y", 0.5f);
    }
}

std::vector<Scenario> scenarios()
{
    std::vector<Scenario> s;
    s.push_back ({ "idle", "defaults, no notes", [] (Core&) {}, 0 });
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        const std::string id = engineTypeInfo (type).id;
        s.push_back ({ "typical." + id, "4 parts x 8 voices, 2 FX per part",
                       [type] (Core& c) { parts (c, type, 8); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 2); }, 8 });
    }
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        const std::string id = engineTypeInfo (type).id;
        s.push_back ({ "worst." + id, "4 parts x 16 voices + layer, 6 FX per part",
                       [type] (Core& c) { parts (c, type, 16); layer (c, type); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); }, 16 });
    }
    s.push_back ({ "one-part.fm", "FM 16 voices + 6 FX on part 1; parts 2-4 silent with 6 FX each",
                   [] (Core& c) { parts (c, EngineType::FM, 16, 1); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); }, 16 });
    s.push_back ({ "silent-racks", "no notes; 6 FX on every part (tails long gone)",
                   [] (Core& c) { for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); }, 0 });
    s.push_back ({ "morph.4x4", "4 parts x 8 voices, morph on: 4 lanes per part (16 parts' worth), 2 FX",
                   [] (Core& c) { parts (c, EngineType::Wavetable, 8); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 2); morphEverywhere (c); }, 8 });
    return s;
}

struct Result
{
    double avg = 0.0, peak = 0.0; ///< percent of one core
    bool valid = true;
};

Result run (const Scenario& sc, const Options& o)
{
    Core core;
    core.prepare (o.rate, o.block);
    sc.setup (core);
    core.prepareEffects();
    core.updateMorph();
    for (int n = 0; n < sc.notes; ++n) core.queueNote (40 + (n * 7) % 36 + n / 6, 0.8f, true);

    std::vector<float> l ((size_t) o.block), r ((size_t) o.block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    const double blockSeconds = o.block / o.rate;
    // warm up past the attacks (and, for silent racks, past every tail so the steady state is measured)
    const double warm = sc.notes == 0 && sc.name != "idle" ? 6.0 : 0.5;
    for (int b = 0; b < (int) (warm / blockSeconds); ++b) core.process (out, o.block, nullptr, 0, Transport {});

    Result best { 1.0e9, 1.0e9, true };
    const int blocks = std::max (1, (int) (o.seconds / blockSeconds));
    for (int rep = 0; rep < o.repeats; ++rep)
    {
        double total = 0.0, worst = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            const auto t0 = Clock::now();
            core.process (out, o.block, nullptr, 0, Transport {});
            const double dt = std::chrono::duration<double> (Clock::now() - t0).count();
            total += dt;
            worst = std::max (worst, dt);
            for (int i = 0; i < o.block; ++i)
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i]) || std::abs (l[(size_t) i]) > 64.0f) best.valid = false;
        }
        best.avg = std::min (best.avg, 100.0 * total / (blocks * blockSeconds));
        best.peak = std::min (best.peak, 100.0 * worst / blockSeconds);
    }
    return best;
}
} // namespace

int main (int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };
        if (a == "--quick") { o.seconds = 0.5; o.repeats = 2; }
        else if (a == "--block") o.block = std::clamp (std::atoi (next().c_str()), 16, Core::maxChunk * 8);
        else if (a == "--rate") o.rate = std::clamp (std::atof (next().c_str()), 22050.0, 192000.0);
        else if (a == "--seconds") o.seconds = std::max (0.05, std::atof (next().c_str()));
        else if (a == "--repeats") o.repeats = std::max (1, std::atoi (next().c_str()));
        else if (a == "--filter") o.filter = next();
        else if (a == "--csv") o.csv = true;
        else if (a == "--list") o.list = true;
        else
        {
            std::printf ("usage: ohpe_bench [--quick] [--block N] [--rate HZ] [--seconds S] [--repeats N] [--filter TEXT] [--csv] [--list]\n");
            return a == "--help" || a == "-h" ? 0 : 1;
        }
    }

    const auto all = scenarios();
    if (o.list)
    {
        for (const auto& s : all) std::printf ("%-20s %s\n", s.name.c_str(), s.description.c_str());
        return 0;
    }
    if (o.csv) std::printf ("scenario;avg_percent;peak_percent\n");
    else
        std::printf ("Ohpe bench: %.0f Hz, block %d, %.1f s x %d (best run). Percent of one CPU core for real time - lower is better.\n"
                     "%-18s %8s %8s   %s\n", o.rate, o.block, o.seconds, o.repeats, "scenario", "avg %", "peak %", "patch");
    // start-up: building a Core (four parts with every engine) - plug-in load time, morph lanes are Parts too
    if (o.filter.empty() || std::string ("startup").find (o.filter) != std::string::npos)
    {
        double ms[2] {};
        for (double& m : ms)
        {
            const auto t0 = Clock::now();
            auto core = std::make_unique<Core>();
            core->prepare (o.rate, o.block);
            m = std::chrono::duration<double, std::milli> (Clock::now() - t0).count();
        }
        if (o.csv) std::printf ("startup.first-ms;%.1f;%.1f\n", ms[0], ms[1]);
        else std::printf ("%-18s %8.0f %8.0f   %s\n", "startup ms", ms[0], ms[1], "build + prepare a Core: first / second instance (milliseconds, not %)");
    }
    bool ok = true;
    double sum = 0.0;
    int count = 0;
    for (const auto& s : all)
    {
        if (! o.filter.empty() && s.name.find (o.filter) == std::string::npos) continue;
        const auto r = run (s, o);
        ok = ok && r.valid;
        sum += r.avg;
        ++count;
        if (o.csv) std::printf ("%s;%.3f;%.3f%s\n", s.name.c_str(), r.avg, r.peak, r.valid ? "" : ";INVALID");
        else std::printf ("%-18s %8.2f %8.2f   %s%s\n", s.name.c_str(), r.avg, r.peak, s.description.c_str(), r.valid ? "" : "  << INVALID OUTPUT");
        std::fflush (stdout);
    }
    if (! o.csv && count > 0) std::printf ("%-18s %8.2f\n", "sum", sum);
    return ok ? 0 : 2;
}
