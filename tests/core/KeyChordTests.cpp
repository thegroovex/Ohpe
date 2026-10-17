// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p45: chord naming of the sounding notes, key-aware spelling, Scale Lock (Nearest / White Keys), sequencer
// transpose in scale degrees, the KeyScale API.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "harmony/KeyScale.h"
#include "seq/StepSequencer.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
std::string name (std::initializer_list<int> notes, int keyRoot = 0, int keyScale = scales::Major, bool german = false)
{
    chords::NoteSet s {};
    for (int n : notes) chords::add (s, n);
    return chords::symbol (chords::detect (s), keyRoot, keyScale, german);
}

struct Rig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    double ppq = 0.0;
    Rig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
    }
    void set (const std::string& id, float v) { REQUIRE_MESSAGE (core.params().set (id, v), id); }
    void run (int blocks, std::vector<MidiEvent> midi = {}, bool playing = false)
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            Transport t;
            t.bpm = 120.0;
            t.playing = playing;
            t.ppqAtBlockStart = ppq;
            core.process (out, block, b == 0 ? midi.data() : nullptr, b == 0 ? (int) midi.size() : 0, t);
            if (playing) ppq += (double) block / sampleRate * 2.0;
            for (auto& v : l) for (float x : v) REQUIRE (std::isfinite (x));
        }
    }
    std::vector<int> sounding (int part = -1) const
    {
        std::vector<int> v;
        const auto s = core.soundingNotes (part);
        for (int n = 0; n < 128; ++n)
            if (chords::has (s, n)) v.push_back (n);
        return v;
    }
};
std::vector<MidiEvent> notes (int ch, std::initializer_list<int> ns, bool on = true)
{
    std::vector<MidiEvent> v;
    for (int n : ns) v.push_back (on ? MidiEvent::noteOn (ch, n, 100, 0) : MidiEvent::noteOff (ch, n, 0));
    return v;
}
} // namespace

TEST_CASE ("p45 chords: naming table - triads, sixths, sevenths, sus, add9, ninths, power chord")
{
    struct Case { std::initializer_list<int> notes; const char* name; };
    const Case cases[] = {
        { { 60, 64, 67 }, "C" },           { { 60, 63, 67 }, "Cm" },        { { 60, 63, 66 }, "Cdim" },
        { { 60, 64, 68 }, "Caug" },        { { 60, 62, 67 }, "Csus2" },     { { 60, 65, 67 }, "Csus4" },
        { { 48, 55 }, "C5" },              { { 48, 55, 60 }, "C5" },        { { 60, 64, 67, 69 }, "C6" },
        { { 60, 63, 67, 69 }, "Cm6" },     { { 60, 64, 67, 71 }, "Cmaj7" }, { { 60, 64, 67, 70 }, "C7" },
        { { 60, 63, 67, 70 }, "Cm7" },     { { 60, 63, 66, 70 }, "Cm7b5" }, { { 60, 63, 66, 69 }, "Cdim7" },
        { { 60, 63, 67, 71 }, "Cm(maj7)" }, { { 60, 65, 67, 70 }, "C7sus4" }, { { 60, 62, 64, 67 }, "Cadd9" },
        { { 60, 62, 63, 67 }, "Cmadd9" },  { { 60, 64, 67, 70, 74 }, "C9" }, { { 60, 64, 67, 71, 74 }, "Cmaj9" },
        { { 60, 63, 67, 70, 74 }, "Cm9" }, { { 60, 64, 67, 69, 74 }, "C6/9" }, { { 60, 64, 68, 70 }, "Caug7" },
        { { 60, 62, 67, 70 }, "C7sus2" },
        { { 60, 64, 70 }, "C7" },          // a seventh without its fifth
        { { 57, 60, 64, 67 }, "Am7" },     // the bass decides between Am7 and C6
        { { 54, 57, 61, 64 }, "F#m7" },
    };
    for (const auto& c : cases)
        CHECK_MESSAGE (name (c.notes) == c.name, c.name);
    CHECK (name ({}) == "");
    CHECK (name ({ 60 }) == "");          // one note is no chord
    CHECK (name ({ 60, 72 }) == "");      // octaves neither
    CHECK (name ({ 60, 61, 62 }) == "");  // a cluster has no name
}

TEST_CASE ("p45 chords: inversions and slash chords")
{
    CHECK (name ({ 52, 55, 60 }) == "C/E");      // 1st inversion
    CHECK (name ({ 55, 60, 64 }) == "C/G");      // 2nd inversion
    CHECK (name ({ 53, 59, 62, 67 }) == "G7/F"); // 3rd inversion
    CHECK (name ({ 54, 62, 66, 69 }, 7) == "D/F#");
    CHECK (name ({ 50, 60, 64, 67 }) == "C/D");  // a bass outside the chord
    CHECK (name ({ 55, 57, 60, 64 }) == "Am7/G");
    chords::NoteSet s {};
    for (int n : { 52, 55, 60 }) chords::add (s, n);
    const auto c = chords::detect (s);
    CHECK (c.root == 0);
    CHECK (c.bass == 4);
    CHECK (c.inversion == 1);
    CHECK (c.slash());
    CHECK (chords::inversionName (c, false) == "1st inversion");
    CHECK (chords::inversionName (c, true) == "1. Umkehrung");
    chords::NoteSet t {};
    for (int n : { 50, 60, 64, 67 }) chords::add (t, n);
    CHECK (chords::detect (t).inversion == -1);
    // the bass doubled higher up is part of the chord (root position)
    chords::NoteSet u {};
    for (int n : { 48, 52, 55, 60 }) chords::add (u, n);
    CHECK (chords::detect (u).inversion == 0);
}

TEST_CASE ("p45 spelling: sharps and flats by key, German H / B, octaves")
{
    auto row = [] (int root, int scale, bool de = false) {
        std::string s;
        for (int pc = 0; pc < 12; ++pc) s += keys::pitchName (pc, root, scale, de) + (pc < 11 ? " " : "");
        return s;
    };
    CHECK (row (0, scales::Major) == "C C# D Eb E F F# G Ab A Bb B");
    CHECK (row (5, scales::Major) == "C Db D Eb E F F# G Ab A Bb B");  // F major: B flat
    CHECK (row (3, scales::Major) == "C Db D Eb E F Gb G Ab A Bb B");  // E flat major
    CHECK (row (6, scales::Major) == "C C# D D# E E# F# G G# A A# B"); // F sharp major: E sharp
    CHECK (row (9, scales::Minor) == "C C# D D# E F F# G G# A Bb B");  // A minor: G sharp, B flat (Neapolitan)
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 })                             // D dorian: the letters of C major
        CHECK (keys::pitchName (pc, 2, scales::Dorian, false) == keys::pitchName (pc, 0, scales::Major, false));
    CHECK (keys::pitchName (3, 2, scales::Dorian, false) == "Eb");      // a minor-type mode lowers its 2nd
    CHECK (row (4, scales::Major) == "C C# D D# E F F# G G# A A# B");
    CHECK (row (10, scales::Major, true) == "C Db D Eb E F Gb G Ab A B H");
    CHECK (keys::flatKey (5, scales::Major));
    CHECK (keys::flatKey (0, scales::Minor) == true); // C minor: three flats
    CHECK_FALSE (keys::flatKey (7, scales::Major));
    CHECK (keys::parentMajor (2, scales::Dorian) == 0);
    CHECK (keys::parentMajor (9, scales::Minor) == 0);
    CHECK (keys::noteName (78, 6, scales::Minor, false) == "F#5");
    CHECK (keys::noteName (78, 3, scales::Major, false) == "Gb5");
    CHECK (keys::noteName (1, 0, scales::Major, false) == "C#-1");
    CHECK (keys::noteName (0, 0, scales::Major, false) == "C-1");
    CHECK (keys::noteName (127, 0, scales::Major, false) == "G9");
    CHECK (keys::noteName (71, 0, scales::Major, true) == "H4");
    CHECK (keys::noteName (70, 5, scales::Major, true) == "B4");
    CHECK (keys::noteName (60, 1, scales::Lydian, false).find ("4") != std::string::npos); // B# would be octave 3
    // chord symbols in the key
    CHECK (name ({ 58, 62, 65, 69 }, 5) == "Bbmaj7");
    CHECK (name ({ 58, 62, 65, 69 }, 5, scales::Major, true) == "Bmaj7");
    CHECK (name ({ 59, 63, 66, 69 }, 4, scales::Major, true) == "H7");
    CHECK (name ({ 61, 65, 68 }, 8) == "Db");
    CHECK (name ({ 61, 65, 68 }, 9) == "C#");
    CHECK (name ({ 54, 58, 61 }, 3) == "Gb");
    CHECK (name ({ 54, 58, 61 }, 11) == "F#");
    for (int root = 0; root < 12; ++root) // every name is one letter with at most one accidental
        for (int scale = 0; scale < scales::numScales; ++scale)
            for (int pc = 0; pc < 12; ++pc)
            {
                const auto n = keys::pitchName (pc, root, scale, false);
                CHECK (n.size() >= 1);
                CHECK (n.size() <= 2);
                const auto sp = keys::spell (pc, root, scale);
                CHECK (((((sp.letter == 0 ? 0 : sp.letter == 1 ? 2 : sp.letter == 2 ? 4 : sp.letter == 3 ? 5 : sp.letter == 4 ? 7 : sp.letter == 5 ? 9 : 11)
                          + sp.accidental) % 12 + 12) % 12) == pc);
            }
}

TEST_CASE ("p45 scale lock: transpose in degrees, white keys")
{
    const auto cMaj = keys::scaleMask (scales::Major);
    CHECK (keys::transposeInKey (60, 2, 0, cMaj) == 64);
    CHECK (keys::transposeInKey (60, 7, 0, cMaj) == 72);
    CHECK (keys::transposeInKey (60, -1, 0, cMaj) == 59);
    CHECK (keys::transposeInKey (64, -3, 0, cMaj) == 59);
    CHECK (keys::transposeInKey (61, 0, 0, cMaj) == 60); // snaps first (ties down)
    CHECK (keys::transposeInKey (126, 5, 0, cMaj) == 127 - 0 * 1 - (scales::inScale (127, 0, scales::Major) ? 0 : 1));
    CHECK (keys::transposeInKey (2, -9, 0, cMaj) == 0);
    for (int n = 0; n < 128; ++n)
        for (int d = -10; d <= 10; ++d)
        {
            const int t = keys::transposeInKey (n, d, 0, cMaj);
            CHECK (t >= 0);
            CHECK (t <= 127);
            CHECK (scales::inScale (t, 0, scales::Major));
        }
    // white keys: D dorian from D4 at C4; black keys silent
    const auto dDor = keys::scaleMask (scales::Dorian);
    const int expect[] = { 62, -1, 64, -1, 65, 67, -1, 69, -1, 71, -1, 72, 74 };
    for (int i = 0; i < 13; ++i) CHECK (keys::whiteKeyNote (60 + i, 2, dDor) == expect[i]);
    // pentatonic: five degrees per octave of white keys
    const auto pent = keys::scaleMask (scales::MajorPentatonic);
    CHECK (keys::whiteKeyNote (60, 0, pent) == 60);
    CHECK (keys::whiteKeyNote (69, 0, pent) == 72); // the 6th white key = the octave
    CHECK (keys::whiteKeyNote (59, 0, pent) == 57); // below C4: downwards
    CHECK (keys::whiteKeyNote (60, 9, keys::scaleMask (scales::Minor)) == 57); // A minor: the root nearest middle C
    for (int n = 0; n < 128; ++n)
    {
        const int w = keys::whiteKeyNote (n, 0, cMaj);
        CHECK (w >= -1);
        CHECK (w <= 127);
    }
    keys::KeyScale k;
    CHECK (k.lockNote (61) == 61); // lock off: neutral
    k.lock = true;
    CHECK (k.lockNote (61) == 60);
    k.lockMode = keys::LockWhiteKeys;
    CHECK (k.lockNote (61) == -1);
    CHECK (k.lockNote (62) == 62);
}

TEST_CASE ("p45 scale lock: parameters appended at the end, KeyScale from the core, host key read-only")
{
    Rig rig;
    const auto& reg = rig.core.params();
    CHECK (reg.indexOf ("key.lock") == reg.indexOf ("metro.tap") + 1);
    CHECK (reg.indexOf ("key.lock.mode") == reg.indexOf ("key.lock") + 1);
    CHECK (reg.get ("key.lock") == 0.0f);
    CHECK (reg.descriptor (reg.indexOf ("key.lock.mode")).choices.size() == (size_t) keys::numLockModes);
    auto k = rig.core.keyScale();
    CHECK (k.root == 0);
    CHECK (k.scale == scales::Major);
    CHECK_FALSE (k.lock);
    CHECK_FALSE (k.fromHost);
    rig.set ("key.root", 6.0f);
    rig.set ("key.scale", (float) scales::Dorian);
    rig.set ("key.lock", 1.0f);
    rig.set ("key.lock.mode", 1.0f);
    k = rig.core.keyScale();
    CHECK (k.root == 6);
    CHECK (k.scale == scales::Dorian);
    CHECK (k.lock);
    CHECK (k.lockMode == keys::LockWhiteKeys);
    rig.core.setHostKey (9, scales::Minor);
    k = rig.core.keyScale();
    CHECK (k.fromHost);
    CHECK (k.root == 9);
    CHECK (k.scale == scales::Minor);
    rig.core.setHostKey (-1, 0);
    CHECK_FALSE (rig.core.keyScale().fromHost);
}

TEST_CASE ("p45 scale lock: off is neutral, Nearest snaps live notes, White Keys maps them, the chord is read from what sounds")
{
    Rig rig;
    // off: the notes pass unchanged
    rig.run (2, notes (1, { 61, 66 }));
    CHECK (rig.sounding (0) == std::vector<int> { 61, 66 });
    rig.run (2, notes (1, { 61, 66 }, false));
    CHECK (rig.sounding().empty());
    // Nearest in C major: C# -> C, F# -> F
    rig.set ("key.lock", 1.0f);
    rig.run (2, notes (1, { 61, 66 }));
    CHECK (rig.sounding (0) == std::vector<int> { 60, 65 });
    rig.run (2, notes (1, { 61, 66 }, false));
    CHECK (rig.sounding().empty());
    // White Keys in A minor: C4 D4 E4 = A3 B3 C4; a black key is silent; its note-off finds nothing
    rig.set ("key.root", 9.0f);
    rig.set ("key.scale", (float) scales::Minor);
    rig.set ("key.lock.mode", 1.0f);
    rig.run (2, notes (1, { 60, 62, 64, 61 }));
    CHECK (rig.sounding (0) == std::vector<int> { 57, 59, 60 });
    rig.run (2, notes (1, { 60, 62, 64, 61 }, false));
    CHECK (rig.sounding().empty());
    // white keys C E G -> A C E: A minor; with the bass changed: the chord the parts play
    rig.run (2, notes (1, { 60, 64, 67 }));
    CHECK (chords::symbol (chords::detect (rig.core.soundingNotes()), 9, scales::Minor, false) == "Am");
    rig.run (2, notes (1, { 60, 64, 67 }, false));
    // lock off again: neutral
    rig.set ("key.lock", 0.0f);
    rig.run (2, notes (1, { 61 }));
    CHECK (rig.sounding (0) == std::vector<int> { 61 });
    rig.run (2, notes (1, { 61 }, false));
    CHECK (rig.sounding().empty());
}

TEST_CASE ("p45 sounding notes: all notes off clears, the one-finger chord counts, the reset forgets")
{
    Rig rig;
    rig.set ("p1.chord", (float) scales::Triad); // one-finger triad in C major
    rig.run (2, notes (1, { 62 }));
    CHECK (rig.sounding (0) == std::vector<int> { 62, 65, 69 });
    CHECK (chords::symbol (chords::detect (rig.core.soundingNotes()), 0, scales::Major, false) == "Dm");
    rig.run (1, { MidiEvent::controller (1, 123, 0, 0) });
    CHECK (rig.sounding (0).empty());
    rig.run (2, notes (1, { 62 }));
    rig.core.reset();
    CHECK (rig.sounding().empty());
}

TEST_CASE ("p45 scale lock: the step sequencer's transpose moves in scale degrees")
{
    auto play = [] (bool lock, int transpose) {
        StepSequencer seq;
        seq.prepare (sampleRate, 512);
        seq.setParam (StepSequencer::Length, 1.0f);
        seq.setParam (StepSequencer::Transpose, (float) transpose);
        StepSequencer::Step s;
        s.on = true;
        s.note = 64; // E4
        seq.setStep (0, s);
        seq.setKeyLock (lock, 0, keys::scaleMask (scales::Major));
        SequencerOutput out;
        std::vector<ParamLockEvent> locks;
        locks.reserve (64);
        out.locks = &locks;
        out.maxLocks = 64;
        Transport tr;
        tr.bpm = 120.0;
        tr.playing = true;
        out.clear();
        seq.process (tr, 512, out);
        for (const auto& e : out.events)
            if (e.isNoteOn()) return (int) e.data1;
        return -1;
    };
    CHECK (play (false, 2) == 66); // semitones
    CHECK (play (true, 2) == 67);  // two degrees of C major above E: G
    CHECK (play (true, -2) == 60);
    CHECK (play (true, 0) == 64);
    CHECK (play (false, 0) == 64);
}
