// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p16: the morph pad on screen (LFO mode, view Morph) - pages, corner assignment from the current sound and from the
// library, dragging the pad, part presets carrying the corners.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/ModFxViews.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
void play (OhpeProcessor& p, int blocks, int note = -1)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && note >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos)
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, 1, false);
}

/** A parameter's value as the host sees it (the registry follows at the next block). */
float hostPlain (OhpeProcessor& p, const juce::String& id)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    return prm->convertFrom0to1 (prm->getValue());
}

/** First factory part preset whose engine differs from `engine`. */
int presetWithOtherEngine (OhpeProcessor& p, const std::string& engine)
{
    const auto& list = p.presets().entries();
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].factory && list[(size_t) i].meta.kind == preset::Kind::Part && ! list[(size_t) i].meta.engine.empty()
            && list[(size_t) i].meta.engine != engine)
            return i;
    return -1;
}
} // namespace

TEST_CASE ("editor p16: the Morph view - pages, corners from the current sound and the library, dragging the pad")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (0);
    e.selectMode (ui::Mode::Lfo);
    e.selectView (ui::modfx::LfoViewMorph);
    CHECK (e.encoder (0).boundId() == "p1.morph.on");
    CHECK (e.encoder (1).boundId() == "p1.morph.x");
    CHECK (e.encoder (2).boundId() == "p1.morph.y");
    CHECK (e.encoder (3).boundId() == "p1.morph.glide");

    auto& core = p.core();
    CHECK_FALSE (core.morph().anyCorner (0));
    e.assignMorphCorner (0, -1); // A = the current sound
    CHECK (core.morph().cornerSet (0, 0));
    CHECK (hostPlain (p, "p1.morph.on") >= 0.5f); // assigning switches the morph on
    const int other = presetWithOtherEngine (p, preset::engineIdOfPart (core.params(), 0));
    REQUIRE (other >= 0);
    e.assignMorphCorner (3, other); // D = a library sound with another engine
    CHECK (core.morph().cornerSet (0, 3));
    CHECK (core.morph().cornerLabel (0, 3) == p.presets().entries()[(size_t) other].meta.name);
    CHECK (core.morph().numLanes (0) >= 2);

    // dragging the pad moves X / Y (bottom right = D)
    auto& d = e.displayComponent();
    const auto pad = d.morphPadArea();
    REQUIRE (! pad.isEmpty());
    const auto at = pad.getBottomRight() - juce::Point<float> (1.0f, 1.0f);
    d.mouseDown (mouseEvent (d, at));
    d.mouseUp (mouseEvent (d, at));
    CHECK (hostPlain (p, "p1.morph.x") > 0.95f);
    CHECK (hostPlain (p, "p1.morph.y") < 0.05f);
    play (p, 40, 60);
    CHECK (core.morph().uiActive (0));
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    e.setLanguage (i18n::Language::English);

    // the corners travel with a part preset (saved as a user preset, loaded into part 2)
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ohpe-morph-ui-test");
    tmp.deleteRecursively();
    p.presets().setUserFolder (tmp);
    preset::Meta meta;
    meta.name = "Morph Test";
    const auto file = p.presets().save (meta, false, 0);
    REQUIRE (file.existsAsFile());
    CHECK (file.loadFileAsString().contains ("\"morph\""));
    const int idx = p.presets().indexOfFile (file);
    REQUIRE (idx >= 0);
    REQUIRE (p.presets().load (idx, 1));
    CHECK (core.morph().cornerSet (1, 0));
    CHECK (core.morph().cornerSet (1, 3));
    CHECK (core.morph().cornerParams (1, 3) == core.morph().cornerParams (0, 3));
    tmp.deleteRecursively();

    e.assignMorphCorner (0, -2);
    CHECK_FALSE (core.morph().cornerSet (0, 0));
    e.assignMorphCorner (0, -3);
    CHECK_FALSE (core.morph().anyCorner (0));
}
