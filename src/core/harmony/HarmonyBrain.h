// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../model/Events.h"
#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"

#include <array>
#include <map>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe
{
/**
    p24: the harmony brain ("Harmonie-Hirn") - a scale and chord memory shared by all four parts.

    * Listening: every note that reaches a part whose follow mode is Off (live keys, host / device MIDI, the
      sequencers, the arpeggiator, the on-screen keyboards) is heard. Following parts are not heard (no feedback).
    * Chord: the pitch classes held right now (two or more) are matched against ten chord shapes (major, minor,
      diminished, augmented, sus2, sus4, major 7, minor 7, dominant 7, fifth); the lowest held note breaks ties.
      The chord stays in memory after the keys are released, until the next chord is played.
    * Key: a pitch-class memory (note starts and held time, fading with a half-life of "harmony.memory" bars)
      is matched against all major and natural minor keys; a new key must win clearly (hysteresis). Until
      anything was heard the key is the global key ("key.root" / "key.scale").
    * History: the last eight chords (newest first) for the display.
    * Follow ("harmony.pN.follow"): Off, Scale (the part's notes snap to the remembered key), Chord (to the
      tones of the remembered chord), Root (to the chord's root). The part's one-finger chord builds on that set.
      Off leaves the part exactly as before (neutral).
    * "harmony.hold" freezes the memory; "harmony.take" (trigger, message thread) writes the remembered key into
      "key.root" / "key.scale"; "harmony.clear" (trigger) forgets everything.
    * Matrix (Part sources, the same value for every part): Chord Root (0 .. 1 over C .. B), Chord Change (1 at a
      new chord, fading in about a third of a second), Tension (share of held notes outside the key, smoothed).

    Threads: the audio thread listens and detects; the published state is atomics (any thread). State section
    "harmony" (key, chord, history, pitch-class memory) is written / read on the message thread.
*/
namespace harmony
{
enum ChordType { Major = 0, Minor, Dim, Aug, Sus2, Sus4, Maj7, Min7, Dom7, Fifth, numChordTypes };
enum Follow { FollowOff = 0, FollowScale, FollowChord, FollowRoot, numFollow };
/** Layouts of the isomorphic on-screen keyboard ("harmony.kb.layout", append-only). p59: index 3 was "In Key" (now
    Fourths + "harmony.kb.inkey", migrated by migrateLayout) and is Sequential (+1 right, +8 up: continuous rows). */
enum Layout { LayoutFourths = 0, LayoutThirds, LayoutWhole, LayoutSequential, numLayouts };
constexpr int legacyLayoutInKey = 3; ///< p59: the old "In Key" layout value (sessions / presets before p59)

/** Pitch classes of a chord shape relative to its root (bit 0 = root). */
uint16_t chordShape (int type) noexcept;
/** Chord symbol suffix ("", "m", "dim", "+", "sus2", "sus4", "maj7", "m7", "7", "5"). */
const char* chordSuffix (int type) noexcept;
std::vector<std::string> followNamesEn();
std::vector<std::string> followNamesDe();
std::vector<std::string> layoutNamesEn();
std::vector<std::string> layoutNamesDe();

struct Chord
{
    int root = -1; ///< pitch class, -1 = none
    int type = Major;
    bool operator== (const Chord& o) const noexcept { return root == o.root && type == o.type; }
    bool operator!= (const Chord& o) const noexcept { return ! (*this == o); }
};

/** Best chord for a set of pitch classes (bit mask, bit 0 = C). bassPc (-1 = none) breaks ties. Needs two pcs. */
bool detectChord (uint16_t pitchClasses, int bassPc, Chord& out) noexcept;
/** Score of a key (root, scale index of scales::Scale) for a pitch-class weight profile. */
float keyScore (const std::array<float, 12>& weights, int root, int scale) noexcept;
/** Best major / natural minor key for the profile; keeps (prevRoot, prevScale) unless another key wins clearly. */
void detectKey (const std::array<float, 12>& weights, int prevRoot, int prevScale, int& root, int& scale) noexcept;

/** Columns / rows of the on-screen keyboard. */
struct GridStep { int col; int row; };
GridStep layoutSteps (int layout) noexcept; ///< semitones (Chromatic)
/** p59: steps of a layout in scale degrees (In Key): Fourths 1 / 3, Thirds 1 / 2, Whole Tones 1 / 3, Sequential 1 / 8. */
GridStep layoutDegrees (int layout) noexcept;
/**
    Note of a cell (col from the left, row from the bottom); `base` = the lowest C (12 * (octave + 1)).
    p59: inKey = only notes of the key, every step a scale step (layoutDegrees); else every note (layoutSteps).
    fixed = the bottom-left cell is C (In Key: the first key note at or above C, the behaviour before p59); not fixed =
    the bottom-left cell is the key's root above `base`, so every chord shape is the same in every key.
    A chromatic scale plays In Key like Chromatic. originRoot (-1 = keyRoot): the root the bottom-left cell starts on
    when not fixed - the grid passes the set key (Core::keyScale, the display header), so the cells stay put while the
    harmony brain's remembered key (keyRoot / keyScale: which notes are In Key, the colours) moves under the fingers.
*/
int gridNote (int layout, int col, int row, int base, int keyRoot, int keyScale, bool inKey = false, bool fixed = true,
              int originRoot = -1) noexcept;
/** p59: the cell's note as gridNote, for the grid's own parameters ("harmony.kb.layout / inkey / fixed"). */
struct GridOptions { int layout = LayoutFourths; bool inKey = false; bool fixed = false; };
GridOptions gridOptions (const ParamRegistry& reg) noexcept;
/** p59: sessions / presets from before p59: "harmony.kb.layout" = 3 (the old In Key) without "harmony.kb.inkey" in the
    document becomes Fourths + In Key. `params` = the document's parameter values; true if it changed anything. */
bool migrateLayout (std::map<std::string, float>& params);
} // namespace harmony

class HarmonyBrain
{
public:
    static constexpr int numParts = 4;
    static constexpr int historySize = 8;
    static constexpr int numSources = 3; ///< Chord Root, Chord Change, Tension

    HarmonyBrain();
    HarmonyBrain (const HarmonyBrain&) = delete;
    HarmonyBrain& operator= (const HarmonyBrain&) = delete;

    /** Registers "harmony.memory / hold / take / clear / kb.layout" and "harmony.pN.follow" (append-only block). */
    static void registerParams (ParamRegistry& reg, int numParts);
    /** p59: "harmony.kb.inkey" (In Key / Chromatic, off) and "harmony.kb.fixed" (bottom-left = C, off = the key's
        root), appended at the very end of the parameter list. */
    static void registerLate59GridParams (ParamRegistry& reg);

    void bind (const ParamRegistry& reg);
    void prepare (double sampleRate) noexcept { sr = sampleRate > 0.0 ? sampleRate : 48000.0; }
    /** Forgets held notes (all notes stopped); the memory stays. */
    void reset() noexcept;

    // ---- audio thread
    /** Block start: parameters, memory fade, detection. */
    void beginBlock (const ParamSnapshot& snap, double bpm, int numSamples, int globalRoot, int globalScale) noexcept;
    int followMode (int part) const noexcept { return part >= 0 && part < numParts ? follow[(size_t) part] : 0; }
    bool listensTo (int part) const noexcept { return followMode (part) == harmony::FollowOff; }
    void noteOn (int part, int note, int velocity) noexcept;
    void noteOff (int part, int note) noexcept;
    /** The set a following part snaps to (root pitch class + mask relative to it); false = the part does not follow. */
    bool followSet (int part, int& root, uint16_t& mask) const noexcept;
    /** Matrix source 0 .. numSources-1 (0 .. 1). */
    float sourceValue (int source) const noexcept;

    // ---- any thread (published state)
    int keyRoot() const noexcept { return pubKeyRoot.load (std::memory_order_relaxed); }
    int keyScale() const noexcept { return pubKeyScale.load (std::memory_order_relaxed); }
    harmony::Chord chord() const noexcept;          ///< root -1 = nothing heard yet
    harmony::Chord history (int index) const noexcept; ///< 0 = newest; root -1 = empty
    int historyCount() const noexcept;
    float weight (int pitchClass) const noexcept;  ///< 0 .. 1 (relative to the strongest)
    uint16_t heldPitchClasses() const noexcept { return pubHeld.load (std::memory_order_relaxed); }
    bool heard() const noexcept { return pubHeard.load (std::memory_order_relaxed); }
    /** Message thread: forget everything (the audio thread applies it at the next block). */
    void requestClear() noexcept { clearRequest.store (true, std::memory_order_release); }
    /** Audio thread / tests: forget everything now. */
    void clearMemory() noexcept;

    // ---- state (message thread)
    std::string toJson() const;
    bool fromJson (const std::string& text);
    bool anyMemory() const noexcept { return heard(); }

private:
    void detectKey (int globalRoot, int globalScale) noexcept;
    void detectChord (int minPitchClasses) noexcept;
    void commit (const harmony::Chord& c) noexcept;

    struct Index { int follow = -1; };
    std::array<Index, numParts> idx {};
    int idxMemory = -1, idxHold = -1, idxClear = -1;
    double sr = 48000.0;

    // audio thread
    std::array<int, numParts> follow {};
    std::array<std::array<uint8_t, 128>, numParts> held {};
    std::array<float, 12> profile {}; ///< pitch-class memory (fading)
    harmony::Chord current, committed; ///< current: follow uses it at once; committed: after it settled (history)
    int stableSamples = 0;
    bool hold = false, wasClear = false;
    float changePulse = 0.0f, tension = 0.0f;
    int lastGlobalRoot = -1, lastGlobalScale = -1;

    // published (atomics)
    std::atomic<int> pubKeyRoot { 0 }, pubKeyScale { 1 };
    std::atomic<int> pubChord { -1 };   ///< root * 16 + type, -1 = none
    std::array<std::atomic<int>, historySize> pubHistory;
    std::array<std::atomic<float>, 12> pubWeight; ///< the raw pitch-class memory
    std::atomic<uint16_t> pubHeld { 0 };
    std::atomic<bool> pubHeard { false };
    std::atomic<bool> clearRequest { false };
    std::atomic<bool> loaded { false }; ///< fromJson: the audio thread adopts the published profile / chord
};
} // namespace ohpe
