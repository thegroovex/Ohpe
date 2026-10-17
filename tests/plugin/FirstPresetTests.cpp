// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p40: a new instance (first start of the app, a new plug-in instance without host state) starts with the first
// factory preset of the library on every part - not with the plain defaults ("Init") - with an empty tape and empty
// sequencers. A restored session / host state replaces it, including the preset name.

#include <doctest/doctest.h>

#include "plugin/PluginProcessor.h"

#include <cmath>

using namespace ohpe;

namespace
{
/** Switches the start sound on for one test (main() switches it off for every other test). */
struct StartSound
{
    explicit StartSound (bool on) : before (OhpeProcessor::startWithFactorySound.load()) { OhpeProcessor::startWithFactorySound.store (on); }
    ~StartSound() { OhpeProcessor::startWithFactorySound.store (before); }
    bool before;
};

bool sameRegistry (OhpeProcessor& a, OhpeProcessor& b, juce::String& firstDifference)
{
    const auto& ra = a.core().params();
    const auto& rb = b.core().params();
    if (ra.size() != rb.size()) { firstDifference = "size"; return false; }
    for (int i = 0; i < ra.size(); ++i)
    {
        const float x = ra.get (i), y = rb.get (i);
        if (std::abs (x - y) > 1.0e-4f * std::max (1.0f, std::abs (y)))
        {
            firstDifference = juce::String (ra.descriptor (i).id) + ": " + juce::String (x) + " vs " + juce::String (y);
            return false;
        }
    }
    return true;
}

juce::String name (const preset::Entry& e) { return juce::String::fromUTF8 (e.meta.name.c_str()); }
} // namespace

TEST_CASE ("first start: a new instance loads the first factory preset on every part, tape and sequencers empty (p40)")
{
    StartSound on (true);
    OhpeProcessor fresh;
    auto& pm = fresh.presets();
    const int first = pm.startSoundIndex();
    REQUIRE (first >= 0);
    const auto& e = pm.entries()[(size_t) first];
    CHECK (e.factory);
    CHECK (e.uid.rfind ("factory:", 0) == 0);
    // first of the built-in bank in the library's default order (by name)
    for (const auto& other : pm.entries())
        if (other.uid.rfind ("factory:", 0) == 0)
            CHECK_MESSAGE (name (other).toLowerCase().compare (name (e).toLowerCase()) >= 0, name (other) + " sorts before " + name (e));

    // the display names it - never "Init"
    CHECK (pm.current() == first);
    CHECK (pm.currentName() == name (e));
    CHECK (pm.currentName() != "Init");
    CHECK (pm.currentUid() == e.uid);

    // exactly what loading it from the library into each part (or as a project) gives - nothing else changed
    {
        StartSound off (false);
        OhpeProcessor plain;
        CHECK (plain.presets().current() == -1);
        CHECK (plain.presets().currentName() == "Init");
        juce::String diff;
        CHECK_MESSAGE (! sameRegistry (fresh, plain, diff), "the start sound differs from the plain defaults");
    }
    {
        StartSound off (false);
        OhpeProcessor manual;
        if (e.meta.kind == preset::Kind::Project) REQUIRE (manual.presets().load (first, 0));
        else
            for (int part = 0; part < Core::numParts; ++part) REQUIRE (manual.presets().load (first, part));
        juce::String diff;
        CHECK_MESSAGE (sameRegistry (fresh, manual, diff), "differs from loading it by hand: " + diff);
    }
    // a part preset: all four parts carry the same sound, each on its own MIDI channel
    if (e.meta.kind == preset::Kind::Part)
    {
        const auto& reg = fresh.core().params();
        CHECK (reg.get ("p1.engine") == reg.get ("p2.engine"));
        CHECK (reg.get ("p1.engine") == reg.get ("p4.engine"));
        CHECK (reg.get ("p1.gain") == doctest::Approx (reg.get ("p3.gain")));
        for (int part = 0; part < Core::numParts; ++part)
            CHECK (reg.get ("p" + std::to_string (part + 1) + ".midich") == doctest::Approx ((float) part + 1.0f));
    }

    // empty tape, empty sequencers, nothing playing
    CHECK (fresh.core().tape().contentEndFrames() <= 0);
    {
        StartSound off (false);
        OhpeProcessor plain;
        CHECK (fresh.core().stepSequencer().toJson() == plain.core().stepSequencer().toJson());
        CHECK (fresh.core().orbitSequencer().toJson() == plain.core().orbitSequencer().toJson());
    }
    CHECK (fresh.core().params().get ("tape.play") < 0.5f);
    CHECK (fresh.core().params().get ("tape.rec") < 0.5f);

    // it plays at once (a note on part 1's channel), finite
    fresh.enableAllBuses();
    fresh.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (fresh.getTotalNumOutputChannels(), 512);
    float peak = 0.0f;
    bool finite = true;
    for (int b = 0; b < 40; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        buffer.clear();
        fresh.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                finite = finite && std::isfinite (buffer.getSample (ch, i));
        peak = juce::jmax (peak, buffer.getMagnitude (0, 0, 512));
    }
    CHECK (finite);
    CHECK (peak > 0.001f);
    CHECK (peak < 4.0f);
}

TEST_CASE ("first start: a restored session / host state wins over the start sound, name included (p40)")
{
    StartSound on (true);
    juce::MemoryBlock session;
    std::string savedUid;
    std::vector<float> savedValues;
    {
        OhpeProcessor a;
        auto& pm = a.presets();
        // another factory preset into part 2 and an edited value: a "song"
        int other = -1;
        for (int i = 0; i < (int) pm.entries().size(); ++i)
            if (i != pm.startSoundIndex() && pm.entries()[(size_t) i].uid.rfind ("factory:", 0) == 0
                && pm.entries()[(size_t) i].meta.kind == preset::Kind::Part) { other = i; break; }
        REQUIRE (other >= 0);
        REQUIRE (pm.load (other, 1));
        a.parameter ("mix.ch3.pan")->setValueNotifyingHost (0.9f);
        savedUid = pm.currentUid();
        CHECK (savedUid == pm.entries()[(size_t) other].uid);
        a.getStateInformation (session);
        for (int i = 0; i < a.core().params().size(); ++i) savedValues.push_back (a.core().params().get (i));
    }
    OhpeProcessor b; // a new instance starts with the start sound, then the host restores its state
    b.setStateInformation (session.getData(), (int) session.getSize());
    CHECK (b.presets().currentUid() == savedUid);
    REQUIRE ((int) savedValues.size() == b.core().params().size());
    int differing = 0;
    for (int i = 0; i < b.core().params().size(); ++i)
        if (std::abs (b.core().params().get (i) - savedValues[(size_t) i]) > 1.0e-4f * std::max (1.0f, std::abs (savedValues[(size_t) i]))) ++differing;
    CHECK (differing == 0);
    CHECK (b.parameter ("mix.ch3.pan")->getValue() == doctest::Approx (0.9f).epsilon (1e-4));

    // a session saved from the plain defaults (older versions, or nothing loaded): the start sound is gone, "Init"
    juce::MemoryBlock plainSession;
    {
        StartSound off (false);
        OhpeProcessor plain;
        plain.getStateInformation (plainSession);
    }
    OhpeProcessor c;
    CHECK (c.presets().currentName() != "Init");
    c.setStateInformation (plainSession.getData(), (int) plainSession.getSize());
    CHECK (c.presets().current() == -1);
    CHECK (c.presets().currentName() == "Init");
    {
        StartSound off (false);
        OhpeProcessor plain;
        juce::String diff;
        CHECK_MESSAGE (sameRegistry (c, plain, diff), "restored defaults differ: " + diff);
    }
    // the round trip keeps the name, and a second save writes the same uid
    juce::MemoryBlock again;
    b.getStateInformation (again);
    OhpeProcessor d;
    d.setStateInformation (again.getData(), (int) again.getSize());
    CHECK (d.presets().currentUid() == savedUid);
}
