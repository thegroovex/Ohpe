// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GridController.h"

#include "LibraryBrowser.h"
#include "ModFxViews.h"
#include "control/ControllerPorts.h"
#include "harmony/HarmonyBrain.h"
#include "perform/PerformancePads.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"
#include "preset/PresetLibrary.h"
#include "seq/ScaleChord.h"
#include "seq/SeqCommon.h"

#include <algorithm>
#include <cmath>

#include "harmony/KeyScale.h"

namespace ohpe::ui
{
using i18n::tr;
namespace g = gridctl;

namespace
{
constexpr int stepRows = 2; ///< Steps mode: the two top rows are the 16 steps
constexpr int drumMarker = 100; ///< playingCh of a pad that plays the drum kit

std::string str (const juce::String& s) { return s.toStdString(); }

const char* pitchNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

juce::String modeName (GridController::PadMode m)
{
    switch (m)
    {
        case GridController::PadMode::Pads: return tr ("Pads");
        case GridController::PadMode::Drum: return tr ("Drum");
        case GridController::PadMode::Steps: return tr ("Steps");
        default: return tr ("Keys");
    }
}

/** p59: line 4 - the grid in a few letters: layout, In Key / Chromatic, fixed C ("4th Key Fix", "Qua Chr"). */
juce::String layoutSummary (int layout, const harmony::GridOptions& o)
{
    const bool de = i18n::current() == i18n::Language::German;
    static const char* en[] = { "4th", "3rd", "Whl", "Seq" };
    static const char* deN[] = { "Qua", "Ter", "Gnz", "Seq" };
    const int l = juce::jlimit (0, (int) harmony::numLayouts - 1, layout);
    juce::String t = de ? deN[l] : en[l];
    t << " " << (o.inKey ? (de ? "Ton" : "Key") : "Chr");
    if (o.fixed) t << " " << (de ? "Fest" : "Fix");
    return t;
}
} // namespace

GridController::GridController (OhpeProcessor& p, Hooks h) : proc (p), hooks (std::move (h))
{
    for (auto& x : padMap) x.store (-1);
    for (auto& x : playing) x.store (-1);
    for (auto& x : playingCh) x.store (1);
    for (auto& x : held) x.store (false);
    resetOutputCache();
    // p49: a plug-in opens the controller's ports itself - one instance of the process at a time
    inPlugin = proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone;
    if (inPlugin) g::Ownership::shared().enrol (this);
    store->addChangeListener (this);
}

GridController::~GridController()
{
    stopTimer();
    store->removeChangeListener (this);
    closePorts(); // lights and display off, ports closed
    g::Ownership::shared().remove (this);
}

// ---------------------------------------------------------------- p49: plug-in, ownership, without the editor
void GridController::setPluginModeForTests (bool on)
{
    if (on == inPlugin) return;
    closePorts();
    inPlugin = on;
    if (on)
    {
        g::Ownership::shared().enrol (this);
        g::Ownership::shared().setEnabled (this, enabled());
    }
    else g::Ownership::shared().remove (this);
    wasOwner = false;
}

void GridController::claim()
{
    if (! inPlugin) return;
    g::Ownership::shared().claim (this);
    poll(); // the old owner lets go on its own next tick; this one tries at once
}

bool GridController::ownedHere() const { return ! inPlugin || g::Ownership::shared().owns (this); }
bool GridController::ownedElsewhere() const { return inPlugin && g::Ownership::shared().ownedElsewhere (this); }

GridController::State GridController::state() const
{
    if (! enabled() && ! testSender) return State::Off;
    if (inPlugin && enabled() && ! ownedHere()) return State::OtherInstance;
    if (isConnected) return State::Connected;
    if (isProbing) return State::Probing;
    if (portBusy) return State::PortBusy;
    if (autoPorts && (notFound || inName.isEmpty() || outName.isEmpty())) return State::NotFound;
    if (inName.isEmpty() || outName.isEmpty()) return State::ChoosePorts;
    return State::Waiting;
}

void GridController::changeListenerCallback (juce::ChangeBroadcaster*)
{
    configure (store->get()); // the same settings in every window; only changes act
}

void GridController::detachFromEditor()
{
    // the last state the window showed
    if (hooks.mode) detached.mode = hooks.mode();
    if (hooks.context) detached.ctx = hooks.context();
    if (hooks.page) detached.page = hooks.page();
    if (hooks.octave) detached.octave = hooks.octave();
    if (hooks.gridShown) detached.grid = hooks.gridShown();
    isHeadless = true;
    auto remember = [this] (const char* key, int v) { // the window opens with it again
        if (auto* obj = proc.uiState.getDynamicObject()) obj->setProperty (key, v);
    };
    Hooks h;
    h.mode = [this] { return detached.mode; };
    h.context = [this] {
        detached.ctx.part = juce::jlimit (0, Core::numParts - 1, (int) std::lround (proc.core().params().get (Core::Ids::editPart)) - 1);
        return detached.ctx;
    };
    h.page = [this] { return detached.page; };
    h.stepPage = [this] (int d) {
        const int pages = juce::jmax (1, ((int) modeParams (detached.mode, proc.core(), detached.ctx).size() + 3) / 4);
        detached.page = juce::jlimit (0, pages - 1, detached.page + d);
    };
    h.selectMode = [this, remember] (Mode m) {
        if (! modeInfo (m).available) return;
        detached.mode = m;
        detached.page = 0;
        remember ("mode", (int) m);
    };
    h.selectPart = [this] (int k) {
        k = juce::jlimit (0, Core::numParts - 1, k);
        setParam (Core::Ids::editPart, (float) (k + 1));
        detached.ctx.part = k;
    };
    h.transport = [this] (OhpeProcessor::TransportKey k) { proc.runTransport (k); };
    h.octave = [this] { return detached.octave; };
    h.gridShown = [this] { return detached.grid; };
    h.showGrid = [this, remember] (bool show) {
        detached.grid = show;
        remember ("grid", show ? 1 : 0);
    };
    h.setOctave = [this, remember] (int o) {
        detached.octave = juce::jlimit (0, 7, o);
        remember ("octave", detached.octave);
    };
    hooks = std::move (h);
}

void GridController::poll()
{
    checkOwnership();
    if (! testSender && enabled() && ++watchTicks >= 25) // once a second: has a port vanished, come back or come free?
    {
        watchTicks = 0;
        watchPorts();
    }
    if (isProbing && --probeTicks <= 0) rejectProbe(); // p53: no identity reply in 2 s
    if (splashTicks > 0) --splashTicks;
    // p53: the virtual controller follows the pads and buttons also before (or without) a connected output
    if (isConnected || isProbing) update();
    else if ((enabled() || testSender) && (++idleTicks & 1) == 0) update(); // the virtual controller (12 Hz)
}

void GridController::checkOwnership()
{
    if (! inPlugin || ! enabled()) return;
    const bool own = g::Ownership::shared().owns (this);
    if (! own)
    {
        wasOwner = false;
        if (! isConnected && input == nullptr && output == nullptr) return;
        // another instance holds it: let go (lights and display off, ports closed so that one can open them)
        if (testSender)
        {
            for (int pad = 0; pad < g::numPads; ++pad) padUp (pad);
            releasePedals(); // p58
            if (isConnected) lightsOff();
            isConnected = false;
            addLog ("released");
        }
        else closePorts();
        return;
    }
    if (wasOwner) return;
    wasOwner = true;
    if (testSender)
    {
        addLog ("virtual device");
        onConnected (false);
        return;
    }
    watchTicks = 25; // ours now: try the ports at once
}

// ---------------------------------------------------------------- ports
juce::StringArray GridController::inputNames()
{
    juce::StringArray s;
    for (const auto& d : juce::MidiInput::getAvailableDevices()) s.add (d.name);
    return s;
}

juce::StringArray GridController::outputNames()
{
    juce::StringArray s;
    for (const auto& d : juce::MidiOutput::getAvailableDevices()) s.add (d.name);
    return s;
}

void GridController::configure (const settings::UserSettings& s)
{
    strip = s.controllerStrip;
    const bool changed = s.controllerProfile != profile || juce::String (s.controllerIn) != wantIn || juce::String (s.controllerOut) != wantOut
                         || s.controllerAuto != autoPorts || juce::String (s.controllerPattern) != pattern
                         || s.controllerChosen != chosenByUser;
    if (s.controllerMaster != (masterTempo ? 1 : 0) || changed) masterTempo = s.controllerMaster == 1;
    if (! changed) return;
    closePorts();
    profile = s.controllerProfile;
    wantIn = juce::String (s.controllerIn);
    wantOut = juce::String (s.controllerOut);
    autoPorts = s.controllerAuto;
    chosenByUser = s.controllerChosen;
    pattern = juce::String (s.controllerPattern);
    rejected.clear();
    notFound = false;
    resolvePorts();
    if (inPlugin)
    {
        g::Ownership::shared().setEnabled (this, enabled());
        wasOwner = false;
        checkOwnership();
    }
    if (enabled()) openPorts();
    if (enabled() || testSender) startTimerHz (25);
    else stopTimer();
}

void GridController::resolvePorts()
{
    // p49: automatic = the ports whose names match the pattern; else the ones chosen
    // p53: automatic in the standalone app too; the input and the output of the same device (its user port), and
    // candidates that did not answer as the controller are skipped
    if (! autoPorts)
    {
        inName = wantIn;
        outName = wantOut;
        return;
    }
    std::vector<std::string> ins, outs;
    for (const auto& n : inputNames()) ins.push_back (n.toStdString());
    for (const auto& n : outputNames()) outs.push_back (n.toStdString());
    inName = outName = {};
    for (const auto& pick : g::autoPortCandidates (ins, outs, pattern.toStdString()))
    {
        const juce::String in (ins[(size_t) pick.in]);
        if (rejected.contains (in)) continue;
        inName = in;
        outName = juce::String (outs[(size_t) pick.out]);
        break;
    }
}

void GridController::openPorts()
{
    if (testSender || ! enabled() || ! ownedHere()) return;
    if (inName.isEmpty() || outName.isEmpty()) return; // p53: both or nothing (a half-open controller shows nothing)
    portBusy = false;
    if (input == nullptr && inName.isNotEmpty())
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (d.name == inName)
            {
                // the controller's pads must not reach the parts twice (as a plain MIDI input too)
                if (auto* dm = proc.standaloneDevices; dm != nullptr && dm->isMidiInputDeviceEnabled (d.identifier))
                {
                    dm->setMidiInputDeviceEnabled (d.identifier, false);
                    disabledInputs.addIfNotAlreadyThere (d.identifier); // p53: given back when Ohpe lets go
                }
                input = juce::MidiInput::openDevice (d.identifier, this);
                if (input != nullptr) input->start();
                else portBusy = true; // p49: there but not to be opened - held by another program (the host)
                break;
            }
    if (output == nullptr && outName.isNotEmpty())
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            if (d.name == outName)
            {
                output = juce::MidiOutput::openDevice (d.identifier);
                if (output == nullptr) portBusy = true;
                break;
            }
    const bool now = input != nullptr && output != nullptr;
    if (now && ! isConnected && ! isProbing)
    {
        // p53: the identity request tells the generation. Connected at once when the ports were chosen by hand or
        // the controller was set up by the user, or when the device has a live port next to the user port (the
        // controller's port pair); else (automatic, by default) only when it answers as the controller.
        std::vector<std::string> ins;
        for (const auto& n : inputNames()) ins.push_back (n.toStdString());
        const bool sure = ! autoPorts || chosenByUser || g::siblingPort (ins, inName.toStdString()) >= 0;
        gen = g::Generation::Unknown;
        sendSysex (g::identityRequestSysex());
        if (sure)
        {
            addLog ("connected");
            onConnected (true);
        }
        else
        {
            isProbing = true;
            probeTicks = 50;
            addLog ("identity?");
        }
    }
}

void GridController::onConnected (bool splash)
{
    isProbing = false;
    notFound = false;
    isConnected = true;
    if (gen == g::Generation::Unknown) gen = g::Generation::First; // until it answers
    resetOutputCache();
    // p53: into the User mode (the controller's own functions off, Ohpe gets the user port), pressure as poly aftertouch
    sendSysex (g::userModeSysex (gen, true));
    if (gen == g::Generation::First) sendSysex (g::aftertouchModeSysex (true));
    splashTicks = splash ? 40 : 0; // ~1.6 s at 25 Hz: "connecting ..." then "connected"
    claimDeviceInputs();
    update();
}

void GridController::claimDeviceInputs()
{
    auto* dm = proc.standaloneDevices;
    if (dm == nullptr || inName.isEmpty()) return;
    const auto key = g::deviceKey (inName.toStdString());
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        if ((d.name == inName || (! key.empty() && g::deviceKey (d.name.toStdString()) == key)) && dm->isMidiInputDeviceEnabled (d.identifier))
        {
            dm->setMidiInputDeviceEnabled (d.identifier, false); // its pads and touches reach the parts only through Ohpe's controller layer
            disabledInputs.addIfNotAlreadyThere (d.identifier);
        }
}

void GridController::releaseDeviceInputs()
{
    if (auto* dm = proc.standaloneDevices)
        for (const auto& id : disabledInputs) dm->setMidiInputDeviceEnabled (id, true);
    disabledInputs.clear();
}

void GridController::rejectProbe()
{
    addLog ("no answer: " + inName);
    rejected.addIfNotAlreadyThere (inName);
    closePorts();
    notFound = true;
    resolvePorts(); // the next candidate (tried by the watch)
    watchTicks = 25;
}

void GridController::closePorts()
{
    for (int pad = 0; pad < g::numPads; ++pad) padUp (pad);
    releasePedals(); // p58
    if (isConnected)
    {
        lightsOff();
        sendSysex (g::userModeSysex (gen, false)); // p53: back to its own (live) mode - released cleanly
    }
    if (input != nullptr) input->stop();
    input = nullptr;
    output = nullptr;
    if (isConnected && ! testSender) addLog ("disconnected");
    isConnected = testSender != nullptr;
    isProbing = false;
    splashTicks = 0;
    portBusy = false;
    releaseDeviceInputs();
}

void GridController::attachForTests (Sender out)
{
    closePorts();
    testSender = std::move (out);
    isConnected = false;
    resetOutputCache();
    gen = g::Generation::First; // a test device answers as the first generation
    if (testSender != nullptr)
    {
        addLog ("virtual device");
        onConnected (testSplash);
    }
}

juce::String GridController::status() const
{
    switch (state())
    {
        case State::Off: return tr ("Off");
        case State::OtherInstance: return tr ("Active in another instance");
        case State::Connected: return inPlugin ? tr ("Active in this instance") : tr ("Connected");
        case State::PortBusy: return inPlugin ? tr ("Port in use - free it in the host") : tr ("Port in use by another program");
        case State::ChoosePorts:
            return inPlugin && autoPorts ? tr ("No port matches - choose the ports") : tr ("Choose the controller's MIDI input and output");
        case State::Probing: return tr ("Checking the device ...");
        case State::NotFound: return inPlugin ? tr ("No port matches - choose the ports") : tr ("No controller found");
        case State::Waiting: break;
    }
    return tr ("Waiting for the controller");
}

juce::String GridController::statusLine() const
{
    const auto st = state();
    if (st == State::Connected)
    {
        juce::String s = status();
        if (gen == g::Generation::First) s << juce::String::fromUTF8 (" \xc2\xb7 ") << tr ("first generation, text display");
        else if (gen == g::Generation::Later) s << juce::String::fromUTF8 (" \xc2\xb7 ") << tr ("later generation, no text display");
        return s;
    }
    return status();
}

void GridController::timerCallback()
{
    poll();
}

void GridController::watchPorts()
{
    if (inPlugin && ! ownedHere()) return;
    {
        // p53: a changed port list (plugged in / out): candidates that did not answer may be tried again
        const auto sig = inputNames().joinIntoString ("\n") + "|" + outputNames().joinIntoString ("\n");
        if (sig != portListSig)
        {
            portListSig = sig;
            rejected.clear();
            notFound = false;
        }
        if (isConnected) claimDeviceInputs(); // the standalone's device manager may come later than the controller
        if (isProbing) return;
        if (! isConnected && input == nullptr && output == nullptr) resolvePorts(); // p49: automatic: plugged in later
        const bool inThere = inputNames().contains (inName), outThere = outputNames().contains (outName);
        if (isConnected && (! inThere || ! outThere))
        {
            // the device is gone: drop the ports without writing to them
            for (int pad = 0; pad < g::numPads; ++pad) padUp (pad);
            releasePedals(); // p58: no pedal stays down on a part when the device is unplugged
            if (input != nullptr) input->stop();
            input = nullptr;
            output = nullptr;
            isConnected = false;
            splashTicks = 0;
            releaseDeviceInputs();
            addLog ("disconnected");
        }
        else if (! isConnected && inThere && outThere)
            openPorts();
        else if (! isConnected && (input != nullptr || output != nullptr) && (! inThere || ! outThere))
            closePorts(); // p53: half open (one side vanished or was never there)
    }
}

void GridController::send (const juce::MidiMessage& m)
{
    if (testSender) testSender (m);
    else if (output != nullptr) output->sendMessageNow (m);
}

void GridController::sendSysex (const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < 2) return; // p53: nothing for an unknown generation
    send (juce::MidiMessage (bytes.data(), (int) bytes.size()));
}

void GridController::addLog (const juce::String& s)
{
    const std::lock_guard<std::mutex> l (logLock);
    logLines.add (s);
    while (logLines.size() > 12) logLines.remove (0);
}

juce::StringArray GridController::log() const
{
    const std::lock_guard<std::mutex> l (logLock);
    return logLines;
}

// ---------------------------------------------------------------- input
void GridController::receive (const juce::MidiMessage& m)
{
    if (m.isNoteOn() || m.isNoteOff())
    {
        const int pad = g::padOfNote (m.getNoteNumber());
        if (pad >= 0)
        {
            const int mode = padModeNow.load();
            const int row = pad / g::cols;
            const bool direct = mode == (int) PadMode::Keys || mode == (int) PadMode::Drum
                                || (mode == (int) PadMode::Steps && row < g::rows - stepRows);
            if (direct) // played at once (the core takes it at the next block)
            {
                if (m.isNoteOn()) padDown (pad, m.getVelocity());
                else padUp (pad);
                return;
            }
        }
    }
    else if (m.isAftertouch())
    {
        if (const int pad = g::padOfNote (m.getNoteNumber()); pad >= 0) padPressure (pad, m.getAfterTouchValue());
        return;
    }
    else if (m.isChannelPressure())
    {
        channelPressure (m.getChannelPressureValue());
        return;
    }
    else if (m.isController() && g::isPedalCc (m.getControllerNumber()))
        pedal (m.getControllerNumber(), m.getControllerValue()); // p58: at once, like a pad (still logged below)
    const std::lock_guard<std::mutex> l (queueLock);
    if (queue.size() < 512) queue.push_back (m);
}

void GridController::padDown (int pad, int velocity)
{
    if (accentNow.load()) velocity = 127; // p59: Accent = fixed full velocity
    held[(size_t) pad].store (true);
    const int mode = padModeNow.load();
    if (playing[(size_t) pad].load() >= 0) padUp (pad);
    held[(size_t) pad].store (true);
    if (mode == (int) PadMode::Drum)
    {
        const int idx = pad; // rows 0 .. 2 = drum pads 1 .. 24
        if (idx >= 24) return;
        const int note = juce::jlimit (0, 127, drumBase.load() + idx);
        playing[(size_t) pad].store (note);
        playingCh[(size_t) pad].store (drumMarker);
        proc.core().queueControllerEvent (Core::drumPart, MidiEvent::noteOn (Core::drumChannel, note, juce::jlimit (1, 127, velocity)));
        return;
    }
    const int note = padMap[(size_t) pad].load();
    if (note < 0) return;
    const int part = partNow.load();
    for (int k = 0; k < (int) pedalPart.size(); ++k) // p58: a pedal held while the part changed moves to the new part
    {
        const int was = pedalPart[(size_t) k].load();
        if (was < 0 || was == part) continue;
        const int cc = k == 0 ? 64 : (k == 1 ? 66 : 67);
        proc.core().queueControllerEvent (was, MidiEvent::controller (1, cc, 0, 0));
        proc.core().queueControllerEvent (part, MidiEvent::controller (1, cc, 127, 0));
        pedalPart[(size_t) k].store (part);
    }
    int ch = 1;
    if (mpeNow.load()) // a free member channel, round robin over 2 .. 16 (like the on-screen grid)
    {
        for (int k = 0; k < 15; ++k)
        {
            const int cand = 2 + (nextChannel - 2 + k) % 15;
            bool busy = false;
            for (int q = 0; q < g::numPads; ++q) busy |= playing[(size_t) q].load() >= 0 && playingCh[(size_t) q].load() == cand;
            if (! busy) { ch = cand; break; }
        }
        nextChannel = 2 + (ch - 2 + 1) % 15;
        MidiEvent bend;
        bend.status = (uint8_t) (0xe0 | ((ch - 1) & 0x0f));
        bend.data1 = 0;
        bend.data2 = 64;
        proc.core().queueControllerEvent (part, bend);
        proc.core().queueControllerEvent (part, MidiEvent::controller (ch, 74, 64, 0));
    }
    playing[(size_t) pad].store (note);
    playingCh[(size_t) pad].store (ch);
    proc.core().queueControllerEvent (part, MidiEvent::noteOn (ch, note, juce::jlimit (1, 127, velocity)));
}

void GridController::padUp (int pad)
{
    held[(size_t) pad].store (false);
    const int note = playing[(size_t) pad].exchange (-1);
    if (note < 0) return;
    const int ch = playingCh[(size_t) pad].load();
    if (ch == drumMarker)
        proc.core().queueControllerEvent (Core::drumPart, MidiEvent::noteOff (Core::drumChannel, note));
    else
        proc.core().queueControllerEvent (partNow.load(), MidiEvent::noteOff (ch, note));
}

void GridController::pedal (int cc, int value)
{
    // p58: the pedal jack's sustain (64), sostenuto (66) and soft (67) reach the part like the same CC from a MIDI
    // keyboard. Channel 1 = the master channel of an MPE part (lower zone), so the pedal holds every member channel's
    // notes; a plain part gets it on its own channel (the core re-channels it). The pedal goes up on the part it went
    // down on, even when the part changed in between.
    const int k = cc == 64 ? 0 : (cc == 66 ? 1 : (cc == 67 ? 2 : -1));
    if (k < 0) return;
    const int v = juce::jlimit (0, 127, value);
    if (v >= 64)
    {
        const int part = partNow.load();
        const int was = pedalPart[(size_t) k].exchange (part);
        if (was >= 0 && was != part) proc.core().queueControllerEvent (was, MidiEvent::controller (1, cc, 0, 0));
        proc.core().queueControllerEvent (part, MidiEvent::controller (1, cc, v, 0));
    }
    else
    {
        const int was = pedalPart[(size_t) k].exchange (-1);
        proc.core().queueControllerEvent (was >= 0 ? was : partNow.load(), MidiEvent::controller (1, cc, v, 0));
    }
}

void GridController::releasePedals()
{
    for (int cc : { 64, 66, 67 })
        if (pedalPart[(size_t) (cc == 64 ? 0 : (cc == 66 ? 1 : 2))].load() >= 0) pedal (cc, 0);
}

void GridController::padPressure (int pad, int value)
{
    const int note = playing[(size_t) pad].load();
    const int ch = playingCh[(size_t) pad].load();
    if (note < 0 || ch == drumMarker) return;
    MidiEvent e;
    if (mpeNow.load()) // the member channel's pressure = this note's pressure
    {
        e.status = (uint8_t) (0xd0 | ((ch - 1) & 0x0f));
        e.data1 = (uint8_t) juce::jlimit (0, 127, value);
    }
    else
    {
        e.status = (uint8_t) (0xa0 | ((ch - 1) & 0x0f));
        e.data1 = (uint8_t) note;
        e.data2 = (uint8_t) juce::jlimit (0, 127, value);
    }
    proc.core().queueControllerEvent (partNow.load(), e);
}

void GridController::channelPressure (int value)
{
    for (int pad = 0; pad < g::numPads; ++pad)
        if (playing[(size_t) pad].load() >= 0) padPressure (pad, value);
}

// ---------------------------------------------------------------- message thread
void GridController::update()
{
    std::vector<juce::MidiMessage> pending;
    {
        const std::lock_guard<std::mutex> l (queueLock);
        pending.swap (queue);
    }
    for (const auto& m : pending) handleQueued (m);
    refreshMap();
    // p53: display lines and lights are computed for the virtual controller too; sent only to a connected device
    refreshDisplay();
    refreshLights();
}

void GridController::handleQueued (const juce::MidiMessage& m)
{
    addLog ("> " + m.getDescription());
    if (m.isSysEx()) // p53: the identity reply - the generation (and, while probing, the proof it is the controller)
    {
        const auto gn = g::identityGeneration (m.getRawData(), (size_t) m.getRawDataSize());
        if (gn == g::Generation::Unknown) return;
        const bool changed = gn != gen;
        gen = gn;
        if (isProbing)
        {
            addLog ("connected");
            onConnected (true);
        }
        else if (isConnected && changed)
        {
            resetOutputCache();
            sendSysex (g::userModeSysex (gen, true));
        }
        return;
    }
    if (m.isNoteOnOrOff())
    {
        const int n = m.getNoteNumber();
        if (const int pad = g::padOfNote (n); pad >= 0)
        {
            const auto mode = padMode();
            const bool down = m.isNoteOn();
            if (mode == PadMode::Pads)
            {
                held[(size_t) pad].store (down);
                const int col = pad % g::cols, row = pad / g::cols;
                const int which = (row >= 4 ? 0 : 2) + (col >= 4 ? 1 : 0); // top-left, top-right, bottom-left, bottom-right
                bool any = false;
                for (int q = 0; q < g::numPads; ++q)
                {
                    const int qc = q % g::cols, qr = q / g::cols;
                    if ((qr >= 4 ? 0 : 2) + (qc >= 4 ? 1 : 0) == which) any |= held[(size_t) q].load();
                }
                setParam (PerformancePads::padId (which), any ? 1.0f : 0.0f);
            }
            else if (mode == PadMode::Steps && down) // the two top rows: steps 1 .. 8 (top), 9 .. 16
            {
                const int row = pad / g::cols, col = pad % g::cols;
                const int step = (g::rows - 1 - row) * g::cols + col;
                const int note = proc.core().lastNote();
                if (deleteHeld) proc.core().stepSequencer().setStep (step, StepSequencer::Step {}); // p59: Delete + step = clear
                else proc.core().stepSequencer().toggleStep (step, note >= 0 ? note : 60);
            }
            return;
        }
        if (n < 11) touched = m.isNoteOn() ? n : (touched == n ? -1 : touched); // encoder touch
        if (n == g::touchNoteStrip && ! m.isNoteOn() && strip == 0) setParam (Core::Ids::perfBend, 0.0f); // springs back
        return;
    }
    if (m.isPitchWheel())
    {
        const int v = m.getPitchWheelValue();
        if (strip == 0) setParam (Core::Ids::perfBend, juce::jlimit (-1.0f, 1.0f, (float) (v - 8192) / 8191.0f));
        else setParam (Core::Ids::perfMod, juce::jlimit (0.0f, 1.0f, (float) v / 16383.0f));
        return;
    }
    if (! m.isController()) return;
    const int cc = m.getControllerNumber(), v = m.getControllerValue();
    if (g::isPedalCc (cc)) return; // p58: played at once in receive() (never a button)
    if (cc >= g::cc::firstEncoder && cc < g::cc::firstEncoder + g::numCells) { turn (cc - g::cc::firstEncoder, g::relativeDelta (v)); return; }
    if (cc == g::cc::masterEncoder || cc == g::cc::tempoEncoder)
    {
        const int d = g::relativeDelta (v);
        if (cc == g::cc::tempoEncoder || masterTempo)
        {
            if (! proc.tempoFollowsHost()) proc.setTempoFromUi (proc.currentTempo() + (float) d * (shiftHeld || selectHeld ? 0.1f : 1.0f));
        }
        else
        {
            const auto& reg = proc.core().params();
            const int idx = reg.indexOf ("mix.master.vol");
            if (idx >= 0)
            {
                const auto& desc = reg.descriptor (idx);
                const float n = juce::jlimit (0.0f, 1.0f, desc.toNormalised (reg.get (idx)) + (float) d * (selectHeld ? 0.002f : 0.01f));
                setParam ("mix.master.vol", desc.fromNormalised (n));
            }
        }
        return;
    }
    if (cc == g::cc::shift)
    {
        if (v > 0) { shiftHeld = true; shiftUsed = false; return; }
        shiftHeld = false;
        // p59: browse mode - Shift alone toggles the favourite of the preset under the cursor
        if (browse && ! shiftUsed && cursor >= 0 && cursor < (int) proc.presets().entries().size())
            proc.presets().setFavourite (cursor, ! proc.presets().entries()[(size_t) cursor].favourite);
        return;
    }
    if (cc == g::cc::select)
    {
        selectHeld = v > 0;
        if (v > 0 && browse) browseLoad (-1); // p59: Select loads the preset under the cursor
        return;
    }
    if (v > 0) button (cc, true);
    else buttonUp (cc);
}

void GridController::button (int cc, bool)
{
    using TK = OhpeProcessor::TransportKey;
    auto transport = [this] (TK k) { if (hooks.transport) hooks.transport (k); };
    if (shiftHeld) shiftUsed = true;
    // p59: Scales held - the upper row selects layout, In Key / Chromatic, fixed C, root, scale
    if (scalesHeld && cc >= g::cc::upperRow && cc < g::cc::upperRow + 8)
    {
        scalesUsed = true;
        scalesMenuButton (cc - g::cc::upperRow);
        return;
    }
    // p59: browse mode
    if (browse)
    {
        if (cc >= g::cc::upperRow && cc < g::cc::upperRow + 8)
        {
            const int k = cc - g::cc::upperRow;
            if (k == BrowseScroll) browseLoad (-1);
            else browseTurn (k, 0); // 0 = reset the column's filter
            return;
        }
        if (cc >= g::cc::lowerRow && cc < g::cc::lowerRow + 8)
        {
            const int k = cc - g::cc::lowerRow;
            browseLoad (k < Core::numParts ? k : -1);
            return;
        }
        switch (cc)
        {
            case g::cc::up: browseStep (-1, true); return;
            case g::cc::down: browseStep (1, true); return;
            case g::cc::left: browseStep (-4, false); return;
            case g::cc::right: browseStep (4, false); return;
            case g::cc::browse:
            case g::cc::device: setBrowse (false); return;
            default: break;
        }
    }
    // p59: Mute / Solo held + part button
    if ((muteHeld || soloHeld) && cc >= g::cc::lowerRow && cc < g::cc::lowerRow + Core::numParts)
    {
        partButtonHeld (cc - g::cc::lowerRow);
        return;
    }
    if (cc >= g::cc::firstScene && cc < g::cc::firstScene + g::cc::numScenes)
    {
        sceneDown (cc - g::cc::firstScene);
        return;
    }
    if (cc >= g::cc::upperRow && cc < g::cc::upperRow + numModes)
    {
        if (hooks.selectMode) hooks.selectMode ((Mode) (cc - g::cc::upperRow));
        return;
    }
    if (cc >= g::cc::lowerRow && cc < g::cc::lowerRow + 8)
    {
        const int k = cc - g::cc::lowerRow;
        if (k < Core::numParts) { if (hooks.selectPart) hooks.selectPart (k); }
        else setPadMode ((PadMode) (k - Core::numParts));
        return;
    }
    switch (cc)
    {
        case g::cc::left: if (hooks.stepPage) hooks.stepPage (-1); break;
        case g::cc::right: if (hooks.stepPage) hooks.stepPage (1); break;
        case g::cc::up: proc.presets().step (-1); break;
        case g::cc::down: proc.presets().step (1); break;
        case g::cc::octaveDown:
        case g::cc::octaveUp:
            if (hooks.octave && hooks.setOctave) hooks.setOctave (juce::jlimit (0, 7, hooks.octave() + (cc == g::cc::octaveUp ? 1 : -1)));
            break;
        case g::cc::play: transport (shiftHeld ? TK::TapePlayStop : TK::PlayStop); break;
        case g::cc::record: transport (shiftHeld ? TK::Arm : TK::Record); break;
        case g::cc::stop: transport (shiftHeld ? TK::ToStart : TK::Stop); break;
        case g::cc::repeat: transport (TK::Loop); break;
        case g::cc::metronome:
            if (shiftHeld)
            {
                const int c = (int) std::lround (proc.core().params().get ("metro.countin"));
                setParam ("metro.countin", (float) ((c + 1) % 3));
            }
            else
                setParam ("metro.on", proc.core().params().get ("metro.on") >= 0.5f ? 0.0f : 1.0f);
            break;
        case g::cc::tapTempo: proc.tapTempo(); break;
        case g::cc::undo: proc.runTapeEdit (shiftHeld ? "tape.edit.redo" : "tape.edit.undo"); break;
        case g::cc::scales:
            if (shiftHeld)
                setParam (Core::Ids::keyLock, proc.core().keyScale().lock ? 0.0f : 1.0f);
            else
            {   // p59: a tap steps the layout (on release); held = the grid options on the display
                scalesHeld = true;
                scalesUsed = false;
            }
            break;
        case g::cc::master: masterTempo = ! masterTempo; break;
        case g::cc::note: setPadMode (PadMode::Keys); break;
        case g::cc::session: setPadMode (PadMode::Steps); break;
        // ---- p59: the further function buttons
        case g::cc::mute: muteHeld = true; muteUsed = false; break;
        case g::cc::solo: soloHeld = true; soloUsed = false; break;
        case g::cc::deleteButton: deleteHeld = true; break;
        case g::cc::device:
            if (hooks.showFxEditor) hooks.showFxEditor (! (hooks.fxEditorShown && hooks.fxEditorShown()));
            break;
        case g::cc::browse: setBrowse (true); break;
        case g::cc::track: if (hooks.selectMode) hooks.selectMode (Mode::Mixer); break;
        case g::cc::clip: if (hooks.selectMode) hooks.selectMode (Mode::Tape); break;
        case g::cc::volume:
        case g::cc::panSend:
        {
            const auto page = cc == g::cc::volume ? EncoderPage::Volume : EncoderPage::PanSend;
            encPage = encPage == page ? EncoderPage::Normal : page;
            refreshMap();
            break;
        }
        case g::cc::accent: accentNow.store (! accentNow.load()); break;
        case g::cc::quantize:
        {
            const juce::String id = "p" + juce::String (partNow.load() + 1) + ".quant";
            setParam (id, proc.core().params().get (id.toStdString()) >= 0.5f ? 0.0f : 1.0f);
            break;
        }
        case g::cc::duplicate: // the step pattern into the next pattern, which then plays and is edited
        {
            auto& sq = proc.core().stepSequencer();
            const int from = sq.getEditPattern();
            if (from + 1 < StepSequencer::numPatterns)
            {
                sq.copyPattern (from, from + 1);
                sq.setEditPattern (from + 1);
                setParam ("seq.step.pattern", (float) (from + 2));
            }
            break;
        }
        case g::cc::doubleButton: // the step pattern twice as long, the steps copied into the new half
        {
            auto& sq = proc.core().stepSequencer();
            const int pat = sq.getEditPattern();
            if (const int len = sq.doubleSteps (pat); len > 0)
            {
                if (sq.getPatternLength (pat) > 0) sq.setPatternLength (pat, len);
                else setParam ("seq.step.length", (float) len);
            }
            break;
        }
        case g::cc::newButton: proc.presets().loadStartSound(); break;
        case g::cc::fixedLength: // step length 4 -> 8 -> 16 -> 32 -> 64 -> 4
        {
            const int len = (int) std::lround (proc.core().params().get ("seq.step.length"));
            int next = 4;
            for (int l : { 4, 8, 16, 32, 64 })
                if (l > len) { next = l; break; }
            setParam ("seq.step.length", (float) next);
            break;
        }
        case g::cc::automation:
        {
            const juce::String id = "p" + juce::String (partNow.load() + 1) + ".motion.rec";
            setParam (id, proc.core().params().get (id.toStdString()) >= 0.5f ? 0.0f : 1.0f);
            break;
        }
        case g::cc::addEffect: addEffect(); break;
        case g::cc::addTrack: proc.tapeMemory().capture(); break; // p67: CAPTURE - the tape memory into the target part
        case g::cc::user:
            if (hooks.showBreeding) hooks.showBreeding (! (hooks.breedingShown && hooks.breedingShown()));
            break;
        default: break;
    }
}

void GridController::buttonUp (int cc)
{
    if (cc >= g::cc::firstScene && cc < g::cc::firstScene + g::cc::numScenes) { sceneUp (cc - g::cc::firstScene); return; }
    switch (cc)
    {
        case g::cc::scales:
            if (scalesHeld && ! scalesUsed)
            {
                if (hooks.gridShown && hooks.showGrid && ! hooks.gridShown())
                    hooks.showGrid (true); // p53: first the grid keyboard on the screen (its layout on the pads), then the next layout
                else
                    setParam ("harmony.kb.layout", (float) (((int) std::lround (proc.core().params().get ("harmony.kb.layout")) + 1) % harmony::numLayouts));
            }
            scalesHeld = false;
            break;
        case g::cc::mute:
        case g::cc::solo:
        {
            const bool isMute = cc == g::cc::mute;
            if (isMute ? (muteHeld && ! muteUsed) : (soloHeld && ! soloUsed)) partButtonHeld (-1); // alone: the edit part
            (isMute ? muteHeld : soloHeld) = false;
            break;
        }
        case g::cc::deleteButton: deleteHeld = false; break;
        default: break;
    }
}

void GridController::partButtonHeld (int part)
{
    const bool mute = muteHeld; // Mute wins when both are held
    const int p = part < 0 ? partNow.load() : part;
    const juce::String id = "mix.ch" + juce::String (juce::jlimit (0, Core::numParts - 1, p) + 1) + (mute ? ".mute" : ".solo");
    setParam (id, proc.core().params().get (id.toStdString()) >= 0.5f ? 0.0f : 1.0f);
    (mute ? muteUsed : soloUsed) = true;
}

void GridController::scalesMenuButton (int k)
{
    auto& reg = proc.core().params();
    if (k < harmony::numLayouts)
    {
        if (hooks.gridShown && hooks.showGrid && ! hooks.gridShown()) hooks.showGrid (true); // the pads take the grid's layout
        setParam ("harmony.kb.layout", (float) k);
    }
    else if (k == 4) setParam ("harmony.kb.inkey", reg.get ("harmony.kb.inkey") >= 0.5f ? 0.0f : 1.0f);
    else if (k == 5) setParam ("harmony.kb.fixed", reg.get ("harmony.kb.fixed") >= 0.5f ? 0.0f : 1.0f);
    else if (k == 6) setParam (Core::Ids::keyRoot, (float) ((((int) std::lround (reg.get (Core::Ids::keyRoot)) + (shiftHeld ? 11 : 1)) % 12)));
    else if (k == 7)
    {
        const int n = (int) scales::namesEn().size();
        setParam ("key.scale", (float) ((((int) std::lround (reg.get ("key.scale")) + (shiftHeld ? n - 1 : 1)) % n)));
    }
}

void GridController::sceneDown (int scene)
{
    const int rate = g::sceneRate (scene);
    if (rate < 0) return;
    if (shiftHeld) { setParam ("seq.arp.rate", (float) rate); return; } // Shift: sets the arp rate and keeps it
    auto& reg = proc.core().params();
    if (scenesHeld == 0)
    {   // note repeat while held: the arp on the edit part at this rate; released = as before
        arpBefore[0] = reg.get ("seq.arp.on");
        arpBefore[1] = reg.get ("seq.arp.part");
        arpBefore[2] = reg.get ("seq.arp.rate");
        setParam ("seq.arp.part", (float) (partNow.load() + 1));
        setParam ("seq.arp.on", 1.0f);
    }
    scenesHeld |= 1 << scene;
    setParam ("seq.arp.rate", (float) rate);
}

void GridController::sceneUp (int scene)
{
    const int bit = 1 << juce::jlimit (0, g::cc::numScenes - 1, scene);
    if ((scenesHeld & bit) == 0) return;
    scenesHeld &= ~bit;
    if (scenesHeld != 0) return;
    setParam ("seq.arp.on", arpBefore[0]);
    setParam ("seq.arp.part", arpBefore[1]);
    setParam ("seq.arp.rate", arpBefore[2]);
}

void GridController::addEffect()
{
    const auto ctx = hooks.context ? hooks.context() : PageContext {};
    const int view = modfx::isRackView (ctx.fxView) ? ctx.fxView : (int) modfx::FxViewRack;
    const auto prefixes = modfx::rackPrefixes (view, ctx.part);
    if (prefixes.empty()) return;
    const auto id = prefixes[(size_t) juce::jlimit (0, (int) prefixes.size() - 1, ctx.fxSlot)] + "type";
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id);
    if (idx < 0) return;
    const auto& d = reg.descriptor (idx);
    const int n = (int) std::lround (d.maxValue) + 1;
    setParam (id, (float) (((int) std::lround (reg.get (idx)) + 1) % juce::jmax (1, n)));
}

// ---------------------------------------------------------------- p59: browse mode (the library on the display)
void GridController::setBrowse (bool on)
{
    if (on == browse) return;
    browse = on;
    encAccum.fill (0);
    auto& pm = proc.presets();
    if (on)
    {
        if (hooks.showLibrary) hooks.showLibrary (true); // the screen's library opens and follows
        else pm.rescan();
        const auto v = pm.view();
        cursor = std::find (v.begin(), v.end(), pm.current()) != v.end() ? pm.current() : (v.empty() ? -1 : v.front());
        browseFollow();
    }
    else if (hooks.showLibrary && hooks.libraryShown && hooks.libraryShown())
        hooks.showLibrary (false); // keeps the preset loaded
}

void GridController::browseFollow()
{
    if (hooks.libraryFollow) hooks.libraryFollow (cursor);
}

void GridController::browseStep (int delta, bool load)
{
    auto& pm = proc.presets();
    const auto v = pm.view();
    if (v.empty()) { cursor = -1; return; }
    const int n = (int) v.size();
    const auto it = std::find (v.begin(), v.end(), cursor);
    int pos = it == v.end() ? (delta > 0 ? -1 : n) : (int) (it - v.begin());
    pos = load ? ((pos + delta) % n + n) % n : juce::jlimit (0, n - 1, pos + delta); // ▲ / ▼ wrap like the preset keys
    cursor = v[(size_t) pos];
    if (load) pm.load (cursor); // instant audition
    browseFollow();
}

void GridController::browseLoad (int part)
{
    if (cursor < 0 || cursor >= (int) proc.presets().entries().size()) return;
    proc.presets().load (cursor, part);
    browseFollow();
}

namespace
{
/** Steps through -1 (none) .. n - 1, wrapping. */
int cycle (int current, int steps, int n)
{
    const int m = n + 1;
    return (((current + 1 + steps) % m) + m) % m - 1;
}

std::vector<std::string> usedEngines (const std::vector<preset::Entry>& entries)
{
    std::vector<std::string> out;
    for (const auto& e : entries)
        if (! e.meta.engine.empty() && std::find (out.begin(), out.end(), e.meta.engine) == out.end()) out.push_back (e.meta.engine);
    std::sort (out.begin(), out.end());
    return out;
}

/** The browse column Source: 0 All, 1 Factory, 2 own presets, 3 .. = the content packs. */
int sourceIndex (const preset::Filter& f, const std::vector<std::string>& packs)
{
    if (! f.pack.empty())
    {
        const auto it = std::find (packs.begin(), packs.end(), f.pack);
        return it == packs.end() ? 0 : 3 + (int) (it - packs.begin());
    }
    return f.bank == preset::Bank::Factory ? 1 : f.bank == preset::Bank::User ? 2 : 0;
}
} // namespace

void GridController::browseTurn (int column, int steps)
{
    auto& pm = proc.presets();
    auto& f = pm.filter;
    const auto& entries = pm.entries();
    const bool reset = steps == 0;
    switch (column)
    {
        case BrowseScroll: browseStep (steps, false); return;
        case BrowseCategory:
        {
            const auto tags = preset::usedTags (entries);
            int cur = -1;
            if (f.tags.size() == 1)
                if (const auto it = std::find (tags.begin(), tags.end(), f.tags.front()); it != tags.end()) cur = (int) (it - tags.begin());
            const int next = reset ? -1 : cycle (cur, steps, (int) tags.size());
            f.tags.clear();
            if (next >= 0) f.tags.push_back (tags[(size_t) next]);
            break;
        }
        case BrowseEngine:
        {
            const auto engines = usedEngines (entries);
            const auto it = std::find (engines.begin(), engines.end(), f.engine);
            const int cur = it == engines.end() ? -1 : (int) (it - engines.begin());
            const int next = reset ? -1 : cycle (cur, steps, (int) engines.size());
            f.engine = next >= 0 ? engines[(size_t) next] : std::string();
            break;
        }
        case BrowseSource:
        {
            const auto packs = preset::usedPacks (entries);
            const int n = 3 + (int) packs.size();
            const int next = reset ? 0 : (((sourceIndex (f, packs) + steps) % n) + n) % n;
            f.pack = next >= 3 ? packs[(size_t) (next - 3)] : std::string();
            f.bank = next == 1 ? preset::Bank::Factory : next == 2 ? preset::Bank::User : preset::Bank::All;
            break;
        }
        case BrowseFavourites: f.favouritesOnly = reset ? false : (std::abs (steps) % 2 == 1 ? ! f.favouritesOnly : f.favouritesOnly); break;
        case BrowseTag:
        {
            const auto tags = preset::usedQuickTags (entries);
            int cur = -1;
            if (f.required.size() == 1)
                if (const auto it = std::find (tags.begin(), tags.end(), f.required.front()); it != tags.end()) cur = (int) (it - tags.begin());
            const int next = reset ? -1 : cycle (cur, steps, (int) tags.size());
            f.required.clear();
            if (next >= 0) f.required.push_back (tags[(size_t) next]);
            break;
        }
        case BrowseRating: f.minRating = reset ? 0 : juce::jlimit (0, 5, f.minRating + steps); break;
        case BrowseSort: f.sort = reset ? preset::Sort::Name : (preset::Sort) ((((int) f.sort + steps) % 4 + 4) % 4); break;
        default: return;
    }
    // the cursor stays on its preset while it passes the filter, else the first one shown
    const auto v = pm.view();
    if (std::find (v.begin(), v.end(), cursor) == v.end()) cursor = v.empty() ? -1 : v.front();
    browseFollow();
}

bool GridController::browseFilterActive (int column) const
{
    const auto& f = proc.presets().filter;
    switch (column)
    {
        case BrowseCategory: return ! f.tags.empty();
        case BrowseEngine: return ! f.engine.empty();
        case BrowseSource: return ! f.pack.empty() || f.bank != preset::Bank::All;
        case BrowseFavourites: return f.favouritesOnly;
        case BrowseTag: return ! f.required.empty();
        case BrowseRating: return f.minRating > 0;
        case BrowseSort: return f.sort != preset::Sort::Name;
        default: return false;
    }
}

juce::String GridController::browseValue (int column) const
{
    const auto& pm = proc.presets();
    const auto& f = pm.filter;
    switch (column)
    {
        case BrowseScroll:
        {
            const auto v = pm.view();
            const auto it = std::find (v.begin(), v.end(), cursor);
            return juce::String (it == v.end() ? 0 : (int) (it - v.begin()) + 1) + "/" + juce::String ((int) v.size());
        }
        case BrowseCategory: return f.tags.empty() ? tr ("All") : f.tags.size() == 1 ? LibraryBrowser::tagText (f.tags.front()) : juce::String ((int) f.tags.size()) + "+";
        case BrowseEngine: return f.engine.empty() ? tr ("All") : LibraryBrowser::engineText (f.engine);
        case BrowseSource:
        {
            if (! f.pack.empty())
            {
                for (const auto& p : pm.contentPacks())
                    if (p.id == f.pack) return juce::String::fromUTF8 ((i18n::current() == i18n::Language::German && ! p.nameDe.empty() ? p.nameDe : p.nameEn).c_str());
                return juce::String::fromUTF8 (f.pack.c_str());
            }
            return f.bank == preset::Bank::Factory ? tr ("Factory") : f.bank == preset::Bank::User ? tr ("User") : tr ("All");
        }
        case BrowseFavourites: return f.favouritesOnly ? tr ("On") : tr ("Off");
        case BrowseTag: return f.required.empty() ? tr ("All") : LibraryBrowser::tagText (f.required.front());
        case BrowseRating: return f.minRating > 0 ? ">=" + juce::String (f.minRating) + "*" : tr ("All");
        case BrowseSort:
        {
            static const char* sorts[] = { "Name", "Rating", "Engine", "Newest" };
            return tr (sorts[juce::jlimit (0, 3, (int) f.sort)]);
        }
        default: return {};
    }
}

void GridController::refreshBrowseDisplay()
{
    // line 1: the eight columns' values (a touched encoder: its name and value in full on line 1)
    static const char* namesEn[] = { "Scroll", "Category", "Engine", "Source", "Favourites", "Tag", "Rating", "Sort" };
    static const char* namesDe[] = { "Liste", "Kategorie", "Engine", "Quelle", "Favoriten", "Merkmal", "Bewertung", "Sortierung" };
    const bool de = i18n::current() == i18n::Language::German;
    std::array<std::string, g::numCells> values {};
    for (int i = 0; i < g::numCells; ++i) values[(size_t) i] = g::shortValue (str (browseValue (i)));
    lines[0] = g::cellLine (values);
    if (touched >= 0 && touched < g::numCells)
        lines[0] = g::fitLine (str (juce::String (de ? namesDe[touched] : namesEn[touched]) + ": " + browseValue (touched)));
    // lines 2 - 4: the preset before, the one under the cursor (marked), the next one -
    // name | category | engine | author or pack, favourite and rating
    const auto& pm = proc.presets();
    const auto v = pm.view();
    const auto& entries = pm.entries();
    const auto it = std::find (v.begin(), v.end(), cursor);
    const int pos = it == v.end() ? -1 : (int) (it - v.begin());
    for (int r = 0; r < 3; ++r)
    {
        const int at = pos + r - 1;
        if (pos < 0 || at < 0 || at >= (int) v.size())
        {
            lines[(size_t) (r + 1)] = g::fitLine (r == 1 && v.empty() ? str (tr ("No presets - turn the filters back")) : std::string());
            continue;
        }
        const auto& e = entries[(size_t) v[(size_t) at]];
        juce::String cat;
        for (const auto& t : e.meta.tags)
            if (preset::isCategory (t)) { cat = LibraryBrowser::tagText (t); break; }
        juce::String who = juce::String::fromUTF8 (e.meta.author.c_str());
        for (const auto& pk : pm.contentPacks())
            if (! e.pack.empty() && pk.id == e.pack) who = juce::String::fromUTF8 ((de && ! pk.nameDe.empty() ? pk.nameDe : pk.nameEn).c_str());
        if (who.isEmpty()) who = e.factory ? tr ("Factory") : tr ("User");
        juce::String marks;
        if (e.favourite) marks << "+";
        if (e.rating > 0) marks << juce::String (e.rating) << "*";
        const auto name = juce::String::fromUTF8 (e.meta.name.c_str()).trim();
        const int whoRoom = juce::jmax (1, 16 - marks.length() - 1);
        // segmentLine trims leading spaces and keeps 16 characters per segment: a placeholder in column 0, replaced by
        // the cursor mark afterwards, keeps every row's name at column 1 (15 characters, one space before the category)
        auto row = g::segmentLine ({ str ("#" + name), str (cat), str (LibraryBrowser::engineText (e.meta.engine)),
                                     str (who.substring (0, whoRoom) + (marks.isEmpty() ? juce::String() : " " + marks)) });
        if (! row.empty()) row[0] = r == 1 ? '>' : ' ';
        lines[(size_t) (r + 1)] = row;
    }
}

void GridController::setPadMode (PadMode m)
{
    if ((int) m == padModeNow.load()) return;
    for (int pad = 0; pad < g::numPads; ++pad) padUp (pad);
    if (padMode() == PadMode::Pads)
        for (int k = 0; k < PerformancePads::numPads; ++k)
            if (proc.core().params().get (PerformancePads::padId (k)) >= 0.5f) setParam (PerformancePads::padId (k), 0.0f);
    padModeNow.store (juce::jlimit (0, (int) PadMode::numModes - 1, (int) m));
    refreshMap();
}

void GridController::setParam (const juce::String& id, float plain)
{
    if (auto* p = proc.parameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
    proc.core().params().set (id.toStdString(), plain); // the screen follows at once
}

void GridController::turn (int encoder, int delta)
{
    if (encoder < 0 || encoder >= g::numCells || delta == 0) return;
    if (browse) // p59: the browse columns (scroll: a row every two ticks, filters: a step every four)
    {
        auto& acc = encAccum[(size_t) encoder];
        acc += delta;
        const int div = encoder == BrowseScroll ? 2 : 4;
        const int steps = acc / div;
        if (steps == 0) return;
        acc -= steps * div;
        browseTurn (encoder, steps);
        return;
    }
    const auto& id = encIds[(size_t) encoder];
    if (id.empty()) return;
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id);
    if (idx < 0) return;
    const auto& d = reg.descriptor (idx);
    if (d.kind == ParamKind::Continuous)
    {
        const float n = juce::jlimit (0.0f, 1.0f, d.toNormalised (reg.get (idx)) + (float) delta * (selectHeld || shiftHeld ? 0.001f : 0.0075f));
        setParam (id, d.fromNormalised (n));
        return;
    }
    // steps: a choice / whole number moves one step every few encoder ticks
    auto& acc = encAccum[(size_t) encoder];
    acc += delta;
    const int steps = acc / 4;
    if (steps == 0) return;
    acc -= steps * 4;
    setParam (id, juce::jlimit (d.minValue, d.maxValue, std::round (reg.get (idx)) + (float) steps));
}

// ---------------------------------------------------------------- state -> pads, display, lights
int GridController::padLayout() const
{
    const bool grid = hooks.gridShown ? hooks.gridShown() : false;
    if (! grid) return harmony::LayoutFourths;
    return juce::jlimit (0, (int) harmony::numLayouts - 1, (int) std::lround (proc.core().params().get ("harmony.kb.layout")));
}

g::PadKey GridController::padKey() const
{
    auto& core = proc.core();
    g::PadKey k;
    k.root = core.harmony().keyRoot(); // the on-screen grid's key (the remembered key; the set key until anything is heard)
    k.scale = core.harmony().keyScale();
    k.lock = core.keyScale().lock; // p45: Scale Lock
    k.origin = core.keyScale().root; // p59: the set key's root bottom left (stays while the remembered key moves)
    return k;
}

int GridController::padNoteNow (int pad) const noexcept
{
    return pad >= 0 && pad < g::numPads ? padMap[(size_t) pad].load() : -1;
}

bool GridController::padNoteDown (int note) const noexcept
{
    if (note < 0 || padMode() != PadMode::Keys) return false;
    for (int pad = 0; pad < g::numPads; ++pad)
        if (held[(size_t) pad].load() && padMap[(size_t) pad].load() == note) return true;
    return false;
}

void GridController::refreshMap()
{
    auto& core = proc.core();
    const auto ctx = hooks.context ? hooks.context() : PageContext {};
    partNow.store (juce::jlimit (0, Core::numParts - 1, ctx.part));
    mpeNow.store (core.params().get ("p" + std::to_string (ctx.part + 1) + ".mpe") >= 0.5f);
    drumBase.store ((int) std::lround (core.params().get ("drum.base")));
    const int layout = padLayout();
    const auto opts = harmony::gridOptions (core.params()); // p59: In Key / Chromatic, fixed C (every layout)
    const int octave = hooks.octave ? hooks.octave() : 3;
    const auto key = padKey();
    const auto mode = padMode();
    for (int pad = 0; pad < g::numPads; ++pad)
    {
        const int col = pad % g::cols, row = pad / g::cols;
        int note = -1;
        if (mode == PadMode::Keys || (mode == PadMode::Steps && row < g::rows - stepRows))
        {
            note = g::padLayoutNote (layout, col, row, octave, key, opts.inKey, opts.fixed);
            // Scale Lock on Chromatic: other notes are dark and silent (In Key has only key notes anyway)
            if (key.lock && ! opts.inKey && ! scales::inScale (note, key.root, key.scale)) note = -1;
        }
        padMap[(size_t) pad].store (note);
    }
    // encoders: the shown page and the next one (Pages.cpp, four per page)
    const auto m = hooks.mode ? hooks.mode() : Mode::Synth;
    const auto ids = modeParams (m, core, ctx);
    const int pg = hooks.page ? hooks.page() : 0;
    for (int i = 0; i < g::numCells; ++i)
    {
        const size_t k = (size_t) (pg * 4 + i);
        encIds[(size_t) i] = k < ids.size() ? ids[k] : std::string();
    }
    // p59: Volume = the four parts' volume (1 - 4) and pan (5 - 8); Pan & Send = send A (1 - 4) and send B (5 - 8)
    if (encPage != EncoderPage::Normal)
        for (int i = 0; i < Core::numParts; ++i)
        {
            const auto ch = "mix.ch" + std::to_string (i + 1) + ".";
            encIds[(size_t) i] = ch + (encPage == EncoderPage::Volume ? "vol" : "senda");
            encIds[(size_t) (i + 4)] = ch + (encPage == EncoderPage::Volume ? "pan" : "sendb");
        }
}

void GridController::refreshDisplay()
{
    auto& core = proc.core();
    const auto& reg = core.params();
    const auto ctx = hooks.context ? hooks.context() : PageContext {};
    const auto m = hooks.mode ? hooks.mode() : Mode::Synth;
    std::array<std::string, g::numCells> names {}, values {};
    std::string touchedText;
    for (int i = 0; i < g::numCells; ++i)
    {
        const auto& id = encIds[(size_t) i];
        const int idx = id.empty() ? -1 : reg.indexOf (id);
        if (idx < 0) continue;
        const auto& d = reg.descriptor (idx);
        const auto compact = modfx::compactName (d.id, d.nameEn, d.nameDe);
        names[(size_t) i] = g::shortName (str (compact));
        auto value = modfx::specialValueText (core, d.id, reg.get (idx));
        if (value.isEmpty()) value = formatValue (d, reg.get (idx));
        values[(size_t) i] = g::shortValue (str (value));
        if (touched == i) touchedText = str (i18n::pick (d.nameEn, d.nameDe) + ": " + value);
    }
    lines[0] = g::cellLine (names);
    lines[1] = g::cellLine (values);

    // line 3: preset, part + engine, tempo, key (or the touched encoder in full)
    const auto key = padKey();
    const auto scaleNames = i18n::current() == i18n::Language::German ? scales::namesDe() : scales::namesEn();
    juce::String keyText = juce::String (pitchNames[((key.root % 12) + 12) % 12]) + " "
                           + juce::String (scaleNames[(size_t) juce::jlimit (0, (int) scaleNames.size() - 1, key.scale)]);
    if (key.lock) keyText << " " << tr ("Lock");
    juce::String engine;
    if (const int ei = reg.indexOf ("p" + std::to_string (ctx.part + 1) + ".engine"); ei >= 0) engine = formatValue (reg.descriptor (ei), reg.get (ei));
    if (touched >= 0 && touched < g::numCells && ! touchedText.empty()) lines[2] = g::fitLine (touchedText);
    else if (touched == g::touchNoteTempo) // p53: the tempo encoder touched
        lines[2] = g::fitLine (str (tr ("Tempo") + ": " + juce::String (proc.currentTempo(), 1) + " BPM"));
    else if (touched == g::touchNoteMaster)
        lines[2] = g::fitLine (str (masterTempo ? tr ("Tempo") + ": " + juce::String (proc.currentTempo(), 1) + " BPM"
                                                : tr ("Volume") + ": " + formatValue (reg.descriptor (reg.indexOf ("mix.master.vol")), reg.get ("mix.master.vol"))));
    else
        lines[2] = g::segmentLine ({ str (proc.presets().currentName()), str (tr ("Part") + " " + juce::String (ctx.part + 1) + " " + engine),
                                     str (juce::String (proc.currentTempo(), 1) + " BPM"), str (keyText) });

    // line 4: page, transport, tape position, pad mode
    const auto titles = pageTitles (m, core, ctx);
    const int pg = hooks.page ? hooks.page() : 0;
    const int n = numPages (modeParams (m, core, ctx));
    juce::String page = tr (modeInfo (m).nameEn);
    if (pg < (int) titles.size() && titles[(size_t) pg].size() > 0) page = tr (titles[(size_t) pg].c_str());
    juce::String count = juce::String (pg + 1) + "/" + juce::String (n);
    if (encPage != EncoderPage::Normal) { page = tr (encPage == EncoderPage::Volume ? "Volume" : "Pan & Send"); count = {}; } // p59
    if (browse) { page = tr ("Browse"); count = {}; }
    page = juce::String (g::fold (str (page)).substr (0, (size_t) juce::jmax (1, g::lineLength / 4 - 2 - count.length()))) + " " + count;
    juce::String tx = tr ("Stop");
    if (core.countInActive()) tx = tr ("Count-in");
    else if (proc.tapeRecording()) tx = "REC";
    else if (proc.tapeRecordReady()) tx = "REC " + tr ("ready");
    else if (proc.tapeRolling() || reg.get (Core::Ids::seqRun) >= 0.5f) tx = tr ("Play");
    if (reg.get ("metro.on") >= 0.5f) tx << " " << tr ("Click");
    auto& tape = core.tape();
    const double secs = juce::jmax (0.0, (double) tape.positionFrames() / juce::jmax (1.0, tape.sampleRate()));
    juce::String padText = modeName (padMode());
    if (padMode() == PadMode::Keys || padMode() == PadMode::Steps)
    {   // p59: layout, In Key / Chromatic, fixed C and the octave (the pad mode shows on its lit button)
        padText = layoutSummary (padLayout(), harmony::gridOptions (reg)) + " O" + juce::String (hooks.octave ? hooks.octave() : 3);
    }
    lines[3] = g::segmentLine ({ str (page), str (tx), str (proc.tapeTimeText (secs).replace (juce::String::fromUTF8 (" \xc2\xb7 "), "  ")), str (padText) });
    if (browse) refreshBrowseDisplay(); // p59: the preset list
    if (scalesHeld) // p59: Scales held - the grid options over the upper row
    {
        const bool de = i18n::current() == i18n::Language::German;
        const auto o = harmony::gridOptions (reg);
        std::array<std::string, g::numCells> names {}, vals {};
        const auto ln = de ? harmony::layoutNamesDe() : harmony::layoutNamesEn();
        for (int l = 0; l < harmony::numLayouts; ++l)
        {
            names[(size_t) l] = g::shortName (ln[(size_t) l]);
            vals[(size_t) l] = l == o.layout ? "[x]" : "";
        }
        names[4] = de ? "Toene" : "Notes";
        vals[4] = o.inKey ? (de ? "Tonart" : "In Key") : (de ? "Chrom." : "Chrom");
        names[5] = de ? "Unten" : "Bottom";
        vals[5] = o.fixed ? (de ? "C fest" : "Fixed C") : (de ? "Grundt." : "Root");
        names[6] = de ? "Grundt." : "Root";
        vals[6] = pitchNames[juce::jlimit (0, 11, (int) std::lround (reg.get (Core::Ids::keyRoot)))];
        names[7] = de ? "Skala" : "Scale";
        vals[7] = g::shortValue (str (juce::String (scaleNames[(size_t) juce::jlimit (0, (int) scaleNames.size() - 1, (int) std::lround (reg.get ("key.scale")))])));
        lines[0] = g::cellLine (names);
        lines[1] = g::cellLine (vals);
        lines[2] = g::fitLine (str ((de ? "Raster: " : "Grid: ") + juce::String (ln[(size_t) o.layout]) + " - "
                                    + (o.inKey ? (de ? "nur Tonart" : "In Key") : (de ? "chromatisch" : "Chromatic")) + " - "
                                    + (o.fixed ? (de ? "unten links C" : "bottom left C") : (de ? "unten links Grundton" : "bottom left root"))));
        lines[3] = g::fitLine (de ? "Scales halten + obere Tasten waehlen (Shift: Grundton / Skala zurueck)"
                                  : "Hold Scales + upper buttons to choose (Shift: root / scale back)");
    }
    if (splashTicks > 0) lines = g::splashLines (splashTicks > 20 ? 0 : 1, i18n::current() == i18n::Language::German); // p53

    // p53: only a connected first-generation device has the text display (a later one: pixel display over USB)
    if (! isConnected || gen == g::Generation::Later) return;
    for (int l = 0; l < g::numLines; ++l)
        if (lines[(size_t) l] != sentLines[(size_t) l])
        {
            sentLines[(size_t) l] = lines[(size_t) l];
            sendSysex (g::displayLineSysex (l, lines[(size_t) l]));
        }
}

void GridController::refreshLights()
{
    auto& core = proc.core();
    const auto& reg = core.params();
    const auto mode = padMode();
    const auto key = padKey();
    const auto chord = core.harmony().chord();
    uint16_t chordMask = 0;
    if (chord.root >= 0)
        for (int i = 0; i < 12; ++i)
            if (((harmony::chordShape (chord.type) >> i) & 1u) != 0) chordMask = (uint16_t) (chordMask | (1u << ((chord.root + i) % 12)));
    const auto ctx = hooks.context ? hooks.context() : PageContext {};
    const int stepPos = core.stepSequencer().playPosition();
    for (int pad = 0; pad < g::numPads; ++pad)
    {
        const int col = pad % g::cols, row = pad / g::cols;
        const bool down = held[(size_t) pad].load();
        g::Colour c = g::Colour::Off;
        if (mode == PadMode::Pads)
        {
            const int which = (row >= 4 ? 0 : 2) + (col >= 4 ? 1 : 0);
            c = reg.get (PerformancePads::padId (which)) >= 0.5f ? g::Colour::Green : g::Colour::AmberDim;
        }
        else if (mode == PadMode::Drum)
        {
            if (pad < 24) c = down ? g::Colour::Green : pad == ctx.drumPad ? g::Colour::Amber : g::Colour::YellowDim;
        }
        else if (mode == PadMode::Steps && row >= g::rows - stepRows)
        {
            const int step = (g::rows - 1 - row) * g::cols + col;
            const bool on = core.stepSequencer().getStep (step).on;
            c = stepPos >= 0 && stepPos % 16 == step ? g::Colour::Green : on ? g::Colour::Yellow : g::Colour::AmberDim;
        }
        else
        {
            const int note = padMap[(size_t) pad].load();
            // p53: a note held on the on-screen keyboard lights its pads too
            const bool screen = note >= 0 && mode == PadMode::Keys && hooks.screenPressed && hooks.screenPressed (note);
            if (note >= 0) c = g::roleColour (g::padRole (note, key, chordMask, down || screen));
        }
        padLight[(size_t) pad] = g::padColourCode (c);
    }
    // buttons
    ccLight.fill (-1);
    const auto m = hooks.mode ? hooks.mode() : Mode::Synth;
    const auto leds = keyLeds (ledInputs (core, ctx.part, m, proc.tapeRecording(), proc.tapeRolling()));
    for (int i = 0; i < numModes; ++i)
    {
        g::Colour c = g::Colour::Off;
        switch (leds.mode[(size_t) i])
        {
            case Led::Focus: c = g::Colour::Yellow; break;
            case Led::Active: c = g::Colour::AmberDim; break;
            case Led::Record: c = g::Colour::Red; break;
            case Led::Play: c = g::Colour::Green; break;
            default: c = modeInfo ((Mode) i).available ? g::Colour::YellowDim : g::Colour::Off; break;
        }
        ccLight[(size_t) (g::cc::upperRow + i)] = g::colourCode (c);
    }
    for (int k = 0; k < 8; ++k)
    {
        const bool on = k < Core::numParts ? ctx.part == k : (int) mode == k - Core::numParts;
        ccLight[(size_t) (g::cc::lowerRow + k)] = g::colourCode (on ? g::Colour::Yellow : g::Colour::YellowDim);
    }
    const int oct = hooks.octave ? hooks.octave() : 3;
    const bool running = proc.tapeRolling() || reg.get (Core::Ids::seqRun) >= 0.5f;
    ccLight[(size_t) g::cc::play] = g::buttonCode (running);
    ccLight[(size_t) g::cc::record] = g::buttonCode (proc.tapeRecordReady(), true, proc.tapeRecording());
    ccLight[(size_t) g::cc::stop] = g::buttonCode (false);
    ccLight[(size_t) g::cc::metronome] = g::buttonCode (reg.get ("metro.on") >= 0.5f);
    ccLight[(size_t) g::cc::tapTempo] = g::buttonCode (false);
    ccLight[(size_t) g::cc::octaveDown] = g::buttonCode (oct < 3);
    ccLight[(size_t) g::cc::octaveUp] = g::buttonCode (oct > 3);
    ccLight[(size_t) g::cc::left] = g::buttonCode (false);
    ccLight[(size_t) g::cc::right] = g::buttonCode (false);
    ccLight[(size_t) g::cc::up] = g::buttonCode (false);
    ccLight[(size_t) g::cc::down] = g::buttonCode (false);
    ccLight[(size_t) g::cc::shift] = g::buttonCode (shiftHeld);
    ccLight[(size_t) g::cc::select] = g::buttonCode (selectHeld);
    ccLight[(size_t) g::cc::scales] = g::buttonCode (key.lock);
    ccLight[(size_t) g::cc::undo] = g::buttonCode (false);
    ccLight[(size_t) g::cc::repeat] = g::buttonCode (false);
    ccLight[(size_t) g::cc::master] = g::buttonCode (masterTempo);
    ccLight[(size_t) g::cc::note] = g::buttonCode (mode == PadMode::Keys);
    ccLight[(size_t) g::cc::session] = g::buttonCode (mode == PadMode::Steps);

    // ---- p59: the further function buttons, lit by state
    auto anyCh = [&] (const char* k) {
        for (int i = 1; i <= Core::numParts; ++i)
            if (reg.get ("mix.ch" + std::to_string (i) + "." + k) >= 0.5f) return true;
        return false;
    };
    const auto pre = "p" + std::to_string (ctx.part + 1) + ".";
    const bool motionRec = reg.get (pre + "motion.rec") >= 0.5f;
    ccLight[(size_t) g::cc::scales] = g::buttonCode (key.lock || scalesHeld, true, scalesHeld);
    ccLight[(size_t) g::cc::mute] = g::buttonCode (anyCh ("mute"), true, muteHeld);
    ccLight[(size_t) g::cc::solo] = g::buttonCode (anyCh ("solo"), true, soloHeld);
    ccLight[(size_t) g::cc::deleteButton] = g::buttonCode (deleteHeld);
    ccLight[(size_t) g::cc::device] = g::buttonCode (hooks.fxEditorShown && hooks.fxEditorShown());
    ccLight[(size_t) g::cc::browse] = g::buttonCode (browse);
    ccLight[(size_t) g::cc::track] = g::buttonCode (m == Mode::Mixer);
    ccLight[(size_t) g::cc::clip] = g::buttonCode (m == Mode::Tape);
    ccLight[(size_t) g::cc::volume] = g::buttonCode (encPage == EncoderPage::Volume);
    ccLight[(size_t) g::cc::panSend] = g::buttonCode (encPage == EncoderPage::PanSend);
    ccLight[(size_t) g::cc::accent] = g::buttonCode (accentNow.load());
    ccLight[(size_t) g::cc::quantize] = g::buttonCode (reg.get (pre + "quant") >= 0.5f);
    ccLight[(size_t) g::cc::duplicate] = g::buttonCode (false);
    ccLight[(size_t) g::cc::doubleButton] = g::buttonCode (false);
    ccLight[(size_t) g::cc::newButton] = g::buttonCode (false);
    ccLight[(size_t) g::cc::fixedLength] = g::buttonCode ((int) std::lround (reg.get ("seq.step.length")) != 16);
    ccLight[(size_t) g::cc::automation] = g::buttonCode (motionRec, true, motionRec && proc.tapeRolling());
    ccLight[(size_t) g::cc::addEffect] = g::buttonCode (false);
    ccLight[(size_t) g::cc::user] = g::buttonCode (hooks.breedingShown && hooks.breedingShown());
    // p67: CAPTURE - dim while the memory holds sound, lit + blinking while a capture is analysed
    ccLight[(size_t) g::cc::addTrack] = proc.tapeMemory().heldSeconds() > 0.05 || proc.tapeMemory().busy()
                                            ? g::buttonCode (proc.tapeMemory().busy(), true, proc.tapeMemory().busy())
                                            : 0;
    ccLight[(size_t) g::cc::stepIn] = 0;
    ccLight[(size_t) g::cc::stepOut] = 0;
    const bool arpOn = reg.get ("seq.arp.on") >= 0.5f;
    const int arpRate = (int) std::lround (reg.get ("seq.arp.rate"));
    for (int s = 0; s < g::cc::numScenes; ++s)
    {
        const bool heldNow = (scenesHeld & (1 << s)) != 0;
        const bool isRate = g::sceneRate (s) == arpRate;
        ccLight[(size_t) (g::cc::firstScene + s)] = g::colourCode (heldNow ? g::Colour::Green : isRate ? (arpOn ? g::Colour::Yellow : g::Colour::YellowDim)
                                                                                                    : g::Colour::AmberDim);
    }
    if (scalesHeld) // the upper row = layouts, In Key, fixed C, root, scale
    {
        const auto o = harmony::gridOptions (reg);
        for (int k = 0; k < 8; ++k)
        {
            g::Colour c = g::Colour::AmberDim;
            if (k < harmony::numLayouts) c = k == o.layout ? g::Colour::Yellow : g::Colour::YellowDim;
            else if (k == 4) c = o.inKey ? g::Colour::Green : g::Colour::GreenDim;
            else if (k == 5) c = o.fixed ? g::Colour::Green : g::Colour::GreenDim;
            ccLight[(size_t) (g::cc::upperRow + k)] = g::colourCode (c);
        }
    }
    else if (browse) // the upper row under the columns: lit = that filter is set; 1 = load
        for (int k = 0; k < 8; ++k)
            ccLight[(size_t) (g::cc::upperRow + k)] = g::colourCode (k == BrowseScroll ? g::Colour::Green
                                                                     : browseFilterActive (k) ? g::Colour::Yellow : g::Colour::YellowDim);
    if (muteHeld || soloHeld) // the part buttons show mute (red) / solo (amber)
        for (int k = 0; k < Core::numParts; ++k)
        {
            const bool on = reg.get ("mix.ch" + std::to_string (k + 1) + (muteHeld ? ".mute" : ".solo")) >= 0.5f;
            ccLight[(size_t) (g::cc::lowerRow + k)] = g::colourCode (muteHeld ? (on ? g::Colour::Red : g::Colour::RedDim) : (on ? g::Colour::Amber : g::Colour::AmberDim));
        }
    else if (browse) // part buttons 1 - 4 load into that part
        for (int k = 0; k < Core::numParts; ++k)
            ccLight[(size_t) (g::cc::lowerRow + k)] = g::colourCode (k == ctx.part ? g::Colour::Green : g::Colour::GreenDim);

    if (! isConnected) return; // p53: the virtual controller only
    for (int pad = 0; pad < g::numPads; ++pad)
        if (padLight[(size_t) pad] != sentPad[(size_t) pad])
        {
            sentPad[(size_t) pad] = padLight[(size_t) pad];
            send (juce::MidiMessage::noteOn (1, g::firstPadNote + pad, (juce::uint8) padLight[(size_t) pad]));
        }
    for (int cc = 0; cc < 128; ++cc)
        if (ccLight[(size_t) cc] >= 0 && ccLight[(size_t) cc] != sentCc[(size_t) cc])
        {
            sentCc[(size_t) cc] = ccLight[(size_t) cc];
            send (juce::MidiMessage::controllerEvent (1, cc, ccLight[(size_t) cc]));
        }
}

void GridController::resetOutputCache()
{
    sentLines.fill (std::string (1, '\x01')); // never equal to a real line: everything is sent again
    sentPad.fill (-1);
    sentCc.fill (-1);
    padLight.fill (0);
    ccLight.fill (-1);
}

void GridController::lightsOff()
{
    for (int pad = 0; pad < g::numPads; ++pad) send (juce::MidiMessage::noteOn (1, g::firstPadNote + pad, (juce::uint8) 0));
    for (int cc = 0; cc < 128; ++cc)
        if (sentCc[(size_t) cc] > 0) send (juce::MidiMessage::controllerEvent (1, cc, 0));
    for (int l = 0; l < g::numLines; ++l) sendSysex (g::clearLineSysex (l));
    resetOutputCache();
}
} // namespace ohpe::ui
