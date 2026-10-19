// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p49: the Grid-Controller inside a plug-in - created by plug-in editors, off by default, no ports (CI) = nothing
// happens; one instance of the process holds it (window opened / clicked last, "Take over here"), the other one lets
// go (lights and display off); it keeps working while the window is closed (parked in the processor); the settings
// section in plug-in mode (automatic ports, which instance) in both languages.
#include <doctest/doctest.h>

#include "control/ControllerPorts.h"
#include "control/GridProtocol.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "ui/PluginEditor.h"

using namespace ohpe;
namespace g = ohpe::gridctl;

namespace
{
struct Sent
{
    std::vector<juce::MidiMessage> msgs;
    int clearedLines() const
    {
        int n = 0;
        for (const auto& m : msgs)
            if (m.isSysEx())
                for (int l = 0; l < g::numLines; ++l)
                {
                    const auto c = g::clearLineSysex (l);
                    if (m.getRawDataSize() == (int) c.size() && std::equal (c.begin(), c.end(), m.getRawData())) ++n;
                }
        return n;
    }
};

struct Instance
{
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;
    Instance (const juce::File& folder) { open (folder); }
    void open (const juce::File& folder)
    {
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        e->userSettingsStore().setFolderForTests (folder);
        e->applyUserSettings();
        e->setLanguage (i18n::Language::English);
    }
    ui::GridController& c() { return *e->gridController(); }
};

void setProfile (OhpeEditor& e, bool on)
{
    auto s = e.userSettingsStore().get();
    s.controllerProfile = on ? settings::ControllerGrid1 : settings::ControllerOff;
    e.userSettingsStore().set (s);
}
} // namespace

TEST_CASE ("p49 grid controller in a plug-in: plug-in editors create it, off by default; no ports = nothing happens")
{
    juce::TemporaryFile tmp;
    tmp.getFile().createDirectory();
    const int enrolled = g::Ownership::shared().size();
    {
        Instance a (tmp.getFile());
        a.e->userSettingsStore().set ({});
        a.e->applyUserSettings();
        REQUIRE (a.e->gridController() != nullptr);
        auto& c = a.c();
        CHECK (c.pluginMode()); // tests run as a plug-in wrapper
        CHECK_FALSE (c.enabled());
        CHECK (c.state() == ui::GridController::State::Off);
        CHECK (a.e->userSettingsStore().get().controllerAuto); // automatic ports: on by default
        // profile on without a controller (CI has no MIDI ports, or none matches): it waits, every tick is harmless
        setProfile (*a.e, true);
        a.e->applyUserSettings();
        CHECK (c.enabled());
        CHECK (c.ownedHere()); // the only instance
        for (int i = 0; i < 60; ++i) c.poll();
        CHECK_FALSE (c.connected());
        const auto st = c.state();
        CHECK ((st == ui::GridController::State::ChoosePorts || st == ui::GridController::State::Waiting || st == ui::GridController::State::PortBusy
                || st == ui::GridController::State::Connected)); // a developer machine may have a matching port
        CHECK (c.status().isNotEmpty());
        // the window closes: the controller is parked in the processor, the same object comes back
        auto* before = a.e->gridController();
        a.ed = nullptr;
        CHECK (a.p.parkedGridController != nullptr);
        before->poll();
        CHECK (before->headless());
        a.open (tmp.getFile());
        CHECK (a.e->gridController() == before);
        CHECK_FALSE (before->headless());
        CHECK (a.p.parkedGridController == nullptr);
        setProfile (*a.e, false);
        a.e->applyUserSettings();
        CHECK (c.state() == ui::GridController::State::Off);
        a.e->userSettingsStore().set ({});
    } // processor with a parked or adopted controller goes cleanly
    CHECK (g::Ownership::shared().size() == enrolled);
}

TEST_CASE ("p49 grid controller in a plug-in: one instance holds it - the other lets go; take over here")
{
    juce::TemporaryFile tmp;
    tmp.getFile().createDirectory();
    Instance a (tmp.getFile()), b (tmp.getFile()); // b's window opened last
    a.e->userSettingsStore().set ({});
    setProfile (*a.e, true);
    a.e->applyUserSettings();
    b.e->applyUserSettings();
    Sent sa, sb;
    a.c().attachForTests ([&sa] (const juce::MidiMessage& m) { sa.msgs.push_back (m); });
    b.c().attachForTests ([&sb] (const juce::MidiMessage& m) { sb.msgs.push_back (m); });
    a.c().poll();
    b.c().poll();
    CHECK (b.c().ownedHere());
    CHECK (b.c().connected());
    CHECK (a.c().ownedElsewhere());
    CHECK_FALSE (a.c().connected());
    CHECK (a.c().state() == ui::GridController::State::OtherInstance);
    CHECK (b.c().status() == "Connected"); // a mock device
    CHECK (a.c().status() == "Active in another instance");

    // a's settings section: which instance, take it over here
    auto& view = a.e->settingsComponent();
    view.setStandalone (false);
    a.e->showSettings (true, ui::SettingsView::Midi);
    auto chips = view.chipTexts();
    CHECK (chips.contains ("CHOOSE PORTS AUTOMATICALLY"));
    CHECK (chips.contains ("TAKE OVER HERE"));
    sa.msgs.clear();
    sb.msgs.clear();
    CHECK (view.clickChip ("TAKE OVER HERE"));
    CHECK (a.c().ownedHere());
    b.c().poll(); // b lets go on its next tick: lights and display off
    CHECK_FALSE (b.c().connected());
    CHECK (sb.clearedLines() == g::numLines);
    a.c().poll();
    CHECK (a.c().connected());
    CHECK (! sa.msgs.empty()); // a sends its display and lights
    a.e->showSettings (true, ui::SettingsView::Midi);
    CHECK_FALSE (view.chipTexts().contains ("TAKE OVER HERE"));
    CHECK (a.e->createComponentSnapshot (a.e->getLocalBounds()).isValid());

    // automatic ports off: stepping a port picks it; on again
    CHECK (view.clickChip ("CHOOSE PORTS AUTOMATICALLY"));
    CHECK_FALSE (a.e->userSettingsStore().get().controllerAuto);
    CHECK (view.clickChip ("CHOOSE PORTS AUTOMATICALLY"));
    CHECK (a.e->userSettingsStore().get().controllerAuto);

    // a click in b's window takes it back
    b.c().claim();
    CHECK (b.c().ownedHere());
    a.c().poll();
    CHECK_FALSE (a.c().connected());

    // German
    a.e->setLanguage (i18n::Language::German);
    CHECK (a.c().status() == juce::String::fromUTF8 ("In anderer Instanz aktiv"));
    a.e->showSettings (true, ui::SettingsView::Midi);
    CHECK (view.chipTexts().contains (i18n::upper (juce::String::fromUTF8 ("Ports automatisch wählen"))));
    CHECK (view.chipTexts().contains ("HIERHER HOLEN"));
    CHECK (i18n::tr ("Port in use - free it in the host") == juce::String::fromUTF8 ("Port belegt \xe2\x80\x93 in der DAW freigeben"));
    CHECK (i18n::tr ("Active in this instance") == "Aktiv in dieser Instanz");
    a.e->setLanguage (i18n::Language::English);
    a.e->showSettings (false);

    // the owner's window closes: it keeps working (parked) - a part button and a mode button act without the window
    a.c().attachForTests (nullptr);
    auto* bc = &b.c();
    b.ed = nullptr;
    REQUIRE (b.p.parkedGridController != nullptr);
    CHECK (bc->headless());
    CHECK (bc->ownedHere());
    bc->poll();
    CHECK (bc->connected());
    bc->receive (juce::MidiMessage::controllerEvent (1, g::cc::lowerRow + 2, 127)); // part 3
    bc->receive (juce::MidiMessage::controllerEvent (1, g::cc::lowerRow + 2, 0));
    bc->update();
    CHECK (std::lround (b.p.core().params().get (Core::Ids::editPart)) == 3);
    bc->receive (juce::MidiMessage::controllerEvent (1, g::cc::right, 127)); // next page
    bc->receive (juce::MidiMessage::controllerEvent (1, g::cc::right, 0));
    bc->update();
    CHECK (! bc->displayLines()[0].empty());
    bc->attachForTests (nullptr);
    a.e->userSettingsStore().set ({});
}
