// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p46: Grid-Controller (Profil 1) - display SysEx and text, encoder deltas, light codes, the pad layout (the on-screen
// grid keyboard's mapping on 8 x 8 pads), pad colours, the core's controller queue, the controller settings.
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "control/GridProtocol.h"
#include "harmony/HarmonyBrain.h"
#include "seq/ScaleChord.h"
#include "settings/UserSettings.h"

#include <doctest/doctest.h>

#include <memory>

using namespace ohpe;
using namespace ohpe::test;
namespace g = ohpe::gridctl;

TEST_CASE ("p46 grid controller: display lines - SysEx frame, 68 characters, 7 bit, folded text")
{
    const auto m = g::displayLineSysex (2, "Lautst\xc3\xa4rke \xc3\x9f \xe2\x80\x93 \xc2\xb7 C\xe2\x99\xaf");
    REQUIRE (m.size() == 8u + 68u + 1u);
    CHECK (m.front() == 0xf0);
    CHECK (m.back() == 0xf7);
    CHECK (m[1] == 0x47);
    CHECK (m[4] == 0x1a); // line 3 (0-based 2)
    CHECK (m[6] == 0x45); // 68 characters
    for (size_t i = 1; i + 1 < m.size(); ++i) CHECK (m[i] < 0x80);
    const std::string text (m.begin() + 8, m.end() - 1);
    CHECK (text.rfind ("Lautstaerke ss - . C#", 0) == 0);
    CHECK (g::displayLineSysex (0, "")[4] == 0x18);
    CHECK (g::displayLineSysex (9, "x")[4] == 0x1b); // clamped to line 4
    CHECK (g::clearLineSysex (1) == std::vector<uint8_t> { 0xf0, 0x47, 0x7f, 0x15, 0x1d, 0x00, 0x00, 0xf7 });
    CHECK (g::aftertouchModeSysex (true).size() == 9u);
    CHECK (g::aftertouchModeSysex (true)[7] == 1);
    CHECK (g::aftertouchModeSysex (false)[7] == 0);
    // umlauts, capitals, broken UTF-8
    CHECK (g::fold ("\xc3\x84\xc3\x96\xc3\x9c\xc3\xa4\xc3\xb6\xc3\xbc") == "AeOeUeaeoeue");
    CHECK (g::fold ("a\xc3") == "a?");
    CHECK (g::fold ("tab\tx") == "tab x");
    CHECK (g::fitLine (std::string (100, 'x')).size() == 68u);
}

TEST_CASE ("p46 grid controller: encoder cells - eight cells over four segments, short names")
{
    std::array<std::string, g::numCells> cells { "Cutoff", "Resonance", "", "Drive", "A", "B", "Filter Envelope Amount", "Mix" };
    const auto line = g::cellLine (cells);
    REQUIRE (line.size() == 68u);
    CHECK (line.substr (0, 8) == " Cutoff ");
    CHECK (line[8] == ' ');
    CHECK (line.substr (9, 8) == "Resonan "); // seven characters at most: cells never touch
    CHECK (line.substr (17, 8) == "        ");
    CHECK (line.substr (26, 8) == " Drive  ");
    const auto seg = g::segmentLine ({ "Preset", "Part 1", "120.0 BPM", "C Major" });
    REQUIRE (seg.size() == 68u);
    CHECK (seg.substr (17, 6) == "Part 1");
    CHECK (seg.substr (51, 7) == "C Major");
    CHECK (g::shortName ("Cutoff") == "Cutoff");
    CHECK (g::shortName ("Filter Cutoff").size() <= 8u);
    CHECK (g::shortName ("Filter Envelope Amount").size() <= 8u);
    CHECK (g::shortName ("Filter Cutoff").front() == 'F');
    CHECK (g::shortName ("Lautst\xc3\xa4rke").size() <= 8u);
    CHECK (g::shortName ("").empty());
    CHECK (g::shortValue ("3.00 kHz") == "3.00kHz"); // values keep their digits and points
    CHECK (g::shortValue ("-12.5 dB") == "-12.5dB");
    CHECK (g::shortValue ("1/16") == "1/16");
    CHECK (g::shortValue ("Harmonic Minor").size() == 7u);
    CHECK (g::shortName ("Ch 1 Volume") == "Volume"); // the page says which channel
    CHECK (g::shortName ("Kanal 1 EQ Tiefen").rfind ("EQ", 0) == 0);
    CHECK (g::shortName ("Ch1 Send A") == "SendA");
    CHECK (g::shortName ("Osc 2 Octave").rfind ("Osc2", 0) == 0); // not a channel prefix: kept
    for (const char* n : { "Filter Cutoff", "Amp Attack", "Resonance", "Lautst\xc3\xa4rke", "Ch 12 Output" })
        CHECK (g::shortName (n).size() <= 7u);
}

TEST_CASE ("p46 grid controller: encoder deltas, light codes, pad notes")
{
    CHECK (g::relativeDelta (1) == 1);
    CHECK (g::relativeDelta (63) == 63);
    CHECK (g::relativeDelta (127) == -1);
    CHECK (g::relativeDelta (65) == -63);
    CHECK (g::relativeDelta (64) == -64);
    CHECK (g::colourCode (g::Colour::Off) == 0);
    CHECK (g::colourCode (g::Colour::RedDim) == 1);
    CHECK (g::colourCode (g::Colour::Red) == 4);
    CHECK (g::colourCode (g::Colour::Amber) == 10);
    CHECK (g::colourCode (g::Colour::YellowDim) == 13);
    CHECK (g::colourCode (g::Colour::Yellow) == 16);
    CHECK (g::colourCode (g::Colour::Green) == 22);
    CHECK (g::colourCode (g::Colour::Green, true) == 23);
    // pads: the 128-colour palette (full / darkest shade of each hue)
    CHECK (g::padColourCode (g::Colour::Off) == 0);
    CHECK (g::padColourCode (g::Colour::Amber) == 9);
    CHECK (g::padColourCode (g::Colour::YellowDim) == 15);
    CHECK (g::padColourCode (g::Colour::Green) == 21);
    CHECK (g::padColourCode (g::Colour::GreenDim) == 23);
    for (int k = 0; k <= (int) g::Colour::Green; ++k) CHECK (g::padColourOf (g::padColourCode ((g::Colour) k)) == (g::Colour) k);
    CHECK (g::padColourOf (100) == g::Colour::Off);
    CHECK (g::buttonCode (true) == 4);
    CHECK (g::buttonCode (false) == 1);
    CHECK (g::buttonCode (false, false) == 0);
    CHECK (g::padNote (0, 0) == 36);
    CHECK (g::padNote (7, 7) == 99);
    CHECK (g::padOfNote (36) == 0);
    CHECK (g::padOfNote (44) == 8); // second row
    CHECK (g::padOfNote (35) == -1);
    CHECK (g::padOfNote (100) == -1);
}

TEST_CASE ("p46 grid controller: pads play the on-screen grid's notes in every layout, key and octave")
{
    using namespace harmony;
    g::PadKey key;
    for (int layout = 0; layout < numLayouts; ++layout)
        for (int root : { 0, 2, 9 })
            for (int scale : { (int) scales::Major, (int) scales::Minor, (int) scales::Dorian, (int) scales::Chromatic })
                for (int octave = 0; octave <= 7; ++octave)
                {
                    key.root = root;
                    key.scale = scale;
                    for (int row = 0; row < g::rows; ++row)
                        for (int col = 0; col < g::cols; ++col)
                        {
                            const int n = g::padLayoutNote (layout, col, row, octave, key);
                            CHECK (n == gridNote (layout, col, row, 12 * (octave + 1), root, scale)); // the same cell, the same note
                            CHECK ((n >= 0 && n <= 127));
                            for (int inKey = 0; inKey < 2; ++inKey) // p59: In Key / Chromatic, fixed C or the key's root
                                for (int fixed = 0; fixed < 2; ++fixed)
                                {
                                    const int m = g::padLayoutNote (layout, col, row, octave, key, inKey != 0, fixed != 0);
                                    CHECK (m == gridNote (layout, col, row, 12 * (octave + 1), root, scale, inKey != 0, fixed != 0));
                                    if (inKey != 0 && m < 127) CHECK (scales::inScale (m, root, scale)); // 127: clamped at the top
                                }
                        }
                }
    key = {};
    // Fourths from C3 (octave 3, like the on-screen keyboard): right +1, up +5
    CHECK (g::padLayoutNote (LayoutFourths, 0, 0, 3, key) == 48);
    CHECK (g::padLayoutNote (LayoutFourths, 1, 0, 3, key) == 49);
    CHECK (g::padLayoutNote (LayoutFourths, 0, 1, 3, key) == 53);
    CHECK (g::padLayoutNote (LayoutFourths, 7, 7, 3, key) == 48 + 7 + 35);
    CHECK (g::padLayoutNote (LayoutThirds, 0, 1, 3, key) == 52);
    CHECK (g::padLayoutNote (LayoutWhole, 1, 1, 3, key) == 55);
    CHECK (g::padLayoutNote (LayoutFourths, 1, 0, 3, key, true) == 50); // p59: Fourths + In Key (the old In Key)
    CHECK (g::padLayoutNote (LayoutFourths, 0, 1, 3, key, true) == 53);
    CHECK (g::padLayoutNote (LayoutSequential, 0, 1, 3, key) == 56);
    // octave: one octave up = +12
    CHECK (g::padLayoutNote (LayoutFourths, 2, 3, 4, key) == g::padLayoutNote (LayoutFourths, 2, 3, 3, key) + 12);
}

TEST_CASE ("p46 grid controller: pad colours - root, key notes, chord tones, held, other notes")
{
    g::PadKey cMajor; // C major
    const uint16_t aMinor = (uint16_t) ((1u << 9) | (1u << 0) | (1u << 4)); // A C E
    CHECK (g::padRole (60, cMajor, 0, false) == g::PadRole::Root);
    CHECK (g::padRole (72, cMajor, aMinor, false) == g::PadRole::Root); // the root wins over a chord tone
    CHECK (g::padRole (62, cMajor, 0, false) == g::PadRole::InKey);
    CHECK (g::padRole (61, cMajor, 0, false) == g::PadRole::Outside);
    CHECK (g::padRole (69, cMajor, aMinor, false) == g::PadRole::ChordTone);
    CHECK (g::padRole (61, cMajor, 0, true) == g::PadRole::Pressed);
    CHECK (g::roleColour (g::PadRole::Root) == g::Colour::Amber);
    CHECK (g::roleColour (g::PadRole::InKey) == g::Colour::YellowDim);
    CHECK (g::roleColour (g::PadRole::ChordTone) == g::Colour::GreenDim);
    CHECK (g::roleColour (g::PadRole::Pressed) == g::Colour::Green);
    CHECK (g::roleColour (g::PadRole::Outside) == g::Colour::Off);
    g::PadKey aMin { 9, scales::Minor, false };
    CHECK (g::padRole (57, aMin, 0, false) == g::PadRole::Root);
    CHECK (g::padRole (60, aMin, 0, false) == g::PadRole::InKey);
    CHECK (g::padRole (61, aMin, 0, false) == g::PadRole::Outside);
}

namespace
{
struct CtlRig
{
    std::unique_ptr<Core> owner = std::make_unique<Core>(); // on the heap (Windows stack)
    Core& core = *owner;
    static constexpr int block = 256;
    std::array<std::vector<float>, numOutputPairs> l, r;
    std::vector<float> all;
    CtlRig()
    {
        core.prepare (sampleRate, block);
        for (auto& v : l) v.assign (block, 0.0f);
        for (auto& v : r) v.assign (block, 0.0f);
    }
    void run (int blocks)
    {
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
        for (int b = 0; b < blocks; ++b)
        {
            Transport t;
            core.process (out, block, nullptr, 0, t);
            all.insert (all.end(), l[0].begin(), l[0].end());
        }
    }
};
} // namespace

TEST_CASE ("p46 grid controller: the core's controller queue plays a part (notes, pressure) and the drum kit")
{
    CtlRig rig;
    rig.run (1);
    // part 1 (not MPE): note on, poly aftertouch, note off on any channel - re-channelled to the part
    rig.core.queueControllerEvent (0, MidiEvent::noteOn (5, 60, 100));
    rig.run (2);
    CHECK (rig.core.lastNote() == 60);
    CHECK (rig.core.harmony().heldPitchClasses() == (uint16_t) 1); // the harmony brain heard C
    MidiEvent at;
    at.status = 0xa0;
    at.data1 = 60;
    at.data2 = 90;
    rig.core.queueControllerEvent (0, at);
    rig.run (2);
    rig.core.queueControllerEvent (0, MidiEvent::noteOff (5, 60));
    rig.run (40);
    CHECK (rig.core.harmony().heldPitchClasses() == 0);
    CHECK (allFinite (rig.all));
    CHECK (noDenormals (rig.all));
    CHECK (peak (rig.all) > 1.0e-4f); // it sounded
    CHECK (peak (rig.all) < 4.0f);
    // the drum kit: notes on the drum channel, not heard by the harmony brain
    rig.core.harmony().clearMemory();
    rig.core.queueControllerEvent (Core::drumPart, MidiEvent::noteOn (1, 38, 110));
    rig.run (2);
    CHECK (rig.core.lastNote() == 38);
    CHECK_FALSE (rig.core.harmony().heard());
    rig.core.queueControllerEvent (Core::drumPart, MidiEvent::noteOff (1, 38));
    // other parts are ignored
    rig.core.queueControllerEvent (7, MidiEvent::noteOn (1, 70, 100));
    rig.run (2);
    CHECK (rig.core.lastNote() == 38);
    CHECK (allFinite (rig.all));
}

TEST_CASE ("p46 grid controller: settings - (p53: on with automatic ports by default), ports and options round trip")
{
    using namespace ohpe::settings;
    UserSettings s;
    CHECK (s.controllerProfile == ControllerGrid1); // p53: connects by itself when the controller is found
    CHECK (s.controllerAuto);
    CHECK_FALSE (s.controllerChosen);
    CHECK (s.controllerIn.empty());
    CHECK (UserSettings::fromJson (s.toJson()) == s);
    s.controllerProfile = ControllerGrid1;
    s.controllerIn = "Pad Grid Port 1";
    s.controllerOut = "Pad Grid Port 1";
    s.controllerMaster = 1;
    s.controllerStrip = 1;
    const auto back = UserSettings::fromJson (s.toJson());
    CHECK (back == s);
    CHECK (back.controllerProfile == ControllerGrid1);
    CHECK (back.controllerIn == "Pad Grid Port 1");
    CHECK (back.controllerStrip == 1);
    CHECK (back.extras.find ("controller") == std::string::npos); // a known key, not an unknown extra
    const auto junk = UserSettings::fromJson (R"({"controller":{"profile":"other","in":5,"chosen":true}})");
    CHECK (junk.controllerProfile == ControllerOff);
    CHECK (junk.controllerIn.empty());
}

// ---------------------------------------------------------------- p53
TEST_CASE ("p53 grid controller: decoding - touch notes and the strip touch are never pads, encoders are relative")
{
    // encoder touches: notes 0 .. 10 (0 .. 7 the eight encoders, 8 master, 9 swing, 10 tempo), the strip touch 12
    for (int n = 0; n <= 10; ++n)
    {
        const auto d = g::decode (0x90, n, 127);
        CHECK (d.kind == g::Input::EncoderTouch);
        CHECK (d.index == n);
        CHECK (d.value == 1);
        CHECK (g::decode (0x90, n, 0).value == 0); // note-on velocity 0 = released
        CHECK (g::decode (0x80, n, 64).value == 0);
    }
    CHECK (g::decode (0x90, g::touchNoteStrip, 127).kind == g::Input::StripTouch);
    CHECK (g::decode (0x90, 11, 127).kind == g::Input::Other);
    CHECK (g::decode (0x90, 35, 127).kind == g::Input::Other);
    // pads 36 .. 99
    CHECK (g::decode (0x90, 36, 90).kind == g::Input::Pad);
    CHECK (g::decode (0x90, 36, 90).index == 0);
    CHECK (g::decode (0x90, 99, 90).index == 63);
    CHECK (g::decode (0xa0, 40, 33).kind == g::Input::Pressure);
    // encoders: CC 71 .. 78, master 79, tempo 14, swing 15 - two's complement relative steps
    for (int i = 0; i < 8; ++i)
    {
        const auto d = g::decode (0xb0, 71 + i, 1);
        CHECK (d.kind == g::Input::EncoderTurn);
        CHECK (d.index == i);
        CHECK (d.value == 1);
        CHECK (g::decode (0xb0, 71 + i, 127).value == -1);
        CHECK (g::decode (0xb0, 71 + i, 63).value == 63);
        CHECK (g::decode (0xb0, 71 + i, 64).value == -64);
        CHECK (g::decode (0xb0, 71 + i, 65).value == -63);
    }
    CHECK (g::decode (0xb0, 79, 3).index == g::touchNoteMaster);
    CHECK (g::decode (0xb0, 14, 3).index == g::touchNoteTempo);
    CHECK (g::decode (0xb0, 15, 3).index == g::touchNoteSwing);
    CHECK (g::decode (0xb0, 14, 125).value == -3);
    // buttons stay buttons, the strip is a 14-bit bend
    CHECK (g::decode (0xb0, g::cc::play, 127).kind == g::Input::Button);
    CHECK (g::decode (0xb0, g::cc::play, 127).index == g::cc::play);
    const auto strip = g::decode (0xe0, 0x00, 0x40);
    CHECK (strip.kind == g::Input::Strip);
    CHECK (strip.value == 8192);
}

TEST_CASE ("p53 grid controller: identity, user / live mode, splash, device keys")
{
    const auto req = g::identityRequestSysex();
    CHECK (req == std::vector<uint8_t> { 0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7 });
    // a first-generation reply (one-byte maker id, model 0x15), a later one (three-byte maker id), another device
    const uint8_t first[] = { 0xf0, 0x7e, 0x00, 0x06, 0x02, 0x47, 0x15, 0x00, 0x19, 0x00, 0x01, 0x00, 0x00, 0x00, 0xf7 };
    const uint8_t later[] = { 0xf0, 0x7e, 0x01, 0x06, 0x02, 0x00, 0x21, 0x1d, 0x67, 0x32, 0x02, 0x00, 0x01, 0x00, 0xf7 };
    const uint8_t other[] = { 0xf0, 0x7e, 0x00, 0x06, 0x02, 0x41, 0x10, 0x01, 0x00, 0x00, 0xf7 };
    const uint8_t note[] = { 0x90, 0x24, 0x40 };
    CHECK (g::identityGeneration (first, sizeof (first)) == g::Generation::First);
    CHECK (g::identityGeneration (later, sizeof (later)) == g::Generation::Later);
    CHECK (g::identityGeneration (other, sizeof (other)) == g::Generation::Unknown);
    CHECK (g::identityGeneration (note, sizeof (note)) == g::Generation::Unknown);
    CHECK (g::identityGeneration (nullptr, 0) == g::Generation::Unknown);
    // user / live mode switch
    CHECK (g::userModeSysex (g::Generation::First, true) == std::vector<uint8_t> { 0xf0, 0x47, 0x7f, 0x15, 0x62, 0x00, 0x01, 0x01, 0xf7 });
    CHECK (g::userModeSysex (g::Generation::First, false)[7] == 0x00);
    CHECK (g::userModeSysex (g::Generation::Later, true) == std::vector<uint8_t> { 0xf0, 0x00, 0x21, 0x1d, 0x01, 0x01, 0x0a, 0x01, 0xf7 });
    CHECK (g::userModeSysex (g::Generation::Unknown, true).empty());
    for (auto gen : { g::Generation::First, g::Generation::Later })
        for (uint8_t b : g::userModeSysex (gen, true))
            if (b != 0xf0 && b != 0xf7) CHECK (b < 0x80);
    // splash: four full lines, EN / DE
    const auto en0 = g::splashLines (0, false), en1 = g::splashLines (1, false), de0 = g::splashLines (0, true), de1 = g::splashLines (1, true);
    for (const auto* l : { &en0, &en1, &de0, &de1 })
        for (const auto& line : *l) CHECK (line.size() == (size_t) g::lineLength);
    CHECK (en0[1].find ("O H P E") != std::string::npos);
    CHECK (en0[2].find ("connecting") != std::string::npos);
    CHECK (en1[2].find ("connected") != std::string::npos);
    CHECK (de0[2].find ("verbindet") != std::string::npos);
    CHECK (de1[2].find ("verbunden") != std::string::npos);
    // the device part of port names: live and user port of one device, macOS / Linux and Windows
    CHECK (g::deviceKey ("Pad Grid User Port") == g::deviceKey ("Pad Grid Live Port"));
    CHECK (g::deviceKey ("MIDIIN2 (Pad Grid)") == g::deviceKey ("Pad Grid"));
    CHECK (g::deviceKey ("MIDIOUT2 (Pad Grid)") == "padgrid");
    CHECK (g::deviceKey ("Pad Grid 2 User Port") != g::deviceKey ("Pad Grid User Port"));
    CHECK (g::deviceKey ("Other Box User Port") != g::deviceKey ("Pad Grid User Port"));
}
