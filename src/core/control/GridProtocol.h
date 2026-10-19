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
      poly aftertouch (or channel pressure). Pad light: a note-on to the pad's note, velocity = an index of the pads'
      128-colour palette (padColourCode).
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

/** p53: touch notes of the encoders: 0 .. 7 = encoders 1 .. 8, 8 = master, 9 = swing, 10 = tempo; 12 = touch strip. */
constexpr int touchNoteSwing = 9, touchNoteTempo = 10;

/** p53: what one message from the controller (user port, channel 1) is - the controller layer consumes every one
    of them; only pads play notes (encoder / strip touches are never notes for the synth). */
enum class Input { Other = 0, Pad, EncoderTouch, StripTouch, EncoderTurn, Button, Strip, Pressure };
struct Decoded
{
    Input kind = Input::Other;
    int index = -1;  ///< pad 0 .. 63; encoder 0 .. 7, 8 = master, 9 = swing, 10 = tempo; button CC
    int value = 0;   ///< touch: 1 / 0; turn: signed steps; button: 127 / 0; strip: 0 .. 16383; pressure 0 .. 127
};
/** Decodes one short MIDI message (status, data1, data2). */
Decoded decode (int status, int data1, int data2) noexcept;
/** Encoder 0 .. 10 of a CC (71 .. 78 -> 0 .. 7, 79 master -> 8, 15 swing -> 9, 14 tempo -> 10), -1 = none. */
int encoderOfCc (int cc) noexcept;
/** Encoder 0 .. 10 of a touch note (0 .. 10), -1 = none. */
inline int encoderOfTouchNote (int note) noexcept { return note >= 0 && note <= touchNoteTempo ? note : -1; }

// ---------------------------------------------------------------- p53: device generation, mode, identity
/** The controller generation: the first one has the four-line character display written over MIDI; a later one
    (pixel display over USB, its own SysEx header) gets pads, buttons, encoders and lights but no text display. */
enum class Generation { Unknown = 0, First, Later };
/** Universal identity request (F0 7E 7F 06 01 F7). */
std::vector<uint8_t> identityRequestSysex();
/** The generation an identity reply (F0 7E dev 06 02 ...) names; Unknown for any other message or maker. */
Generation identityGeneration (const uint8_t* data, size_t size) noexcept;
/** The SysEx that switches the controller into its User mode (true; Ohpe gets the user port) or back to its Live
    mode (false, when Ohpe lets go). Empty for Unknown. */
std::vector<uint8_t> userModeSysex (Generation g, bool user);
/** p53: the connect splash on the four display lines: "Ohpe" and "connecting..." (stage 0) / "connected" (stage 1),
    in the UI language (German: "verbindet..." / "verbunden"). */
std::array<std::string, numLines> splashLines (int stage, bool german);
/** p53: the device part of a port name (case, direction, "user" / "live" / "port" and the Windows "MIDIIN2 (...)"
    prefix dropped): the live and the user port of one device get the same key. */
std::string deviceKey (const std::string& portName);

/** Relative encoder value -> steps (+1 .. +63 / -1 .. -64). */
inline int relativeDelta (int value) noexcept { return value >= 64 ? value - 128 : value; }

// ---------------------------------------------------------------- lights
/** Pad / coloured button colours (the controller has red, amber, yellow and green, each dim or full). */
enum class Colour { Off = 0, RedDim, Red, AmberDim, Amber, YellowDim, Yellow, GreenDim, Green };
/** Light code of a pad / coloured button (note velocity / CC value): 0 off, red 1 / 4, amber 7 / 10, yellow 13 / 16,
    green 19 / 22 (dim / full). blink: + 1 (slow). */
int colourCode (Colour c, bool blink = false) noexcept;
/** Pad light: index of the pads' colour palette (four shades per hue: light, full, dark, darker; red 4 .., amber /
    orange 8 .., yellow 12 .., green 20 ..): full = the hue's full shade, dim = its darkest shade, Off = 0. */
int padColourCode (Colour c) noexcept;
/** The colour a pad palette index stands for (the virtual controller view; Off for anything else). */
Colour padColourOf (int code) noexcept;
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
/** Short form of a value for a cell: as it is when it fits, else without spaces, else cut ("3.00 kHz" -> "3.00kHz";
    digits, signs and points are never dropped from the middle). */
std::string shortValue (const std::string& utf8, int width = cellWidth - 1);
/** The SysEx message (F0 .. F7) that writes `text` (fitLine) into display line 0 .. 3. */
std::vector<uint8_t> displayLineSysex (int line, const std::string& utf8);
/** The SysEx that clears display line 0 .. 3. */
std::vector<uint8_t> clearLineSysex (int line);
/** The SysEx that sets the pads' pressure to poly aftertouch (true) or channel pressure (false). */
std::vector<uint8_t> aftertouchModeSysex (bool poly);

// ---------------------------------------------------------------- pad layout (the on-screen grid keyboard's principle)
/**
    The key the pads show: root pitch class, scale (scales::Scale) and Scale Lock. The controller fills it like the
    on-screen grid: root / scale = the harmony brain's remembered key, lock = Core::keyScale().lock (p45).
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
