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
//   ohpe_bench --csv            machine-readable output (name;avg;peak;rtf_avg;rtf_worst_block)
//   ohpe_bench --list           scenario names only
//   ohpe_bench --hash           p79: also render every scenario timing-free and print a hash of its output (bit-identity
//                               check of an optimisation: same machine and build settings)
//   ohpe_bench --write FILE     p79: also write the --csv lines to FILE (a new baseline)
//   ohpe_bench --baseline FILE  p79: compare the averages with an earlier --csv run (tools/bench/baseline-<os>.csv)
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
#include <cstdint>
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
    bool csv = false, list = false, hash = false;
    std::string baseline; ///< p79: compare with an earlier --csv run
    std::string write;    ///< p79: also write the --csv lines to this file (a new baseline)
};

struct Scenario
{
    std::string name, description;
    std::function<void (Core&)> setup; ///< parameters, corners ... (after prepare)
    int notes = 0;                     ///< chord notes held on channel 1 (every part listening there plays them)
    /** p79: MIDI per block (drum patterns, a chord that is released ...); `block` counts from the first warm-up block. */
    std::function<void (long block, int blockSamples, double rate, std::vector<MidiEvent>&)> events;
    double warmSeconds = -1.0; ///< p79: warm-up before measuring (-1: 0.5 s, or 6 s for silent scenarios)
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

/** p79: all four master rack slots, plus both mixer sends of the four part channels (send delay / send reverb). */
void masterRack (Core& core)
{
    static const int types[FxRack::numMasterSlots] = { InsertSlot::Chorus, InsertSlot::FilterDrive, InsertSlot::Echo, InsertSlot::Hall };
    for (int k = 0; k < FxRack::numMasterSlots; ++k) setParam (core, FxRack::masterSlotPrefix (k) + "type", (float) types[k]);
    for (int c = 1; c <= Core::numParts; ++c)
    {
        setParam (core, "mix.ch" + std::to_string (c) + ".senda", 0.3f);
        setParam (core, "mix.ch" + std::to_string (c) + ".sendb", 0.3f);
    }
}

/** p79: the 3D stage on every part, moving on a path with room. */
void space3d (Core& core, int numParts = Core::numParts)
{
    for (int p = 0; p < numParts; ++p)
    {
        const auto pp = partPrefix (p);
        setParam (core, pp + "space.on", 1.0f);
        setParam (core, pp + "space.path", 1.0f);
        setParam (core, pp + "space.room", 0.5f);
    }
}

/** p79: Key Gravity on every part (C major, default quality). */
void gravity (Core& core, int numParts = Core::numParts)
{
    for (int p = 0; p < numParts; ++p) setParam (core, partPrefix (p) + "grav.amt", 0.8f);
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

/** p79: a sixteenth-note drum pattern at 120 BPM on the drum channel (kick, snare, hats, a tom or clap every bar). */
void drumPattern (long block, int blockLen, double rate, std::vector<MidiEvent>& out)
{
    const double step = 0.125; // sixteenths at 120 BPM
    const double blockSeconds = blockLen / rate;
    const double t0 = (double) block * blockSeconds, t1 = t0 + blockSeconds;
    for (long k = (long) std::ceil (t0 / step); (double) k * step < t1; ++k)
    {
        const int offset = std::clamp ((int) ((k * step - t0) / blockSeconds * blockLen), 0, std::max (0, blockLen - 1));
        const int s = (int) (k % 16);
        auto hit = [&] (int pad, int vel) { out.push_back (MidiEvent::noteOn (Core::drumChannel, 36 + pad, vel, offset)); };
        if (s % 4 == 0) hit (0, 110);           // kick
        if (s == 4 || s == 12) hit (1, 100);    // snare
        hit (s % 2 == 0 ? 2 : 3, 70 + 20 * (s % 2)); // hats
        if (s == 14) hit (4 + (int) ((k / 16) % 8), 90); // a tom / clap / perc every bar
    }
}

/** p79: a 16-note chord on channel 1 for one second, then released (the "tails die away" scenarios). */
void chordThenRelease (long block, int blockLen, double rate, std::vector<MidiEvent>& out)
{
    const long releaseBlock = (long) (rate / blockLen);
    for (int n = 0; n < 16; ++n)
    {
        const int note = 40 + (n * 7) % 36 + n / 6;
        if (block == 0) out.push_back (MidiEvent::noteOn (1, note, 100, 0));
        if (block == releaseBlock) out.push_back (MidiEvent::noteOff (1, note, 0));
    }
}

std::vector<Scenario> scenarios()
{
    std::vector<Scenario> s;
    s.push_back ({ "idle", "defaults, no notes", [] (Core&) {}, 0, {}, -1.0 });
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        const std::string id = engineTypeInfo (type).id;
        s.push_back ({ "typical." + id, "4 parts x 8 voices, 2 FX per part",
                       [type] (Core& c) { parts (c, type, 8); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 2); }, 8, {}, -1.0 });
    }
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        const std::string id = engineTypeInfo (type).id;
        s.push_back ({ "worst." + id, "4 parts x 16 voices + layer, 6 FX per part",
                       [type] (Core& c) { parts (c, type, 16); layer (c, type); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); }, 16, {}, -1.0 });
    }
    s.push_back ({ "one-part.fm", "FM 16 voices + 6 FX on part 1; parts 2-4 silent with 6 FX each",
                   [] (Core& c) { parts (c, EngineType::FM, 16, 1); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); }, 16, {}, -1.0 });
    s.push_back ({ "silent-racks", "no notes; 6 FX on every part (tails long gone)",
                   [] (Core& c) { for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); }, 0, {}, -1.0 });
    s.push_back ({ "morph.4x4", "4 parts x 8 voices, morph on: 4 lanes per part (16 parts' worth), 2 FX",
                   [] (Core& c) { parts (c, EngineType::Wavetable, 8); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 2); morphEverywhere (c); }, 8, {}, -1.0 });
    // ---- p79 (path to 1.0)
    for (int t = 0; t < numEngineTypes; ++t)
    {
        const auto type = (EngineType) t;
        const std::string id = engineTypeInfo (type).id;
        s.push_back ({ "chord16." + id, "one part, 16-note chord on 16 voices, no FX",
                       [type] (Core& c) { parts (c, type, 16, 1); }, 16, {}, -1.0 });
    }
    s.push_back ({ "drums", "drum kit, sixteenth pattern at 120 BPM, parts silent", [] (Core&) {}, 0, drumPattern, 0.5 });
    s.push_back ({ "fx.full", "1 part x 8 voices; 6 FX on every part, 4 master FX, both sends",
                   [] (Core& c) { parts (c, EngineType::Analog, 8, 1); for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6); masterRack (c); }, 8, {}, -1.0 });
    s.push_back ({ "space3d", "4 parts x 8 voices, 3D stage moving with room on every part",
                   [] (Core& c) { parts (c, EngineType::Wavetable, 8); space3d (c); }, 8, {}, -1.0 });
    s.push_back ({ "gravity", "4 parts x 8 voices, Key Gravity 80 % on every part",
                   [] (Core& c) { parts (c, EngineType::Analog, 8); gravity (c); }, 8, {}, -1.0 });
    s.push_back ({ "everything", "4 parts x 16 voices, 6 FX + 3D + gravity per part, 4 master FX, drums",
                   [] (Core& c)
                   {
                       parts (c, EngineType::Wavetable, 16);
                       for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6);
                       masterRack (c);
                       space3d (c);
                       gravity (c);
                   }, 16, drumPattern, 0.5 });
    s.push_back ({ "silent.after-play", "full setup played 1 s, released; measured after 9 s (hall tails still fading)",
                   [] (Core& c)
                   {
                       parts (c, EngineType::Analog, 16);
                       for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6);
                       masterRack (c);
                       space3d (c);
                       gravity (c);
                   }, 0, chordThenRelease, 10.0 });
    s.push_back ({ "silent.long-after", "full setup played 1 s, released; measured after 19 s (every tail gone: all asleep)",
                   [] (Core& c)
                   {
                       parts (c, EngineType::Analog, 16);
                       for (int p = 0; p < Core::numParts; ++p) rack (c, p, 6);
                       masterRack (c);
                       space3d (c);
                       gravity (c);
                   }, 0, chordThenRelease, 20.0 });
    s.push_back ({ "silent.master-fx", "no notes; 4 master FX and both sends (tails long gone)",
                   [] (Core& c) { masterRack (c); }, 0, {}, -1.0 });
    return s;
}

struct Result
{
    double avg = 0.0, peak = 0.0; ///< percent of one core
    bool valid = true;
    uint64_t hash = 0;            ///< --hash: FNV-1a over the rendered output (bit-identity check, same machine / build)
    double rms = 0.0;
};

std::unique_ptr<Core> makeCore (const Scenario& sc, const Options& o)
{
    auto core = std::make_unique<Core>();
    core->prepare (o.rate, o.block);
    sc.setup (*core);
    core->prepareEffects();
    core->updateMorph();
    for (int n = 0; n < sc.notes; ++n) core->queueNote (40 + (n * 7) % 36 + n / 6, 0.8f, true);
    return core;
}

Result run (const Scenario& sc, const Options& o)
{
    auto corePtr = makeCore (sc, o);
    auto& core = *corePtr;

    std::vector<float> l ((size_t) o.block), r ((size_t) o.block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    const double blockSeconds = o.block / o.rate;
    std::vector<MidiEvent> midi;
    midi.reserve (256);
    long block = 0;
    auto step = [&]
    {
        midi.clear();
        if (sc.events) sc.events (block, o.block, o.rate, midi);
        ++block;
        core.process (out, o.block, midi.data(), (int) midi.size(), Transport {});
    };
    // warm up past the attacks (and, for silent racks, past every tail so the steady state is measured)
    const double warm = sc.warmSeconds >= 0.0 ? sc.warmSeconds : (sc.notes == 0 && sc.name != "idle" ? 6.0 : 0.5);
    for (int b = 0; b < (int) (warm / blockSeconds); ++b) step();

    Result best { 1.0e9, 1.0e9, true };
    const int blocks = std::max (1, (int) (o.seconds / blockSeconds));
    for (int rep = 0; rep < o.repeats; ++rep)
    {
        double total = 0.0, worst = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            const auto t0 = Clock::now();
            step();
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

/** p79: renders the scenario from a fresh core (warm-up included, 2 s more) and hashes every output sample. Timing-free. */
void hashRun (const Scenario& sc, const Options& o, Result& res)
{
    auto corePtr = makeCore (sc, o);
    auto& core = *corePtr;
    std::vector<float> l ((size_t) o.block), r ((size_t) o.block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    const double blockSeconds = o.block / o.rate;
    const double warm = sc.warmSeconds >= 0.0 ? sc.warmSeconds : (sc.notes == 0 && sc.name != "idle" ? 6.0 : 0.5);
    const int blocks = (int) ((warm + 2.0) / blockSeconds);
    std::vector<MidiEvent> midi;
    uint64_t h = 1469598103934665603ull;
    double sq = 0.0;
    for (long b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (sc.events) sc.events (b, o.block, o.rate, midi);
        core.process (out, o.block, midi.data(), (int) midi.size(), Transport {});
        for (int i = 0; i < o.block; ++i)
            for (const float v : { l[(size_t) i], r[(size_t) i] })
            {
                uint32_t bits;
                std::memcpy (&bits, &v, sizeof bits);
                h = (h ^ bits) * 1099511628211ull;
                sq += (double) v * v;
            }
    }
    res.hash = h;
    res.rms = std::sqrt (sq / (2.0 * blocks * o.block));
}

/** p79: a baseline file (the --csv output of an earlier run): scenario -> average percent. */
std::vector<std::pair<std::string, double>> readBaseline (const std::string& path)
{
    std::vector<std::pair<std::string, double>> b;
    if (FILE* f = std::fopen (path.c_str(), "r"))
    {
        char line[512];
        while (std::fgets (line, sizeof line, f))
        {
            const std::string s (line);
            const auto a = s.find (';');
            if (a == std::string::npos || s[0] == '#' || s.rfind ("scenario", 0) == 0) continue;
            b.emplace_back (s.substr (0, a), std::atof (s.c_str() + a + 1));
        }
        std::fclose (f);
    }
    else
        std::fprintf (stderr, "ohpe_bench: no baseline file %s (comparison skipped)\n", path.c_str());
    return b;
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
        else if (a == "--hash") o.hash = true;
        else if (a == "--baseline") o.baseline = next();
        else if (a == "--write") o.write = next();
        else
        {
            std::printf ("usage: ohpe_bench [--quick] [--block N] [--rate HZ] [--seconds S] [--repeats N] [--filter TEXT] [--csv] [--list] [--hash] [--baseline FILE] [--write FILE]\n");
            return a == "--help" || a == "-h" ? 0 : 1;
        }
    }

    const auto all = scenarios();
    if (o.list)
    {
        for (const auto& s : all) std::printf ("%-20s %s\n", s.name.c_str(), s.description.c_str());
        return 0;
    }
    const auto base = o.baseline.empty() ? std::vector<std::pair<std::string, double>>() : readBaseline (o.baseline);
    auto baseOf = [&] (const std::string& n) { for (const auto& b : base) if (b.first == n) return b.second; return -1.0; };
    if (o.csv) std::printf ("scenario;avg_percent;peak_percent;rtf_avg;rtf_worst_block%s\n", o.hash ? ";hash;rms" : "");
    else
        std::printf ("Ohpe bench: %.0f Hz, block %d, %.1f s x %d (best run). Percent of one CPU core for real time - lower is better.\n"
                     "RTF = realtime factor (audio time / compute time; avg over all blocks, and of the slowest block) - higher is better.\n"
                     "%-20s %8s %8s %9s %9s %8s   %s\n", o.rate, o.block, o.seconds, o.repeats, "scenario", "avg %", "peak %", "RTF avg",
                     "RTF worst", base.empty() ? "" : "vs base", "patch");
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
        if (o.csv) std::printf ("startup.first-ms;%.1f;%.1f;0;0\n", ms[0], ms[1]);
        else std::printf ("%-20s %8.0f %8.0f   %s\n", "startup ms", ms[0], ms[1], "build + prepare a Core: first / second instance (milliseconds, not %)");
    }
    FILE* csvFile = o.write.empty() ? nullptr : std::fopen (o.write.c_str(), "w");
    if (csvFile != nullptr)
        std::fprintf (csvFile, "# ohpe_bench %.0f Hz block %d %.1f s x %d\nscenario;avg_percent;peak_percent;rtf_avg;rtf_worst_block\n", o.rate,
                      o.block, o.seconds, o.repeats);
    bool ok = true;
    double sum = 0.0;
    int count = 0;
    for (const auto& s : all)
    {
        if (! o.filter.empty() && s.name.find (o.filter) == std::string::npos) continue;
        auto r = run (s, o);
        if (o.hash) hashRun (s, o, r);
        ok = ok && r.valid;
        sum += r.avg;
        ++count;
        const double rtfAvg = r.avg > 0.0 ? 100.0 / r.avg : 0.0, rtfWorst = r.peak > 0.0 ? 100.0 / r.peak : 0.0;
        if (csvFile != nullptr) std::fprintf (csvFile, "%s;%.3f;%.3f;%.1f;%.1f\n", s.name.c_str(), r.avg, r.peak, rtfAvg, rtfWorst);
        if (o.csv)
        {
            std::printf ("%s;%.3f;%.3f;%.1f;%.1f", s.name.c_str(), r.avg, r.peak, rtfAvg, rtfWorst);
            if (o.hash) std::printf (";%016llx;%.6f", (unsigned long long) r.hash, r.rms);
            std::printf ("%s\n", r.valid ? "" : ";INVALID");
        }
        else
        {
            char vs[32] = "";
            if (const double b = baseOf (s.name); b > 0.0) std::snprintf (vs, sizeof vs, "%+.0f%%", 100.0 * (r.avg - b) / b);
            std::printf ("%-20s %8.2f %8.2f %9.1f %9.1f %8s   %s%s", s.name.c_str(), r.avg, r.peak, rtfAvg, rtfWorst, vs, s.description.c_str(),
                         r.valid ? "" : "  << INVALID OUTPUT");
            if (o.hash) std::printf ("  [hash %016llx rms %.6f]", (unsigned long long) r.hash, r.rms);
            std::printf ("\n");
        }
        std::fflush (stdout);
    }
    if (! o.csv && count > 0)
    {
        double baseSum = 0.0; // only scenarios the baseline has
        for (const auto& b : base)
            for (const auto& sc : all)
                if (sc.name == b.first && (o.filter.empty() || sc.name.find (o.filter) != std::string::npos)) baseSum += b.second;
        std::printf ("%-20s %8.2f", "sum", sum);
        if (baseSum > 0.0) std::printf ("   (baseline sum %.2f)", baseSum);
        std::printf ("\n");
    }
    if (csvFile != nullptr) std::fclose (csvFile);
    return ok ? 0 : 2;
}
