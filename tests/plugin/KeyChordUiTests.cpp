// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p45: the voice strip never overlaps (VOICES legend, the longest note names, the chord under the dots; EN / DE,
// light / night skin, with and without soft keys), the chord of the sounding notes on the display, the key control
// on the header (drag, wheel, menu, Scale Lock, HOST read-only).

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
void process (OhpeProcessor& p, int blocks, juce::MidiBuffer first = {})
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi = first;
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

juce::MidiBuffer notes (std::initializer_list<int> ns, bool on = true)
{
    juce::MidiBuffer m;
    for (int n : ns) m.addEvent (on ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, n), 0);
    return m;
}

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos, juce::ModifierKeys mods = {})
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, pos != downPos);
}

struct Fixture
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;
    Fixture()
    {
        p.prepareToPlay (48000.0, 256);
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        REQUIRE (e != nullptr);
    }
    ~Fixture() { e->setLanguage (i18n::Language::English); }
};

void checkNoOverlap (const ui::VoiceStripLayout& l, juce::Rectangle<float> area)
{
    CHECK (l.dot > 2.0f);
    CHECK_FALSE (l.dots.intersects (l.legend));
    CHECK_FALSE (l.dots.intersects (l.note));
    CHECK_FALSE (l.chord.intersects (l.legend));
    CHECK_FALSE (l.chord.intersects (l.note));
    CHECK_FALSE (l.chord.intersects (l.dots));
    CHECK_FALSE (l.legend.intersects (l.note));
    CHECK (l.dots.getRight() < l.note.getX());
    CHECK (area.contains (l.legend));
    CHECK (area.contains (l.note));
    CHECK (area.contains (l.dots));
    CHECK (l.chord.getBottom() <= area.getBottom() + 0.01f);
    // the longest note names fit their room at the font the display uses
    for (const char* n : { "C#-1", "Db-1", "A#9", "Ab9", "G9", "H#-2" })
        CHECK (juce::GlyphArrangement::getStringWidth (ui::brandFont (l.noteFont), n) <= l.note.getWidth() + 0.5f);
    CHECK (juce::GlyphArrangement::getStringWidth (ui::labelFont (l.legendFont), "STIMMEN") <= l.legend.getWidth() + 0.5f);
}
} // namespace

TEST_CASE ("p45 voice strip: legend, note, dots and chord never overlap (EN / DE, sizes, soft keys)")
{
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        i18n::setCurrent (lang);
        const auto legend = i18n::upper (i18n::tr ("Voices"));
        for (float w : { 520.0f, 760.0f, 1080.0f, 1360.0f, 2400.0f })
            for (bool soft : { true, false })
            {
                const juce::Rectangle<float> area (30.0f, 400.0f, w, w * (soft ? 0.072f : 0.13f));
                checkNoOverlap (ui::layoutVoiceStrip (area, soft, legend), area);
            }
    }
    i18n::setCurrent (i18n::Language::English);
}

TEST_CASE ("p45 voice strip: the display shows the chord that sounds (spelled in the key), the note readout too")
{
    Fixture f;
    for (auto lang : { i18n::Language::English, i18n::Language::German })
        for (int dark = 0; dark < 2; ++dark)
        {
            f.e->setLanguage (lang);
            ui::applySkin (dark ? ui::Skin::Dark : ui::Skin::Light);
            f.e->selectPart (0);
            f.e->selectMode (ui::Mode::Synth);
            f.e->selectEngine (EngineType::Vocal);
            auto& d = f.e->displayComponent();
            checkNoOverlap (d.voiceStripLayout(), d.getLocalBounds().toFloat());
        }
    ui::applySkin (ui::Skin::Light);
    f.e->setLanguage (i18n::Language::English);
    auto& d = f.e->displayComponent();
    process (f.p, 2, notes ({ 54, 57, 61, 64 })); // F#3 A3 C#4 E4
    f.e->advanceAnimation (0.05f);
    CHECK (d.shownChord() == "F#m7");
    process (f.p, 2, notes ({ 54, 57, 61, 64 }, false));
    process (f.p, 2, notes ({ 57, 62, 66 })); // D/A (2nd inversion) in D major
    f.p.setKeyFromUi (2, scales::Major);
    f.e->advanceAnimation (0.05f);
    CHECK (d.shownChord() == "D/A");
    process (f.p, 2, notes ({ 57, 62, 66 }, false));
    f.e->advanceAnimation (0.05f);
    CHECK (d.shownChord() == "D/A"); // the last chord stays (dimmed)
    f.p.setKeyFromUi (3, scales::Major); // E flat major: flats
    process (f.p, 2, notes ({ 56, 60, 63 }));
    f.e->advanceAnimation (0.05f);
    CHECK (d.shownChord() == "Ab");
    process (f.p, 2, notes ({ 56, 60, 63 }, false));
}

TEST_CASE ("p45 key control: on the header left of TAP, drag / wheel / menu, Scale Lock, HOST read-only")
{
    Fixture f;
    auto& d = f.e->displayComponent();
    auto& k = d.headerKey();
    REQUIRE (k.isVisible());
    CHECK (k.getWidth() > 20);
    CHECK (k.getRight() <= d.headerTap().getX());
    CHECK_FALSE (k.getBounds().intersects (d.headerTempo().getBounds()));
    CHECK (k.shownText() == "C MAJ");
    // drag up 24 px = two semitones
    const juce::Point<float> c (k.getWidth() * 0.3f, k.getHeight() * 0.5f);
    k.mouseDown (mouseEvent (k, c, c));
    k.mouseDrag (mouseEvent (k, c.translated (0.0f, -24.0f), c));
    k.mouseUp (mouseEvent (k, c.translated (0.0f, -24.0f), c));
    CHECK (f.p.keyScale().root == 2);
    k.mouseWheelMove (mouseEvent (k, c, c), juce::MouseWheelDetails { 0.0f, -1.0f, false, false, false });
    CHECK (f.p.keyScale().root == 1);
    k.mouseWheelMove (mouseEvent (k, c, c, juce::ModifierKeys::shiftModifier), juce::MouseWheelDetails { 0.0f, 1.0f, false, false, false });
    CHECK (f.p.keyScale().scale == scales::Minor);
    CHECK (k.shownText() == "C# MIN");
    // the menu
    k.applyMenuResult (ui::KeyReadout::RootBase + 6);
    k.applyMenuResult (ui::KeyReadout::ScaleBase + scales::Dorian);
    CHECK (f.p.keyScale().root == 6);
    CHECK (f.p.keyScale().scale == scales::Dorian);
    CHECK (k.shownText() == "F# DOR");
    k.applyMenuResult (ui::KeyReadout::LockToggle);
    CHECK (f.p.keyScale().lock);
    CHECK (f.p.core().params().get (Core::Ids::keyLock) == 1.0f);
    k.applyMenuResult (ui::KeyReadout::LockModeBase + keys::LockWhiteKeys);
    CHECK (f.p.keyScale().lockMode == keys::LockWhiteKeys);
    // a click on the lock toggles it
    const auto lc = k.lockArea().getCentre();
    k.mouseDown (mouseEvent (k, lc, lc));
    k.mouseUp (mouseEvent (k, lc, lc));
    CHECK_FALSE (f.p.keyScale().lock);
    k.applyMenuResult (ui::KeyReadout::LearnBase + 2);
    CHECK (f.p.learningParamId() == Core::Ids::keyLock);
    f.p.cancelMidiLearn();
    // German names
    f.e->setLanguage (i18n::Language::German);
    f.p.setKeyFromUi (11, scales::Major);
    CHECK (k.shownText() == "H DUR");
    f.e->setLanguage (i18n::Language::English);
    // the host gives the key: read-only, HOST shown, the lock stays editable
    f.p.core().setHostKey (9, scales::Minor);
    f.e->advanceAnimation (0.05f);
    CHECK (f.p.keyScale().fromHost);
    CHECK (k.shownText().startsWith ("HOST"));
    f.p.setKeyFromUi (0, scales::Major);
    CHECK (f.p.keyScale().root == 9);
    k.mouseWheelMove (mouseEvent (k, c, c), juce::MouseWheelDetails { 0.0f, 1.0f, false, false, false });
    CHECK (f.p.keyScale().root == 9);
    k.applyMenuResult (ui::KeyReadout::LockToggle);
    CHECK (f.p.keyScale().lock);
    CHECK_FALSE (k.getBounds().intersects (d.headerTempo().getBounds()));
    f.p.core().setHostKey (-1, 0);
    f.e->advanceAnimation (0.05f);
    CHECK_FALSE (f.p.keyScale().fromHost);
}

TEST_CASE ("p45 scale lock through the processor: MIDI notes snap, white keys map, lock off is neutral")
{
    Fixture f;
    f.p.setKeyFromUi (0, scales::Major);
    f.p.setScaleLockFromUi (true, keys::LockNearest);
    process (f.p, 2, notes ({ 61 }));
    CHECK (chords::has (f.p.core().soundingNotes (0), 60));
    process (f.p, 2, notes ({ 61 }, false));
    CHECK (chords::count (f.p.core().soundingNotes()) == 0);
    f.p.setScaleLockFromUi (false, keys::LockNearest);
    process (f.p, 2, notes ({ 61 }));
    CHECK (chords::has (f.p.core().soundingNotes (0), 61));
    process (f.p, 2, notes ({ 61 }, false));
}
