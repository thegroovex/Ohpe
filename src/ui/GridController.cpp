// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GridController.h"

#include "ModFxViews.h"
#include "control/ControllerPorts.h"
#include "harmony/HarmonyBrain.h"
#include "perform/PerformancePads.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"
#include "seq/ScaleChord.h"

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
    const std::lock_guard<std::mutex> l (queueLock);
    if (queue.size() < 512) queue.push_back (m);
}

void GridController::padDown (int pad, int velocity)
{
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
                proc.core().stepSequencer().toggleStep (step, note >= 0 ? note : 60);
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
    if (cc == g::cc::shift) { shiftHeld = v > 0; return; }
    if (cc == g::cc::select) { selectHeld = v > 0; return; }
    if (v > 0) button (cc, true);
}

void GridController::button (int cc, bool)
{
    using TK = OhpeProcessor::TransportKey;
    auto transport = [this] (TK k) { if (hooks.transport) hooks.transport (k); };
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
            {
                setParam (Core::Ids::keyLock, proc.core().keyScale().lock ? 0.0f : 1.0f);
            }
            else if (hooks.gridShown && hooks.showGrid && ! hooks.gridShown())
                hooks.showGrid (true); // p53: first the grid keyboard on the screen (its layout on the pads), then the next layout
            else
                setParam ("harmony.kb.layout", (float) (((int) std::lround (proc.core().params().get ("harmony.kb.layout")) + 1) % harmony::numLayouts));
            break;
        case g::cc::master: masterTempo = ! masterTempo; break;
        case g::cc::note: setPadMode (PadMode::Keys); break;
        case g::cc::session: setPadMode (PadMode::Steps); break;
        default: break;
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
    const int octave = hooks.octave ? hooks.octave() : 3;
    const auto key = padKey();
    const auto mode = padMode();
    for (int pad = 0; pad < g::numPads; ++pad)
    {
        const int col = pad % g::cols, row = pad / g::cols;
        int note = -1;
        if (mode == PadMode::Keys || (mode == PadMode::Steps && row < g::rows - stepRows))
        {
            note = g::padLayoutNote (layout, col, row, octave, key);
            // Scale Lock on a chromatic layout: other notes are dark and silent (In Key has only key notes anyway)
            if (key.lock && layout != harmony::LayoutInKey && ! scales::inScale (note, key.root, key.scale)) note = -1;
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
    const juce::String count = juce::String (pg + 1) + "/" + juce::String (n);
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
    {
        const int layout = padLayout();
        const auto ln = i18n::current() == i18n::Language::German ? harmony::layoutNamesDe() : harmony::layoutNamesEn();
        padText << " " << juce::String (ln[(size_t) layout]) << " O" << juce::String (hooks.octave ? hooks.octave() : 3);
    }
    lines[3] = g::segmentLine ({ str (page), str (tx), str (proc.tapeTimeText (secs).replace (juce::String::fromUTF8 (" \xc2\xb7 "), "  ")), str (padText) });
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
