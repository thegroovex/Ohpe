// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p62 (1.0 format freeze): the frozen fixtures through the plug-in's own paths - a saved host session (state JSON +
// tape audio) restores, a session's single-uid ui block names every part, and the golden .ohpepack / the old flat zip
// install. Fixtures: tests/formats/fixtures (docs/FORMATS.md).
#include <doctest/doctest.h>

#include "model/FormatVersions.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Updater.h"

#include <memory>

using namespace ohpe;

namespace
{
juce::File fixture (const char* rel) { return juce::File (OHPE_SOURCE_DIR).getChildFile ("tests/formats/fixtures").getChildFile (rel); }

struct FormatDir
{
    juce::TemporaryFile tmp;
    juce::File root;
    FormatDir() : root (tmp.getFile()) { root.createDirectory(); }
    ~FormatDir() { root.deleteRecursively(); }
};
} // namespace

TEST_CASE ("formats (plug-in): a saved session with tape audio restores; re-saved it is the current version")
{
    juce::MemoryBlock mb;
    REQUIRE (fixture ("state/plugin-state-v1-with-tape.bin").loadFileAsData (mb));
    auto p = std::make_unique<OhpeProcessor>();
    p->prepareToPlay (48000.0, 256);
    p->setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (p->core().params().get ("tempo") == doctest::Approx (125.0f));
    CHECK (p->core().params().get ("p1.vol") == doctest::Approx (0.625f));
    CHECK (p->core().tape().lengthSeconds() == doctest::Approx (30.0));
    CHECK (p->core().tape().contentEndFrames (0) > 2 * 4096);

    juce::MemoryBlock again;
    p->getStateInformation (again);
    const auto text = juce::String::fromUTF8 ((const char*) again.getData());
    const auto parsed = juce::JSON::parse (text);
    CHECK ((int) parsed["version"] == formats::state);
    CHECK (parsed["format"].toString() == "ohpe-state");
    auto q = std::make_unique<OhpeProcessor>();
    q->prepareToPlay (48000.0, 256);
    q->setStateInformation (again.getData(), (int) again.getSize());
    CHECK (q->core().params().get ("tempo") == doctest::Approx (125.0f));
    CHECK (q->core().tape().contentEndFrames (0) == p->core().tape().contentEndFrames (0));
}

TEST_CASE ("formats (plug-in): the session ui block - old single 'preset' and the current 'presets'")
{
    const auto parsed = juce::JSON::parse (fixture ("state/state-v1-0.6-in-key.json").loadFileAsString());
    REQUIRE (parsed.isObject());
    for (const auto& u : OhpeProcessor::sessionPresetUids (parsed["sections"]["ui"])) CHECK (u == "factory:Glass Keys.ohpepreset");
    const auto cur = juce::JSON::parse (fixture ("state/state-v1.json").loadFileAsString());
    REQUIRE (cur.isObject());
    const auto uids = OhpeProcessor::sessionPresetUids (cur["sections"]["ui"]);
    CHECK (uids[0] == "factory:Glass Keys.ohpepreset");
    CHECK (uids[1].empty());
    CHECK (uids[2] == "user:Deep Bass.ohpepreset");
}

TEST_CASE ("formats (plug-in): the golden .ohpepack (format 1) and the old flat zip install")
{
    FormatDir d;
    const auto data = d.root.getChildFile ("data"), content = data.getChildFile ("Content");
    Updater u;
    u.setDataFolderForTests (data);

    const auto pack = d.root.getChildFile ("format-fixture-1.0.0.ohpepack");
    REQUIRE (fixture ("pack/format-fixture-1.0.0.ohpepack").copyFileTo (pack));
    u.installPackFile (pack);
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().message == "Content pack installed");
    CHECK (content.getChildFile ("format-fixture/presets/Pack Keys.ohpepreset").existsAsFile());
    CHECK (content.getChildFile ("format-fixture/readme.txt").existsAsFile());

    const auto flat = d.root.getChildFile ("Ohpe-content-legacy-flat-1.0.0.zip");
    REQUIRE (fixture ("pack/Ohpe-content-legacy-flat-1.0.0.zip").copyFileTo (flat));
    u.installPackFile (flat);
    REQUIRE (u.waitIdle (20000));
    CHECK (u.snapshot().message == "Content pack installed");
    CHECK (content.getChildFile ("legacy-flat/presets/Flat Keys.ohpepreset").existsAsFile());
    CHECK (u.installedPacks().size() == 2);
}
