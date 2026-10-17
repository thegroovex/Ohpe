// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p35: the performance pass - every optimisation renders what the code before it rendered (bit for bit, or within a
// tolerance far below hearing), no tail is cut, nothing becomes NaN or denormal.
#include "TestHelpers.h"
#include "OhpeCore.h"
#include "dsp/DspUtil.h"
#include "engines/FmEngine.h"
#include "engines/Part.h"
#include "fx/FxRack.h"
#include "fx/InsertSlot.h"
#include "model/CpuLoad.h"
#include "model/ParamSnapshot.h"

#include <doctest/doctest.h>

#include <functional>
#include <memory>
#include <random>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr int block = 256;

struct Render
{
    std::vector<float> l, r;
};

/** Renders a core for `seconds`: setup() after prepare, perBlock() before every block (notes, checks). */
Render renderCore (const std::function<void (Core&)>& setup, double seconds, const std::function<void (Core&, int)>& perBlock = {})
{
    auto owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    core.prepare (sampleRate, block);
    REQUIRE (core.params().set ("mix.master.limit", 0.0f));
    setup (core);
    while (core.prepareEffects()) {}
    std::vector<float> l (block), r (block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    Render rec;
    const int blocks = (int) (seconds * sampleRate / block);
    for (int b = 0; b < blocks; ++b)
    {
        if (perBlock) perBlock (core, b);
        core.process (out, block, nullptr, 0, Transport {});
        rec.l.insert (rec.l.end(), l.begin(), l.end());
        rec.r.insert (rec.r.end(), r.begin(), r.end());
    }
    return rec;
}

float maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    float d = 0.0f;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i) d = std::max (d, std::abs (a[i] - b[i]));
    return d;
}

struct AllowSleep
{
    bool before = Part::sleepAllowed;
    explicit AllowSleep (bool allow) { Part::sleepAllowed = allow; }
    ~AllowSleep() { Part::sleepAllowed = before; }
};

void set (Core& core, const std::string& id, float v) { REQUIRE_MESSAGE (core.params().set (id, v), id); }

int blockAt (double seconds) { return (int) (seconds * sampleRate / block); }
} // namespace

// ------------------------------------------------------------------------------------------------ FM: polynomial sine
TEST_CASE ("p35 dsp: sinTurns matches the library sine")
{
    double worst = 0.0;
    for (int i = -400000; i <= 400000; ++i)
    {
        const float x = (float) i * 1.0e-4f; // -40 .. 40 turns
        worst = std::max (worst, std::abs ((double) dsp::sinTurns (x) - std::sin (2.0 * 3.14159265358979323846 * (double) x)));
    }
    MESSAGE ("sinTurns max error " << worst);
    CHECK (worst < 4.0e-7);
    CHECK (dsp::sinTurns (0.25f) == doctest::Approx (1.0f).epsilon (1e-6));
    CHECK (dsp::sinTurns (-0.25f) == doctest::Approx (-1.0f).epsilon (1e-6));
    CHECK (std::abs (dsp::sinTurns (0.0f)) == 0.0f);
    CHECK (std::isfinite (dsp::sinTurns (1.0e9f))); // beyond the reduction: the library path
}

TEST_CASE ("p35 fm: the polynomial sine renders what the library sine rendered (every algorithm, feedback, an operator off)")
{
    struct Restore { ~Restore() { fm::useLibrarySine = false; } } restore;
    float worst = 0.0f;
    for (int algo = 0; algo < fm::numAlgorithms; ++algo)
        for (float feedback : { 0.0f, 0.35f })
            for (bool opOff : { false, true })
            {
                Stereo out[2];
                for (int mode = 0; mode < 2; ++mode)
                {
                    fm::useLibrarySine = mode == 1;
                    auto e = std::make_unique<FmEngine>();
                    e->prepare (sampleRate, 512);
                    e->setCommon (plainCommon());
                    e->setPolyphony (8);
                    e->setParam (fm::Algo, (float) algo);
                    e->setParam (fm::Index, 0.6f);
                    e->setParam (fm::Feedback, feedback);
                    if (opOff) e->setParam (fm::Op3Level, 0.0f);
                    for (int note : { 40, 52, 59, 64, 71, 83 }) e->noteOn (note, 0.9f);
                    out[mode] = renderEngine (*e, (size_t) (0.5 * sampleRate));
                }
                CAPTURE (algo);
                CAPTURE (feedback);
                CAPTURE (opOff);
                REQUIRE (allFinite (out[0].l));
                REQUIRE (allFinite (out[0].r));
                CHECK (noDenormals (out[0].l));
                CHECK (peak (out[0].l) > 0.01f);
                const float d = std::max (maxDiff (out[0].l, out[1].l), maxDiff (out[0].r, out[1].r));
                worst = std::max (worst, d);
                CHECK (d < 1.0e-4f); // about -80 dB below the notes - and the library sine itself differs per platform
            }
    MESSAGE ("fm polynomial vs library sine: max difference " << worst);
}

// ------------------------------------------------------------------------------------------------ sleeping parts
TEST_CASE ("p35 parts sleep: echo and reverb tails are never cut, the output equals the awake reference")
{
    auto setup = [] (Core& c) {
        set (c, "p1.fx.type", (float) InsertSlot::Reverb);
        set (c, "p1.fx2.type", (float) InsertSlot::Echo);
        set (c, "p1.fx2.echo.sync", 0.0f); // free time
        set (c, "p1.fx2.echo.time", 1500.0f);
        set (c, "p1.fx2.echo.feedback", 0.45f);
        set (c, "p1.fx2.echo.mix", 0.5f);
    };
    bool asleepAt3s = true, asleepAtEnd = false;
    auto play = [&] (Core& c, int b) {
        if (b == 0) c.queueNote (60, 0.9f, true);
        if (b == blockAt (0.25)) c.queueNote (60, 0.0f, false);
        if (b == blockAt (3.0)) asleepAt3s = c.part (0).isAsleep(); // echoes of the note still ring
        asleepAtEnd = c.part (0).isAsleep();
    };
    Render awake, sleeping;
    {
        AllowSleep a (false);
        awake = renderCore (setup, 40.0, play);
        CHECK_FALSE (asleepAtEnd);
    }
    {
        AllowSleep a (true);
        sleeping = renderCore (setup, 40.0, play);
    }
    CHECK_FALSE (asleepAt3s);
    CHECK (asleepAtEnd);
    REQUIRE (allFinite (sleeping.l));
    CHECK (noDenormals (sleeping.l));
    CHECK (peak (awake.l, (size_t) (2.0 * sampleRate), (size_t) (4.0 * sampleRate)) > 1.0e-3f); // the echoes are there
    const float d = std::max (maxDiff (awake.l, sleeping.l), maxDiff (awake.r, sleeping.r));
    MESSAGE ("sleeping vs awake: max difference " << d);
    CHECK (d < 1.0e-5f); // only what lay below -120 dBFS on the part's output
    // the reference itself had decayed into inaudibility where the part slept
    CHECK (peak (awake.l, awake.l.size() - (size_t) sampleRate) < 1.0e-5f);
}

TEST_CASE ("p35 parts sleep: silent parts with full racks sleep, a note wakes the part at once")
{
    AllowSleep a (true);
    bool allAsleep = false, wokeAtOnce = false;
    auto setup = [] (Core& c) {
        const int types[6] = { InsertSlot::Chorus, InsertSlot::Reverb, InsertSlot::FilterDrive, InsertSlot::Phaser, InsertSlot::Echo, InsertSlot::Hall };
        for (int p = 0; p < Core::numParts; ++p)
            for (int k = 0; k < FxRack::numPartSlots; ++k)
                set (c, FxRack::partSlotPrefix ("p" + std::to_string (p + 1) + ".", k) + "type", (float) types[k]);
    };
    const int noteBlock = blockAt (6.0);
    auto rec = renderCore (setup, 7.0, [&] (Core& c, int b) {
        if (b == noteBlock)
        {
            allAsleep = true;
            for (int p = 0; p < Core::numParts; ++p) allAsleep = allAsleep && c.part (p).isAsleep();
            c.queueNote (64, 0.9f, true);
        }
        if (b == noteBlock + 2) wokeAtOnce = ! c.part (0).isAsleep() && c.part (1).isAsleep();
    });
    CHECK (allAsleep);
    CHECK (wokeAtOnce);
    REQUIRE (allFinite (rec.l));
    CHECK (noDenormals (rec.l));
    CHECK (peak (rec.l, (size_t) ((noteBlock + 1) * block)) > 1.0e-3f);
    CHECK (peak (rec.l, (size_t) (5.0 * sampleRate), (size_t) (noteBlock * block)) == 0.0f); // asleep: exact silence
}

TEST_CASE ("p35 parts sleep: a part that keeps playing never sleeps (bit-identical to the awake reference)")
{
    auto setup = [] (Core& c) {
        set (c, "p1.fx.type", (float) InsertSlot::Chorus);
        set (c, "p1.amp.s", 1.0f);
    };
    auto play = [] (Core& c, int b) { if (b == 0) c.queueNote (48, 0.8f, true); };
    Render a, b;
    {
        AllowSleep s (false);
        a = renderCore (setup, 6.0, play);
    }
    {
        AllowSleep s (true);
        b = renderCore (setup, 6.0, play);
    }
    CHECK (maxDiff (a.l, b.l) == 0.0f);
    CHECK (maxDiff (a.r, b.r) == 0.0f);
}

// ------------------------------------------------------------------------------------------------ changed-only pull
TEST_CASE ("p35 snapshot: pullChanged gives exactly the values of a full pull")
{
    auto owner = std::make_unique<Core>();
    auto& reg = owner->params();
    ParamSnapshot changed, full;
    changed.prepare (reg);
    full.prepare (reg);
    changed.pullChanged (reg);
    full.pull (reg);
    std::mt19937 rng (35);
    const int n = reg.size();
    auto anyIndex = [&] { return (int) (rng() % (uint32_t) n); };
    auto anyValue = [&] (int i) {
        const auto& d = reg.descriptor (i);
        return d.minValue + (d.maxValue - d.minValue) * (float) (rng() % 1000) / 999.0f;
    };
    int mismatches = 0;
    for (int step = 0; step < 3000; ++step)
    {
        const int ops = 1 + (int) (rng() % 6);
        for (int k = 0; k < ops; ++k)
        {
            const int i = anyIndex();
            const float v = anyValue (i);
            switch (rng() % 8)
            {
                case 0: case 1: case 2: reg.set (i, v); break;
                case 3: changed.setLock (i, v); full.setLock (i, v); break;
                case 4: if (rng() % 8 == 0) { changed.clearLocks(); full.clearLocks(); } break;
                case 5: changed.setMotion (i, v); full.setMotion (i, v); break;
                case 6: changed.clearMotion (i, reg.get (i)); full.clearMotion (i, reg.get (i)); break;
                default: changed.setModulated (i, v); full.setModulated (i, v); break; // a block's modulation
            }
        }
        if (step == 1500) reg.resetToDefaults();
        changed.pullChanged (reg);
        full.pull (reg);
        for (int i = 0; i < n; ++i)
            if (! (changed[i] == full[i])) ++mismatches;
    }
    CHECK (mismatches == 0);
}

TEST_CASE ("p35 core: automation between blocks reaches the sound as before (changed-only pull)")
{
    // a filter sweep by registry writes between blocks: the block values follow every write
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    core.prepare (sampleRate, block);
    std::vector<float> l (block), r (block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    const int cutoff = core.params().indexOf ("p1.analog.cutoff");
    REQUIRE (cutoff >= 0);
    for (int b = 0; b < 50; ++b)
    {
        const float v = 200.0f + 100.0f * (float) b;
        core.params().set (cutoff, v);
        core.process (out, block, nullptr, 0, Transport {});
        CHECK (core.blockValue (cutoff) == core.params().get (cutoff));
    }
}

// ------------------------------------------------------------------------------------------------ CPU meter
TEST_CASE ("p35 cpu meter: average follows, the peak holds two seconds and falls, bad input is ignored")
{
    CpuLoad m;
    CHECK (m.average() == 0.0f);
    const double blockSeconds = 256.0 / 48000.0;
    m.add (0.25 * blockSeconds, blockSeconds);
    CHECK (m.average() == doctest::Approx (0.25f)); // the first block sets it
    for (int b = 0; b < 2000; ++b) m.add (0.1 * blockSeconds, blockSeconds); // ~10 s
    CHECK (m.average() == doctest::Approx (0.1f).epsilon (1e-3));
    m.add (0.9 * blockSeconds, blockSeconds); // one slow block
    CHECK (m.peak() == doctest::Approx (0.9f));
    CHECK (m.average() < 0.2f); // one block barely moves the average
    for (int b = 0; b < (int) (1.5 / blockSeconds); ++b) m.add (0.1 * blockSeconds, blockSeconds);
    CHECK (m.peak() == doctest::Approx (0.9f)); // held
    for (int b = 0; b < (int) (5.0 / blockSeconds); ++b) m.add (0.1 * blockSeconds, blockSeconds);
    CHECK (m.peak() == doctest::Approx (0.1f).epsilon (0.05)); // fallen back to the load
    m.add (NAN, blockSeconds);
    m.add (1.0, 0.0);
    m.add (-1.0, blockSeconds);
    CHECK (std::isfinite (m.average()));
    CHECK (m.average() == doctest::Approx (0.1f).epsilon (1e-2));
    m.reset();
    CHECK (m.average() == 0.0f);
    CHECK (m.peak() == 0.0f);
}
