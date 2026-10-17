// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Pages.h"
#include "control/GridProtocol.h"
#include "plugin/PluginProcessor.h"
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
    p46: Grid-Controller (Profil 1) - an external 8 x 8 pad grid controller with a four-line display, standalone app only
    (Settings > MIDI > Controller: profile, MIDI input and output). Off by default; plug-in builds never create it.

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
class GridController : private juce::Timer, private juce::MidiInputCallback
{
public:
    enum class PadMode { Keys = 0, Pads, Drum, Steps, numModes };

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
    };
    using Sender = std::function<void (const juce::MidiMessage&)>;

    GridController (OhpeProcessor& p, Hooks hooks);
    ~GridController() override;

    // ---- ports (message thread)
    /** Opens / closes the ports the settings name (profile off = closed). Standalone app only. */
    void configure (const settings::UserSettings& s);
    bool connected() const noexcept { return isConnected; }
    bool enabled() const noexcept { return profile != settings::ControllerOff; }
    /** "Connected" / "Waiting for ..." (translated). */
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
    void addLog (const juce::String& s);
    gridctl::PadKey padKey() const;

    // pads (MIDI thread)
    void padDown (int pad, int velocity);
    void padUp (int pad);
    void padPressure (int pad, int value);
    void channelPressure (int value);

    OhpeProcessor& proc;
    Hooks hooks;
    int profile = settings::ControllerOff;
    juce::String inName, outName;
    int strip = 0;
    bool masterTempo = false;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    Sender testSender;
    bool isConnected = false;
    int watchTicks = 0;

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
