// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p59: grid keyboard layouts - Fourths / Thirds / Whole Tones / Sequential, In Key / Chromatic, the fixed C, the
// migration of the old In Key layout, the parameters at the end of the list, the chord shapes of the chord guide.
#include <doctest/doctest.h>

#include "OhpeCore.h"
#include "control/GridProtocol.h"
#include "harmony/HarmonyBrain.h"
#include "seq/ScaleChord.h"

#include <memory>

using namespace ohpe;
using namespace ohpe::harmony;

TEST_CASE ("p59 grid layouts: Chromatic steps of every layout, fixed C and the key's root")
{
    // fixed: bottom-left = C (48 = C3); right / up in semitones
    CHECK (gridNote (LayoutFourths, 1, 1, 48, 2, scales::Major, false, true) == 48 + 1 + 5);
    CHECK (gridNote (LayoutThirds, 1, 1, 48, 2, scales::Major, false, true) == 48 + 1 + 4);
    CHECK (gridNote (LayoutWhole, 1, 1, 48, 2, scales::Major, false, true) == 48 + 2 + 5);
    CHECK (gridNote (LayoutSequential, 1, 1, 48, 2, scales::Major, false, true) == 48 + 1 + 8);
    // sequential rows continue: the last pad of a row + 1 = the first pad of the next row
    CHECK (gridNote (LayoutSequential, 7, 0, 48, 0, scales::Major) + 1 == gridNote (LayoutSequential, 0, 1, 48, 0, scales::Major));
    // not fixed (the default): bottom-left = the key's root above the octave's C, rows relative
    for (int root = 0; root < 12; ++root)
        for (int layout = 0; layout < numLayouts; ++layout)
        {
            CHECK (gridNote (layout, 0, 0, 48, root, scales::Minor, false, false) == 48 + root);
            const auto st = layoutSteps (layout);
            CHECK (gridNote (layout, 3, 2, 48, root, scales::Minor, false, false) == 48 + root + 3 * st.col + 2 * st.row);
        }
    auto core = std::make_unique<Core>(); // on the heap (Windows stack)
    CHECK (gridOptions (core->params()).fixed == false); // default: not fixed
}

TEST_CASE ("p59 grid layouts: In Key - only key notes, a row up = the same number of scale degrees")
{
    const int degreesPerRow[] = { 3, 2, 3, 8 }; // Fourths, Thirds, Whole Tones, Sequential
    for (int layout = 0; layout < numLayouts; ++layout)
    {
        CHECK (layoutDegrees (layout).row == degreesPerRow[layout]);
        for (int root : { 0, 2, 7, 9, 11 })
            for (int scale : { (int) scales::Major, (int) scales::Minor, (int) scales::Dorian })
                for (int fixed = 0; fixed < 2; ++fixed)
                    for (int row = 0; row < 8; ++row)
                        for (int col = 0; col < 8; ++col)
                        {
                            const int n = gridNote (layout, col, row, 36, root, scale, true, fixed != 0);
                            if (n >= 127) continue; // clamped at the top
                            REQUIRE (scales::inScale (n, root, scale));
                            // a row up = degreesPerRow scale steps above the cell below
                            if (row > 0)
                                CHECK (n == scales::degreeAbove (gridNote (layout, col, row - 1, 36, root, scale, true, fixed != 0),
                                                                 degreesPerRow[layout], root, scale));
                            if (col > 0)
                                CHECK (n == scales::degreeAbove (gridNote (layout, col - 1, row, 36, root, scale, true, fixed != 0), 1, root, scale));
                        }
    }
    // not fixed: the bottom-left cell is the root in every key; fixed: the first key note at or above C
    CHECK (gridNote (LayoutFourths, 0, 0, 48, 2, scales::Minor, true, false) == 50); // D
    CHECK (gridNote (LayoutFourths, 0, 0, 48, 2, scales::Minor, true, true) == 48);  // C is in D minor
    CHECK (gridNote (LayoutFourths, 0, 0, 48, 2, scales::Major, true, true) == 49);  // D major: no C - the first key note is C#
    // a chromatic scale plays In Key like Chromatic
    CHECK (gridNote (LayoutThirds, 2, 1, 48, 0, scales::Chromatic, true, true) == gridNote (LayoutThirds, 2, 1, 48, 0, scales::Chromatic));
}

TEST_CASE ("p59 grid layouts: not fixed - the same chord shape gives the same chord in every key")
{
    // In Key + Fourths: root (0,0), third (2,0), fifth (1,1) - the chord guide's triad shape
    for (int root = 0; root < 12; ++root)
    {
        const int r = gridNote (LayoutFourths, 0, 0, 48, root, scales::Major, true, false);
        const int t = gridNote (LayoutFourths, 2, 0, 48, root, scales::Major, true, false);
        const int f = gridNote (LayoutFourths, 1, 1, 48, root, scales::Major, true, false);
        CHECK (t - r == 4); // major third
        CHECK (f - r == 7); // fifth
        const int s7 = gridNote (LayoutFourths, 0, 2, 48, root, scales::Major, true, false); // 7th: two rows up
        CHECK (s7 - r == 11); // maj7 on the root of a major key
        // the minor key: the same shape is a minor triad
        const int rm = gridNote (LayoutFourths, 0, 0, 48, root, scales::Minor, true, false);
        CHECK (gridNote (LayoutFourths, 2, 0, 48, root, scales::Minor, true, false) - rm == 3);
        CHECK (gridNote (LayoutFourths, 1, 1, 48, root, scales::Minor, true, false) - rm == 7);
        // Chromatic Fourths: major (0,0) (4,0) (2,1); minor (0,0) (3,0) (2,1); dom7 + (0,2); maj7 + (1,2)... as the guide
        const int c0 = gridNote (LayoutFourths, 0, 0, 48, root, scales::Major, false, false);
        CHECK (gridNote (LayoutFourths, 4, 0, 48, root, scales::Major) - gridNote (LayoutFourths, 0, 0, 48, root, scales::Major) == 4);
        CHECK (gridNote (LayoutFourths, 2, 1, 48, root, scales::Major, false, false) - c0 == 7);
        CHECK (gridNote (LayoutFourths, 0, 2, 48, root, scales::Major, false, false) - c0 == 10); // b7
        CHECK (gridNote (LayoutFourths, 1, 2, 48, root, scales::Major, false, false) - c0 == 11); // maj7
        CHECK (gridNote (LayoutFourths, 2, 2, 48, root, scales::Major, false, false) - c0 == 12); // octave
    }
}

TEST_CASE ("p59 grid layouts: parameters at the end of the list, migration of the old In Key layout")
{
    auto owner = std::make_unique<Core>();
    auto& reg = owner->params();
    REQUIRE (reg.indexOf ("harmony.kb.inkey") >= 0);
    CHECK (reg.indexOf ("harmony.kb.fixed") == reg.indexOf ("harmony.kb.inkey") + 1);
    CHECK (reg.indexOf ("harmony.kb.inkey") == reg.indexOf ("p4.mpe.pcurve") + 1);
    CHECK (reg.get ("harmony.kb.inkey") == 0.0f);
    CHECK (reg.get ("harmony.kb.fixed") == 0.0f);
    CHECK (reg.descriptor (reg.indexOf ("harmony.kb.layout")).choices.size() == 4);

    // a document from before: layout 3 (In Key) -> Fourths + In Key
    std::map<std::string, float> old { { "harmony.kb.layout", 3.0f } };
    CHECK (migrateLayout (old));
    CHECK (old["harmony.kb.layout"] == (float) LayoutFourths);
    CHECK (old["harmony.kb.inkey"] == 1.0f);
    std::map<std::string, float> thirds { { "harmony.kb.layout", 1.0f } };
    CHECK_FALSE (migrateLayout (thirds));
    std::map<std::string, float> now { { "harmony.kb.layout", 3.0f }, { "harmony.kb.inkey", 0.0f } }; // p59: Sequential
    CHECK_FALSE (migrateLayout (now));
    CHECK (now["harmony.kb.layout"] == 3.0f);

    // a session from before p59 (no "harmony.kb.inkey"): In Key comes back as Fourths + In Key
    reg.set ("harmony.kb.layout", 3.0f);
    reg.set ("harmony.kb.inkey", 0.0f);
    auto json = owner->saveState();
    const auto at = json.find ("\"harmony.kb.inkey\"");
    REQUIRE (at != std::string::npos);
    json.replace (at, std::string ("\"harmony.kb.inkey\"").size(), "\"harmony.kb.unknownkey\"");
    REQUIRE (owner->loadState (json));
    CHECK (reg.get ("harmony.kb.layout") == (float) LayoutFourths);
    CHECK (reg.get ("harmony.kb.inkey") == 1.0f);
    // a p59 session keeps Sequential and both toggles (round trip)
    reg.set ("harmony.kb.layout", (float) LayoutSequential);
    reg.set ("harmony.kb.inkey", 0.0f);
    reg.set ("harmony.kb.fixed", 1.0f);
    REQUIRE (owner->loadState (owner->saveState()));
    CHECK (reg.get ("harmony.kb.layout") == (float) LayoutSequential);
    CHECK (reg.get ("harmony.kb.inkey") == 0.0f);
    CHECK (reg.get ("harmony.kb.fixed") == 1.0f);
}

TEST_CASE ("p59 grid layouts: the controller pads take In Key / fixed like the screen")
{
    gridctl::PadKey key;
    key.root = 4;
    key.scale = scales::Minor;
    for (int layout = 0; layout < numLayouts; ++layout)
        for (int inKey = 0; inKey < 2; ++inKey)
            for (int fixed = 0; fixed < 2; ++fixed)
                for (int pad = 0; pad < gridctl::numPads; ++pad)
                    CHECK (gridctl::padLayoutNote (layout, pad % 8, pad / 8, 3, key, inKey != 0, fixed != 0)
                           == gridNote (layout, pad % 8, pad / 8, 48, 4, scales::Minor, inKey != 0, fixed != 0));
}
