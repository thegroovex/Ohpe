// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "SampleTestHelpers.h"
#include "drum/DrumKit.h"
#include "engines/GrainEngine.h"
#include "engines/SamplerEngine.h"
#include "sample/SampleState.h"

#include <doctest/doctest.h>

#include <map>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct KitRig
{
    drum::DrumKit kit;
    KitRig() { kit.prepare (sampleRate, 256); }

    Stereo render (size_t n, int block = 256)
    {
        Stereo s (n);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        for (size_t pos = 0; pos < n; pos += (size_t) block)
        {
            const int len = (int) std::min ((size_t) block, n - pos);
            kit.render (l.data(), r.data(), len);
            std::copy (l.begin(), l.begin() + len, s.l.begin() + (long) pos);
            std::copy (r.begin(), r.begin() + len, s.r.begin() + (long) pos);
        }
        return s;
    }

    /** local parameter index of a pad parameter */
    static int idx (int pad, int param) { return 2 + pad * drum::DrumKit::numPadParams + param; }
};

struct CoreRig
{
    Core core;
    static constexpr int block = 480;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> recL;
    CoreRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
    }
    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, {});
            recL.insert (recL.end(), l[0].begin(), l[0].end());
        }
    }
};
} // namespace

TEST_CASE ("drum: parameters - 24 pads, English and German names, append-only block")
{
    const auto d = drum::DrumKit::paramDescriptors();
    CHECK (d.size() == 2 + (size_t) drum::DrumKit::numPads * drum::DrumKit::numPadParams);
    for (const auto& p : d)
    {
        CHECK_FALSE (p.nameEn.empty());
        CHECK_FALSE (p.nameDe.empty());
    }
    Core core;
    CHECK (core.params().indexOf ("drum.vol") >= 0);
    CHECK (core.params().indexOf ("drum.pad1.type") >= 0);
    CHECK (core.params().indexOf ("drum.pad24.tone") >= 0);
}

TEST_CASE ("drum: silence when idle, every default pad sounds, stays finite and decays")
{
    KitRig rig;
    CHECK (peak (rig.render (4800).l) == 0.0f);
    for (int p = 0; p < drum::DrumKit::numPads; ++p)
    {
        CAPTURE (p + 1);
        rig.kit.triggerPad (p, 1.0f);
        auto hit = rig.render (4800);
        CHECK (rms (hit.l) + rms (hit.r) > 0.003f);
        CHECK (allFinite (hit.l));
        CHECK (noDenormals (hit.l));
        CHECK (peak (hit.l) < 2.0f);
        rig.render (48000 * 4);
        CHECK (rig.kit.activeVoiceCount() == 0);
    }
    CHECK (peak (rig.render (1024).l) == 0.0f);
}

TEST_CASE ("drum: every model at extreme settings is finite and ends")
{
    KitRig rig;
    for (int type = 0; type < (int) drum::PadType::numTypes; ++type)
        for (float x : { 0.0f, 1.0f })
        {
            CAPTURE (type);
            CAPTURE (x);
            rig.kit.setParam (KitRig::idx (0, drum::DrumKit::pType), (float) type);
            rig.kit.setParam (KitRig::idx (0, drum::DrumKit::pTune), x > 0.5f ? 24.0f : -24.0f);
            rig.kit.setParam (KitRig::idx (0, drum::DrumKit::pDecay), x);
            rig.kit.setParam (KitRig::idx (0, drum::DrumKit::pTone), x);
            rig.kit.triggerPad (0, 1.0f);
            auto out = rig.render (48000 * 6, 97);
            CHECK (allFinite (out.l));
            CHECK (noDenormals (out.l));
            CHECK (rig.kit.activeVoiceCount() == 0);
        }
}

TEST_CASE ("drum: choke groups cut each other, pads are monophonic, others ring on")
{
    KitRig rig;
    // factory kit: pad 7 open hat and pad 6 closed hat share choke group 1; pad 1 kick has none
    REQUIRE (rig.kit.pad (5).choke == 1);
    REQUIRE (rig.kit.pad (6).choke == 1);
    rig.kit.triggerPad (0, 1.0f);
    rig.kit.triggerPad (6, 1.0f);
    rig.render (4800);
    CHECK (rig.kit.isPadSounding (6));
    rig.kit.triggerPad (5, 1.0f); // closed hat chokes the open one
    rig.render (480);             // 10 ms > 3 ms fade
    CHECK_FALSE (rig.kit.isPadSounding (6));
    CHECK (rig.kit.isPadSounding (5));
    CHECK (rig.kit.isPadSounding (0)); // the kick is not in the group

    // a different group (2) is not affected
    rig.kit.triggerPad (18, 1.0f);
    rig.kit.triggerPad (5, 1.0f);
    rig.render (480);
    CHECK (rig.kit.isPadSounding (18));

    // retriggering a pad leaves one voice for it
    rig.kit.reset();
    for (int i = 0; i < 5; ++i) { rig.kit.triggerPad (9, 1.0f); rig.render (480); }
    CHECK (rig.kit.activeVoiceCount() == 1);

    // choke group 0 = no choking
    rig.kit.setParam (KitRig::idx (6, drum::DrumKit::pChoke), 0.0f);
    rig.kit.triggerPad (6, 1.0f);
    rig.kit.triggerPad (5, 1.0f);
    rig.render (480);
    CHECK (rig.kit.isPadSounding (6));
}

TEST_CASE ("drum: tune transposes, decay sets the length, pan places the pad")
{
    KitRig rig;
    auto tomFreq = [&] (float tune) {
        rig.kit.reset();
        rig.kit.setParam (KitRig::idx (9, drum::DrumKit::pTune), tune);
        rig.kit.setParam (KitRig::idx (9, drum::DrumKit::pDecay), 1.0f);
        rig.kit.triggerPad (9, 1.0f);
        auto out = rig.render (48000);
        return measureFrequency (out.l, sampleRate, 24000, 48000); // after the pitch sweep
    };
    const double f0 = tomFreq (0.0f), f12 = tomFreq (12.0f);
    CHECK (f0 == doctest::Approx (105.0 * std::pow (2.0, 4.0 / 12.0)).epsilon (0.02)); // pad 10: high tom voicing
    CHECK (f12 / f0 == doctest::Approx (2.0).epsilon (0.01));

    auto length = [&] (float decay) {
        rig.kit.reset();
        rig.kit.setParam (KitRig::idx (0, drum::DrumKit::pDecay), decay);
        rig.kit.triggerPad (0, 1.0f);
        size_t blocks = 0;
        while (rig.kit.isPadSounding (0) && blocks < 1000) { rig.render (256); ++blocks; }
        return blocks;
    };
    CHECK (length (0.0f) * 3 < length (1.0f));

    rig.kit.reset();
    rig.kit.setParam (KitRig::idx (0, drum::DrumKit::pPan), -1.0f);
    rig.kit.triggerPad (0, 1.0f);
    auto left = rig.render (4800);
    CHECK (rms (left.l) > 0.05f);
    CHECK (rms (left.r) < 1.0e-4f);
}

TEST_CASE ("drum: sample pads - user sample over factory sample over model, source, tune and length")
{
    KitRig rig;
    const int pad = 2;
    auto freqOfHit = [&] (size_t n = 19200) {
        rig.kit.reset();
        rig.kit.triggerPad (pad, 1.0f);
        auto out = rig.render (n);
        return measureFrequency (out.l, sampleRate, 1000, n);
    };
    // no sample: the pad's model (snare) plays
    rig.kit.triggerPad (pad, 1.0f);
    CHECK (rms (rig.render (4800).l) > 0.01f);

    // factory sample, then a user sample on top; clearing the user sample brings the factory one back
    auto factory = makeSine (220.0, 44100.0, 1.0, 0.5f, "factory:snare");
    auto user = makeSine (440.0, 48000.0, 1.0, 0.5f, "/user/snare.wav");
    rig.kit.setFactorySample (pad, factory);
    CHECK (std::abs (centsBetween (freqOfHit(), 220.0)) < 6.0); // +-4 cents humanising
    rig.kit.setPadSample (pad, user);
    CHECK ((rig.kit.effectiveSample (pad) == user));
    CHECK (std::abs (centsBetween (freqOfHit(), 440.0)) < 6.0);
    rig.kit.setParam (KitRig::idx (pad, drum::DrumKit::pTune), 12.0f);
    CHECK (std::abs (centsBetween (freqOfHit(), 880.0)) < 6.0);
    rig.kit.setParam (KitRig::idx (pad, drum::DrumKit::pTune), 0.0f);
    rig.kit.setPadSample (pad, nullptr);
    CHECK ((rig.kit.effectiveSample (pad) == factory));
    CHECK ((rig.kit.padSample (pad) == nullptr));

    // decay >= 0.5 plays the whole sample, lower values shorten it
    auto lengthOf = [&] (float decay) {
        rig.kit.reset();
        rig.kit.setParam (KitRig::idx (pad, drum::DrumKit::pDecay), decay);
        rig.kit.triggerPad (pad, 1.0f);
        size_t blocks = 0;
        while (rig.kit.isPadSounding (pad) && blocks < 1000) { rig.render (256); ++blocks; }
        return blocks * 256;
    };
    CHECK (lengthOf (0.5f) == doctest::Approx (48000.0).epsilon (0.02));   // 1 s at 44.1 kHz -> 48000 samples
    CHECK (lengthOf (0.1f) < 12000);
    rig.kit.setParam (KitRig::idx (pad, drum::DrumKit::pDecay), 0.5f);

    // source = Synth ignores the sample
    rig.kit.setParam (KitRig::idx (pad, drum::DrumKit::pSource), (float) drum::PadSource::Synth);
    rig.kit.reset();
    rig.kit.triggerPad (pad, 1.0f);
    auto synth = rig.render (19200);
    CHECK (std::abs (centsBetween (measureFrequency (synth.l, sampleRate, 1000, 19200), 220.0)) > 50.0);
    rig.kit.setParam (KitRig::idx (pad, drum::DrumKit::pSource), (float) drum::PadSource::Sample);

    // replacing the sample while it sounds stops the hit (no stale read)
    rig.kit.triggerPad (pad, 1.0f);
    rig.render (256);
    rig.kit.setFactorySample (pad, nullptr);
    rig.render (256);
    CHECK_FALSE (rig.kit.isPadSounding (pad));

    // soft hits are quieter and darker
    rig.kit.setFactorySample (pad, makeSine (5000.0, 48000.0, 0.5));
    auto level = [&] (float vel) {
        rig.kit.reset();
        rig.kit.triggerPad (pad, vel);
        return rms (rig.render (9600).l, 960);
    };
    CHECK (level (0.2f) < 0.35f * level (1.0f));
}

TEST_CASE ("drum: the core plays the kit on MIDI channel 10 through mixer channel 5")
{
    CoreRig rig;
    rig.run (20, { MidiEvent::noteOn (Core::drumChannel, 36, 120, 0) });
    CHECK (rms (rig.recL) > 0.01f);
    CHECK (rig.core.part (0).activeVoiceCount() == 0);

    CoreRig muted;
    muted.core.params().set ("mix.ch5.mute", 1.0f);
    muted.run (20, { MidiEvent::noteOn (Core::drumChannel, 36, 120, 0) });
    CHECK (peak (muted.recL) == 0.0f);

    CoreRig base;
    base.core.params().set ("drum.base", 48.0f);
    base.run (5, { MidiEvent::noteOn (Core::drumChannel, 36, 120, 0) });
    CHECK (peak (base.recL) == 0.0f); // below pad 1 now
}

TEST_CASE ("core: engines Grain, Vocal and Sampler selectable and audible through the core")
{
    for (auto t : { EngineType::Grain, EngineType::Vocal, EngineType::Sampler })
    {
        CAPTURE ((int) t);
        CoreRig rig;
        rig.core.params().set ("p1.engine", (float) t);
        rig.run (20, { MidiEvent::noteOn (1, 60, 100, 0) });
        CHECK (rms (rig.recL) > 0.005f);
        CHECK (allFinite (rig.recL));
    }
}

TEST_CASE ("state: engine and drum parameters and sample assignments round-trip")
{
    Core a;
    a.params().set ("p1.grain.density", 55.0f);
    a.params().set ("p1.vocal.vowel", 3.25f);
    a.params().set ("p1.sampler.mode", 1.0f);
    a.params().set ("drum.pad7.choke", 3.0f);
    a.params().set ("drum.pad12.src", (float) drum::PadSource::Synth);

    // fake "files": the decoder hands out synthetic samples by path
    std::map<std::string, SamplePtr> files;
    files["/s/keys-c4.wav"] = makeSine (261.63, 48000.0, 0.5, 0.5f, "/s/keys-c4.wav");
    files["/s/keys-c5.wav"] = makeSine (523.25, 48000.0, 0.5, 0.5f, "/s/keys-c5.wav");
    files["/s/texture.flac"] = makeSine (100.0, 48000.0, 0.5, 0.5f, "/s/texture.flac");
    files["/s/hit.aif"] = makeSine (900.0, 48000.0, 0.1, 0.5f, "/s/hit.aif");
    auto decode = [&] (const std::string& path) -> SamplePtr {
        const auto it = files.find (path);
        return it != files.end() ? it->second : nullptr;
    };

    auto* samplerA = dynamic_cast<SamplerEngine*> (a.part (0).engine (EngineType::Sampler));
    auto* grainA = dynamic_cast<GrainEngine*> (a.part (0).engine (EngineType::Grain));
    REQUIRE (samplerA != nullptr);
    REQUIRE (grainA != nullptr);
    sampler::ZoneSpec z1, z2;
    z1.sample = files["/s/keys-c4.wav"]; z1.keyLo = 0; z1.keyHi = 66; z1.rootKey = 60;
    z2.sample = files["/s/keys-c5.wav"]; z2.keyLo = 67; z2.keyHi = 127; z2.rootKey = 72; z2.loopStart = 100; z2.loopEnd = 2000;
    samplerA->setZones ({ z1, z2 });
    grainA->setSample (files["/s/texture.flac"]);
    a.drumKit().setPadSample (11, files["/s/hit.aif"]);
    a.setStateSection ("samples", sample::saveAssignments (a));
    const auto json = a.saveState();

    Core b;
    REQUIRE (b.loadState (json));
    for (const char* id : { "p1.grain.density", "p1.vocal.vowel", "p1.sampler.mode", "drum.pad7.choke", "drum.pad12.src" })
    {
        CAPTURE (id);
        CHECK (b.params().get (id) == doctest::Approx (a.params().get (id)));
    }
    const auto r = sample::restoreAssignments (b, b.stateSection ("samples"), decode);
    CHECK (r.loaded == 4);
    CHECK (r.missing == 0);
    auto* samplerB = dynamic_cast<SamplerEngine*> (b.part (0).engine (EngineType::Sampler));
    auto* grainB = dynamic_cast<GrainEngine*> (b.part (0).engine (EngineType::Grain));
    const auto zones = samplerB->zones();
    REQUIRE (zones.size() == 2);
    CHECK (zones[0].sample->path() == "/s/keys-c4.wav");
    CHECK (zones[1].sample->path() == "/s/keys-c5.wav");
    CHECK (zones[1].keyLo == 67);
    CHECK (zones[1].rootKey == 72);
    CHECK (zones[1].loopEnd == 2000);
    CHECK (grainB->sample()->path() == "/s/texture.flac");
    CHECK (b.drumKit().padSample (11)->path() == "/s/hit.aif");
    CHECK ((b.drumKit().padSample (10) == nullptr));

    // a missing file is reported, the rest loads; a state without samples clears everything
    files.erase ("/s/texture.flac");
    const auto r2 = sample::restoreAssignments (b, b.stateSection ("samples"), decode);
    CHECK (r2.loaded == 3);
    CHECK (r2.missing == 1);
    CHECK ((grainB->sample() == nullptr));

    Core empty;
    REQUIRE (b.loadState (empty.saveState()));
    CHECK (b.stateSection ("samples").empty());
    sample::restoreAssignments (b, b.stateSection ("samples"), decode);
    CHECK (samplerB->zones().empty());
    CHECK ((b.drumKit().padSample (11) == nullptr));
}
