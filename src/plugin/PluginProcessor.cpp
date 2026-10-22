// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "ParameterBridge.h"
#include "Localisation.h"
#include "TapeIO.h"
#include "model/StateSerializer.h"
#include "tape/TapeFile.h"
#include "ui/PluginEditor.h"

#include <chrono>
#include <memory>
#include <utility>

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
                                       Core::Ids::tapeRedo, Core::Ids::tapeRtz,
                                       // p11: tape transport
                                       Core::Ids::tapeTxPlay, Core::Ids::tapeTxTapePlay, Core::Ids::tapeTxStop, Core::Ids::tapeTxRec, Core::Ids::tapeTxLoop,
                                       Core::Ids::tapeTxArm,
                                       // p18: preset breeding
                                       Core::Ids::libBreed, Core::Ids::libBreedGo, Core::Ids::libBreedNext,
                                       // p23: bounce to engine
                                       Core::Ids::bounceGo,
                                       // p24: harmony brain
                                       Core::Ids::harmonyTake, Core::Ids::harmonyClear,
                                       // p38: clear the edit track / the whole tape
                                       Core::Ids::tapeClearTrack, Core::Ids::tapeClearAll,
                                       // p42: the tape loop by hand
                                       Core::Ids::tapeLoopSetIn, Core::Ids::tapeLoopSetOut, Core::Ids::tapeLoopUndo, Core::Ids::tapeLoopRedo,
                                       Core::Ids::tapeLoopHalf, Core::Ids::tapeLoopDouble,
                                       // p67: tape memory
                                       Core::Ids::tapememGo,
                                       // p68: Voice Control quick map per part
                                       "p1.voice.map", "p2.voice.map", "p3.voice.map", "p4.voice.map" };
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
    // p35: changed host values are marked by a listener per parameter (the first block copies every value)
    hostChangedWords = (reg.size() + 63) / 64;
    hostChanged.reset (new std::atomic<uint64_t>[(size_t) juce::jmax (1, hostChangedWords)]);
    for (int w = 0; w < hostChangedWords; ++w) hostChanged[(size_t) w].store (~uint64_t (0));
    hostListeners.reserve ((size_t) reg.size());
    for (int i = 0; i < reg.size(); ++i)
    {
        auto l = std::make_unique<HostParamListener>();
        l->owner = this;
        l->index = i;
        apvts.addParameterListener (juce::String (reg.descriptor (i).id), l.get());
        hostListeners.push_back (std::move (l));
    }
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
    tasteStore = std::make_unique<TasteStore> (*this); // p69
    // p40: start with the first factory preset, not with the plain defaults ("Init"); host state / a stored session replace it
    if (startWithFactorySound.load()) presetManager->loadStartSound();
    presetUidCache = presetManager->currentUids();
    // p20: a finished import switches the part's user wavetable / user formants on (host automation follows)
    userTableManager.onPublished = [this] (int part, UserTableManager::Target target) {
        const auto id = "p" + juce::String (part + 1) + (target == UserTableManager::Target::Vocal ? ".usertable.vocal" : ".usertable.wt");
        if (auto* prm = parameter (id)) prm->setValueNotifyingHost (1.0f);
        sendChangeMessage();
    };
    // p23: a bounce switches the part's engine and its sample settings like the UI (host automation follows)
    bounceManager.setParam = [this] (const std::string& id, float plain) { setPlainFromUi (id.c_str(), plain); };
    tapeMemoryManager.setParam = bounceManager.setParam; // p67
    syncSpaceModes(); // p41: the 3D switches start from the current values (the link remembers them)
    startTimerHz (20);
}

OhpeProcessor::~OhpeProcessor()
{
    parkedGridController.reset(); // p49: before anything it refers to
    stopTimer();
    for (const auto& l : hostListeners) apvts.removeParameterListener (juce::String (ohpeCore.params().descriptor (l->index).id), l.get());
}

juce::RangedAudioParameter* OhpeProcessor::parameter (int i) const
{
    return i >= 0 && i < (int) params.size() ? params[(size_t) i] : nullptr;
}

void OhpeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    ohpeCore.prepare (sampleRate, juce::jmax (16, samplesPerBlock));
    ohpeCore.setStandaloneHost (wrapperType == wrapperType_Standalone); // p39: the click in a plug-in is opt-in
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

void OhpeProcessor::pushHostParameter (size_t i) noexcept
{
    const float v = rawValues[i]->load (std::memory_order_relaxed);
    if (! (v == lastPushed[i].load (std::memory_order_relaxed)))
    {
        lastPushed[i].store (v, std::memory_order_relaxed);
        ohpeCore.params().set ((int) i, v);
    }
}

void OhpeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (scratch.getNumSamples() < n) // host broke its promise: stay safe (rare)
        scratch.setSize (2, n, false, false, true);

    const auto startTime = std::chrono::steady_clock::now(); // p35: the CPU meter

    // 1. host parameters -> registry (only changed values: MIDI-mapped values set inside the core are kept).
    //    p35: only the parameters the listener marked, plus one word of 64 per block re-checked as a sweep
    auto& reg = ohpeCore.params();
    for (int w = 0; w < hostChangedWords; ++w)
    {
        uint64_t bits = hostChanged[(size_t) w].exchange (0, std::memory_order_acquire);
        if (w == sweepWord) bits = ~uint64_t (0);
        while (bits != 0)
        {
            const auto i = (size_t) (w * 64 + ParamSnapshot::lowestBit (bits));
            bits &= bits - 1;
            if (i < rawValues.size()) pushHostParameter (i);
        }
    }
    if (++sweepWord >= hostChangedWords) sweepWord = 0;

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
    hostTempo.store (tr.hostDriven && wrapperType != wrapperType_Standalone, std::memory_order_relaxed); // p39
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

    // p35: the CPU meter - this block's processing time against the time the block stands for
    if (const double rate = getSampleRate(); rate > 0.0 && n > 0)
        cpuMeter.add (std::chrono::duration<double> (std::chrono::steady_clock::now() - startTime).count(), (double) n / rate);
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
    userTableManager.capture().push (inL, inR, n); // p20: recording the live input for the wavetable analysis
    bounceManager.pushInput (inL, inR, n);         // p23: recording the live input for a bounce
}

void OhpeProcessor::timerCallback()
{
    ohpeCore.tape().housekeeping(); // 0.2: block pool for recording, finished passes -> undo steps
    if (const int lat = ohpeCore.latencySamples(); lat != getLatencySamples()) setLatencySamples (lat); // p66: Key Gravity latency
    if (ohpeCore.midiMapper().applyPendingLearns (ohpeCore.midiMap()))
        sendChangeMessage();
    samples.timerTick();
    userTableManager.timerTick(); // p20
    bounceManager.timerTick();    // p23
    tapeMemoryManager.timerTick(); // p67: a finished capture analysis goes into its part
    pollRemoteControls();
    watchTapeLoop (juce::Time::getMillisecondCounterHiRes() * 0.001); // p42: loop range rules, loop undo steps
    applyPendingPresetUid(); // p40
    tasteStore->tick (juce::Time::getMillisecondCounterHiRes() * 0.001); // p69: learning breed signals
    {
        auto uids = presetManager->currentUids();
        const juce::SpinLock::ScopedLockType sl (presetUidLock);
        presetUidCache = std::move (uids);
    }
    if (ohpeCore.effectsWaiting()) ohpeCore.prepareEffects(); // 0.7: FX rack slot types chosen by the host / a preset
    ohpeCore.updateMorph(); // p16: morph plans follow the part's own sound under empty corners
    ohpeCore.updateTapeGrain(); // p28: Grain "Source = Tape" granulates the edit track / the tape mix
}

void OhpeProcessor::syncSpaceModes()
{
    const auto& reg = ohpeCore.params();
    // p41: the 3D switch (pN.space.mode) and the values it stands for (pN.space.on, space.out) in step - read from the
    // host side (a host / MIDI / encoder move is there at once), written with a gesture so automation follows
    spaceModeLink.update (
        Core::numParts,
        [this, &reg] (const std::string& id) {
            const int i = reg.indexOf (id);
            return i >= 0 && i < (int) rawValues.size() ? rawValues[(size_t) i]->load (std::memory_order_relaxed) : 0.0f;
        },
        [this] (const std::string& id, float plain) { setPlainFromUi (id.c_str(), plain); });
}

void OhpeProcessor::pollRemoteControls()
{
    const auto& reg = ohpeCore.params();
    syncSpaceModes(); // p41
    // p39: "metro.tap" - a rising edge (MIDI button, host, UI) is one tap
    if (metroTapParam < 0) metroTapParam = reg.indexOf (Metronome::Ids::tap);
    if (metroTapParam >= 0)
    {
        const float v = juce::jmax (rawValues[(size_t) metroTapParam]->load (std::memory_order_relaxed), reg.get (metroTapParam));
        if (v >= 0.5f && metroTapLast < 0.5f) tapTempo();
        metroTapLast = v;
    }
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
            else if (id == Core::Ids::libBreed) breedRequests[0].fetch_add (1); // p18: the editor polls these
            else if (id == Core::Ids::libBreedGo) breedRequests[1].fetch_add (1);
            else if (id == Core::Ids::libBreedNext) breedRequests[2].fetch_add (1);
            else if (id == Core::Ids::bounceGo) bounceManager.bounce(); // p23: the edit part, the bounce settings
            else if (id == Core::Ids::tapememGo) tapeMemoryManager.capture(); // p67: the tape memory settings
            else if (id == Core::Ids::harmonyTake) takeHarmonyKey();    // p24
            else if (id == Core::Ids::harmonyClear) ohpeCore.harmony().requestClear(); // p24
            else if (id == Core::Ids::tapeClearAll) tapeClearAllRequests.fetch_add (1); // p38: the editor asks first
            else if (id.endsWith (".voice.map")) // p68: quick map; the display says what happened
            {
                const int part = id.substring (1, 2).getIntValue() - 1;
                const int placed = applyVoiceQuickMap (part);
                {
                    const juce::ScopedLock sl (transportLock);
                    transportMessage = placed > 0 ? i18n::pick ("Voice: vowel \xe2\x86\x92 filter + morph", "Stimme: Vokal \xe2\x86\x92 Filter + Morph")
                                                  : i18n::pick ("Voice: no free routing", "Stimme: keine freie Zuweisung");
                }
                transportMessages.fetch_add (1);
            }
            else if (id.startsWith ("tape.edit.loop")) // p42: the loop by hand; the display shows what happened
            {
                const auto msg = tapeLoopAction (id);
                {
                    const juce::ScopedLock sl (transportLock);
                    transportMessage = msg;
                }
                transportMessages.fetch_add (1);
            }
            else if (id.startsWith ("tape.tx."))
            {
                const auto action = id == Core::Ids::tapeTxPlay ? TransportKey::PlayStop
                                  : id == Core::Ids::tapeTxTapePlay ? TransportKey::TapePlayStop
                                  : id == Core::Ids::tapeTxStop ? TransportKey::Stop
                                  : id == Core::Ids::tapeTxRec  ? TransportKey::Record
                                  : id == Core::Ids::tapeTxLoop ? TransportKey::Loop
                                                                : TransportKey::Arm;
                const auto msg = runTransport (action);
                {
                    const juce::ScopedLock sl (transportLock);
                    transportMessage = msg;
                }
                transportMessages.fetch_add (1);
            }
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
    else if (id == Core::Ids::tapeClearTrack) ok = tape.clearTrack (track); // p38
    else if (id == Core::Ids::tapeClearAll) ok = tape.clearAll();           // p38: after the confirmation
    sendChangeMessage();
    return ok;
}

// ---------------------------------------------------------------- p39: tempo, tap tempo
bool OhpeProcessor::tempoFollowsHost() const noexcept
{
    if (hostTempoActive()) return true;
    return syncSourceParam >= 0 && (int) std::lround (ohpeCore.params().get (syncSourceParam)) == Core::SyncMidiClock;
}

float OhpeProcessor::currentTempo() const noexcept
{
    return tempoFollowsHost() ? (float) ohpeCore.transportBpm() : ohpeCore.params().get (Core::Ids::tempo);
}

void OhpeProcessor::setTempoFromUi (float bpm)
{
    if (tempoFollowsHost() || ! std::isfinite (bpm)) return;
    setPlainFromUi (Core::Ids::tempo, juce::jlimit (20.0f, 300.0f, std::round (bpm * 10.0f) / 10.0f));
}

void OhpeProcessor::tapTempo()
{
    double bpm = 0.0;
    {
        const juce::SpinLock::ScopedLockType sl (tapLock);
        bpm = tapper.tap (juce::Time::getMillisecondCounterHiRes() * 0.001);
    }
    taps.fetch_add (1, std::memory_order_relaxed);
    if (bpm > 0.0) setTempoFromUi ((float) bpm);
}

// ---------------------------------------------------------------- p42: the tape loop by hand
double OhpeProcessor::tapeLengthSeconds() const { return juce::jmax (1.0, const_cast<Core&> (ohpeCore).tape().lengthSeconds()); }
double OhpeProcessor::tapeBpm() const { return juce::jmax (20.0, (double) ohpeCore.params().get (Core::Ids::tempo)); }

tapeloop::Range OhpeProcessor::tapeLoopRange() const
{
    const auto& reg = ohpeCore.params();
    return { (double) reg.get ("tape.loopin"), (double) reg.get ("tape.loopout") };
}

tapeloop::Snap OhpeProcessor::tapeLoopSnap() const
{
    return (tapeloop::Snap) juce::jlimit (0, 2, (int) std::lround (ohpeCore.params().get (Core::Ids::tapeLoopSnap)));
}

void OhpeProcessor::writeTapeLoop (tapeloop::Range r)
{
    // out first when the loop moves right, in first when it moves left: the pair is never crossed on the way
    if (r.in >= ohpeCore.params().get ("tape.loopout"))
    {
        setPlainFromUi ("tape.loopout", (float) r.out);
        setPlainFromUi ("tape.loopin", (float) r.in);
    }
    else
    {
        setPlainFromUi ("tape.loopin", (float) r.in);
        setPlainFromUi ("tape.loopout", (float) r.out);
    }
}

juce::String OhpeProcessor::tapeTimeText (double seconds) const
{
    seconds = juce::jmax (0.0, seconds);
    const int ms = (int) std::lround (seconds * 1000.0);
    const double beats = seconds * tapeBpm() / 60.0 + 1.0e-6;
    const int bar = (int) (beats / tapeloop::beatsPerBar) + 1, beat = (int) std::fmod (beats, (double) tapeloop::beatsPerBar) + 1;
    return juce::String (ms / 60000) + ":" + juce::String ((ms / 1000) % 60).paddedLeft ('0', 2) + "." + juce::String ((ms / 100) % 10)
           + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (bar) + "." + juce::String (beat);
}

juce::String OhpeProcessor::setTapeLoopEdge (bool inEdge, bool free)
{
    using i18n::tr;
    auto& tape = ohpeCore.tape();
    const double sr = juce::jmax (1.0, tape.sampleRate());
    const double at = (double) tape.positionFrames() / sr;
    const auto snap = free ? tapeloop::Snap::Off : tapeLoopSnap();
    const auto before = tapeLoopRange();
    const auto after = inEdge ? tapeloop::setIn (before, at, tapeLengthSeconds(), snap, tapeBpm())
                              : tapeloop::setOut (before, at, tapeLengthSeconds(), snap, tapeBpm());
    writeTapeLoop (after);
    tapeLoopTracker.commit (before, tapeLoopRange());
    if (! inEdge)
    {
        setPlainFromUi ("tape.loop", 1.0f); // loop out closes the loop: it plays at once
        // the playhead is already past the new loop out (snapped back to the nearest line): into the loop at once
        if (tapeRolling() && ohpeCore.params().get ("tape.reverse") < 0.5f && at >= after.out && after.valid())
            tape.locate (after.in + std::fmod (at - after.out, after.length()));
    }
    return (inEdge ? tr ("Loop in") : tr ("Loop out")) + " " + tapeTimeText (inEdge ? after.in : after.out);
}

juce::String OhpeProcessor::tapeLoopAction (const juce::String& id, bool free)
{
    using i18n::tr;
    if (id == Core::Ids::tapeLoopSetIn) return setTapeLoopEdge (true, free);
    if (id == Core::Ids::tapeLoopSetOut) return setTapeLoopEdge (false, free);
    const auto before = tapeLoopRange();
    const auto snap = free ? tapeloop::Snap::Off : tapeLoopSnap();
    if (id == Core::Ids::tapeLoopUndo || id == Core::Ids::tapeLoopRedo)
    {
        const bool undo = id == Core::Ids::tapeLoopUndo;
        tapeloop::Range r;
        if (! (undo ? tapeLoopTracker.undo (before, r) : tapeLoopTracker.redo (before, r)))
            return undo ? tr ("Nothing to undo") : tr ("Nothing to redo");
        writeTapeLoop (r);
        return (undo ? tr ("Loop undone") : tr ("Loop redone")) + ": " + tapeTimeText (r.in) + juce::String::fromUTF8 (" \xe2\x80\x93 ") + tapeTimeText (r.out);
    }
    if (id == Core::Ids::tapeLoopHalf || id == Core::Ids::tapeLoopDouble)
    {
        if (! before.valid()) return tr ("Set loop in and out first");
        const auto after = id == Core::Ids::tapeLoopHalf ? tapeloop::halve (before, tapeLengthSeconds(), snap, tapeBpm())
                                                          : tapeloop::doubled (before, tapeLengthSeconds(), snap, tapeBpm());
        writeTapeLoop (after);
        tapeLoopTracker.commit (before, tapeLoopRange());
        return (id == Core::Ids::tapeLoopHalf ? tr ("Loop halved") : tr ("Loop doubled")) + ": " + tapeTimeText (after.in)
               + juce::String::fromUTF8 (" \xe2\x80\x93 ") + tapeTimeText (after.out);
    }
    return {};
}

void OhpeProcessor::dragTapeLoopEdge (bool inEdge, double seconds, bool free)
{
    const auto snap = free ? tapeloop::Snap::Off : tapeLoopSnap();
    writeTapeLoop (tapeloop::dragEdge (tapeLoopRange(), inEdge, seconds, tapeLengthSeconds(), snap, tapeBpm()));
}

void OhpeProcessor::endTapeLoopDrag()
{
    tapeloop::Range fixed;
    tapeLoopTracker.observe (tapeLoopRange(), 0.0, tapeLengthSeconds(), tapeloop::minFreeSeconds, fixed);
    tapeLoopTracker.flush(); // the whole drag is one step
}

void OhpeProcessor::watchTapeLoop (double now)
{
    tapeloop::Range fixed;
    // encoders / MIDI / host: the edge that moved wins, the other one gives way (free minimum: the grid is for keys)
    if (tapeLoopTracker.observe (tapeLoopRange(), now, tapeLengthSeconds(), tapeloop::minFreeSeconds, fixed))
        writeTapeLoop (fixed);
}

// ---------------------------------------------------------------- p38: tape files
bool OhpeProcessor::saveTapeFile (const juce::File& file, juce::String& message)
{
    using i18n::tr;
    if (tapeRecording()) { message = tr ("Not while recording"); return false; }
    const auto& reg = ohpeCore.params();
    tapefile::Settings s;
    s.loopIn = reg.get ("tape.loopin");
    s.loopOut = reg.get ("tape.loopout");
    s.loop = reg.get ("tape.loop") >= 0.5f;
    s.editTrack = (int) std::lround (reg.get (Core::Ids::tapeEditTrack));
    const auto data = tapefile::save (ohpeCore.tape(), s);
    const auto target = file.withFileExtension (tapefile::extension);
    juce::TemporaryFile tmp (target);
    {
        juce::FileOutputStream out (tmp.getFile());
        if (! out.openedOk() || ! out.write (data.data(), data.size()))
        {
            message = tr ("Could not write the tape file");
            return false;
        }
        out.flush();
        if (out.getStatus().failed()) { message = tr ("Could not write the tape file"); return false; }
    }
    if (! tmp.overwriteTargetFileWithTemporary()) { message = tr ("Could not write the tape file"); return false; }
    message = tr ("Tape saved") + ": " + target.getFileName();
    return true;
}

bool OhpeProcessor::loadTapeFile (const juce::File& file, juce::String& message)
{
    using i18n::tr;
    if (tapeRecording()) { message = tr ("Not while recording"); return false; }
    juce::MemoryBlock mb;
    tapefile::Settings s;
    if (! file.loadFileAsData (mb) || ! tapefile::load (ohpeCore.tape(), mb.getData(), mb.getSize(), s))
    {
        message = tr ("Could not read the tape file");
        return false;
    }
    setPlainFromUi ("tape.loopin", (float) s.loopIn);
    setPlainFromUi ("tape.loopout", (float) s.loopOut);
    setPlainFromUi ("tape.loop", s.loop ? 1.0f : 0.0f);
    setPlainFromUi (Core::Ids::tapeEditTrack, (float) s.editTrack);
    tapeLoopTracker.history().clear(); // p42: a new tape starts a new loop history
    tapeLoopTracker.reset (tapeLoopRange());
    message = tr ("Tape loaded") + ": " + file.getFileName();
    sendChangeMessage();
    return true;
}

// ---------------------------------------------------------------- p24: harmony brain
void OhpeProcessor::takeHarmonyKey()
{
    const auto& h = ohpeCore.harmony();
    setPlainFromUi (Core::Ids::keyRoot, (float) h.keyRoot());
    setPlainFromUi (Core::Ids::keyScale, (float) h.keyScale());
}

// ---------------------------------------------------------------- p45: key, Scale Lock
void OhpeProcessor::setKeyFromUi (int root, int scale)
{
    if (ohpeCore.keyScale().fromHost) return; // the host gives the key: read-only
    setPlainFromUi (Core::Ids::keyRoot, (float) (((root % 12) + 12) % 12));
    setPlainFromUi (Core::Ids::keyScale, (float) juce::jlimit (0, scales::numScales - 1, scale));
}

void OhpeProcessor::setScaleLockFromUi (bool on, int mode)
{
    setPlainFromUi (Core::Ids::keyLock, on ? 1.0f : 0.0f);
    setPlainFromUi (Core::Ids::keyLockMode, (float) juce::jlimit (0, keys::numLockModes - 1, mode));
}

// ---------------------------------------------------------------- p68: Voice Control
int OhpeProcessor::applyVoiceQuickMap (int part)
{
    int placed = 0;
    const auto changes = voice::VoiceControl::quickMap (ohpeCore.params(), juce::jlimit (0, Core::numParts - 1, part), &placed);
    for (const auto& [id, v] : changes) setPlainFromUi (id.c_str(), v);
    return placed;
}

// ---------------------------------------------------------------- p11: tape transport
void OhpeProcessor::setPlainFromUi (const char* id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
    ohpeCore.params().set (id, plain); // the UI and the next action see it at once
}

bool OhpeProcessor::tapeRolling() const { return ohpeCore.params().get ("tape.play") >= 0.5f; }
bool OhpeProcessor::tapeRecordReady() const { return ohpeCore.params().get ("tape.rec") >= 0.5f; }
bool OhpeProcessor::tapeRecording() const { return tapeRolling() && tapeRecordReady() && armedTrackMask() != 0; }

int OhpeProcessor::armedTrackMask() const
{
    int mask = 0;
    for (int t = 0; t < TapeMachine::numTracks; ++t)
        if (ohpeCore.params().get ("tape.tr" + std::to_string (t + 1) + ".arm") >= 0.5f) mask |= 1 << t;
    return mask;
}

juce::String OhpeProcessor::runTransport (TransportKey action)
{
    using i18n::tr;
    auto& reg = ohpeCore.params();
    auto& tape = ohpeCore.tape();
    const bool linked = reg.get (Core::Ids::tapeTxLink) >= 0.5f;
    const int editTrack = juce::jlimit (0, TapeMachine::numTracks - 1, (int) std::lround (reg.get (Core::Ids::tapeEditTrack)) - 1);
    const std::string armId = "tape.tr" + std::to_string (editTrack + 1) + ".arm";
    auto trackList = [] (int mask) {
        juce::StringArray s;
        for (int t = 0; t < TapeMachine::numTracks; ++t)
            if ((mask >> t) & 1) s.add (juce::String (t + 1));
        return s.joinIntoString (", ");
    };
    auto start = [&] {
        setPlainFromUi ("tape.play", 1.0f);
        if (linked) setPlainFromUi (Core::Ids::seqRun, 1.0f);
    };
    auto stop = [&] {
        setPlainFromUi ("tape.rec", 0.0f);
        setPlainFromUi ("tape.play", 0.0f);
        if (linked) setPlainFromUi (Core::Ids::seqRun, 0.0f);
    };
    juce::String msg;
    switch (action)
    {
        case TransportKey::PlayStop:
        {
            const bool seqRunning = reg.get (Core::Ids::seqRun) >= 0.5f;
            if (! linked) // the sequencer alone; the tape keeps its own transport
            {
                setPlainFromUi (Core::Ids::seqRun, seqRunning ? 0.0f : 1.0f);
                msg = seqRunning ? tr ("Stopped") : tr ("Play");
                break;
            }
            if (tapeRolling() || seqRunning) { stop(); msg = tr ("Stopped"); }
            else { start(); msg = tr ("Play"); }
            break;
        }
        case TransportKey::TapePlayStop:
            if (tapeRolling()) { stop(); msg = tr ("Stopped"); }
            else { start(); msg = tr ("Play"); }
            break;
        case TransportKey::Stop: // stops everything, linked or not
            if (tapeRolling() || tapeRecordReady() || reg.get (Core::Ids::seqRun) >= 0.5f)
            {
                stop();
                setPlainFromUi (Core::Ids::seqRun, 0.0f);
                msg = tr ("Stopped");
            }
            else { tape.locate (0.0); msg = tr ("Back to the start"); }
            break;
        case TransportKey::Record:
            if (ohpeCore.countInActive()) // p39: REC again during the count-in cancels it
            {
                stop();
                msg = tr ("Count-in cancelled");
                break;
            }
            if (tapeRecording())
            {
                setPlainFromUi ("tape.rec", 0.0f); // punch out, the tape keeps playing
                msg = tr ("Recording stopped - the tape keeps playing");
                break;
            }
            if (armedTrackMask() == 0) setPlainFromUi (armId.c_str(), 1.0f);
            setPlainFromUi ("tape.rec", 1.0f);
            if (! tapeRolling())
            {
                start();
                if (const int bars = (int) std::lround (reg.get (Metronome::Ids::countIn)); bars > 0) // p39
                {
                    msg = tr ("Count-in") + " " + juce::String (bars) + " - " + tr ("Recording track") + " " + trackList (armedTrackMask());
                    break;
                }
            }
            msg = tr ("Recording track") + " " + trackList (armedTrackMask());
            break;
        case TransportKey::ToStart:
            tape.locate (0.0);
            msg = tr ("Back to the start");
            break;
        case TransportKey::Loop:
        {
            const bool on = reg.get ("tape.loop") < 0.5f;
            if (on && reg.get ("tape.loopout") <= reg.get ("tape.loopin") + 0.05f)
            {
                // no loop range yet: the recorded length, or four bars of the tempo
                const double sr = juce::jmax (1.0, tape.sampleRate());
                double len = (double) tape.contentEndFrames() / sr;
                if (len < 0.5) len = 4.0 * 4.0 * 60.0 / juce::jmax (20.0, (double) reg.get (Core::Ids::tempo));
                setPlainFromUi ("tape.loopin", 0.0f);
                setPlainFromUi ("tape.loopout", (float) len);
            }
            setPlainFromUi ("tape.loop", on ? 1.0f : 0.0f);
            msg = on ? tr ("Loop on") : tr ("Loop off");
            break;
        }
        case TransportKey::Arm:
        {
            const bool armed = reg.get (armId) >= 0.5f;
            setPlainFromUi (armId.c_str(), armed ? 0.0f : 1.0f);
            msg = (armed ? tr ("Track disarmed") : tr ("Track armed")) + ": " + juce::String (editTrack + 1);
            break;
        }
    }
    return msg; // the editor follows on its timer (no change message: that would reload the UI state)
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

bool OhpeProcessor::setMidiMapping (int index, const MidiMapping& m)
{
    if (index < 0 || ohpeCore.params().indexOf (m.paramId) < 0) return false;
    if (! ohpeCore.midiMap().replace ((size_t) index, m)) return false;
    ohpeCore.midiMapChanged();
    sendChangeMessage();
    return true;
}

bool OhpeProcessor::removeMidiMapping (int index)
{
    if (index < 0 || ! ohpeCore.midiMap().remove ((size_t) index)) return false;
    ohpeCore.midiMapChanged();
    sendChangeMessage();
    return true;
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
    userTableManager.storeInState(); // p20
    if (juce::MessageManager::existsAndIsCurrentThread()) tapeMemoryManager.storeInState(); // p67 (else: the last one stored)
    // p40: the loaded preset's uid travels with the session (a copy: the editor owns uiState)
    // p50: one uid per part ("presets"); "preset" keeps the edit part's for older versions
    std::array<std::string, 4> uids;
    if (juce::MessageManager::existsAndIsCurrentThread()) uids = presetManager->currentUids();
    else
    {
        const juce::SpinLock::ScopedLockType sl (presetUidLock);
        uids = presetUidRestore.load() ? presetUidPending : presetUidCache;
    }
    const auto& uid = uids[(size_t) presetManager->editPart()];
    auto uiCopy = uiState.clone();
    if (! uiCopy.isObject()) uiCopy = juce::var (new juce::DynamicObject());
    if (auto* o = uiCopy.getDynamicObject())
    {
        if (uid.empty()) o->removeProperty ("preset");
        else o->setProperty ("preset", juce::String::fromUTF8 (uid.c_str()));
        juce::Array<juce::var> all;
        for (const auto& u : uids) all.add (juce::String::fromUTF8 (u.c_str()));
        o->setProperty ("presets", all);
    }
    const auto ui = juce::JSON::toString (uiCopy, true);
    const auto json = ohpeCore.saveState (ui.toStdString());
    dest.replaceAll (json.data(), json.size());
    tapeio::appendTapeAudio (ohpeCore, dest); // 0.2: tape audio after the JSON (see TapeIO.h)
}

void OhpeProcessor::offerRecovery (int reason, std::function<void()> restore)
{
    recoveryOffer = { reason, std::move (restore) }; // the editor shows it from its timer
}

OhpeProcessor::RecoveryOffer OhpeProcessor::takeRecoveryOffer() { return std::exchange (recoveryOffer, {}); }

void OhpeProcessor::setStateInformation (const void* data, int size)
{
    // p63: a damaged state never takes the instance down: refused as a whole, or (a loader gave up half way)
    // the instance starts over with the factory start sound
    stateLoadFailed.store (false);
    std::string json, ui;
    const uint8_t* tape = nullptr;
    size_t tapeSize = 0;
    try
    {
        if (data == nullptr || size <= 0) { stateLoadFailed.store (true); return; }
        tapeio::splitState (data, size, json, tape, tapeSize); // 0.2: JSON + optional tape audio
        if (! ohpeCore.loadState (json, &ui))
        {
            stateLoadFailed.store (true);
            return;
        }
        tapeio::restoreTape (ohpeCore, tape, tapeSize);
        samples.restoreFromState();
        userTableManager.restoreFromState(); // p20
        tapeMemoryManager.restoreFromState(); // p67: after "samples" (which clears every assignment)
    }
    catch (...)
    {
        stateLoadFailed.store (true);
        try { ohpeCore.loadState (std::make_unique<Core>()->saveState()); } // parameters and sections of a new instance
        catch (...) { ohpeCore.params().resetToDefaults(); }
        if (juce::MessageManager::existsAndIsCurrentThread() && startWithFactorySound.load()) presetManager->loadStartSound();
        syncHostParametersFromRegistry();
        sendChangeMessage();
        return;
    }
    auto parsed = juce::JSON::parse (juce::String::fromUTF8 (ui.c_str()));
    uiState = parsed.isObject() ? parsed : juce::var (new juce::DynamicObject());
    {   // p40: the preset that was loaded when the session was saved (none = "Init"); replaces the start sound's name
        const juce::SpinLock::ScopedLockType sl (presetUidLock);
        presetUidPending = sessionPresetUids (parsed);
        presetUidCache = presetUidPending;
        presetUidRestore.store (true);
    }
    if (juce::MessageManager::existsAndIsCurrentThread()) applyPendingPresetUid();
    syncHostParametersFromRegistry();
    sendChangeMessage();
}

std::array<std::string, 4> OhpeProcessor::sessionPresetUids (const juce::var& ui)
{
    // p50: "presets" = one uid per part; a session from before (only "preset") gives that one to every part.
    // p62: the migration lives in the core (state::sessionPresetUids) so the format tests cover it.
    if (! ui.isObject()) return {};
    return state::sessionPresetUids (juce::JSON::toString (ui, true).toStdString());
}

void OhpeProcessor::applyPendingPresetUid()
{
    if (! presetUidRestore.load()) return;
    std::array<std::string, 4> uid;
    {
        const juce::SpinLock::ScopedLockType sl (presetUidLock);
        uid = presetUidPending;
        presetUidRestore.store (false);
    }
    presetManager->restoreCurrent (uid);
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
