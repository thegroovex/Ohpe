// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "dsp/DspUtil.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct CoreRig
{
    Core core;
    static constexpr int block = 480;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> recL, recR;
    std::array<std::vector<float>, numOutputPairs> recAuxL;

    CoreRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
    }

    void run (int blocks, std::vector<MidiEvent> firstBlockMidi = {}, Transport tr = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            core.process (out, block, b == 0 ? firstBlockMidi.data() : nullptr, b == 0 ? (int) firstBlockMidi.size() : 0, tr);
            recL.insert (recL.end(), l[0].begin(), l[0].end());
            recR.insert (recR.end(), r[0].begin(), r[0].end());
            for (int o = 1; o < numOutputPairs; ++o)
                recAuxL[(size_t) o].insert (recAuxL[(size_t) o].end(), l[(size_t) o].begin(), l[(size_t) o].end());
        }
    }
};
} // namespace

TEST_CASE ("core: silence when idle, finite output")
{
    CoreRig rig;
    rig.run (100);
    CHECK (peak (rig.recL) == 0.0f);
    CHECK (peak (rig.recR) == 0.0f);
}

TEST_CASE ("core: MIDI note plays the analog engine at the right pitch on the main output")
{
    CoreRig rig;
    auto& p = rig.core.params();
    p.set ("p1.analog.shape", 0.5f);
    p.set ("p1.analog.detune", 0.0f);
    p.set ("p1.analog.oscmix", 0.0f);
    p.set ("p1.analog.cutoff", 18000.0f);
    p.set ("p1.analog.reso", 0.0f);
    p.set ("p1.mod.amt", 0.0f);
    rig.run (100, { MidiEvent::noteOn (1, 69, 127, 0) });
    CHECK (rms (rig.recL, 4800) > 0.01f);
    CHECK (allFinite (rig.recL));
    CHECK (std::abs (centsBetween (measureFrequency (rig.recL, sampleRate, 4800, rig.recL.size()), 440.0)) < 3.0);
    CHECK (rig.core.activeVoices() == 1);
    CHECK (rig.core.lastNote() == 69);
}

TEST_CASE ("core: engine switch to FM changes the sound, releases ring out")
{
    CoreRig rig;
    rig.core.params().set ("p1.engine", (float) EngineType::FM);
    rig.run (20, { MidiEvent::noteOn (1, 60, 100, 0) });
    CHECK (rms (rig.recL) > 0.005f);
    CHECK (rig.core.part (0).currentEngineType() == EngineType::FM);
    rig.run (2, { MidiEvent::noteOff (1, 60, 0) });
    rig.run (300);
    CHECK (rig.core.activeVoices() == 0);
}

TEST_CASE ("core: unimplemented engine slots are silent (phase 2 placeholders)")
{
    // the first slot that is still reserved (none left = nothing to check)
    int reserved = -1;
    for (int t = 0; t < numEngineTypes && reserved < 0; ++t)
        if (! engineTypeInfo ((EngineType) t).implemented) reserved = t;
    if (reserved < 0) return;
    CoreRig rig;
    rig.core.params().set ("p1.engine", (float) reserved);
    rig.run (20, { MidiEvent::noteOn (1, 60, 100, 0) });
    CHECK (peak (rig.recL) == 0.0f);
}

TEST_CASE ("core: channel routing to aux output 5")
{
    CoreRig rig;
    rig.core.params().set ("mix.ch1.out", 5.0f);
    rig.run (40, { MidiEvent::noteOn (1, 60, 100, 0) });
    CHECK (peak (rig.recL) == 0.0f);
    CHECK (rms (rig.recAuxL[5]) > 0.005f);
    CHECK (peak (rig.recAuxL[4]) == 0.0f);
}

TEST_CASE ("core: MIDI learn through the core, mapped CC does not play notes")
{
    CoreRig rig;
    const int cutoff = rig.core.params().indexOf ("p1.analog.cutoff");
    rig.core.midiMapper().armLearn (cutoff);
    rig.run (1, { MidiEvent::controller (1, 74, 0) });
    CHECK (rig.core.midiMapper().applyPendingLearns (rig.core.midiMap()));
    rig.run (1, { MidiEvent::controller (1, 74, 127) });
    CHECK (rig.core.params().get (cutoff) == doctest::Approx (20000.0f));
    CHECK (rig.core.midiMap().isMapped ("p1.analog.cutoff"));
}

TEST_CASE ("core: internal clock runs the step sequencer into the part")
{
    CoreRig rig;
    StepSequencer::Step s;
    s.on = true;
    s.note = 64;
    rig.core.stepSequencer().setStep (0, s);
    rig.core.params().set (Core::Ids::seqRun, 1.0f);
    rig.run (10);
    CHECK (rms (rig.recL) > 0.001f);
    CHECK (rig.core.lastNote() == 64);
    CHECK (rig.core.sequencerPosition() >= 0);
    rig.core.params().set (Core::Ids::seqRun, 0.0f);
    rig.run (300);
    CHECK (rig.core.activeVoices() == 0);
    CHECK (rig.core.sequencerPosition() == -1);
}

TEST_CASE ("core: parameter lock changes the sound only on its step")
{
    CoreRig rig;
    auto& p = rig.core.params();
    const int vol = p.indexOf ("p1.vol");
    StepSequencer::Step s;
    s.on = true;
    s.locks[0] = { vol, 0.0f }; // step 0 silent through a volume lock
    rig.core.stepSequencer().setStep (0, s);
    rig.core.params().set (Core::Ids::seqRun, 1.0f);
    rig.run (5); // 2400 samples, inside step 0 (6000 samples)
    CHECK (peak (rig.recL) == 0.0f);
    CHECK (p.get (vol) == doctest::Approx (0.8f)); // the registry (host value) is untouched
}

TEST_CASE ("core: on-screen keyboard notes")
{
    CoreRig rig;
    rig.core.queueNote (72, 0.9f, true);
    rig.run (10);
    CHECK (rig.core.lastNote() == 72);
    CHECK (rms (rig.recL) > 0.001f);
    rig.core.queueNote (72, 0.0f, false);
    rig.run (300);
    CHECK (rig.core.activeVoices() == 0);
}

TEST_CASE ("core: block size does not change the sound")
{
    auto render = [] (int blockSize) {
        Core c;
        c.prepare (sampleRate, blockSize);
        std::vector<float> L (9600, 0.0f), R (9600, 0.0f), out;
        int pos = 0;
        bool first = true;
        while (pos < 9600)
        {
            const int n = std::min (blockSize, 9600 - pos);
            OutputBuffers ob;
            ob.left[0] = L.data() + pos;
            ob.right[0] = R.data() + pos;
            auto on = MidiEvent::noteOn (1, 57, 100, 0);
            c.process (ob, n, first ? &on : nullptr, first ? 1 : 0, {});
            first = false;
            pos += n;
        }
        return L;
    };
    const auto a = render (64);
    const auto b = render (512);
    double diff = 0.0;
    for (size_t i = 0; i < a.size(); ++i) diff = std::max (diff, (double) std::abs (a[i] - b[i]));
    CHECK (diff < 1.0e-4);
    // odd host block sizes: same level, finite
    const auto c = render (97);
    CHECK (allFinite (c));
    CHECK (rms (c) == doctest::Approx (rms (a)).epsilon (0.05));
}
