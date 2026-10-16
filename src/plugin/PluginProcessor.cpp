// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "ParameterBridge.h"
#include "TapeIO.h"
#include "ui/PluginEditor.h"

namespace ohpe
{
juce::AudioProcessor::BusesProperties OhpeProcessor::makeBuses()
{
    // aux outputs: on in the standalone app (multi-out to the audio interface), off by default in hosts
    const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int i = 1; i <= numAuxOutputs; ++i)
        b = b.withOutput ("Out " + juce::String (i), juce::AudioChannelSet::stereo(), standalone);
    // 0.2 (tape): audio input for recording, off until the host / user enables it
    b = b.withInput ("Input", juce::AudioChannelSet::stereo(), false);
    return b;
}

namespace
{
const char* const* triggerIds()
{
    static const char* const ids[] = { Core::Ids::libOpen, Core::Ids::libPrev, Core::Ids::libNext, Core::Ids::tapeLift,
                                       Core::Ids::tapeCut, Core::Ids::tapeDrop, Core::Ids::tapeSplice, Core::Ids::tapeUndo,
                                       Core::Ids::tapeRedo, Core::Ids::tapeRtz };
    return ids;
}
} // namespace

OhpeProcessor::OhpeProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "OhpeParameters", createParameterLayout (ohpeCore.params()))
{
    const auto& reg = ohpeCore.params();
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto id = juce::String (reg.descriptor (i).id);
        rawValues.push_back (apvts.getRawParameterValue (id));
        params.push_back (apvts.getParameter (id));
        jassert (rawValues.back() != nullptr);
    }
    lastPushed.reset (new std::atomic<float>[(size_t) reg.size()]);
    for (int i = 0; i < reg.size(); ++i) lastPushed[(size_t) i].store (NAN);
    midiScratch.reserve (2048);
    syncSourceParam = reg.indexOf (Core::Ids::syncSource);
    tempoParam = reg.indexOf (Core::Ids::tempo);
    uiState = juce::var (new juce::DynamicObject());
    // 0.3: remote controls (order = triggerIds below)
    for (int t = 0; t < numTriggers; ++t)
    {
        triggerParam[(size_t) t] = reg.indexOf (triggerIds()[t]);
        triggerLast[(size_t) t] = 0.0f;
        triggerPresses[(size_t) t].store (0);
    }
    presetManager = std::make_unique<PresetManager> (*this);
    startTimerHz (20);
}

OhpeProcessor::~OhpeProcessor() { stopTimer(); }

juce::RangedAudioParameter* OhpeProcessor::parameter (int i) const
{
    return i >= 0 && i < (int) params.size() ? params[(size_t) i] : nullptr;
}

void OhpeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    ohpeCore.prepare (sampleRate, juce::jmax (16, samplesPerBlock));
    setLatencySamples (ohpeCore.latencySamples()); // 0.8: limiter look-ahead, constant (on and off)
    scratch.setSize (2, juce::jmax (16, samplesPerBlock));
    inputScratch.setSize (2, juce::jmax (16, samplesPerBlock)); // 0.2: copy of the input bus
    keyboardState.reset();
}

bool OhpeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 0.2: one audio input bus (tape recording): disabled, mono or stereo
    if (layouts.inputBuses.size() > 1) return false;
    for (const auto& in : layouts.inputBuses)
        if (! in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (layouts.outputBuses.isEmpty() || layouts.outputBuses[0] != juce::AudioChannelSet::stereo())
        return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
        if (! layouts.outputBuses[i].isDisabled() && layouts.outputBuses[i] != juce::AudioChannelSet::stereo())
            return false;
    return true;
}

void OhpeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (scratch.getNumSamples() < n) // host broke its promise: stay safe (rare)
        scratch.setSize (2, n, false, false, true);

    // 1. host parameters -> registry (only changed values: MIDI-mapped values set inside the core are kept)
    auto& reg = ohpeCore.params();
    for (size_t i = 0; i < rawValues.size(); ++i)
    {
        const float v = rawValues[i]->load (std::memory_order_relaxed);
        if (! (v == lastPushed[i].load (std::memory_order_relaxed)))
        {
            lastPushed[i].store (v, std::memory_order_relaxed);
            reg.set ((int) i, v);
        }
    }

    // 2. MIDI (on-screen keyboard state merges into the buffer)
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    midiScratch.clear();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.getRawDataSize() > 3 || msg.isSysEx() || midiScratch.size() >= midiScratch.capacity()) continue;
        const auto* d = msg.getRawData();
        MidiEvent e;
        e.sampleOffset = juce::jlimit (0, n - 1, meta.samplePosition);
        e.status = d[0];
        e.data1 = msg.getRawDataSize() > 1 ? d[1] : 0;
        e.data2 = msg.getRawDataSize() > 2 ? d[2] : 0;
        midiScratch.push_back (e);
    }

    // 3. transport
    Transport tr;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) tr.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
            {
                tr.ppqAtBlockStart = *ppq;
                tr.hostDriven = true;
            }
            tr.playing = pos->getIsPlaying();
        }
    if (! tr.hostDriven)
        if (auto* ext = externalTransport.load (std::memory_order_acquire))
            if (syncSourceParam >= 0 && (int) std::lround (reg.get (syncSourceParam)) == Core::SyncLink)
                ext->fill (tr, n, getSampleRate(), reg.get (tempoParam));

    // 3b. 0.2: audio input (before the outputs overwrite the shared channels)
    captureInput (buffer, n);

    // 4. outputs: main + aux buses (disabled buses / missing device channels -> null = fall back to main)
    OutputBuffers out;
    const int deviceChannels = deviceOutputChannels.load (std::memory_order_relaxed);
    int channelBase = 0;
    for (int bus = 0; bus < numOutputPairs; ++bus)
    {
        if (bus >= getBusCount (false)) break;
        auto busBuffer = getBusBuffer (buffer, false, bus);
        const int chans = busBuffer.getNumChannels();
        const bool deviceHasIt = deviceChannels < 0 || channelBase + 2 <= deviceChannels || bus == 0;
        if (chans >= 2 && deviceHasIt)
        {
            out.left[(size_t) bus] = busBuffer.getWritePointer (0);
            out.right[(size_t) bus] = busBuffer.getWritePointer (1);
        }
        else if (chans == 1 && bus == 0)
        {
            out.left[0] = busBuffer.getWritePointer (0);
            out.right[0] = scratch.getWritePointer (0);
        }
        channelBase += chans;
    }
    if (out.left[0] == nullptr) { out.left[0] = scratch.getWritePointer (0); out.right[0] = scratch.getWritePointer (1); }

    ohpeCore.process (out, n, midiScratch.data(), (int) midiScratch.size(), tr);

    // clear channels the core did not write (disabled targets, extra channels)
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        bool written = false;
        for (int p = 0; p < numOutputPairs; ++p)
            written |= (buffer.getWritePointer (ch) == out.left[(size_t) p] || buffer.getWritePointer (ch) == out.right[(size_t) p]);
        if (! written) buffer.clear (ch, 0, n);
    }

    // 5. values changed by MIDI mapping -> host (keeps automation lanes and the UI in sync)
    ParamChange change;
    while (ohpeCore.midiMapper().popChange (change))
        if (auto* p = parameter (change.paramIndex))
        {
            if (change.normValue >= 0.5f) // 0.3: a MIDI press on a remote trigger
                for (int t = 0; t < numTriggers; ++t)
                    if (triggerParam[(size_t) t] == change.paramIndex) triggerPresses[(size_t) t].fetch_add (1);
            lastPushed[(size_t) change.paramIndex].store (reg.descriptor (change.paramIndex).fromNormalised (change.normValue), std::memory_order_relaxed);
            p->setValueNotifyingHost (change.normValue);
        }
    midi.clear();

    // 6. standalone: MIDI out (clock, sequencer notes) - the standalone player sends this buffer
    //    to the selected MIDI output after processBlock
    if (wrapperType == wrapperType_Standalone)
        for (const auto& e : ohpeCore.midiOutput())
        {
            if (e.status == midiclock::songPosition) midi.addEvent (juce::MidiMessage (e.status, e.data1, e.data2), e.sampleOffset);
            else if (e.status >= 0xf0) midi.addEvent (juce::MidiMessage (e.status), e.sampleOffset);
            else midi.addEvent (juce::MidiMessage (e.status, e.data1, e.data2), e.sampleOffset);
        }
}

// ---------------------------------------------------------------- MIDI learn
void OhpeProcessor::captureInput (juce::AudioBuffer<float>& buffer, int n)
{
    const float* inL = nullptr;
    const float* inR = nullptr;
    if (getBusCount (true) > 0 && getBus (true, 0)->isEnabled() && inputScratch.getNumSamples() >= n)
    {
        const auto in = getBusBuffer (buffer, true, 0);
        const int chans = juce::jmin (2, in.getNumChannels());
        for (int c = 0; c < chans; ++c) inputScratch.copyFrom (c, 0, in, c, 0, n);
        if (chans == 1) inputScratch.copyFrom (1, 0, in, 0, 0, n);
        if (chans > 0)
        {
            inL = inputScratch.getReadPointer (0);
            inR = inputScratch.getReadPointer (1);
        }
    }
    ohpeCore.setAudioInput (inL, inR);
}

void OhpeProcessor::timerCallback()
{
    ohpeCore.tape().housekeeping(); // 0.2: block pool for recording, finished passes -> undo steps
    if (ohpeCore.midiMapper().applyPendingLearns (ohpeCore.midiMap()))
        sendChangeMessage();
    samples.timerTick();
    pollRemoteControls();
    if (ohpeCore.effectsWaiting()) ohpeCore.prepareEffects(); // 0.7: FX rack slot types chosen by the host / a preset
}

void OhpeProcessor::pollRemoteControls()
{
    const auto& reg = ohpeCore.params();
    for (int t = 0; t < numTriggers; ++t)
    {
        const int idx = triggerParam[(size_t) t];
        if (idx < 0) continue;
        // host / UI value (not yet seen by an audio block when the host is stopped)
        const float v = rawValues[(size_t) idx]->load (std::memory_order_relaxed);
        int presses = triggerPresses[(size_t) t].exchange (0);
        if (presses == 0 && v >= 0.5f && triggerLast[(size_t) t] < 0.5f) presses = 1; // rising edge
        triggerLast[(size_t) t] = juce::jmax (v, reg.get (idx));
        if (presses > 0) triggerLast[(size_t) t] = 1.0f;
        for (int k = 0; k < juce::jmin (presses, 8); ++k)
        {
            const juce::String id (triggerIds()[t]);
            if (id == Core::Ids::libOpen) libraryToggles.fetch_add (1);
            else if (id == Core::Ids::libPrev) presetManager->step (-1);
            else if (id == Core::Ids::libNext) presetManager->step (1);
            else runTapeEdit (triggerIds()[t]);
        }
    }
}

bool OhpeProcessor::runTapeEdit (const char* triggerId)
{
    auto& reg = ohpeCore.params();
    auto& tape = ohpeCore.tape();
    const int track = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (reg.get (Core::Ids::tapeEditTrack)) - 1);
    const double in = reg.get ("tape.loopin"), out = reg.get ("tape.loopout");
    const double at = tape.sampleRate() > 0.0 ? (double) tape.positionFrames() / tape.sampleRate() : 0.0;
    const juce::String id (triggerId);
    bool ok = false;
    if (id == Core::Ids::tapeLift) ok = tape.lift (track, juce::jmin (in, out), juce::jmax (in, out));
    else if (id == Core::Ids::tapeCut) ok = tape.cut (track, juce::jmin (in, out), juce::jmax (in, out));
    else if (id == Core::Ids::tapeDrop) ok = tape.drop (track, at);
    else if (id == Core::Ids::tapeSplice) ok = tape.splice (track, at);
    else if (id == Core::Ids::tapeUndo) ok = tape.undo();
    else if (id == Core::Ids::tapeRedo) ok = tape.redo();
    else if (id == Core::Ids::tapeRtz) { tape.locate (0.0); ok = true; }
    sendChangeMessage();
    return ok;
}

void OhpeProcessor::editRegistry (const std::function<void (Core&)>& fn)
{
    pullHostParametersIntoRegistry();
    fn (ohpeCore);
    syncHostParametersFromRegistry();
}

void OhpeProcessor::armMidiLearn (const juce::String& paramId)
{
    ohpeCore.midiMapper().armLearn (ohpeCore.params().indexOf (paramId.toStdString()));
    sendChangeMessage();
}

void OhpeProcessor::cancelMidiLearn()
{
    ohpeCore.midiMapper().cancelLearn();
    sendChangeMessage();
}

juce::String OhpeProcessor::learningParamId() const
{
    const int i = ohpeCore.midiMapper().learningParam();
    return i >= 0 ? juce::String (ohpeCore.params().descriptor (i).id) : juce::String();
}

void OhpeProcessor::clearMidiMapping (const juce::String& paramId)
{
    ohpeCore.midiMap().clearParam (paramId.toStdString());
    ohpeCore.midiMapChanged();
    sendChangeMessage();
}

void OhpeProcessor::clearAllMidiMappings()
{
    ohpeCore.midiMap().clear();
    ohpeCore.midiMapChanged();
    sendChangeMessage();
}

juce::String OhpeProcessor::describeMidiMapping (const juce::String& paramId) const
{
    juce::StringArray parts;
    for (const auto& m : ohpeCore.midiMap().forParam (paramId.toStdString()))
    {
        juce::String s = m.type == MidiSourceType::CC ? "CC " + juce::String (m.number)
                                                      : juce::MidiMessage::getMidiNoteName (m.number, true, true, 3);
        s << " / " << (m.channel == 0 ? juce::String ("Omni") : "Ch " + juce::String (m.channel));
        parts.add (s);
    }
    return parts.joinIntoString (", ");
}

bool OhpeProcessor::exportMidiMap (const juce::File& file) const
{
    return file.replaceWithText (juce::String::fromUTF8 (ohpeCore.midiMap().toJson().c_str()));
}

bool OhpeProcessor::importMidiMap (const juce::File& file)
{
    if (! ohpeCore.midiMap().fromJson (file.loadFileAsString().toStdString()))
        return false;
    ohpeCore.midiMapChanged();
    sendChangeMessage();
    return true;
}

// ---------------------------------------------------------------- state
void OhpeProcessor::pullHostParametersIntoRegistry()
{
    // host changes not yet seen by an audio block (e.g. set while the plug-in is not playing)
    auto& reg = ohpeCore.params();
    for (size_t i = 0; i < rawValues.size(); ++i)
    {
        const float v = rawValues[i]->load (std::memory_order_relaxed);
        if (! (v == lastPushed[i].load (std::memory_order_relaxed)))
            reg.set ((int) i, v);
    }
}

void OhpeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    pullHostParametersIntoRegistry();
    samples.storeInState();
    const auto ui = juce::JSON::toString (uiState, true);
    const auto json = ohpeCore.saveState (ui.toStdString());
    dest.replaceAll (json.data(), json.size());
    tapeio::appendTapeAudio (ohpeCore, dest); // 0.2: tape audio after the JSON (see TapeIO.h)
}

void OhpeProcessor::setStateInformation (const void* data, int size)
{
    std::string json, ui;
    const uint8_t* tape = nullptr;
    size_t tapeSize = 0;
    tapeio::splitState (data, size, json, tape, tapeSize); // 0.2: JSON + optional tape audio
    if (! ohpeCore.loadState (json, &ui))
        return;
    tapeio::restoreTape (ohpeCore, tape, tapeSize);
    samples.restoreFromState();
    auto parsed = juce::JSON::parse (juce::String::fromUTF8 (ui.c_str()));
    uiState = parsed.isObject() ? parsed : juce::var (new juce::DynamicObject());
    syncHostParametersFromRegistry();
    sendChangeMessage();
}

void OhpeProcessor::syncHostParametersFromRegistry()
{
    const auto& reg = ohpeCore.params();
    for (int i = 0; i < reg.size(); ++i)
        if (auto* p = params[(size_t) i])
        {
            const float norm = reg.getNormalised (i);
            if (std::abs (p->getValue() - norm) > 1.0e-6f)
                p->setValueNotifyingHost (norm);
            lastPushed[(size_t) i].store (NAN, std::memory_order_relaxed); // re-sync on the next block
        }
}

juce::AudioProcessorEditor* OhpeProcessor::createEditor() { return new OhpeEditor (*this); }
} // namespace ohpe

#ifndef OHPE_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ohpe::OhpeProcessor(); }
#endif
