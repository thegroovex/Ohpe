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

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
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
    float worst = 0.0f, worstRms = 0.0f;
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
                std::vector<float> diff (out[0].l.size());
                for (size_t i = 0; i < diff.size(); ++i) diff[i] = out[0].l[i] - out[1].l[i];
                worst = std::max (worst, d);
                worstRms = std::max (worstRms, rms (diff) / std::max (1.0e-9f, rms (out[1].l)));
                // a six-operator stack at a high index multiplies the last bits of every sine down the stack: still
                // -60 dB at worst, -80 dB in the RMS (and the library sine itself differs that much between platforms)
                CHECK (d < 1.0e-3f);
                CHECK (rms (diff) < 1.0e-4f * std::max (1.0f, rms (out[1].l)));
            }
    MESSAGE ("fm polynomial vs library sine: max difference " << worst << ", difference RMS / signal RMS " << worstRms);
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

// ================================================================================================ p79 (path to 1.0)
#include "fx/Limiter.h"
#include "dsp/SleepGate.h"

namespace
{
/** The master limiter as it was before p79 (shifted history, true peak every sample, full window scan, log every
    sample): the reference the optimised limiter must match bit for bit. */
class ReferenceLimiter
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        look = std::max (8, (int) (0.0015f * sr));
        delayL.assign ((size_t) look, 0.0f);
        delayR.assign ((size_t) look, 0.0f);
        reqRing.assign ((size_t) look, 1.0f);
        avgRing.assign ((size_t) look, 1.0f);
        relFast = std::exp (-1.0f / (0.06f * sr));
        relSlow = std::exp (-1.0f / (0.35f * sr));
        slowFollow = std::exp (-1.0f / (0.4f * sr));
        for (int ph = 0; ph < 3; ++ph)
        {
            const double frac = (double) (ph + 1) * 0.25;
            double total = 0.0;
            for (int j = 0; j < taps; ++j)
            {
                const double t = frac - (double) (j - (half - 1));
                const double pi = 3.14159265358979323846;
                const double sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double u = t / ((double) half + 0.5);
                const double w = std::abs (u) >= 1.0 ? 0.0 : 0.5 + 0.5 * std::cos (pi * u);
                coef[(size_t) ph][(size_t) j] = sinc * w;
                total += sinc * w;
            }
            for (auto& c : coef[(size_t) ph]) c /= total;
        }
        sum = (double) look;
    }
    void resetGain()
    {
        std::fill (reqRing.begin(), reqRing.end(), 1.0f);
        std::fill (avgRing.begin(), avgRing.end(), 1.0f);
        held = 1.0f;
        sum = (double) look;
        gain = 1.0f;
        workDb = 0.0f;
    }
    void passThrough (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            push (histL, l[i]);
            push (histR, r[i]);
            delayL[(size_t) pos] = histL[(size_t) (half - 1)];
            delayR[(size_t) pos] = histR[(size_t) (half - 1)];
            if (++pos >= look) pos = 0;
            l[i] = delayL[(size_t) pos];
            r[i] = delayR[(size_t) pos];
        }
    }
    void process (float* l, float* r, int n)
    {
        const float target = ceiling * 0.97f;
        for (int i = 0; i < n; ++i)
        {
            push (histL, l[i]);
            push (histR, r[i]);
            const float xl = histL[(size_t) (half - 1)], xr = histR[(size_t) (half - 1)];
            const float pk = std::max (truePeak (histL), truePeak (histR));
            const float req = pk > target ? target / pk : 1.0f;
            reqRing[(size_t) pos] = req;
            float m = 1.0f;
            for (float v : reqRing) m = std::min (m, v);
            const float grDb = -dsp::gainToDb (std::max (held, 1.0e-6f));
            workDb = grDb + (workDb - grDb) * slowFollow;
            const float w = std::clamp ((workDb - 1.0f) * (1.0f / 3.0f), 0.0f, 1.0f);
            const float rel = relFast + (relSlow - relFast) * w;
            held = m < held ? m : m + (held - m) * rel;
            sum += (double) held - (double) avgRing[(size_t) pos];
            avgRing[(size_t) pos] = held;
            gain = std::min (1.0f, (float) (sum / (double) look));
            delayL[(size_t) pos] = xl;
            delayR[(size_t) pos] = xr;
            if (++pos >= look) pos = 0;
            l[i] = std::clamp (delayL[(size_t) pos] * gain, -ceiling, ceiling);
            r[i] = std::clamp (delayR[(size_t) pos] * gain, -ceiling, ceiling);
        }
    }

private:
    static constexpr int half = 12, taps = 24;
    using History = std::array<float, (size_t) taps>;
    static void push (History& h, float x)
    {
        for (size_t j = 0; j + 1 < h.size(); ++j) h[j] = h[j + 1];
        h[h.size() - 1] = x;
    }
    float truePeak (const History& h) const
    {
        float pk = std::abs (h[(size_t) (half - 1)]);
        for (const auto& c : coef)
        {
            double v = 0.0;
            for (size_t j = 0; j < h.size(); ++j) v += (double) c[j] * (double) h[j];
            pk = std::max (pk, (float) std::abs (v));
        }
        return pk;
    }
    float sr = 48000.0f, ceiling = 0.966f;
    int look = 72, pos = 0;
    std::vector<float> delayL, delayR, reqRing, avgRing;
    History histL {}, histR {};
    std::array<std::array<double, (size_t) taps>, 3> coef {};
    double sum = 72.0;
    float held = 1.0f, gain = 1.0f, workDb = 0.0f, relFast = 0.999f, relSlow = 0.9999f, slowFollow = 0.9999f;
};
} // namespace

TEST_CASE ("p79 limiter: the optimised limiter renders what the previous one rendered, bit for bit")
{
    for (const float rate : { 44100.0f, 48000.0f, 96000.0f })
    {
        ReferenceLimiter ref;
        Limiter lim;
        ref.prepare (rate);
        lim.prepare (rate);
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        int differing = 0;
        for (int blk = 0; blk < 3000; ++blk)
        {
            const int n = 1 + (int) (rng() % 512);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            const int mode = (blk / 120) % 5; // silence, quiet, near the ceiling, hard limiting, quiet with spikes
            const float amp = mode == 0 ? 0.0f : mode == 1 ? 0.1f : mode == 2 ? 0.9f : mode == 3 ? 2.5f : 0.5f;
            for (int i = 0; i < n; ++i)
            {
                l[(size_t) i] = amp * u (rng);
                r[(size_t) i] = amp * u (rng);
                if (mode == 4 && rng() % 300 == 0) l[(size_t) i] = 3.0f;
            }
            auto l2 = l, r2 = r;
            const bool off = (blk / 700) % 4 == 3; // limiter switched off: the delay only
            if (off) { ref.passThrough (l.data(), r.data(), n); lim.passThrough (l2.data(), r2.data(), n); }
            else { ref.process (l.data(), r.data(), n); lim.process (l2.data(), r2.data(), n); }
            if (blk % 1100 == 0) { ref.resetGain(); lim.resetGain(); }
            differing += (l != l2 || r != r2) ? 1 : 0;
        }
        CHECK_MESSAGE (differing == 0, "rate " << rate);
    }
}

TEST_CASE ("p79 master sleep: send delay, send reverb and master rack sleep in silence; no tail is cut")
{
    auto setup = [] (Core& c) {
        set (c, "mix.ch1.senda", 0.6f);
        set (c, "mix.ch1.sendb", 0.6f);
        set (c, "fxa.delay.feedback", 0.6f);
        set (c, FxRack::masterSlotPrefix (0) + "type", (float) InsertSlot::Echo);
        set (c, FxRack::masterSlotPrefix (1) + "type", (float) InsertSlot::Hall);
    };
    bool asleepAt3s = true, asleepAtEnd = false;
    auto play = [&] (Core& c, int b) {
        if (b == 0) c.queueNote (60, 0.9f, true);
        if (b == blockAt (0.25)) c.queueNote (60, 0.0f, false);
        if (b == blockAt (3.0)) asleepAt3s = c.mixer().stageAsleep (0) || c.mixer().stageAsleep (1) || c.mixer().stageAsleep (2);
        asleepAtEnd = c.mixer().stageAsleep (0) && c.mixer().stageAsleep (1) && c.mixer().stageAsleep (2);
    };
    Render awake, sleeping;
    {
        AllowSleep a (false);
        awake = renderCore (setup, 45.0, play);
        CHECK_FALSE (asleepAtEnd);
    }
    {
        AllowSleep a (true);
        sleeping = renderCore (setup, 45.0, play);
    }
    CHECK_FALSE (asleepAt3s); // the echoes still ring
    CHECK (asleepAtEnd);
    REQUIRE (allFinite (sleeping.l));
    CHECK (noDenormals (sleeping.l));
    CHECK (peak (awake.l, (size_t) (1.0 * sampleRate), (size_t) (3.0 * sampleRate)) > 1.0e-3f); // the tails are there
    const float d = std::max (maxDiff (awake.l, sleeping.l), maxDiff (awake.r, sleeping.r));
    MESSAGE ("master sleeping vs awake: max difference " << d);
    CHECK (d < 1.0e-5f); // only what lay below -120 dBFS
    // a note wakes everything within its block
    bool woke = false;
    renderCore (setup, 32.0, [&] (Core& c, int b) {
        if (b == blockAt (30.0)) c.queueNote (64, 0.9f, true);
        if (b == blockAt (30.0) + 2) woke = ! c.mixer().stageAsleep (0) && ! c.mixer().stageAsleep (1) && ! c.mixer().stageAsleep (2);
    });
    CHECK (woke);
}

TEST_CASE ("p79 sleep gate: NaN is never silent, a loud block wakes, the hold counts silent blocks only")
{
    dsp::SleepGate g;
    g.prepare (1000.0); // hold 4500 samples
    std::vector<float> z (500, 0.0f), loud (500, 0.5f), nanv (500, 0.0f);
    nanv[100] = std::numeric_limits<float>::quiet_NaN();
    CHECK (dsp::SleepGate::silent (z.data(), z.data(), 500));
    CHECK_FALSE (dsp::SleepGate::silent (nanv.data(), z.data(), 500));
    CHECK_FALSE (dsp::SleepGate::silent (loud.data(), z.data(), 500));
    for (int b = 0; b < 8; ++b) { REQUIRE (g.shouldProcess (true)); g.afterProcess (true, z.data(), z.data(), 500); }
    CHECK_FALSE (g.isAsleep()); // 4000 < 4500
    REQUIRE (g.shouldProcess (true));
    g.afterProcess (true, z.data(), z.data(), 500);
    CHECK (g.isAsleep());
    CHECK_FALSE (g.shouldProcess (true));
    CHECK (g.shouldProcess (false)); // wakes
    CHECK_FALSE (g.isAsleep());
}

TEST_CASE ("p79 core: denormals flush inside process() only; the caller's floating-point mode is restored")
{
    auto owner = std::make_unique<Core>();
    owner->prepare (sampleRate, block);
    std::vector<float> l (block), r (block);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    owner->process (out, block, nullptr, 0, Transport {});
    volatile float tiny = 1.0e-38f; // a normal number; half of it is denormal
    volatile float half = tiny * 0.5f;
    CHECK (half != 0.0f); // flush-to-zero is not left switched on
    CHECK (allFinite (l));
}
