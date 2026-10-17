// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p18: the breeding view - opening it (RANDOM, library, MIDI remote), breeding a brood with thumbnails and level
// matching, auditioning, keeping, saving a child as a favourite, Cancel restoring the sound, the session in the state.

#include <doctest/doctest.h>

#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
struct TempFolder
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("ohpe-breed-presets-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    TempFolder() { dir.createDirectory(); }
    ~TempFolder() { dir.deleteRecursively(); }
};

int factoryPart (OhpeProcessor& p, const std::string& engine)
{
    const auto& list = p.presets().entries();
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].factory && list[(size_t) i].meta.kind == preset::Kind::Part && list[(size_t) i].meta.engine == engine) return i;
    return -1;
}

void process (OhpeProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}
} // namespace

TEST_CASE ("editor p18: the breeding view - brood, thumbnails, audition, keep, save, cancel")
{
    TempFolder tmp;
    OhpeProcessor p;
    p.presets().setUserFolder (tmp.dir);
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (0);
    auto& reg = p.core().params();
    const float engineBefore = reg.get ("p1.engine");

    e.showBreeding (true);
    REQUIRE (e.breedingVisible());
    CHECK (e.overlayOpen());
    CHECK_FALSE (e.libraryVisible());
    auto& v = e.breedingComponent();
    v.setSeedBase (4711);
    CHECK (v.session().parent (0).set); // A = the current sound
    v.breedNew();                       // no parent B yet: nothing bred
    CHECK (v.session().empty());
    CHECK (v.statusMessage().isNotEmpty());

    const int fm = factoryPart (p, "fm");
    REQUIRE (fm >= 0);
    v.setParentFromLibrary (1, fm);
    REQUIRE (v.session().ready());
    v.setAmount (0.35f);
    v.breedNew();
    REQUIRE (v.session().current() != nullptr);
    REQUIRE (v.session().current()->children.size() == 6);
    REQUIRE (v.waitForSketches (120000));
    for (int i = 0; i < 6; ++i)
    {
        const auto s = v.sketch (i);
        CHECK (s.nonFinite == 0);
        CHECK (s.peakDb < 0.0f);
        CHECK (v.session().current()->children[(size_t) i].matched);
    }
    e.advanceAnimation (0.2f);

    // audition: the child plays in the edit part
    v.audition (2, false);
    const auto& child = v.session().current()->children[2];
    const auto& schema = v.session().schema();
    CHECK (reg.get ("p1.engine") == doctest::Approx (child.genome[(size_t) schema.engineGene()]));
    CHECK (reg.get ("p1.gain") == doctest::Approx (child.genome[(size_t) schema.gainGene()]).epsilon (0.001));
    process (p, 8);

    // keep two, breed again: generation 2 from them, undo / redo
    v.toggleKept (1);
    v.toggleKept (4);
    v.breedAgain();
    CHECK (v.session().current()->number == 2);
    CHECK (v.session().numGenerations() == 2);
    v.undo();
    CHECK (v.session().current()->number == 1);
    v.redo();
    CHECK (v.session().current()->number == 2);

    // SAVE: the child becomes a user preset, marked as a favourite
    const int saved = v.saveChild (0);
    REQUIRE (saved >= 0);
    const auto& entry = p.presets().entries()[(size_t) saved];
    CHECK_FALSE (entry.factory);
    CHECK (entry.favourite);
    CHECK (v.session().current()->children[0].kept);

    // the session lives in the state
    CHECK (p.core().stateSection ("breed").find ("generations") != std::string::npos);

    // Cancel restores the sound from before the view opened
    e.breedingComponent().close (false);
    CHECK_FALSE (e.breedingVisible());
    CHECK_FALSE (e.overlayOpen());
    process (p, 2);
    CHECK (reg.get ("p1.engine") == doctest::Approx (engineBefore));

    // a new editor brings the session back (state section)
    ed.reset();
    std::unique_ptr<juce::AudioProcessorEditor> ed2 (p.createEditor());
    auto& e2 = *dynamic_cast<OhpeEditor*> (ed2.get());
    CHECK (e2.breedingComponent().session().numGenerations() == 2);

    // the library's Breed button: current sound x that preset, bred at once
    const int analog = factoryPart (p, "wavetable");
    REQUIRE (analog >= 0);
    e2.showLibrary (true);
    REQUIRE (e2.libraryComponent().onBreed != nullptr);
    e2.libraryComponent().onBreed (analog);
    CHECK_FALSE (e2.libraryVisible());
    CHECK (e2.breedingVisible());
    CHECK (e2.breedingComponent().session().numGenerations() == 3);
    CHECK (e2.breedingComponent().session().parent (1).name == p.presets().entries()[(size_t) analog].meta.name);
    e2.showBreeding (false); // closing with the key keeps the sound
    CHECK_FALSE (e2.breedingVisible());
}

TEST_CASE ("editor p18: breeding remote controls are MIDI-mappable triggers")
{
    OhpeProcessor p;
    for (const char* id : { "lib.breed", "lib.breed.go", "lib.breed.next" })
    {
        CAPTURE (id);
        auto* prm = p.parameter (id);
        REQUIRE (prm != nullptr);
    }
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    CHECK (p.breedRequestCount (0) == 0);
    p.parameter ("lib.breed")->setValueNotifyingHost (1.0f);
    p.pollRemoteControls();
    CHECK (p.breedRequestCount (0) == 1);
    p.pollRemoteControls();
    CHECK (p.breedRequestCount (0) == 1); // held: no repeat
    e.advanceAnimation (0.05f); // the editor's timer polls the counters (the timer does not run in the tests)
    p.parameter ("lib.breed")->setValueNotifyingHost (0.0f);
    p.pollRemoteControls();
    p.parameter ("lib.breed.next")->setValueNotifyingHost (1.0f);
    p.pollRemoteControls();
    CHECK (p.breedRequestCount (2) == 1);
}
