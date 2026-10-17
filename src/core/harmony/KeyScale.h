// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../seq/ScaleChord.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe
{
/**
    p45: the key in one place - key-aware note spelling, chord naming of the notes that sound, Scale Lock.

    * KeyScale: root (0 = C .. 11 = B, "key.root"), scale (scales::Scale, "key.scale"), Scale Lock ("key.lock") and its
      mode ("key.lock.mode": Nearest = every note snaps to the nearest note of the key; White Keys = the white keys
      play the key's degrees upwards from the key's root at C (C4 = the root in octave 4), black keys are silent).
      fromHost: the key comes from the host (read-only on screen). Plain value type, any thread; the processor and the
      core hand it out (Core::keyScale(), OhpeProcessor::keyScale()).
    * Spelling: the parent major key of the scale (C dorian -> B flat major) gives the letters of the scale's notes;
      the other five pitch classes are spelled relative to the tonic (b2, b3, #4, b6, b7). A double accidental falls
      back to the simple sharp / flat name of the key's side (flat keys: flats).
    * Chord naming (chords::detect): all sounding notes (two or more pitch classes) are matched against 24 chord
      qualities (triads, sixths, sevenths, sus, add9, ninths, power chord); sevenths may omit their fifth; the lowest
      note gives the bass: a bass that is not the root makes a slash chord ("D/F#", inversions) - a bass outside the
      chord is tried as a slash bass over the upper notes too ("C/D").
*/
namespace keys
{
enum LockMode { LockNearest = 0, LockWhiteKeys, numLockModes };
std::vector<std::string> lockModeNamesEn();
std::vector<std::string> lockModeNamesDe();

/** Pitch-class mask of a scale (bit 0 = the root; chromatic = all twelve). */
inline uint16_t scaleMask (int scale) noexcept { return scale == scales::Chromatic ? (uint16_t) 0xfff : scales::mask (scale); }

/** The note `degrees` steps of the set above (> 0) or below (< 0) a note (snapped to the set first). 0 .. 127. */
int transposeInKey (int note, int degrees, int root, uint16_t mask) noexcept;
/** White Keys mode: the note a key plays (white keys -> degrees of the set from root at C), -1 = black key (silent). */
inline int whiteKeyNote (int note, int root, uint16_t mask) noexcept { return scales::whiteKeyNote (note, root, mask); }

struct KeyScale
{
    int root = 0;                 ///< pitch class 0 .. 11
    int scale = scales::Major;    ///< scales::Scale
    bool lock = false;            ///< Scale Lock on
    int lockMode = LockNearest;   ///< LockMode
    bool fromHost = false;        ///< the key comes from the host (read-only)

    uint16_t mask() const noexcept { return scaleMask (scale); }
    bool contains (int note) const noexcept { return scales::inMask (note, root, mask()); }
    int snap (int note) const noexcept { return scales::quantiseMask (note, root, mask()); }
    int step (int note, int degrees) const noexcept { return transposeInKey (note, degrees, root, mask()); }
    /** What an incoming note becomes with Scale Lock as set (-1 = silent); lock off: unchanged. */
    int lockNote (int note) const noexcept
    {
        if (! lock) return note;
        return lockMode == LockWhiteKeys ? whiteKeyNote (note, root, mask()) : snap (note);
    }
    bool operator== (const KeyScale& o) const noexcept
    {
        return root == o.root && scale == o.scale && lock == o.lock && lockMode == o.lockMode && fromHost == o.fromHost;
    }
    bool operator!= (const KeyScale& o) const noexcept { return ! (*this == o); }
};

/** Pitch class of the parent major key (major / its modes; minor and its relatives use the relative major). */
int parentMajor (int root, int scale) noexcept;
/** True when the key is written with flats. */
bool flatKey (int root, int scale) noexcept;
/** Letter (0 = C .. 6 = B) and accidental (-2 .. +2) of a pitch class in the key. */
struct Spelling { int letter = 0; int accidental = 0; };
Spelling spell (int pc, int root, int scale) noexcept;
/** Note name of a pitch class in the key ("F#", "Bb"; German: "H" for B, "B" for B flat). */
std::string pitchName (int pc, int root, int scale, bool german);
/** Name with octave ("F#5", "C#-1"; middle C = C4 = 60). */
std::string noteName (int midiNote, int root, int scale, bool german);
} // namespace keys

namespace chords
{
/** Chord qualities (append-only order, see suffix()). */
enum Quality
{
    Major = 0, Minor, Dim, Aug, Sus2, Sus4, Power, Sixth, MinorSixth, Maj7, Dom7, Min7, HalfDim7, Dim7, MinMaj7, Dom7Sus4,
    Add9, MinorAdd9, Dom9, Maj9, Min9, SixNine, Aug7, Dom7Sus2, numQualities
};
uint16_t shape (int quality) noexcept;   ///< pitch classes relative to the root
const char* suffix (int quality) noexcept; ///< "", "m", "dim", "aug", "sus2", "sus4", "5", "6", "m6", "maj7", "7", ...

struct Name
{
    int root = -1;      ///< pitch class, -1 = no chord
    int bass = -1;      ///< pitch class of the lowest note
    int quality = Major;
    int inversion = 0;  ///< 0 = root position, 1 = third in the bass, 2 = fifth, 3 = seventh; -1 = bass outside the chord
    bool valid() const noexcept { return root >= 0; }
    bool slash() const noexcept { return valid() && bass >= 0 && bass != root; }
    bool operator== (const Name& o) const noexcept { return root == o.root && bass == o.bass && quality == o.quality; }
    bool operator!= (const Name& o) const noexcept { return ! (*this == o); }
};

/** Sounding notes as a 128-bit set (bit n of words[n / 64] = MIDI note n). */
using NoteSet = std::array<uint64_t, 2>;
inline void add (NoteSet& s, int note) noexcept
{
    if (note >= 0 && note < 128) s[(size_t) (note >> 6)] |= (uint64_t) 1 << (note & 63);
}
inline bool has (const NoteSet& s, int note) noexcept { return note >= 0 && note < 128 && ((s[(size_t) (note >> 6)] >> (note & 63)) & 1u) != 0; }
int count (const NoteSet& s) noexcept;
int lowest (const NoteSet& s) noexcept; ///< -1 = empty
uint16_t pitchClasses (const NoteSet& s) noexcept;

/** Best name for a pitch-class set with its bass (-1 = none); invalid when nothing fits (or fewer than two pcs). */
Name detect (uint16_t pitchClasses, int bassPc) noexcept;
Name detect (const NoteSet& notes) noexcept;
/** Chord symbol in the key ("F#m7", "D/F#", "Bbmaj7"; German: "H7", "B/D"). Empty for no chord. */
std::string symbol (const Name& c, int keyRoot, int keyScale, bool german);
/** Spoken position ("root position", "1st inversion", ...; German too); empty for no chord. */
std::string inversionName (const Name& c, bool german);
} // namespace chords
} // namespace ohpe
