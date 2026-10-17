// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GridController.h"

#include "ModFxViews.h"
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
}

GridController::~GridController()
{
    stopTimer();
    closePorts();
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
    const bool changed = s.controllerProfile != profile || juce::String (s.controllerIn) != inName || juce::String (s.controllerOut) != outName;
    if (s.controllerMaster != (masterTempo ? 1 : 0) || changed) masterTempo = s.controllerMaster == 1;
    if (! changed) return;
    closePorts();
    profile = s.controllerProfile;
    inName = juce::String (s.controllerIn);
    outName = juce::String (s.controllerOut);
    if (enabled()) openPorts();
    if (enabled() || testSender) startTimerHz (25);
    else stopTimer();
}

void GridController::openPorts()
{
    if (testSender || ! enabled()) return;
    if (input == nullptr && inName.isNotEmpty())
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (d.name == inName)
            {
                // the controller's pads must not reach the parts twice (as a plain MIDI input too)
                if (auto* dm = proc.standaloneDevices; dm != nullptr && dm->isMidiInputDeviceEnabled (d.identifier))
                    dm->setMidiInputDeviceEnabled (d.identifier, false);
                input = juce::MidiInput::openDevice (d.identifier, this);
                if (input != nullptr) input->start();
                break;
            }
    if (output == nullptr && outName.isNotEmpty())
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            if (d.name == outName) { output = juce::MidiOutput::openDevice (d.identifier); break; }
    const bool now = input != nullptr && output != nullptr;
    if (now && ! isConnected)
    {
        isConnected = true;
        addLog ("connected");
        resetOutputCache();
        sendSysex (g::aftertouchModeSysex (true));
        update();
    }
}

void GridController::closePorts()
{
    for (int pad = 0; pad < g::numPads; ++pad) padUp (pad);
    if (isConnected) lightsOff();
    if (input != nullptr) input->stop();
    input = nullptr;
    output = nullptr;
    if (isConnected && ! testSender) addLog ("disconnected");
    isConnected = testSender != nullptr;
}

void GridController::attachForTests (Sender out)
{
    closePorts();
    testSender = std::move (out);
    isConnected = testSender != nullptr;
    resetOutputCache();
    if (isConnected) addLog ("virtual device");
    if (isConnected) sendSysex (g::aftertouchModeSysex (true));
}

juce::String GridController::status() const
{
    if (! enabled() && ! testSender) return tr ("Off");
    if (isConnected) return tr ("Connected");
    if (inName.isEmpty() || outName.isEmpty()) return tr ("Choose the controller's MIDI input and output");
    return tr ("Waiting for the controller");
}

void GridController::timerCallback()
{
    if (! testSender && enabled() && ++watchTicks >= 25) // once a second: has a port vanished or come back?
    {
        watchTicks = 0;
        const bool inThere = inputNames().contains (inName), outThere = outputNames().contains (outName);
        if (isConnected && (! inThere || ! outThere))
        {
            // the device is gone: drop the ports without writing to them
            for (int pad = 0; pad < g::numPads; ++pad) padUp (pad);
            if (input != nullptr) input->stop();
            input = nullptr;
            output = nullptr;
            isConnected = false;
            addLog ("disconnected");
        }
        else if (! isConnected && inThere && outThere)
            openPorts();
    }
    if (isConnected) update();
}

void GridController::send (const juce::MidiMessage& m)
{
    if (testSender) testSender (m);
    else if (output != nullptr) output->sendMessageNow (m);
}

void GridController::sendSysex (const std::vector<uint8_t>& bytes)
{
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
    if (! isConnected) return;
    refreshDisplay();
    refreshLights();
}

void GridController::handleQueued (const juce::MidiMessage& m)
{
    addLog ("> " + m.getDescription());
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

void GridController::refreshMap()
{
    auto& core = proc.core();
    const auto ctx = hooks.context ? hooks.context() : PageContext {};
    partNow.store (juce::jlimit (0, Core::numParts - 1, ctx.part));
    mpeNow.store (core.params().get ("p" + std::to_string (ctx.part + 1) + ".mpe") >= 0.5f);
    drumBase.store ((int) std::lround (core.params().get ("drum.base")));
    const int layout = juce::jlimit (0, (int) harmony::numLayouts - 1, (int) std::lround (core.params().get ("harmony.kb.layout")));
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
        values[(size_t) i] = g::shortName (str (value));
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
    page = juce::String (g::fold (str (page)).substr (0, (size_t) juce::jmax (1, g::lineLength / 4 - 1 - count.length()))) + " " + count;
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
        const int layout = juce::jlimit (0, (int) harmony::numLayouts - 1, (int) std::lround (reg.get ("harmony.kb.layout")));
        const auto ln = i18n::current() == i18n::Language::German ? harmony::layoutNamesDe() : harmony::layoutNamesEn();
        padText << " " << juce::String (ln[(size_t) layout]) << " O" << juce::String (hooks.octave ? hooks.octave() : 3);
    }
    lines[3] = g::segmentLine ({ str (page), str (tx), str (proc.tapeTimeText (secs).replace (juce::String::fromUTF8 (" \xc2\xb7 "), "  ")), str (padText) });

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
            if (note >= 0) c = g::roleColour (g::padRole (note, key, chordMask, down));
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
