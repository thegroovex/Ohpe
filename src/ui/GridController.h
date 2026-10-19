// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Pages.h"
#include "control/GridProtocol.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UserSettingsStore.h"
#include "settings/UserSettings.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace ohpe::ui
{
/**
    p46: Grid-Controller (Profil 1) - an external 8 x 8 pad grid controller with a four-line display (Settings > MIDI >
    Controller: profile, MIDI input and output). Off by default.

    p49: in a plug-in it opens the controller's (user) ports itself, bypassing the host: the ports chosen, or found by
    name ("Choose ports automatically", settings "auto" / "pattern"). Only one instance of the process holds them
    (gridctl::Ownership: the window opened or clicked last, or "Take over here"); the others leave the ports closed.
    The object belongs to the editor while it is open; when the window closes it is parked in the processor with
    hooks of its own (mode, page, part, octave, transport), so the controller keeps working until the instance goes.
    A port that cannot be opened (held by the host) is tried again every second.

    * Display: line 1 / 2 = names / values of the eight encoders (the shown Ohpe page and the next one, as Pages.cpp puts
      them on the four screen encoders), line 3 = preset, part + engine, tempo, key (a touched encoder shows its full
      name and value there), line 4 = page, transport, tape position, pad mode.
    * Encoders 1-8 edit those parameters (Select held = fine); the master encoder sets the volume or the tempo (Master
      button toggles); the tempo encoder sets the tempo.
    * Pads, four modes (second button row, keys 5-8):
      Keys - the on-screen grid keyboard on 8 x 8 pads: the same layout ("harmony.kb.layout": Fourths / Thirds / Whole
             Tones / In Key), the same note of the same column and row from the same lowest C (the keyboard's octave),
             the key from the key / harmony memory (Scale Lock: other notes stay dark); colours: root amber, key notes
             yellow, chord tones green dim, held pads green. Velocity and pressure play the edit part (MPE part: one
             member channel per pad, pressure = channel pressure; else poly aftertouch on the part's channel).
      Pads - the four performance pads (stutter, tape stop, sweep, freeze), one 4 x 4 quarter each, held = on.
      Drum - the 24 drum pads on the three bottom rows.
      Steps - the 16 steps of the step sequencer on the two top rows, the keys layout on the six rows below.
    * Buttons: first row under the display = the eight mode keys; second row 1-4 = parts, 5-8 = pad modes; arrows
      left / right = page, up / down = preset; octave - / +; play / record / stop (Shift: tape play / arm / to start),
      repeat = loop, metronome (Shift: count-in), tap tempo, undo (Shift: redo), scales = next grid layout (Shift:
      Scale Lock).
    * Touch strip: pitch bend (springs back on release) or the mod wheel (settings).
    * Lights follow the state; on (re)connect everything is sent again; ports that vanish are watched and reopened.

    Threads: receive() may run on the MIDI input thread (pads go straight to the core's controller queue, everything
    else is queued); update() runs on the message thread (the timer, or tests).
*/
class GridController : private juce::Timer, private juce::MidiInputCallback, private juce::ChangeListener
{
public:
    enum class PadMode { Keys = 0, Pads, Drum, Steps, numModes };
    /** p49: what the settings section shows. */
    enum class State { Off = 0, ChoosePorts, Waiting, PortBusy, Connected, OtherInstance, Probing, NotFound }; ///< p53: + Probing, NotFound

    /** The editor's side: what the screen shows and how to change it. */
    struct Hooks
    {
        std::function<Mode()> mode;
        std::function<PageContext()> context;
        std::function<int()> page;
        std::function<void (int)> stepPage;
        std::function<void (Mode)> selectMode;
        std::function<void (int)> selectPart;
        std::function<void (OhpeProcessor::TransportKey)> transport;
        std::function<int()> octave;
        std::function<void (int)> setOctave;
        std::function<bool()> gridShown;       ///< p53: the on-screen grid keyboard is shown (else the piano keys)
        std::function<void (bool)> showGrid;   ///< p53: shows the grid keyboard (the controller's Scales button)
        std::function<bool (int)> screenPressed; ///< p53: the note is held on the on-screen keyboard (lit on the pads too)
    };
    using Sender = std::function<void (const juce::MidiMessage&)>;

    GridController (OhpeProcessor& p, Hooks hooks);
    ~GridController() override;

    /** p49: the editor's hooks again (window reopened). */
    void setHooks (Hooks h) { hooks = std::move (h); isHeadless = false; }
    /** p49: the window closes - from now on its own hooks (the last mode, page, part and octave; transport and part
        through the processor). */
    void detachFromEditor();
    bool headless() const noexcept { return isHeadless; }

    // ---- p49: plug-in (one instance of the process holds the ports)
    bool pluginMode() const noexcept { return inPlugin; }
    void setPluginModeForTests (bool on);
    /** This instance takes the controller (window opened / clicked, "Take over here"). */
    void claim();
    bool ownedHere() const;
    bool ownedElsewhere() const;
    State state() const;
    /** The ports used now (chosen, or found by name). */
    juce::String inputPort() const { return inName; }
    juce::String outputPort() const { return outName; }
    /** One tick of the watch (the timer; tests): ownership hand-over, ports. */
    void poll();
    /** Screenshots: the port names shown (no port is opened). */
    void setPortNamesForTests (const juce::String& in, const juce::String& out) { inName = in; outName = out; }

    // ---- ports (message thread)
    /** Opens / closes the ports the settings name (profile off = closed). Standalone app only. */
    void configure (const settings::UserSettings& s);
    bool connected() const noexcept { return isConnected; }
    /** p53: the generation the controller answered with (First until it answers; Unknown while not connected). */
    gridctl::Generation generation() const noexcept { return gen; }
    /** p53: the connect splash is on the display now. */
    bool splashShown() const noexcept { return splashTicks > 0; }
    /** p53: the ports are open and wait for the identity reply (tests: a reply through receive()). */
    bool probing() const noexcept { return isProbing; }
    /** p53 (tests): the next connect of attachForTests shows the splash (as a real port does). */
    void setSplashForTests (bool on) { testSplash = on; }
    /** p53: the settings' line under the ports: "Connected - first generation", "No controller found", ... */
    juce::String statusLine() const;
    bool enabled() const noexcept { return profile != settings::ControllerOff; }
    /** "Connected" / "Waiting for ..." / p49 "Active in this instance", "Port in use - free it in the host" ... (translated). */
    juce::String status() const;
    /** Port names of the system (inputs / outputs). */
    static juce::StringArray inputNames();
    static juce::StringArray outputNames();

    // ---- tests / the virtual controller view
    /** Connects to `out` instead of a port (a mock device); null disconnects. */
    void attachForTests (Sender out);
    /** A message from the controller (any thread). */
    void receive (const juce::MidiMessage& m);
    /** Message thread: handles queued input, then refreshes display and lights (only changes are sent). */
    void update();
    std::array<std::string, gridctl::numLines> displayLines() const { return lines; }
    std::array<int, gridctl::numPads> padCodes() const { return padLight; }
    int buttonCode (int cc) const noexcept { return cc >= 0 && cc < 128 ? ccLight[(size_t) cc] : -1; }
    /** The note pad 0 .. 63 plays in Keys mode now (-1 = none). */
    int padNoteNow (int pad) const noexcept;
    /** p53: the note is held on a pad (Keys mode) - the on-screen grid keyboard shows it pressed. */
    bool padNoteDown (int note) const noexcept;
    /** The parameter IDs on the eight encoders (empty = free). */
    std::array<std::string, gridctl::numCells> encoderIds() const { return encIds; }
    PadMode padMode() const noexcept { return (PadMode) padModeNow.load(); }
    void setPadMode (PadMode m);
    bool masterIsTempo() const noexcept { return masterTempo; }
    int stripMode() const noexcept { return strip; }
    /** The last messages in and out ("> ..." in, "< ..." out), newest last (virtual controller view). */
    juce::StringArray log() const;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override; ///< p49: the settings changed (any window)
    void checkOwnership();
    void watchPorts();
    void resolvePorts();
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override { receive (m); }

    void send (const juce::MidiMessage& m);
    void sendSysex (const std::vector<uint8_t>& bytes);
    void handleQueued (const juce::MidiMessage& m);
    void button (int cc, bool down);
    void turn (int encoder, int delta);
    void setParam (const juce::String& id, float plain);
    void refreshMap();
    void refreshDisplay();
    void refreshLights();
    void resetOutputCache();
    void lightsOff();
    void openPorts();
    void closePorts();
    void onConnected (bool splash); ///< p53: user mode, aftertouch mode, splash, everything sent again
    void claimDeviceInputs();       ///< p53: the controller's ports (and its live port) off as plain MIDI inputs
    void releaseDeviceInputs();
    void rejectProbe();             ///< p53: no identity reply - not the controller, try the next candidate
    void addLog (const juce::String& s);
    gridctl::PadKey padKey() const;
    /** p53: the pads' layout: the grid keyboard's ("harmony.kb.layout") while it is shown on the screen, else Fourths. */
    int padLayout() const;

    // pads (MIDI thread)
    void padDown (int pad, int velocity);
    void padUp (int pad);
    void padPressure (int pad, int value);
    void channelPressure (int value);

    OhpeProcessor& proc;
    Hooks hooks;
    int profile = settings::ControllerOff;
    juce::String inName, outName;            ///< the ports used (p49: found by name when automatic)
    juce::String wantIn, wantOut, pattern;   ///< p49: the settings' ports and name pattern
    bool autoPorts = false, inPlugin = false, wasOwner = false, portBusy = false, isHeadless = false;
    struct Detached { Mode mode = Mode::Synth; PageContext ctx; int page = 0; int octave = 3; bool grid = false; } detached; ///< p49 (p53: grid)
    juce::SharedResourcePointer<UserSettingsStore> store; ///< p49
    int strip = 0;
    bool masterTempo = false;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    Sender testSender;
    bool isConnected = false;
    int watchTicks = 0;
    // p53: generation, identity probe, splash, the device manager's inputs taken over, candidates that did not answer
    gridctl::Generation gen = gridctl::Generation::Unknown;
    bool isProbing = false, chosenByUser = false, testSplash = false, notFound = false;
    int probeTicks = 0, splashTicks = 0, idleTicks = 0;
    juce::StringArray rejected, disabledInputs;
    juce::String portListSig;

    // input queue (MIDI thread -> message thread)
    std::mutex queueLock;
    std::vector<juce::MidiMessage> queue;

    // pads (shared with the MIDI thread)
    std::array<std::atomic<int>, gridctl::numPads> padMap {}; ///< note per pad (Keys / Steps rows), -1 = none
    std::array<std::atomic<int>, gridctl::numPads> playing {}; ///< note sounding from the pad, -1 = none
    std::array<std::atomic<int>, gridctl::numPads> playingCh {};
    std::array<std::atomic<bool>, gridctl::numPads> held {};
    std::atomic<int> padModeNow { 0 }, partNow { 0 }, drumBase { 36 };
    std::atomic<bool> mpeNow { false };
    int nextChannel = 2;

    // state (message thread)
    bool shiftHeld = false, selectHeld = false;
    int touched = -1;
    std::array<std::string, gridctl::numCells> encIds {};
    std::array<int, gridctl::numCells> encAccum {};
    std::array<std::string, gridctl::numLines> lines {}, sentLines {};
    std::array<int, gridctl::numPads> padLight {}, sentPad {};
    std::array<int, 128> ccLight {}, sentCc {};
    mutable std::mutex logLock;
    juce::StringArray logLines;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridController)
};
} // namespace ohpe::ui
