// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p63 (1.0 phase A, item 4): audio engine stress. Sample-rate changes (44.1 .. 192 kHz), buffer sizes
// (1, odd sizes, variable blocks, empty blocks), silence tails (no denormals, the CPU cost stays bounded),
// the output guard (NaN / Inf never reach the outputs) and a short offline soak (default 10 minutes of
// audio; OHPE_SOAK_MINUTES sets the length - docs/en/SOAK-TEST.md describes the manual 8-hour run).

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "preset/Preset.h"
#include "tape/ITapeTrack.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace ohpe;
namespace fs = std::filesystem;

namespace
{
fs::path factoryDir() { return fs::path (OHPE_SOURCE_DIR) / "resources" / "presets" / "factory"; }

bool applyFactory (Core& core, const std::string& file)
{
    std::ifstream in (factoryDir() / file, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    preset::Preset p;
    if (! preset::fromJson (ss.str(), p)) return false;
    if (p.meta.kind == preset::Kind::Project) preset::applyProject (p, core);
    else
        for (int part = 0; part < Core::numParts; ++part) preset::applyPart (p, core.params(), part);
    while (core.prepareEffects()) {}
    return true;
}

bool isSubnormal (float v) noexcept { return v != 0.0f && std::fpclassify (v) == FP_SUBNORMAL; }

/** Output statistics over everything rendered. */
struct Watch
{
    bool finite = true;
    float peak = 0.0f;
    int64_t subnormals = 0;
    int64_t samples = 0;

    void add (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v = x[i];
            finite = finite && std::isfinite (v);
            if (std::isfinite (v)) peak = std::max (peak, std::abs (v));
            subnormals += isSubnormal (v) ? 1 : 0;
        }
        samples += n;
    }
};

/** Renders on all eight output pairs (multi-out as the standalone uses it). */
struct Rig
{
    std::unique_ptr<Core> core = std::make_unique<Core>();
    std::vector<std::vector<float>> buffers;
    OutputBuffers out;
    Watch watch;
    double ppq = 0.0;
    double rate = 48000.0;

    void prepare (double sr, int maxBlock)
    {
        rate = sr;
        core->prepare (sr, maxBlock);
        while (core->prepareEffects()) {}
        buffers.assign (2 * numOutputPairs, std::vector<float> ((size_t) std::max (1, maxBlock)));
        for (int o = 0; o < numOutputPairs; ++o)
        {
            out.left[(size_t) o] = buffers[(size_t) (2 * o)].data();
            out.right[(size_t) o] = buffers[(size_t) (2 * o + 1)].data();
        }
    }

    void block (int n, const std::vector<MidiEvent>& midi = {}, bool playing = true)
    {
        Transport t;
        t.bpm = 124.0;
        t.playing = playing;
        t.hostDriven = true;
        t.ppqAtBlockStart = ppq;
        core->process (out, n, midi.empty() ? nullptr : midi.data(), (int) midi.size(), t);
        if (playing) ppq += (double) n * t.bpm / 60.0 / rate;
        for (auto& b : buffers) watch.add (b.data(), n);
    }
};

std::vector<MidiEvent> chord (bool on, int n)
{
    std::vector<MidiEvent> e;
    for (int part = 1; part <= Core::numParts; ++part)
        for (int note : { 48, 55, 60, 64 })
            e.push_back (on ? MidiEvent::noteOn (part, note + part, 100, std::min (n - 1, part)) : MidiEvent::noteOff (part, note + part, std::min (n - 1, part)));
    return e;
}
} // namespace

TEST_CASE ("stress: every sample rate and buffer size renders finite, bounded audio without denormals")
{
    Rig rig;
    REQUIRE (applyFactory (*rig.core, "project-ambient-drift.ohpepreset"));
    std::mt19937 rng (63);
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int pattern = 0; pattern < 6; ++pattern)
        {
            // 1, odd sizes, a large block, variable blocks (with empty ones) inside one prepared maximum
            const int sizes[] = { 1, 7, 333, 4096, 2048, 512 };
            const int maxBlock = sizes[pattern];
            rig.prepare (sr, maxBlock);
            CAPTURE (sr);
            CAPTURE (maxBlock);
            const int64_t total = (int64_t) (0.25 * sr);
            int64_t done = 0;
            bool noteOn = false;
            while (done < total)
            {
                int n = maxBlock;
                if (pattern >= 4) n = std::uniform_int_distribution<int> (0, maxBlock) (rng); // variable, sometimes 0
                n = (int) std::min<int64_t> (n, total - done);
                std::vector<MidiEvent> midi;
                if (n > 0 && ! noteOn && done >= total / 10) { midi = chord (true, n); noteOn = true; }
                else if (n > 0 && noteOn && done >= total / 2) { midi = chord (false, n); noteOn = false; }
                rig.block (n, midi);
                done += n;
            }
            CHECK (rig.watch.finite);
            CHECK (rig.watch.peak < 16.0f);
            CHECK (rig.watch.subnormals == 0);
        }
    CHECK (rig.core->nonFiniteBlocks() == 0);
}

TEST_CASE ("stress: silence tails stay free of denormals and the CPU cost stays bounded")
{
    Rig rig;
    REQUIRE (applyFactory (*rig.core, "project-ambient-drift.ohpepreset"));
    constexpr int block = 256;
    rig.prepare (48000.0, block);
    for (int b = 0; b < 400; ++b) rig.block (block, b == 0 ? chord (true, block) : std::vector<MidiEvent> {}, false);
    rig.block (block, chord (false, block), false);
    // 60 s of tail (reverb, delay, filters decaying towards zero): compare the cost of the first and the last seconds
    const int perSecond = 48000 / block;
    auto secondCost = [&] {
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < perSecond; ++b) rig.block (block, {}, false);
        return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    };
    double early = 0.0, late = 0.0;
    for (int s = 0; s < 60; ++s)
    {
        const double c = secondCost();
        if (s >= 1 && s < 4) early += c;
        if (s >= 57) late += c;
    }
    MESSAGE ("tail cost: early " << early / 3.0 * 1000.0 << " ms per second of audio, late " << late / 3.0 * 1000.0 << " ms");
    CHECK (rig.watch.finite);
    CHECK (rig.watch.subnormals == 0);
    // denormal stalls cost 10 .. 100 times more; a generous factor keeps noisy CI runners from failing the check
    CHECK (late < 4.0 * early + 0.05);
}

TEST_CASE ("stress: NaN / Inf at the input never reach the outputs, the engine recovers")
{
    Rig rig;
    constexpr int block = 128;
    rig.prepare (48000.0, block);
    auto& reg = rig.core->params();
    // the input monitored through the tape (track 1 armed on the input) and recorded
    reg.set ("tape.tr1.arm", 1.0f);
    reg.set ("tape.tr1.src", (float) tapesource::input);
    reg.set ("tape.play", 1.0f);
    reg.set ("tape.rec", 1.0f);
    std::vector<float> bad (block), good (block);
    for (int i = 0; i < block; ++i)
    {
        bad[(size_t) i] = i % 4 == 0 ? NAN : (i % 4 == 1 ? INFINITY : (i % 4 == 2 ? -INFINITY : 1.0e38f));
        good[(size_t) i] = 0.2f * (float) std::sin (0.05 * i);
    }
    for (int b = 0; b < 50; ++b)
    {
        rig.core->setAudioInput (bad.data(), bad.data());
        rig.block (block, b == 0 ? chord (true, block) : std::vector<MidiEvent> {}, false);
    }
    CHECK (rig.watch.finite);
    CHECK (rig.watch.peak < 16.0f);
    reg.set ("tape.rec", 0.0f);
    reg.set ("tape.play", 0.0f);
    rig.core->tape().housekeeping();
    // afterwards: clean input, a new chord - finite and audible again
    rig.watch = {};
    for (int b = 0; b < 200; ++b)
    {
        rig.core->setAudioInput (good.data(), good.data());
        rig.block (block, b == 10 ? chord (true, block) : std::vector<MidiEvent> {}, false);
    }
    CHECK (rig.watch.finite);
    CHECK (rig.watch.peak > 1.0e-3f);
    CHECK (rig.watch.peak < 16.0f);
}

TEST_CASE ("stress: random patches (every parameter anywhere in its range) never put NaN / Inf on the outputs")
{
    Rig rig;
    constexpr int block = 256;
    rig.prepare (48000.0, block);
    auto& reg = rig.core->params();
    std::mt19937 rng (6302);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (int round = 0; round < 12; ++round)
    {
        for (int i = 0; i < reg.size(); ++i)
        {
            const auto& id = reg.descriptor (i).id;
            if (id.rfind ("tape.", 0) == 0 || id.rfind ("pads.", 0) == 0 || id.rfind ("sync.", 0) == 0) continue; // transport / performance
            reg.setNormalised (i, u (rng));
        }
        while (rig.core->prepareEffects()) {}
        for (int b = 0; b < 60; ++b) rig.block (block, b == 0 ? chord (true, block) : (b == 40 ? chord (false, block) : std::vector<MidiEvent> {}));
    }
    MESSAGE ("blocks the output guard silenced: " << rig.core->nonFiniteBlocks());
    CHECK (rig.watch.finite);
}

TEST_CASE ("stress: soak - minutes of offline audio with sequencers, automation and preset changes stay healthy")
{
    const char* env = std::getenv ("OHPE_SOAK_MINUTES");
    const double minutes = env != nullptr && *env != 0 ? std::max (0.1, std::atof (env)) : 10.0;
    Rig rig;
    constexpr int block = 256;
    constexpr double sr = 48000.0;
    rig.prepare (sr, block);
    const std::vector<std::string> presets { "project-ambient-drift.ohpepreset", "project-arp-sequence.ohpepreset",
                                             "project-deep-house.ohpepreset", "project-boom-kit.ohpepreset" };
    auto& reg = rig.core->params();
    std::vector<int> automated;
    for (const char* id : { "p1.vol", "p2.vol", "p1.analog.cutoff", "tempo", "mix.ch1.pan" })
        if (const int i = reg.indexOf (id); i >= 0) automated.push_back (i);
    std::mt19937 rng (6303);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    const int64_t totalBlocks = (int64_t) (minutes * 60.0 * sr / block);
    const int64_t blocksPerMinute = (int64_t) (60.0 * sr / block);
    const auto t0 = std::chrono::steady_clock::now();
    float lastMinutePeak = 0.0f;
    for (int64_t b = 0; b < totalBlocks; ++b)
    {
        if (b % blocksPerMinute == 0)
        {
            CHECK (applyFactory (*rig.core, presets[(size_t) ((b / blocksPerMinute) % (int64_t) presets.size())]));
            lastMinutePeak = 0.0f;
        }
        for (int i : automated)
            if (b % 7 == 0) reg.setNormalised (i, 0.3f + 0.4f * u (rng));
        std::vector<MidiEvent> midi;
        if (b % 40 == 0) midi = chord (true, block);
        else if (b % 40 == 30) midi = chord (false, block);
        rig.block (block, midi);
        lastMinutePeak = std::max (lastMinutePeak, rig.watch.peak);
        if (! rig.watch.finite) break;
    }
    const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    MESSAGE ("soak: " << minutes << " min of audio in " << seconds << " s (" << 100.0 * seconds / (minutes * 60.0) << " % of real time), peak "
                      << rig.watch.peak << ", guard " << rig.core->nonFiniteBlocks());
    CHECK (rig.watch.finite);
    CHECK (rig.watch.peak < 16.0f);
    CHECK (rig.watch.subnormals == 0);
    CHECK (rig.core->nonFiniteBlocks() == 0);
    // all notes off, the transport stops: the voices end (no stuck or leaking voices after hours of playing)
    rig.block (block, chord (false, block), false);
    for (int b = 0; b < (int) (20.0 * sr / block); ++b) rig.block (block, {}, false);
    CHECK (rig.core->activeVoices() == 0);
}
