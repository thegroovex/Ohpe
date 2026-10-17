// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe
{
/**
    Scales and one-finger chords. The scale list is the value of the "key.scale" parameter and
    the chord list the value of "pN.chord": both append-only.
*/
namespace scales
{
enum Scale
{
    Chromatic = 0, Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, HarmonicMinor, MelodicMinor,
    MajorPentatonic, MinorPentatonic, Blues, WholeTone, numScales
};

/** Bit mask of the scale's pitch classes relative to the root (bit 0 = root). */
inline uint16_t mask (int scale) noexcept
{
    static constexpr uint16_t m[numScales] = {
        0xfff,                                   // chromatic
        0b101010110101, 0b010110101101,          // major, natural minor
        0b011010101101, 0b010110101011,          // dorian, phrygian
        0b101011010101, 0b011010110101,          // lydian, mixolydian
        0b010101101011, 0b100110101101,          // locrian, harmonic minor
        0b101010101101,                          // melodic minor (ascending)
        0b001010010101, 0b010010101001,          // major / minor pentatonic
        0b010011101001, 0b010101010101           // blues, whole tone
    };
    return m[std::clamp (scale, 0, numScales - 1)];
}

inline std::vector<std::string> namesEn()
{
    return { "Chromatic", "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
             "Harmonic Minor", "Melodic Minor", "Major Pentatonic", "Minor Pentatonic", "Blues", "Whole Tone" };
}
inline std::vector<std::string> namesDe()
{
    return { "Chromatisch", "Dur", "Moll", "Dorisch", "Phrygisch", "Lydisch", "Mixolydisch", "Lokrisch",
             "Harmonisch Moll", "Melodisch Moll", "Dur-Pentatonik", "Moll-Pentatonik", "Blues", "Ganzton" };
}

inline bool inMask (int note, int root, uint16_t m) noexcept
{
    const int pc = ((note - root) % 12 + 12) % 12;
    return ((m >> pc) & 1u) != 0;
}

inline bool inScale (int note, int root, int scale) noexcept { return inMask (note, root, mask (scale)); }

/** p24: nearest note of a pitch-class set (mask relative to root; ties resolve downwards). 0xfff = no change. */
inline int quantiseMask (int note, int root, uint16_t m) noexcept
{
    if ((m & 0xfff) == 0xfff || (m & 0xfff) == 0) return std::clamp (note, 0, 127);
    for (int d = 0; d < 12; ++d)
    {
        if (note - d >= 0 && inMask (note - d, root, m)) return note - d;
        if (note + d <= 127 && inMask (note + d, root, m)) return note + d;
    }
    return std::clamp (note, 0, 127);
}

/** Nearest note of the scale (ties resolve downwards). */
inline int quantise (int note, int root, int scale) noexcept
{
    if (scale == Chromatic) return std::clamp (note, 0, 127);
    return quantiseMask (note, root, mask (scale));
}

/** The note `degrees` steps of the set above an in-set note (p24: any pitch-class set). */
inline int degreeAboveMask (int note, int degrees, int root, uint16_t m) noexcept
{
    if ((m & 0xfff) == 0) return note + degrees;
    int n = note;
    for (int k = 0; k < degrees; ++k)
    {
        const int from = n++;
        while (! inMask (n, root, m) && n < from + 12) ++n; // the next note of the set lies within an octave
    }
    return n;
}

/** The note `degrees` scale steps above an in-scale note. */
inline int degreeAbove (int note, int degrees, int root, int scale) noexcept { return degreeAboveMask (note, degrees, root, mask (scale)); }

/**
    p45: Scale Lock White Keys - the note a key plays: the white keys play the degrees of the set (mask relative to
    root) upwards, C4 (60) = the root nearest middle C; black keys give -1 (silent), so does a note beyond 0 .. 127.
*/
inline int whiteKeyNote (int note, int root, uint16_t m) noexcept
{
    if (note < 0 || note > 127) return -1;
    static constexpr int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const int wi = whiteIndex[note % 12];
    if (wi < 0) return -1;
    m &= 0xfff;
    if (m == 0) m = 0xfff;
    std::array<int, 12> steps {};
    int n = 0;
    for (int i = 0; i < 12; ++i)
        if (((m >> i) & 1u) != 0) steps[(size_t) n++] = i;
    root = ((root % 12) + 12) % 12;
    const int rel = (note / 12) * 7 + wi - 35; // white keys counted from C4
    const int oct = rel >= 0 ? rel / n : -((-rel + n - 1) / n), d = rel - oct * n;
    const int out = 60 + (root > 6 ? root - 12 : root) + oct * 12 + steps[(size_t) d];
    return out >= 0 && out <= 127 ? out : -1;
}

enum Chord { Off = 0, Triad, Seventh, Sus2, Sus4, Power, Octave, Sixth, Add9, numChords };

inline std::vector<std::string> chordNamesEn() { return { "Off", "Triad", "Seventh", "Sus2", "Sus4", "Power", "Octave", "Sixth", "Add9" }; }
inline std::vector<std::string> chordNamesDe() { return { "Aus", "Dreiklang", "Septakkord", "Sus2", "Sus4", "Quinte", "Oktave", "Sexte", "Add9" }; }

/**
    Chord notes for one key, root first. In a scale the chord is diatonic (stacked scale steps,
    e.g. a triad on the 2nd degree of C major is minor); with the chromatic scale or a note
    outside the scale it uses fixed major-type intervals. Returns the number of notes (<= 5).
*/
inline int buildMask (int note, int chord, int root, uint16_t m, std::array<int, 5>& out) noexcept
{
    int n = 0;
    auto add = [&] (int x) { if (x >= 0 && x <= 127 && n < 5) out[(size_t) n++] = x; };
    add (note);
    const bool diatonic = (m & 0xfff) != 0xfff && inMask (note, root, m);
    auto deg = [&] (int d, int fallback) { add (diatonic ? degreeAboveMask (note, d, root, m) : note + fallback); };
    switch (chord)
    {
        case Triad: deg (2, 4); deg (4, 7); break;
        case Seventh: deg (2, 4); deg (4, 7); deg (6, 10); break;
        case Sus2: deg (1, 2); deg (4, 7); break;
        case Sus4: deg (3, 5); deg (4, 7); break;
        case Power: add (note + 7); add (note + 12); break;
        case Octave: add (note + 12); break;
        case Sixth: deg (2, 4); deg (4, 7); deg (5, 9); break;
        case Add9: deg (2, 4); deg (4, 7); deg (8, 14); break;
        default: break;
    }
    return n;
}

/** Same as buildMask with the scale's pitch classes (p24: the set may be a chord or any other set too). */
inline int build (int note, int chord, int root, int scale, std::array<int, 5>& out) noexcept
{
    return buildMask (note, chord, root, scale == Chromatic ? (uint16_t) 0xfff : mask (scale), out);
}
} // namespace scales

/**
    Applies scale quantisation and one-finger chords to the notes going into one part and
    remembers what each key produced, so its note-off releases exactly those notes even if the
    settings changed in between. Audio thread, no allocation.
*/
class NoteShaper
{
public:
    void set (bool quantiseOn, int chordType, int keyRoot, int keyScale) noexcept
    {
        const uint16_t m = keyScale == scales::Chromatic ? (uint16_t) 0xfff : scales::mask (keyScale);
        setMask (quantiseOn, chordType, keyRoot, m, keyRoot, m);
    }
    /** p24: quantise to one pitch-class set (mask relative to quantRoot; the harmony brain's follow) and build the
        one-finger chords in another (the key). */
    void setMask (bool quantiseOn, int chordType, int quantRoot, uint16_t quantMask, int chordRoot, uint16_t chordMask) noexcept
    {
        quantise = quantiseOn;
        chord = chordType;
        root = quantRoot;
        setMaskBits = quantMask;
        chordRootPc = chordRoot;
        chordMaskBits = chordMask;
    }

    /** p45: Scale Lock White Keys - the white keys play the degrees of the key (from its root at C), black keys are
        silent; applied before quantising (process (e, emit, false) skips it, e.g. for the in-key grid). */
    void setWhiteKeys (bool on, int keyRoot, uint16_t keyMask) noexcept
    {
        whiteKeys = on;
        whiteRoot = keyRoot;
        whiteMask = keyMask;
    }

    bool active() const noexcept { return quantise || chord != scales::Off || whiteKeys; }

    void reset() noexcept
    {
        for (auto& m : map) m.count = 0;
        refs.fill (0);
    }

    /** Calls emit (MidiEvent) for every resulting event (note on / off); other messages pass. */
    template <typename F>
    void process (const MidiEvent& e, F&& emit, bool mapWhiteKeys = true) noexcept
    {
        if (! e.isNoteOn() && ! e.isNoteOff()) { emit (e); return; }
        auto& m = map[(size_t) e.data1];
        const int ch = e.channel();
        if (e.isNoteOff())
        {
            for (int k = 0; k < m.count; ++k)
            {
                const int note = m.notes[(size_t) k];
                if (refs[(size_t) note] > 0 && --refs[(size_t) note] == 0) emit (MidiEvent::noteOff (ch, note, e.sampleOffset));
            }
            m.count = 0;
            return;
        }
        if (m.count > 0) // retrigger of a key still down: release what it played
            process (MidiEvent::noteOff (ch, e.data1, e.sampleOffset), emit);
        int in = e.data1;
        if (whiteKeys && mapWhiteKeys)
        {
            in = whiteKeyMap (in);
            if (in < 0) return; // a black key: silent (its note-off finds nothing to release)
        }
        const int base = quantise ? scales::quantiseMask (in, root, setMaskBits) : in;
        std::array<int, 5> notes {};
        const int n = chord != scales::Off ? scales::buildMask (base, chord, chordRootPc, chordMaskBits, notes) : (notes[0] = base, 1);
        m.count = 0;
        for (int k = 0; k < n; ++k)
        {
            const int note = notes[(size_t) k];
            bool dup = false;
            for (int j = 0; j < m.count; ++j) dup |= m.notes[(size_t) j] == note;
            if (dup) continue;
            m.notes[(size_t) m.count++] = note;
            ++refs[(size_t) note];
            emit (MidiEvent::noteOn (ch, note, e.data2, e.sampleOffset));
        }
    }

private:
    struct Mapping { std::array<int, 5> notes {}; int count = 0; };
    std::array<Mapping, 128> map {};
    std::array<int, 128> refs {};
    int whiteKeyMap (int note) const noexcept { return scales::whiteKeyNote (note, whiteRoot, whiteMask); }
    bool quantise = false, whiteKeys = false;
    int chord = scales::Off, root = 0, chordRootPc = 0, whiteRoot = 0;
    uint16_t whiteMask = 0xfff;
    uint16_t setMaskBits = scales::mask (scales::Major), chordMaskBits = scales::mask (scales::Major);
};
} // namespace ohpe
