// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Wavetable banks: the user-table library also reads the wavetables of installed content packs (read-only, listed
// with their pack), a part uses one by its reference "<pack>/<table>", part presets and projects store only that
// reference, loading resolves it again - and names the pack when it is missing.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserTables.h"
#include "preset/Preset.h"

#include <cmath>

using namespace ohpe;

namespace
{
void writeWaveSet (const juce::File& f, int frames)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::AudioBuffer<float> buf (1, frames * 2048);
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const int fr = i / 2048;
        const double ph = 2.0 * 3.14159265358979 * (double) (i % 2048) / 2048.0;
        float v = 0.0f;
        for (int h = 1; h <= fr + 1; ++h) v += (float) (0.5 / h * std::sin (ph * h));
        buf.setSample (0, i, v);
    }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (f);
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
    REQUIRE (w != nullptr);
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

struct Dir
{
    juce::TemporaryFile tmp;
    juce::File root;
    Dir() : root (tmp.getFile()) { root.createDirectory(); }
    ~Dir() { root.deleteRecursively(); }
};
} // namespace

TEST_CASE ("wavetable banks: pack tables listed read-only, used by reference, stored and resolved in presets")
{
    Dir d;
    const auto content = d.root.getChildFile ("Content");
    writeWaveSet (content.getChildFile ("glass/wavetables/glass-choir.wav"), 8);
    writeWaveSet (content.getChildFile ("glass/wavetables/vowels/aah.wav"), 2);
    content.getChildFile ("glass/pack.json").replaceWithText (R"({"format":1,"id":"glass","version":"1.0.0","kind":"wavetables","name":{"en":"Glass","de":"Glas"}})");
    content.getChildFile ("atmo/presets").createDirectory(); // a presets pack: no tables
    content.getChildFile ("atmo/presets/x.ohpepreset").replaceWithText ("{}");
    content.getChildFile ("glass/wavetables/notes.txt").replaceWithText ("not a table");

    OhpeProcessor proc;
    auto& ut = proc.userTables();
    ut.setContentFolder (content);
    ut.setLibraryFolder (d.root.getChildFile ("Wavetables"));

    // listed with their pack (UI language), sorted
    i18n::setCurrent (i18n::Language::German);
    auto tables = ut.packTables();
    REQUIRE (tables.size() == 2);
    CHECK (tables[0].ref() == "glass/glass-choir");
    CHECK (tables[0].packName == "Glas");
    CHECK (tables[1].ref() == "glass/vowels/aah");
    CHECK (tables[1].name() == "aah");
    i18n::setCurrent (i18n::Language::English);
    CHECK (ut.packTables()[0].packName == "Glass");
    // read-only: nothing is written into the user library or the pack
    const auto before = content.findChildFiles (juce::File::findFiles, true).size();

    // use one: published at once, stored by reference only
    REQUIRE (ut.usePackTable (1, "glass/glass-choir"));
    auto t = usertable::tableOf (proc.core(), 1, usertable::Target::Wavetable);
    REQUIRE (t != nullptr);
    CHECK (t->numFrames() == 8);
    CHECK (t->contentRef == "glass/glass-choir");
    CHECK (ut.lastMessage() == "Wavetable ready");
    CHECK (content.findChildFiles (juce::File::findFiles, true).size() == before);
    CHECK_FALSE (d.root.getChildFile ("Wavetables").exists());
    CHECK_FALSE (ut.usePackTable (1, "glass/nope"));
    CHECK_FALSE (ut.usePackTable (1, "other-pack/table"));
    CHECK (ut.lastMessage() == "Content pack missing - install it in Settings > Updates");
    CHECK (usertable::tableOf (proc.core(), 1, usertable::Target::Wavetable) == t); // failures keep the table

    // part preset: the usertables block holds the reference; loading resolves it
    auto p = preset::capturePart (proc.core().params(), 1, { "Glass Lead" });
    p.userTables = ut.partJson (1);
    REQUIRE (p.userTables.has_value());
    CHECK (p.userTables->find ("\"content\":\"glass/glass-choir\"") != std::string::npos);
    CHECK (p.userTables->find ("\"data\"") == std::string::npos);
    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (p), back));
    REQUIRE (proc.presets().apply (back, 2));
    auto t2 = usertable::tableOf (proc.core(), 2, usertable::Target::Wavetable);
    REQUIRE (t2 != nullptr);
    CHECK (t2->contentRef == "glass/glass-choir");
    CHECK (t2->numFrames() == 8);

    // project state round trip
    ut.storeInState();
    const auto state = proc.core().stateSection ("usertables");
    CHECK (state.find ("glass/glass-choir") != std::string::npos);
    usertable::setTable (proc.core(), 1, usertable::Target::Wavetable, nullptr);
    usertable::setTable (proc.core(), 2, usertable::Target::Wavetable, nullptr);
    const auto r = ut.restoreFromState();
    CHECK (r.loaded == 2);
    CHECK (usertable::tableOf (proc.core(), 2, usertable::Target::Wavetable) != nullptr);

    // the pack is removed: the preset names it, the part plays the built-in table
    content.getChildFile ("glass").deleteRecursively();
    CHECK (ut.packTables().empty());
    REQUIRE (proc.presets().apply (back, 3));
    CHECK (usertable::tableOf (proc.core(), 3, usertable::Target::Wavetable) == nullptr);
    CHECK (ut.missingPacks() == juce::StringArray ("glass"));
    CHECK (ut.lastMessage() == "Content pack missing - install it in Settings > Updates");
    CHECK (ut.lastDetail() == "glass");
}
