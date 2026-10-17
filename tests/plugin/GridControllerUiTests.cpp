// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p46: Grid-Controller (Profil 1) against a mock device - display lines (SysEx) per page, encoders -> parameters,
// the pads = the on-screen grid keyboard (every layout, octave, key), pad / button lights, buttons, disconnect /
// reconnect, the settings section and the virtual controller view, both languages. Plug-ins never create it.
#include <doctest/doctest.h>

#include "control/GridProtocol.h"
#include "harmony/HarmonyBrain.h"
#include "perform/PerformancePads.h"
#include "seq/ScaleChord.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "ui/PluginEditor.h"

using namespace ohpe;
namespace g = ohpe::gridctl;

namespace
{
struct Mock
{
    std::vector<juce::MidiMessage> sent;
    int sysexCount() const
    {
        int n = 0;
        for (const auto& m : sent) n += m.isSysEx() ? 1 : 0;
        return n;
    }
    int lastPadCode (int pad) const
    {
        for (auto it = sent.rbegin(); it != sent.rend(); ++it)
            if (it->isNoteOnOrOff() && it->getNoteNumber() == g::firstPadNote + pad) return it->getVelocity();
        return -1;
    }
    int lastCc (int cc) const
    {
        for (auto it = sent.rbegin(); it != sent.rend(); ++it)
            if (it->isController() && it->getControllerNumber() == cc) return it->getControllerValue();
        return -1;
    }
};

struct Rig
{
    juce::TemporaryFile tmp;
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;
    Mock mock;
    ui::GridController* c = nullptr;
    Rig()
    {
        p.prepareToPlay (48000.0, 256);
        tmp.getFile().createDirectory();
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        e->userSettingsStore().setFolderForTests (tmp.getFile());
        e->userSettingsStore().set ({});
        e->applyUserSettings();
        e->setLanguage (i18n::Language::English);
        c = &e->gridControllerForTests();
        c->attachForTests ([this] (const juce::MidiMessage& m) { mock.sent.push_back (m); });
        c->update();
    }
    ~Rig()
    {
        c->attachForTests (nullptr); // the mock goes before the editor
        e->userSettingsStore().set ({});
        i18n::setCurrent (i18n::Language::English);
    }
    void in (const juce::MidiMessage& m)
    {
        c->receive (m);
        c->update();
    }
    void cc (int number, int value) { in (juce::MidiMessage::controllerEvent (1, number, value)); }
    void press (int number)
    {
        cc (number, 127);
        cc (number, 0);
    }
    void play (int blocks)
    {
        juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            p.processBlock (buffer, midi);
        }
    }
    void setHost (const juce::String& id, float plain)
    {
        auto* prm = p.parameter (id);
        REQUIRE (prm != nullptr);
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        p.core().params().set (id.toStdString(), plain);
    }
};
} // namespace

TEST_CASE ("p46 grid controller: plug-in editors never create it; off by default")
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    CHECK (e.gridController() == nullptr); // tests run as a plug-in wrapper (not the standalone app)
    CHECK (settings::UserSettings {}.controllerProfile == settings::ControllerOff);
}

TEST_CASE ("p46 grid controller: display lines follow the page; encoders edit the page's parameters")
{
    Rig r;
    auto& e = *r.e;
    CHECK (r.c->connected());
    CHECK (r.mock.sysexCount() >= 5); // aftertouch mode + four display lines
    const auto ids = ui::modeParams (e.currentMode(), r.p.core(), e.pageContext());
    const auto enc = r.c->encoderIds();
    for (int i = 0; i < 8; ++i) CHECK (enc[(size_t) i] == (i < (int) ids.size() ? ids[(size_t) i] : std::string()));
    // the first four = what the screen encoders show
    for (int i = 0; i < 4; ++i) CHECK (juce::String (enc[(size_t) i]) == e.encoder (i).boundId());
    auto lines = r.c->displayLines();
    for (const auto& l : lines) CHECK (l.size() == 68u);
    MESSAGE ("Synth page 1:\n" << lines[0] << "\n" << lines[1] << "\n" << lines[2] << "\n" << lines[3]);
    CHECK (lines[2].find ("Part 1") != std::string::npos);
    CHECK (lines[2].find ("BPM") != std::string::npos);
    CHECK (lines[3].find ("1/") != std::string::npos);
    CHECK (lines[3].find ("Keys Fourths O3") != std::string::npos);

    // encoder 1 up: the parameter of encoder 1 changes (host notified)
    const juce::String id0 (enc[0]);
    auto* prm = r.p.parameter (id0);
    REQUIRE (prm != nullptr);
    const float before = prm->getValue();
    for (int k = 0; k < 8; ++k) r.cc (g::cc::firstEncoder, 3);
    CHECK (prm->getValue() != doctest::Approx (before));
    for (int k = 0; k < 8; ++k) r.cc (g::cc::firstEncoder, 125); // back down
    CHECK (prm->getValue() == doctest::Approx (before).epsilon (0.02));

    // touching encoder 2 shows its full name and value on line 3
    r.in (juce::MidiMessage::noteOn (1, 1, (juce::uint8) 127));
    const auto& d = r.p.core().params().descriptor (r.p.core().params().indexOf (enc[1]));
    CHECK (r.c->displayLines()[2].rfind (g::fold (d.nameEn), 0) == 0);
    r.in (juce::MidiMessage::noteOff (1, 1));
    CHECK (r.c->displayLines()[2].find ("Part 1") != std::string::npos);

    // page right: the encoders move on (Pages.cpp), the display follows
    const int sysexBefore = r.mock.sysexCount();
    r.press (g::cc::right);
    CHECK (e.shownPage() == 1);
    const auto enc2 = r.c->encoderIds();
    for (int i = 0; i < 8; ++i) CHECK (enc2[(size_t) i] == (4 + i < (int) ids.size() ? ids[(size_t) 4 + (size_t) i] : std::string()));
    CHECK (r.mock.sysexCount() > sysexBefore);
    r.press (g::cc::left);
    CHECK (e.shownPage() == 0);

    // mode keys (first row under the display) and parts (second row 1-4)
    r.press (g::cc::upperRow + (int) ui::Mode::Envelope);
    CHECK (e.currentMode() == ui::Mode::Envelope);
    lines = r.c->displayLines();
    MESSAGE ("Envelope page 1:\n" << lines[0] << "\n" << lines[1] << "\n" << lines[2] << "\n" << lines[3]);
    r.press (g::cc::upperRow + (int) ui::Mode::Mixer);
    CHECK (e.currentMode() == ui::Mode::Mixer);
    lines = r.c->displayLines();
    MESSAGE ("Mixer page 1:\n" << lines[0] << "\n" << lines[1] << "\n" << lines[2] << "\n" << lines[3]);
    CHECK (r.mock.lastCc (g::cc::upperRow + (int) ui::Mode::Mixer) == g::colourCode (g::Colour::Yellow));
    r.press (g::cc::lowerRow + 2);
    CHECK (e.currentPart() == 2);
    CHECK (r.c->displayLines()[2].find ("Part 3") != std::string::npos);
    CHECK (r.mock.lastCc (g::cc::lowerRow + 2) == g::colourCode (g::Colour::Yellow));
    CHECK (r.mock.lastCc (g::cc::lowerRow + 0) == g::colourCode (g::Colour::YellowDim));
}

TEST_CASE ("p46 grid controller: the pads are the on-screen grid keyboard - layouts, octave, key, colours")
{
    Rig r;
    auto& e = *r.e;
    auto& grid = e.gridKeyboard();
    for (int layout = 0; layout < harmony::numLayouts; ++layout)
    {
        r.setHost ("harmony.kb.layout", (float) layout);
        r.c->update();
        for (int row = 0; row < ui::GridKeyboard::rows; ++row) // the grid's four rows = the pads' four lower rows
            for (int col = 0; col < g::cols; ++col)
                CHECK (r.c->padNoteNow (row * g::cols + col) == grid.noteAt (col, row));
    }
    r.setHost ("harmony.kb.layout", (float) harmony::LayoutFourths);
    r.c->update();
    CHECK (r.c->padNoteNow (0) == 48); // C3, the keyboard's octave 3
    CHECK (r.c->padNoteNow (8) == 53);
    // octave + on the controller moves the on-screen keyboard and grid too
    r.press (g::cc::octaveUp);
    CHECK (e.keyboardComponent().getBaseOctave() == 4);
    CHECK (r.c->padNoteNow (0) == 60);
    CHECK (r.c->padNoteNow (0) == grid.noteAt (0, 0));
    r.press (g::cc::octaveDown);
    CHECK (r.c->padNoteNow (0) == 48);

    // colours: C major (nothing heard yet) - C root amber, C# off, D yellow dim
    CHECK (r.c->padCodes()[0] == g::padColourCode (g::Colour::Amber));
    CHECK (r.c->padCodes()[1] == 0);
    CHECK (r.c->padCodes()[2] == g::padColourCode (g::Colour::YellowDim));
    CHECK (r.mock.lastPadCode (0) == g::padColourCode (g::Colour::Amber));

    // In Key: only key notes, the key from the key setting (D minor)
    r.setHost ("key.root", 2.0f);
    r.setHost ("key.scale", (float) scales::Minor);
    r.play (2);
    r.setHost ("harmony.kb.layout", (float) harmony::LayoutInKey);
    r.c->update();
    for (int pad = 0; pad < g::numPads; ++pad)
    {
        const int n = r.c->padNoteNow (pad);
        CHECK (scales::inScale (n, 2, scales::Minor));
        CHECK (r.c->padCodes()[(size_t) pad] != 0); // every pad is a key note: lit
    }
    CHECK (r.c->padNoteNow (0) == grid.noteAt (0, 0));

    // playing a pad: velocity to the edit part, the pad lights green while held
    r.setHost ("harmony.kb.layout", (float) harmony::LayoutFourths);
    r.c->update();
    r.in (juce::MidiMessage::noteOn (1, g::padNote (2, 1), (juce::uint8) 100));
    r.play (2);
    CHECK (r.p.core().lastNote() == r.c->padNoteNow (g::cols + 2));
    CHECK (r.c->padCodes()[(size_t) (g::cols + 2)] == g::padColourCode (g::Colour::Green));
    r.in (juce::MidiMessage::aftertouchChange (1, g::padNote (2, 1), 80));
    r.play (1);
    r.in (juce::MidiMessage::noteOff (1, g::padNote (2, 1)));
    r.play (1);
    CHECK (r.c->padCodes()[(size_t) (g::cols + 2)] != g::padColourCode (g::Colour::Green));

    // the scales button steps through the layouts like the on-screen layout setting
    const int layoutBefore = (int) std::lround (r.p.core().params().get ("harmony.kb.layout"));
    r.press (g::cc::scales);
    CHECK ((int) std::lround (r.p.core().params().get ("harmony.kb.layout")) == (layoutBefore + 1) % harmony::numLayouts);
}

TEST_CASE ("p46 grid controller: pad modes, transport and metronome buttons, touch strip")
{
    Rig r;
    auto& core = r.p.core();
    // Drum: the three bottom rows play the 24 drum pads
    r.press (g::cc::lowerRow + 4 + (int) ui::GridController::PadMode::Drum);
    CHECK (r.c->padMode() == ui::GridController::PadMode::Drum);
    r.in (juce::MidiMessage::noteOn (1, g::padNote (1, 0), (juce::uint8) 110));
    r.play (2);
    CHECK (core.lastNote() == (int) std::lround (core.params().get ("drum.base")) + 1);
    r.in (juce::MidiMessage::noteOff (1, g::padNote (1, 0)));
    CHECK (r.c->padCodes()[40] == 0); // rows 3 .. 7 unused
    // Pads: a quarter of the grid holds a performance pad
    r.press (g::cc::lowerRow + 4 + (int) ui::GridController::PadMode::Pads);
    r.in (juce::MidiMessage::noteOn (1, g::padNote (0, 7), (juce::uint8) 100));
    CHECK (core.params().get (PerformancePads::padId (0)) >= 0.5f);
    r.in (juce::MidiMessage::noteOff (1, g::padNote (0, 7)));
    CHECK (core.params().get (PerformancePads::padId (0)) < 0.5f);
    // Steps: the top two rows toggle steps 1 .. 16
    r.press (g::cc::session);
    CHECK (r.c->padMode() == ui::GridController::PadMode::Steps);
    CHECK_FALSE (core.stepSequencer().getStep (3).on);
    r.in (juce::MidiMessage::noteOn (1, g::padNote (3, 7), (juce::uint8) 100));
    CHECK (core.stepSequencer().getStep (3).on);
    CHECK (r.c->padCodes()[(size_t) (7 * g::cols + 3)] == g::padColourCode (g::Colour::Yellow));
    r.in (juce::MidiMessage::noteOn (1, g::padNote (3, 7), (juce::uint8) 100));
    CHECK_FALSE (core.stepSequencer().getStep (3).on);
    r.press (g::cc::note);
    CHECK (r.c->padMode() == ui::GridController::PadMode::Keys);

    // metronome (and its light), count-in with Shift
    CHECK (core.params().get ("metro.on") < 0.5f);
    r.press (g::cc::metronome);
    CHECK (core.params().get ("metro.on") >= 0.5f);
    CHECK (r.mock.lastCc (g::cc::metronome) == 4);
    r.cc (g::cc::shift, 127);
    r.press (g::cc::metronome);
    r.cc (g::cc::shift, 0);
    CHECK ((int) std::lround (core.params().get ("metro.countin")) == 1);
    r.press (g::cc::metronome);
    CHECK (core.params().get ("metro.on") < 0.5f);

    // play / stop: the sequencer runs, the play light follows
    r.press (g::cc::play);
    CHECK (core.params().get (Core::Ids::seqRun) >= 0.5f);
    CHECK (r.mock.lastCc (g::cc::play) == 4);
    r.press (g::cc::stop);
    r.c->update();
    CHECK (core.params().get (Core::Ids::seqRun) < 0.5f);

    // tempo encoder
    const float bpm = r.p.currentTempo();
    r.cc (g::cc::tempoEncoder, 2);
    CHECK (r.p.currentTempo() == doctest::Approx (bpm + 2.0f));

    // touch strip: bend while touched, back to the middle on release
    r.in (juce::MidiMessage::noteOn (1, g::touchNoteStrip, (juce::uint8) 127));
    r.in (juce::MidiMessage::pitchWheel (1, 16383));
    CHECK (core.params().get (Core::Ids::perfBend) == doctest::Approx (1.0f).epsilon (0.01));
    r.in (juce::MidiMessage::noteOff (1, g::touchNoteStrip));
    CHECK (core.params().get (Core::Ids::perfBend) == doctest::Approx (0.0f));
}

TEST_CASE ("p46 grid controller: disconnect / reconnect resends everything; settings section; German")
{
    Rig r;
    auto& e = *r.e;
    r.c->attachForTests (nullptr);
    CHECK_FALSE (r.c->connected());
    r.mock.sent.clear();
    r.c->attachForTests ([&r] (const juce::MidiMessage& m) { r.mock.sent.push_back (m); });
    r.c->update();
    CHECK (r.c->connected());
    CHECK (r.mock.sysexCount() == 5); // aftertouch mode + all four lines again
    int pads = 0;
    for (const auto& m : r.mock.sent) pads += m.isNoteOn (true) && g::padOfNote (m.getNoteNumber()) >= 0 ? 1 : 0;
    CHECK (pads == g::numPads); // every pad light again
    // nothing changed: nothing is sent
    const auto n = r.mock.sent.size();
    r.c->update();
    CHECK (r.mock.sent.size() == n);

    // the settings: MIDI page with the controller section and the virtual controller view
    auto& view = e.settingsComponent();
    view.setStandalone (true);
    e.showSettings (true, ui::SettingsView::Midi);
    auto chips = view.chipTexts();
    CHECK (chips.contains ("GRID CONTROLLER PROFILE 1"));
    CHECK (view.clickChip ("GRID CONTROLLER PROFILE 1"));
    CHECK (e.userSettingsStore().get().controllerProfile == settings::ControllerGrid1);
    CHECK (view.clickChip ("MOD"));
    CHECK (e.userSettingsStore().get().controllerStrip == 1);
    CHECK (view.clickChip ("VIEW"));
    CHECK (view.controllerPreviewShown());
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());

    // German: the display speaks German (folded to the display's characters)
    e.setLanguage (i18n::Language::German);
    r.c->attachForTests ([&r] (const juce::MidiMessage& m) { r.mock.sent.push_back (m); });
    r.c->update();
    const auto lines = r.c->displayLines();
    MESSAGE ("German:\n" << lines[0] << "\n" << lines[1] << "\n" << lines[2] << "\n" << lines[3]);
    CHECK (lines[3].find ("Tasten Quarten O3") != std::string::npos);
    view.showControllerPreview (false);
    e.showSettings (false);
}
