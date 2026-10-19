// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p24: the harmony brain - scale and chord memory across all parts, follow modes, the grid keyboard's layouts.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "harmony/HarmonyBrain.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"
#include "seq/ScaleChord.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct Rig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    Stereo rec;
    Rig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE_MESSAGE (core.params().set (id, v), id); }
    void run (int blocks, std::vector<MidiEvent> midi = {})
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            Transport t;
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
            rec.l.insert (rec.l.end(), l[0].begin(), l[0].end());
            rec.r.insert (rec.r.end(), r[0].begin(), r[0].end());
        }
    }
    std::vector<MidiEvent> chord (int ch, std::initializer_list<int> notes, bool on = true)
    {
        std::vector<MidiEvent> v;
        for (int n : notes) v.push_back (on ? MidiEvent::noteOn (ch, n, 100, 0) : MidiEvent::noteOff (ch, n, 0));
        return v;
    }
};

std::string name (const harmony::Chord& c)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return c.root < 0 ? std::string ("-") : std::string (n[c.root]) + harmony::chordSuffix (c.type);
}
} // namespace

TEST_CASE ("p24 harmony: parameters appended after the performance pads; matrix sources Chord Root / Chord Change / Tension")
{
    auto owner = std::make_unique<Core>();
    const auto& reg = owner->params();
    int expect = reg.indexOf ("pads.freeze.mix") + 1; // after the performance pads (p25)
    for (const char* id : { "harmony.memory", "harmony.hold", "harmony.take", "harmony.clear", "harmony.kb.layout", "harmony.p1.follow",
                            "harmony.p2.follow", "harmony.p3.follow", "harmony.p4.follow" })
        CHECK_MESSAGE (reg.indexOf (id) == expect++, id);
    CHECK (reg.get ("harmony.memory") == 8.0f);
    CHECK (reg.get ("harmony.p1.follow") == 0.0f);
    CHECK (reg.descriptor (reg.indexOf ("harmony.p1.follow")).choices.size() == (size_t) harmony::numFollow);
    CHECK (reg.descriptor (reg.indexOf ("harmony.kb.layout")).choices.size() == (size_t) harmony::numLayouts);
    CHECK (mod::SrcChordRoot == mod::SrcPadHeld + 1);
    CHECK (mod::SrcChordRoot == 29);
    CHECK (mod::SrcTension == 31);
    CHECK (mod::sourceIndexOfKey ("chordroot") == mod::SrcChordRoot);
    CHECK (mod::sourceIndexOfKey ("chordchange") == mod::SrcChordChange);
    CHECK (mod::sourceIndexOfKey ("tension") == mod::SrcTension);
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    CHECK (preset::isExcludedFromProject ("harmony.take"));
    CHECK (preset::isExcludedFromProject ("harmony.clear"));
    CHECK_FALSE (preset::isExcludedFromProject ("harmony.memory"));
}

TEST_CASE ("p24 harmony: chord and key detection")
{
    using namespace harmony;
    auto pcs = [] (std::initializer_list<int> l) { uint16_t m = 0; for (int p : l) m = (uint16_t) (m | (1u << (p % 12))); return m; };
    Chord c;
    REQUIRE (detectChord (pcs ({ 0, 4, 7 }), 0, c));
    CHECK (name (c) == "C");
    REQUIRE (detectChord (pcs ({ 9, 0, 4 }), 9, c));
    CHECK (name (c) == "Am");
    REQUIRE (detectChord (pcs ({ 0, 4, 9 }), 0, c)); // C E A: the A minor chord over C
    CHECK (name (c) == "Am");
    REQUIRE (detectChord (pcs ({ 7, 11, 2, 5 }), 7, c));
    CHECK (name (c) == "G7");
    REQUIRE (detectChord (pcs ({ 2, 5, 9, 0 }), 2, c));
    CHECK (name (c) == "Dm7");
    REQUIRE (detectChord (pcs ({ 0, 7 }), 0, c));
    CHECK (name (c) == "C5");
    REQUIRE (detectChord (pcs ({ 11, 2, 5 }), 11, c));
    CHECK (name (c) == "Bdim");
    REQUIRE (detectChord (pcs ({ 0, 5, 7 }), 0, c));
    CHECK (name (c) == "Csus4");
    CHECK_FALSE (detectChord (pcs ({ 4 }), 4, c));
    CHECK_FALSE (detectChord (0, -1, c));

    std::array<float, 12> w {};
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) w[(size_t) pc] = 1.0f;
    w[0] = 3.0f; w[7] = 2.0f; w[4] = 1.5f;
    int root = -1, scale = -1;
    detectKey (w, -1, -1, root, scale);
    CHECK (root == 0);
    CHECK (scale == scales::Major);
    w.fill (0.0f);
    for (int pc : { 9, 11, 0, 2, 4, 5, 7 }) w[(size_t) pc] = 1.0f;
    w[9] = 3.0f; w[4] = 2.0f; w[0] = 1.5f;
    detectKey (w, -1, -1, root, scale);
    CHECK (root == 9);
    CHECK (scale == scales::Minor);
    // hysteresis: a near tie keeps the previous key
    w.fill (1.0f);
    detectKey (w, 2, scales::Major, root, scale);
    CHECK (root == 2);
    // nothing heard: the previous key
    w.fill (0.0f);
    detectKey (w, 5, scales::Minor, root, scale);
    CHECK (root == 5);
    CHECK (scale == scales::Minor);
}

TEST_CASE ("p24 harmony: grid layouts and mask quantising")
{
    using namespace harmony;
    CHECK (gridNote (LayoutFourths, 0, 0, 48, 0, scales::Major) == 48);
    CHECK (gridNote (LayoutFourths, 1, 0, 48, 0, scales::Major) == 49);
    CHECK (gridNote (LayoutFourths, 0, 1, 48, 0, scales::Major) == 53);
    CHECK (gridNote (LayoutThirds, 0, 1, 48, 0, scales::Major) == 52);
    CHECK (gridNote (LayoutWhole, 3, 2, 48, 0, scales::Major) == 64);
    // In Key (p59: Fourths + In Key, fixed C): scale steps (C major from C3: D, F one row up)
    CHECK (gridNote (LayoutFourths, 1, 0, 48, 0, scales::Major, true) == 50);
    CHECK (gridNote (LayoutFourths, 0, 1, 48, 0, scales::Major, true) == 53);
    CHECK (gridNote (LayoutFourths, 2, 0, 49, 0, scales::Major, true) == 53); // C# is not in the key: the grid starts on D (D E F)
    for (int layout = 0; layout < numLayouts; ++layout)
        for (int row = 0; row < 4; ++row) // the on-screen grid: 4 x 24
            for (int col = 0; col < 24; ++col)
            {
                const int n = gridNote (layout, col, row, 36, 9, scales::Minor);
                CHECK ((n >= 0 && n <= 127));
                for (int fixed = 0; fixed < 2; ++fixed)
                    CHECK (scales::inScale (gridNote (layout, col, row, 36, 9, scales::Minor, true, fixed != 0), 9, scales::Minor));
            }
    // quantising to a chord (A minor: A C E)
    const uint16_t am = chordShape (Minor);
    CHECK (scales::quantiseMask (62, 9, am) == 60); // D -> C (tie resolves down)
    CHECK (scales::quantiseMask (65, 9, am) == 64);
    CHECK (scales::quantiseMask (70, 9, am) == 69);
    CHECK (scales::quantiseMask (59, 9, 1) == 57); // root only
    CHECK (scales::quantiseMask (61, 0, 0xfff) == 61);
}

TEST_CASE ("p24 harmony: part 1 plays a chord, part 2 follows it; follow off is neutral")
{
    Rig rig;
    // nothing heard yet: the key is the global key
    rig.set ("key.root", 2.0f);
    rig.set ("key.scale", (float) scales::Minor);
    rig.run (2);
    auto& h = rig.core.harmony();
    CHECK (h.keyRoot() == 2);
    CHECK (h.keyScale() == scales::Minor);
    CHECK_FALSE (h.heard());

    // part 2 (channel 2) follows the chord; follow off before = the note as played
    rig.run (2, { MidiEvent::noteOn (2, 65, 100, 0) });
    CHECK (rig.core.lastNote() == 65);
    rig.run (2, { MidiEvent::noteOff (2, 65, 0) });
    CHECK (h.heard()); // part 2 did not follow: it was heard
    rig.core.harmony().requestClear();
    rig.run (1);

    rig.set ("harmony.p2.follow", (float) harmony::FollowChord);
    rig.run (12, rig.chord (1, { 57, 60, 64 })); // A minor on part 1 (a chord commits after 40 ms)
    CHECK (h.heard());
    CHECK (name (h.chord()) == "Am");
    CHECK (h.history (0) == h.chord());
    CHECK (rig.core.part (0).modMatrix().sourceValue (mod::SrcChordRoot) == doctest::Approx (9.0f / 11.0f));
    rig.run (2, { MidiEvent::noteOn (2, 66, 100, 0) }); // F# -> E
    CHECK (rig.core.lastNote() == 64);
    rig.run (2, { MidiEvent::noteOff (2, 66, 0) });

    // the chord stays after the keys are released (memory); the next chord replaces it
    rig.run (4, rig.chord (1, { 57, 60, 64 }, false));
    CHECK (name (h.chord()) == "Am");
    rig.run (12, rig.chord (1, { 53, 57, 60 }));
    CHECK (name (h.chord()) == "F");
    CHECK (name (h.history (1)) == "Am");
    CHECK (h.historyCount() == 2);
    rig.run (2, { MidiEvent::noteOn (2, 63, 100, 0) }); // D# -> F (F A C: F is two semitones away, C three)
    CHECK (rig.core.lastNote() == 65);
    rig.run (2, { MidiEvent::noteOff (2, 63, 0) });

    // a part following "Root" snaps to the chord's root; one following "Scale" to the remembered key
    rig.set ("harmony.p2.follow", (float) harmony::FollowRoot);
    rig.run (2, { MidiEvent::noteOn (2, 70, 100, 0) });
    CHECK (rig.core.lastNote() == 65);
    rig.run (2, { MidiEvent::noteOff (2, 70, 0) });
    rig.set ("harmony.p2.follow", (float) harmony::FollowScale);
    rig.run (2, { MidiEvent::noteOn (2, 61, 100, 0) });
    CHECK (scales::inScale (rig.core.lastNote(), h.keyRoot(), h.keyScale()));
    rig.run (2, { MidiEvent::noteOff (2, 61, 0) });

    // a following part is not heard
    rig.run (12, rig.chord (2, { 62, 66, 69 }));
    CHECK (name (h.chord()) == "F");
    rig.run (4, rig.chord (2, { 62, 66, 69 }, false));
    rig.run (4, rig.chord (1, { 53, 57, 60 }, false));

    // follow off: back to the note as played
    rig.set ("harmony.p2.follow", 0.0f);
    rig.run (2, { MidiEvent::noteOn (2, 66, 100, 0) });
    CHECK (rig.core.lastNote() == 66);
    rig.run (2, { MidiEvent::noteOff (2, 66, 0) });

    // tension: notes outside the key; chord change pulse fades
    CHECK (rig.core.part (0).modMatrix().sourceValue (mod::SrcChordChange) < 0.5f);
    CHECK (allFinite (rig.rec.l));
    CHECK (noDenormals (rig.rec.l));
}

TEST_CASE ("p24 harmony: the key follows what is played; hold freezes it; take / clear")
{
    Rig rig;
    rig.set ("key.root", 0.0f);
    rig.set ("key.scale", (float) scales::Major);
    auto& h = rig.core.harmony();
    // an E minor progression: Em - Am - B7 - Em, with the melody notes of the key
    const std::vector<std::vector<int>> prog = { { 52, 55, 59 }, { 57, 60, 64 }, { 59, 63, 66, 69 }, { 52, 55, 59 }, { 52, 55, 59, 64 } };
    for (int round = 0; round < 3; ++round)
        for (const auto& c : prog)
        {
            std::vector<MidiEvent> on, off;
            for (int n : c) { on.push_back (MidiEvent::noteOn (1, n, 100, 0)); off.push_back (MidiEvent::noteOff (1, n, 0)); }
            rig.run (40, on);
            rig.run (2, off);
        }
    CHECK (h.keyRoot() == 4);
    CHECK (h.keyScale() == scales::Minor);
    CHECK (name (h.chord()) == "Em");
    for (int pc = 0; pc < 12; ++pc) CHECK ((h.weight (pc) >= 0.0f && h.weight (pc) <= 1.0f));
    CHECK (h.weight (4) > 0.5f); // E is among the strongest

    // hold: a new chord is not taken
    rig.set ("harmony.hold", 1.0f);
    rig.run (20, rig.chord (1, { 50, 54, 57 }));
    CHECK (name (h.chord()) == "Em");
    rig.run (2, rig.chord (1, { 50, 54, 57 }, false));
    rig.set ("harmony.hold", 0.0f);

    // state round trip (project section "harmony")
    const auto state = rig.core.saveState();
    CHECK (state.find ("\"harmony\"") != std::string::npos);
    Rig other;
    REQUIRE (other.core.loadState (state));
    CHECK (other.core.harmony().toJson() == h.toJson());
    other.run (1);
    CHECK (other.core.harmony().keyRoot() == 4);
    CHECK (other.core.harmony().keyScale() == scales::Minor);
    CHECK (name (other.core.harmony().chord()) == "Em");
    CHECK (other.core.harmony().historyCount() == h.historyCount());

    // clear (trigger parameter, rising edge)
    rig.set ("harmony.clear", 1.0f);
    rig.run (1);
    rig.set ("harmony.clear", 0.0f);
    rig.run (1);
    CHECK_FALSE (h.heard());
    CHECK (h.historyCount() == 0);
    CHECK (h.keyRoot() == 0); // back to the global key
    CHECK (h.chord().root < 0);
    CHECK_FALSE (h.fromJson ("[1,2]"));
    CHECK (h.fromJson ("{\"key\":[99,-4],\"chord\":[3,77],\"profile\":[1,2,3]}")); // clamped / ignored
    CHECK (h.keyRoot() == 11);
    CHECK (h.chord().root < 0);
    CHECK_FALSE (h.heard());
    rig.run (1);
    CHECK (h.keyRoot() == 0); // nothing heard: the global key
    // a project without the section forgets
    rig.run (12, rig.chord (1, { 57, 60, 64 }));
    CHECK (h.heard());
    Rig empty;
    REQUIRE (rig.core.loadState (empty.core.saveState()));
    rig.run (1);
    CHECK_FALSE (h.heard());
}

TEST_CASE ("p24 harmony: grid events reach one part (MPE with expression, plain parts on their channel)")
{
    Rig rig;
    rig.set ("p3.mpe", 1.0f);
    rig.set ("p1.vol", 0.0f);
    MidiEvent bend;
    bend.status = 0xe3; // channel 4
    bend.data1 = 0;
    bend.data2 = 80;
    rig.core.queueGridEvent (2, MidiEvent::noteOn (4, 62, 100, 0));
    rig.core.queueGridEvent (2, bend);
    rig.run (4);
    CHECK (rig.core.part (2).activeVoiceCount() == 1);
    CHECK (rig.core.lastNote() == 62);
    rig.core.queueGridEvent (2, MidiEvent::noteOff (4, 62, 0));
    rig.core.queueGridEvent (1, MidiEvent::noteOn (9, 67, 100, 0)); // part 2 is not MPE: its own channel
    rig.run (4);
    CHECK (rig.core.part (1).activeVoiceCount() == 1);
    CHECK (rig.core.lastNote() == 67);
    rig.core.queueGridEvent (1, MidiEvent::noteOff (9, 67, 0));
    rig.core.queueGridEvent (7, MidiEvent::noteOn (1, 60, 100, 0)); // no such part: ignored
    rig.run (8);
    CHECK (allFinite (rig.rec.l));
    CHECK (noDenormals (rig.rec.l));
    // the grid notes were heard (part 3 and part 2 do not follow)
    CHECK (rig.core.harmony().heard());
}

TEST_CASE ("p24 harmony: part presets keep the follow mode (a global ID, not part of the sound)")
{
    Rig rig;
    rig.set ("harmony.p1.follow", (float) harmony::FollowChord);
    const auto pre = preset::capturePart (rig.core.params(), 0, {});
    for (const auto& [id, v] : pre.params) CHECK (id.find ("harmony") == std::string::npos);
    rig.set ("harmony.p1.follow", (float) harmony::FollowScale);
    preset::applyPart (pre, rig.core.params(), 0);
    CHECK (rig.core.params().get ("harmony.p1.follow") == (float) harmony::FollowScale);
}
