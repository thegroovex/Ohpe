// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ohpe::gridctl
{
/**
    p46: Grid-Controller (Profil 1) - the wire protocol of an 8 x 8 pad grid controller with a four-line character
    display (64 velocity / pressure pads, 8 touch encoders + a master encoder, two rows of 8 buttons under the display,
    transport and navigation buttons, a touch strip). Own implementation from the controller's publicly documented
    MIDI / SysEx interface; no message thread, no JUCE (core tests check every encoding).

    * Pads: notes 36 .. 99 on channel 1, pad (col, row) = 36 + row * 8 + col (row 0 = the bottom row). Pressure as
      poly aftertouch (or channel pressure). Pad light: a note-on to the pad's note, velocity = colour code.
    * Encoders: CC 71 .. 78 (relative: 1 .. 63 up, 127 .. 65 down), master encoder CC 79, tempo CC 14, swing CC 15;
      touching an encoder sends note 0 .. 10 (velocity > 0 = touched).
    * Buttons: CC (127 pressed, 0 released); the light of a button is the same CC sent back (value = light code).
    * Touch strip: pitch bend on channel 1, touching it note 12.
    * Display: 4 lines of 68 characters, one SysEx per line (header bytes, line, length, text); 7-bit characters.
*/

constexpr int cols = 8, rows = 8, numPads = cols * rows;
constexpr int firstPadNote = 36;
constexpr int lineLength = 68, numLines = 4, numCells = 8, cellWidth = 8;

/** Pad (0 .. 63, row-major from the bottom left) of a note; -1 = not a pad. */
inline int padOfNote (int note) noexcept { return note >= firstPadNote && note < firstPadNote + numPads ? note - firstPadNote : -1; }
inline int padNote (int col, int row) noexcept { return firstPadNote + row * cols + col; }

// ---------------------------------------------------------------- controls (CC numbers)
namespace cc
{
constexpr int tapTempo = 3, metronome = 9, tempoEncoder = 14, swingEncoder = 15;
constexpr int upperRow = 20;   ///< 20 .. 27: the button row right under the display
constexpr int master = 28, stop = 29;
constexpr int left = 44, right = 45, up = 46, down = 47;
constexpr int select = 48, shift = 49, note = 50, session = 51;
constexpr int octaveDown = 54, octaveUp = 55, repeat = 56, accent = 57, scales = 58, user = 59;
constexpr int firstEncoder = 71; ///< 71 .. 78
constexpr int masterEncoder = 79;
constexpr int play = 85, record = 86;
constexpr int lowerRow = 102;  ///< 102 .. 109: the second button row
constexpr int undo = 119;
} // namespace cc

constexpr int touchNoteMaster = 8, touchNoteStrip = 12;

/** Relative encoder value -> steps (+1 .. +63 / -1 .. -64). */
inline int relativeDelta (int value) noexcept { return value >= 64 ? value - 128 : value; }

// ---------------------------------------------------------------- lights
/** Pad / coloured button colours (the controller has red, amber, yellow and green, each dim or full). */
enum class Colour { Off = 0, RedDim, Red, AmberDim, Amber, YellowDim, Yellow, GreenDim, Green };
/** Light code of a pad / coloured button (note velocity / CC value): 0 off, red 1 / 4, amber 7 / 10, yellow 13 / 16,
    green 19 / 22 (dim / full). blink: + 1 (slow). */
int colourCode (Colour c, bool blink = false) noexcept;
/** Light code of a plain (one-colour) button: 0 off, 1 dim, 4 on (blink: 2 / 5). */
int buttonCode (bool on, bool dimWhenOff = true, bool blink = false) noexcept;

// ---------------------------------------------------------------- display
/** Text folded to the display's 7-bit character set (umlauts ae / oe / ue, sharp s ss, other symbols '?'). */
std::string fold (const std::string& utf8);
/** The line exactly lineLength characters long (folded, cut or padded with spaces). */
std::string fitLine (const std::string& utf8);
/** One display line of 8 cells, two over each of the four display segments: "cell0... cell1..." (8 + 1 + 8 chars
    per segment, cells cut to 7 characters so that neighbours never touch, centred). */
std::string cellLine (const std::array<std::string, numCells>& cells);
/** One display line of 4 segments of 17 characters (left aligned, cut). */
std::string segmentLine (const std::array<std::string, 4>& segments);
/** Short form of a name for a cell: a leading channel / track / pad number ("Ch 1", "Kanal 1", "Track 3") is dropped
    first, then vowels and last letters of the longest words until it fits ("Filter Cutoff" -> "FltrCtff"); a name
    that fits stays. */
std::string shortName (const std::string& utf8, int width = cellWidth - 1);
/** The SysEx message (F0 .. F7) that writes `text` (fitLine) into display line 0 .. 3. */
std::vector<uint8_t> displayLineSysex (int line, const std::string& utf8);
/** The SysEx that clears display line 0 .. 3. */
std::vector<uint8_t> clearLineSysex (int line);
/** The SysEx that sets the pads' pressure to poly aftertouch (true) or channel pressure (false). */
std::vector<uint8_t> aftertouchModeSysex (bool poly);

// ---------------------------------------------------------------- pad layout (the on-screen grid keyboard's principle)
/**
    The key the pads show: root pitch class, scale (scales::Scale) and Scale Lock. Minimal interface until the key
    API is on main everywhere: the controller fills it from the core's key (Core::keyScale() where it exists, else the
    harmony brain's remembered key and "key.lock").
*/
struct PadKey
{
    int root = 0;
    int scale = 1; ///< scales::Major
    bool lock = false;
};

/** The note of pad (col, row) - exactly the on-screen grid keyboard's cell at the same column and row
    (harmony::gridNote with the grid's layout and the lowest C of `octave`, 12 * (octave + 1)). */
int padLayoutNote (int layout, int col, int row, int octave, const PadKey& key) noexcept;

/** What a pad shows in the keyboard mode. */
enum class PadRole { Outside = 0, InKey, Root, ChordTone, Pressed };
/** chordMask: pitch classes of the remembered chord (bit 0 = C), 0 = none. */
PadRole padRole (int note, const PadKey& key, uint16_t chordMask, bool pressed) noexcept;
/** Light of a pad role, like the on-screen cells: root amber (full), key note yellow dim, chord tone green dim,
    pressed green (full), other note off (graphite on screen). */
Colour roleColour (PadRole r) noexcept;
} // namespace ohpe::gridctl
