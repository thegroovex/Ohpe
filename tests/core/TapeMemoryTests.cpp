// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p67: tape memory - ring buffer wrap, capture correctness (pitch, slices), state round trip, parameters.
#include "OhpeCore.h"
#include "engines/SamplerEngine.h"
#include "preset/Preset.h"
#include "tapemem/TapeMemory.h"

#include <doctest/doctest.h>

#include <cmath>
#include <map>

using namespace ohpe;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

/** A decaying tone at `hz` (a few harmonics). */
std::vector<float> tone (double hz, double seconds, float amp = 0.5f, double decay = 3.0)
{
    std::vector<float> v ((size_t) (seconds * kRate));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / kRate;
        const double env = std::exp (-t * decay) * std::min (1.0, t / 0.002);
        v[i] = (float) (amp * env * (std::sin (2 * kPi * hz * t) + 0.3 * std::sin (4 * kPi * hz * t) + 0.1 * std::sin (6 * kPi * hz * t)));
    }
    return v;
}

bounce::Audio stereo (std::vector<float> m)
{
    bounce::Audio a;
    a.rate = kRate;
    a.r = m;
    a.l = std::move (m);
    return a;
}

double midiOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }

std::map<std::string, float> applied;
bounce::Setter recorder()
{
    applied.clear();
    return [] (const std::string& id, float v) { applied[id] = v; };
}
} // namespace

TEST_CASE ("p67 tape memory: parameters appended after harmony.kb.fixed, defaults, not part of project presets")
{
    auto core = std::make_unique<Core>();
    const auto& reg = core->params();
    const int first = reg.indexOf (Core::Ids::tapememLen);
    REQUIRE (first >= 0);
    CHECK (first == reg.indexOf ("harmony.kb.fixed") + 1);
    int k = 0;
    for (const char* id : { Core::Ids::tapememLen, Core::Ids::tapememSrc, Core::Ids::tapememMode, Core::Ids::tapememTo, Core::Ids::tapememGo })
    {
        CHECK (reg.indexOf (id) == first + k++);
        CHECK (preset::isExcludedFromProject (id));
    }
    CHECK (reg.get (Core::Ids::tapememLen) == (float) tapemem::Len30s);
    CHECK (reg.get (Core::Ids::tapememSrc) == (float) tapemem::SourceMain);
    CHECK (reg.get (Core::Ids::tapememMode) == (float) tapemem::ModeChromatic);
    CHECK (reg.get (Core::Ids::tapememTo) == (float) tapemem::TargetEditPart);
    CHECK (reg.get (Core::Ids::tapememGo) == 0.0f);
    CHECK (reg.descriptor (reg.indexOf (Core::Ids::tapememSrc)).choices.size() == (size_t) tapemem::numSources);
    CHECK (reg.descriptor (reg.indexOf (Core::Ids::tapememTo)).choices.size() == (size_t) tapemem::numTargets);
    CHECK (tapemem::lengthSeconds (tapemem::Len5s) == 5.0);
    CHECK (tapemem::lengthSeconds (tapemem::Len30s) == 30.0);
}

TEST_CASE ("p67 tape memory: ring buffer keeps the newest frames in order across the wrap, 16-bit, low memory")
{
    tapemem::RingBuffer rb;
    rb.prepare (1000.0, 2.0); // 2 s + 0.5 s guard at 1 kHz
    CHECK (rb.capacityFrames() == 2500);
    CHECK (rb.memoryBytes() == 2500 * 2 * sizeof (int16_t));
    CHECK (rb.available() == 0);
    CHECK (rb.snapshot (1.0).frames() == 0);
    // a ramp, pushed in odd blocks, more than twice the capacity
    std::vector<float> l (37), r (37);
    int64_t counter = 0;
    for (int b = 0; b < 200; ++b)
    {
        for (int i = 0; i < 37; ++i, ++counter)
        {
            l[(size_t) i] = (float) (counter % 1000) / 1000.0f;
            r[(size_t) i] = -l[(size_t) i];
        }
        rb.push (l.data(), r.data(), 37);
    }
    CHECK (rb.framesWritten() == counter);
    CHECK (rb.available() == 2000);
    const auto a = rb.snapshot (1.5);
    REQUIRE (a.frames() == 1500);
    for (int64_t i = 0; i < a.frames(); ++i)
    {
        const int64_t c = counter - 1500 + i;
        CHECK (a.l[(size_t) i] == doctest::Approx ((double) (c % 1000) / 1000.0).epsilon (0.0002));
        CHECK (a.r[(size_t) i] == doctest::Approx (-(double) (c % 1000) / 1000.0).epsilon (0.0002));
    }
    CHECK (rb.snapshot (99.0).frames() == 2000); // at most what is held
    // headroom up to +6 dB, junk is silenced
    const float hot[] = { 1.9f, -1.9f, NAN, INFINITY };
    rb.push (hot, nullptr, 4);
    const auto h = rb.snapshot (0.004);
    REQUIRE (h.frames() == 4);
    CHECK (h.l[0] == doctest::Approx (1.9).epsilon (0.001));
    CHECK (h.r[1] == doctest::Approx (-1.9).epsilon (0.001)); // mono copied
    CHECK (h.l[2] == 0.0f);
    CHECK (h.l[3] == 0.0f);
    rb.clear();
    CHECK (rb.available() == 0);
    rb.push (l.data(), r.data(), 37);
    CHECK (rb.available() == 0); // starts over (the guard is not capturable)
    // 30 s at 48 kHz stay small
    tapemem::RingBuffer big;
    big.prepare (48000.0);
    CHECK (big.memoryBytes() < 6u * 1024u * 1024u);
}

TEST_CASE ("p67 tape memory: pitch detection finds the note of test tones")
{
    for (double hz : { 82.41, 110.0, 261.63, 440.0, 987.77 })
    {
        const auto t = tone (hz, 0.5, 0.5f, 0.5);
        float conf = 0.0f;
        const float f = tapemem::detectPitch (t.data(), (int64_t) t.size(), kRate, &conf);
        CHECK (std::abs (midiOf (f) - midiOf (hz)) < 0.1);
        CHECK (conf > 0.8f);
    }
    std::vector<float> noise (24000);
    uint32_t s = 1;
    for (auto& x : noise) { s = s * 1664525u + 1013904223u; x = (float) ((double) s / 4294967296.0 - 0.5); }
    float conf = 1.0f;
    const float f = tapemem::detectPitch (noise.data(), (int64_t) noise.size(), kRate, &conf);
    CHECK ((f == 0.0f || conf < 0.7f));
}

TEST_CASE ("p67 tape memory: Chromatic capture - silence trimmed, pitch corrected to the semitone, root detected")
{
    // 1 s of silence, then a slightly sharp A3 (+30 cents), then silence
    std::vector<float> m (48000, 0.0f);
    const double hz = 220.0 * std::pow (2.0, 30.0 / 1200.0);
    const auto t = tone (hz, 1.5);
    m.insert (m.end(), t.begin(), t.end());
    m.insert (m.end(), 24000, 0.0f);
    const auto c = tapemem::build (stereo (m), tapemem::ModeChromatic, "Capture 1");
    REQUIRE ((c.sample != nullptr));
    CHECK (c.root == 57);
    CHECK (c.cents == doctest::Approx (-30.0).epsilon (0.1));
    CHECK (c.sample->numFrames() < (int64_t) (1.7 * kRate)); // the silence is gone
    CHECK (c.sample->peak() == doctest::Approx (bounce::targetPeak).epsilon (0.02));
    const float corrected = tapemem::detectPitch (c.sample->channel (0) + 2000, c.sample->numFrames() - 2000, kRate);
    CHECK (std::abs (midiOf (corrected) - 57.0) < 0.06);
    // into the Sampler: zone root = the note, engine and mode set
    auto core = std::make_unique<Core>();
    REQUIRE (tapemem::apply (*core, 1, c, recorder()));
    CHECK (applied["p2.engine"] == (float) EngineType::Sampler);
    CHECK (applied["p2.sampler.mode"] == (float) sampler::ModeZones);
    CHECK (applied["p2.sampler.root"] == 57.0f);
    auto* e = dynamic_cast<SamplerEngine*> (core->part (1).engine (EngineType::Sampler));
    REQUIRE (e != nullptr);
    REQUIRE (e->zones().size() == 1);
    CHECK (e->zones()[0].rootKey == 57);
    CHECK ((e->zones()[0].sample == c.sample));
    CHECK_FALSE (tapemem::apply (*core, 4, c, recorder()));
    // silence gives nothing
    CHECK ((tapemem::build (stereo (std::vector<float> (48000, 0.0f)), tapemem::ModeChromatic, "x").sample == nullptr));
}

TEST_CASE ("p67 tape memory: Chromatic picks the strongest note; Slices finds one slice per hit")
{
    // four hits: quiet, loud, quiet, quiet - 0.4 s apart
    std::vector<float> m (4800, 0.0f);
    const double freqs[] = { 330.0, 196.0, 440.0, 523.25 };
    const float amps[] = { 0.1f, 0.6f, 0.1f, 0.1f };
    for (int k = 0; k < 4; ++k)
    {
        auto t = tone (freqs[k], 0.4, amps[k], 12.0);
        m.insert (m.end(), t.begin(), t.end());
    }
    const auto chroma = tapemem::build (stereo (m), tapemem::ModeChromatic, "c");
    REQUIRE ((chroma.sample != nullptr));
    CHECK (chroma.root == 55); // G3 = the loud one
    const auto sl = tapemem::build (stereo (m), tapemem::ModeSlices, "s");
    REQUIRE ((sl.sample != nullptr));
    CHECK (sl.slices >= 4);
    CHECK (sl.slices <= 5);
    auto core = std::make_unique<Core>();
    REQUIRE (tapemem::apply (*core, 0, sl, recorder()));
    CHECK (applied["p1.sampler.mode"] == (float) sampler::ModeSlices);
    CHECK (applied["p1.sampler.slicebase"] == (float) tapemem::sliceBaseKey);
    for (const auto& s : { chroma.sample, sl.sample })
        for (int64_t i = 0; i < s->numFrames(); ++i)
        {
            REQUIRE (std::isfinite (s->channel (0)[i]));
            REQUIRE (std::abs (s->channel (0)[i]) <= 1.0f);
        }
}

TEST_CASE ("p67 tape memory: captures survive the session (state section round trip, old and junk sections)")
{
    std::vector<float> m (4800, 0.0f);
    const auto t = tone (261.63, 0.6);
    m.insert (m.end(), t.begin(), t.end());
    const auto c = tapemem::build (stereo (m), tapemem::ModeChromatic, "Capture 3");
    REQUIRE ((c.sample != nullptr));
    const auto json = tapemem::saveCaptures ({ { 2, c } });
    CHECK (json.find ("\"format\":\"ohpe-tapememory\"") != std::string::npos);
    CHECK (json.find ("\"version\":1") != std::string::npos);
    const auto back = tapemem::restoreCaptures (json);
    REQUIRE (back.size() == 1);
    CHECK (back[0].part == 2);
    CHECK (back[0].capture.root == c.root);
    CHECK (back[0].capture.mode == tapemem::ModeChromatic);
    CHECK (back[0].capture.name == "Capture 3");
    REQUIRE (back[0].capture.sample->numFrames() == c.sample->numFrames());
    for (int64_t i = 0; i < c.sample->numFrames(); i += 97)
        CHECK (back[0].capture.sample->channel (1)[i] == doctest::Approx (c.sample->channel (1)[i]).epsilon (0.0001));
    // through the core's state: the section travels with saveState / loadState
    auto core = std::make_unique<Core>();
    core->setStateSection ("tapememory", json);
    auto other = std::make_unique<Core>();
    REQUIRE (other->loadState (core->saveState()));
    CHECK (tapemem::restoreCaptures (other->stateSection ("tapememory")).size() == 1);
    // nothing stored, junk, a different format: nothing restored, no throw
    CHECK (tapemem::saveCaptures ({}).empty());
    CHECK (tapemem::restoreCaptures ("").empty());
    CHECK (tapemem::restoreCaptures ("{nope").empty());
    CHECK (tapemem::restoreCaptures (R"({"format":"ohpe-samples","parts":{}})").empty());
    CHECK (tapemem::restoreCaptures (R"({"format":"ohpe-tapememory","version":1,"parts":{"p1":{"rate":48000,"channels":2,"pcm16":"AAAA"}}})").empty());
}
