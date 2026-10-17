// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "KeyScale.h"

#include <algorithm>
#include <cstdlib>

namespace ohpe
{
namespace
{
int pcOf (int n) noexcept { return ((n % 12) + 12) % 12; }
int popcount12 (uint16_t m) noexcept
{
    int n = 0;
    for (m &= 0xfff; m != 0; m = (uint16_t) (m & (m - 1))) ++n;
    return n;
}
constexpr uint16_t bits (std::initializer_list<int> pcs)
{
    uint16_t m = 0;
    for (int p : pcs) m = (uint16_t) (m | (1u << p));
    return m;
}
uint16_t rotateDown (uint16_t absMask, int root) noexcept // absolute pitch classes -> relative to root
{
    root = pcOf (root);
    absMask &= 0xfff;
    return (uint16_t) (((absMask >> root) | (absMask << (12 - root))) & 0xfff);
}
constexpr int naturalPc[7] = { 0, 2, 4, 5, 7, 9, 11 };
constexpr int majorSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
} // namespace

namespace keys
{
std::vector<std::string> lockModeNamesEn() { return { "Nearest", "White Keys" }; }
std::vector<std::string> lockModeNamesDe() { return { "Nächster Ton", "Weiße Tasten" }; }

int transposeInKey (int note, int degrees, int root, uint16_t mask) noexcept
{
    mask &= 0xfff;
    if (mask == 0) return std::clamp (note + degrees, 0, 127);
    int n = scales::quantiseMask (std::clamp (note, 0, 127), root, mask);
    if (degrees > 0) n = scales::degreeAboveMask (n, degrees, root, mask);
    for (int k = 0; k < -degrees; ++k)
    {
        const int from = n--;
        while (! scales::inMask (n, root, mask) && n > from - 12) --n;
    }
    if (n > 127) { n = 127; while (n > 0 && ! scales::inMask (n, root, mask)) --n; }
    if (n < 0) { n = 0; while (n < 127 && ! scales::inMask (n, root, mask)) ++n; }
    return n;
}

namespace
{
/** Offset of the parent major key above the tonic, and the tonic's degree (0-based) in it. */
void parentOf (int scale, int& offset, int& degree) noexcept
{
    switch (scale)
    {
        case scales::Minor: case scales::HarmonicMinor: case scales::MelodicMinor:
        case scales::MinorPentatonic: case scales::Blues: offset = 3; degree = 5; break;
        case scales::Dorian: offset = 10; degree = 1; break;
        case scales::Phrygian: offset = 8; degree = 2; break;
        case scales::Lydian: offset = 7; degree = 3; break;
        case scales::Mixolydian: offset = 5; degree = 4; break;
        case scales::Locrian: offset = 1; degree = 6; break;
        default: offset = 0; degree = 0; break; // major, major pentatonic, chromatic, whole tone
    }
}
/** Letter of each major key (0 C, 1 Db, 2 D, 3 Eb, 4 E, 5 F, 6 F#, 7 G, 8 Ab, 9 A, 10 Bb, 11 B). */
constexpr int majorKeyLetter[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
int accidentalOf (int pc, int letter) noexcept
{
    int a = pcOf (pc - naturalPc[letter]);
    return a > 6 ? a - 12 : a;
}
Spelling simple (int pc, bool flats) noexcept
{
    static constexpr int sharpLetter[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    static constexpr int flatLetter[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
    const int l = flats ? flatLetter[pc] : sharpLetter[pc];
    return { l, accidentalOf (pc, l) };
}
} // namespace

int parentMajor (int root, int scale) noexcept
{
    int off = 0, deg = 0;
    parentOf (scale, off, deg);
    return pcOf (root + off);
}

bool flatKey (int root, int scale) noexcept
{
    const int p = parentMajor (root, scale);
    return p == 1 || p == 3 || p == 5 || p == 8 || p == 10;
}

Spelling spell (int pc, int root, int scale) noexcept
{
    pc = pcOf (pc);
    root = pcOf (root);
    int off = 0, deg = 0;
    parentOf (scale, off, deg);
    const int parent = pcOf (root + off);
    const int pl = majorKeyLetter[parent];
    Spelling s;
    const int inParent = pcOf (pc - parent);
    int k = -1;
    for (int i = 0; i < 7; ++i)
        if (majorSteps[i] == inParent) k = i;
    if (k >= 0) // a note of the parent major key: its letter
    {
        s.letter = (pl + k) % 7;
    }
    else // the five others: a white key keeps its plain name; else relative to the tonic (#1 / b2, b3, #4, b6, b7)
    {
        for (int l = 0; l < 7; ++l)
            if (naturalPc[l] == pc) return { l, 0 };
        static constexpr int degreeOf[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
        const int tl = (pl + deg) % 7;
        const int rel = pcOf (pc - root);
        // a major-type mode (ionian, lydian, mixolydian) raises its tonic (C#: a secondary dominant's third); a minor-type mode lowers its 2nd
        s.letter = (rel == 1 && (deg == 0 || deg == 3 || deg == 4)) ? tl : (tl + degreeOf[rel]) % 7;
    }
    s.accidental = accidentalOf (pc, s.letter);
    if (std::abs (s.accidental) > 1 && k < 0) return simple (pc, flatKey (root, scale));
    return s;
}

std::string pitchName (int pc, int root, int scale, bool german)
{
    const auto s = spell (pc, root, scale);
    static const char* letters = "CDEFGAB";
    std::string acc;
    for (int i = 0; i < std::abs (s.accidental); ++i) acc += s.accidental > 0 ? "#" : "b";
    if (german && s.letter == 6) // German: H = B, B = B flat
    {
        if (s.accidental == 0) return "H";
        if (s.accidental == -1) return "B";
        if (s.accidental > 0) return "H" + acc;
        return "B" + acc.substr (1);
    }
    return std::string (1, letters[s.letter]) + acc;
}

std::string noteName (int midiNote, int root, int scale, bool german)
{
    midiNote = std::clamp (midiNote, 0, 127);
    const int pc = midiNote % 12;
    const auto s = spell (pc, root, scale);
    int octave = midiNote / 12 - 1;
    if (s.letter == 6 && pc < 6) --octave;      // B# is in the octave below C
    else if (s.letter == 0 && pc > 6) ++octave; // Cb in the octave above B
    return pitchName (pc, root, scale, german) + std::to_string (octave);
}
} // namespace keys

namespace chords
{
namespace
{
struct QualityInfo { uint16_t shape; const char* suffix; int rank; bool canOmitFifth; };
const QualityInfo& info (int q) noexcept
{
    static const QualityInfo table[numQualities] = {
        { bits ({ 0, 4, 7 }), "", 0, false },             { bits ({ 0, 3, 7 }), "m", 1, false },
        { bits ({ 0, 3, 6 }), "dim", 6, false },          { bits ({ 0, 4, 8 }), "aug", 21, false },
        { bits ({ 0, 2, 7 }), "sus2", 7, false },         { bits ({ 0, 5, 7 }), "sus4", 5, false },
        { bits ({ 0, 7 }), "5", 8, false },               { bits ({ 0, 4, 7, 9 }), "6", 9, false },
        { bits ({ 0, 3, 7, 9 }), "m6", 10, false },       { bits ({ 0, 4, 7, 11 }), "maj7", 4, true },
        { bits ({ 0, 4, 7, 10 }), "7", 2, true },         { bits ({ 0, 3, 7, 10 }), "m7", 3, true },
        { bits ({ 0, 3, 6, 10 }), "m7b5", 11, false },    { bits ({ 0, 3, 6, 9 }), "dim7", 12, false },
        { bits ({ 0, 3, 7, 11 }), "m(maj7)", 13, false }, { bits ({ 0, 5, 7, 10 }), "7sus4", 14, false },
        { bits ({ 0, 2, 4, 7 }), "add9", 15, false },     { bits ({ 0, 2, 3, 7 }), "madd9", 16, false },
        { bits ({ 0, 2, 4, 7, 10 }), "9", 17, true },     { bits ({ 0, 2, 4, 7, 11 }), "maj9", 18, true },
        { bits ({ 0, 2, 3, 7, 10 }), "m9", 19, true },    { bits ({ 0, 2, 4, 7, 9 }), "6/9", 20, false },
        { bits ({ 0, 4, 8, 10 }), "aug7", 22, false },    { bits ({ 0, 2, 7, 10 }), "7sus2", 23, false },
    };
    return table[std::clamp (q, 0, numQualities - 1)];
}

/** Best match of the whole set; score lower = better, INT_MAX = none. */
void bestOf (uint16_t pcs, int bassPc, int penalty, Name& best, int& bestScore) noexcept
{
    pcs &= 0xfff;
    const int n = popcount12 (pcs);
    if (n < 2) return;
    for (int r = 0; r < 12; ++r)
    {
        if (((pcs >> r) & 1u) == 0) continue; // the root sounds
        const uint16_t rel = rotateDown (pcs, r);
        for (int q = 0; q < numQualities; ++q)
        {
            const auto& qi = info (q);
            int score = -1;
            if (rel == qi.shape) score = qi.rank;
            else if (qi.canOmitFifth && n >= 3 && rel == (uint16_t) (qi.shape & ~(1u << 7))) score = qi.rank + 30;
            if (score < 0) continue;
            if (bassPc >= 0 && bassPc != r) score += 40;
            score += penalty;
            if (score < bestScore)
            {
                bestScore = score;
                best.root = r;
                best.quality = q;
            }
        }
    }
}
} // namespace

uint16_t shape (int quality) noexcept { return info (quality).shape; }
const char* suffix (int quality) noexcept { return info (quality).suffix; }

int count (const NoteSet& s) noexcept
{
    int n = 0;
    for (auto w : s)
        for (; w != 0; w &= w - 1) ++n;
    return n;
}

int lowest (const NoteSet& s) noexcept
{
    for (int i = 0; i < 128; ++i)
        if (has (s, i)) return i;
    return -1;
}

uint16_t pitchClasses (const NoteSet& s) noexcept
{
    uint16_t m = 0;
    for (int i = 0; i < 128; ++i)
        if (has (s, i)) m = (uint16_t) (m | (1u << (i % 12)));
    return m;
}

namespace
{
Name detectImpl (uint16_t pcs, int bassPc, bool bassMayBeForeign) noexcept
{
    pcs &= 0xfff;
    Name best;
    int score = 1 << 30;
    bestOf (pcs, bassPc, 0, best, score);
    // a bass under another chord ("C/D"): the upper notes without the bass, when at least a triad remains
    if (bassPc >= 0 && bassMayBeForeign)
    {
        const uint16_t upper = (uint16_t) (pcs & ~(1u << bassPc));
        if (popcount12 (upper) >= 3)
        {
            Name up;
            int upScore = 1 << 30;
            bestOf (upper, -1, 50, up, upScore);
            if (up.valid() && upScore < score) { best = up; score = upScore; }
        }
    }
    if (! best.valid()) return {};
    best.bass = bassPc >= 0 ? bassPc : best.root;
    const uint16_t rel = shape (best.quality);
    const int bassRel = pcOf (best.bass - best.root);
    if (((rel >> bassRel) & 1u) == 0 && bassRel != 7) best.inversion = -1; // outside the chord (an omitted fifth counts as in)
    else
    {
        int idx = 0;
        for (int i = 1; i < bassRel; ++i) idx += (rel >> i) & 1;
        best.inversion = bassRel == 0 ? 0 : idx + 1;
    }
    return best;
}
} // namespace

Name detect (uint16_t pitchClasses, int bassPc) noexcept { return detectImpl (pitchClasses, bassPc, true); }

Name detect (const NoteSet& notes) noexcept
{
    const int low = lowest (notes);
    if (low < 0) return {};
    const int bass = low % 12;
    bool doubled = false; // the bass pitch class sounds higher up too: it is part of the chord
    for (int n = low + 12; n < 128 && ! doubled; n += 12) doubled = has (notes, n);
    return detectImpl (pitchClasses (notes), bass, ! doubled);
}

std::string symbol (const Name& c, int keyRoot, int keyScale, bool german)
{
    if (! c.valid()) return {};
    std::string s = keys::pitchName (c.root, keyRoot, keyScale, german) + suffix (c.quality);
    if (c.slash()) s += "/" + keys::pitchName (c.bass, keyRoot, keyScale, german);
    return s;
}

std::string inversionName (const Name& c, bool german)
{
    if (! c.valid()) return {};
    if (c.inversion < 0) return german ? "Bass außerhalb des Akkords" : "bass outside the chord";
    switch (c.inversion)
    {
        case 0: return german ? "Grundstellung" : "root position";
        case 1: return german ? "1. Umkehrung" : "1st inversion";
        case 2: return german ? "2. Umkehrung" : "2nd inversion";
        case 3: return german ? "3. Umkehrung" : "3rd inversion";
        default: return german ? std::to_string (c.inversion) + ". Umkehrung" : std::to_string (c.inversion) + "th inversion";
    }
}
} // namespace chords
} // namespace ohpe
