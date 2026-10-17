// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OhpeCore.h"

#include "model/StateSerializer.h"

#include <cstring>
#include <limits>

namespace ohpe
{
Core::Core()
{
    for (int i = 0; i < numParts; ++i)
        parts[(size_t) i] = std::make_unique<Part> (i + 1);

    buildParameterLayout (registry, parts, mix, stepSeq);
    registry.freeze();

    for (auto& p : parts) p->bind (registry);
    mix.bind (registry);
    drums.bind (registry);
    tapeMachine.bind (registry);
    masterRack.bindMaster (registry); // 0.7 (the part racks belong to the parts)
    mix.setMasterRack (&masterRack);
    mix.setPerformancePads (&perfPads); // p25
    stepSeq.setRegistry (&registry);
    tempoIndex = registry.indexOf (Ids::tempo);
    seqRunIndex = registry.indexOf (Ids::seqRun);
    for (const auto& d : stepSeq.getParamDescriptors())
        stepParamIndex.push_back (registry.indexOf (std::string (Ids::stepPrefix) + d.id));
    for (const auto& d : stepSeq.getChainParamDescriptors()) // local indices continue: Pattern, Song
        stepParamIndex.push_back (registry.indexOf (std::string (Ids::stepPrefix) + d.id));
    stepParamLast.assign (stepParamIndex.size(), NAN);

    auto withLate = [] (std::vector<ParamDescriptor> list, const std::vector<ParamDescriptor>& late) {
        list.insert (list.end(), late.begin(), late.end()); // 0.5: local indices continue (Swing, Human)
        return list;
    };
    bindSequencer (euclidBinding, euclid, Ids::euclidPrefix, withLate (euclid.getParamDescriptors(), euclid.getLateParamDescriptors()));
    bindSequencer (arpBinding, arp, Ids::arpPrefix, arp.getParamDescriptors());
    bindSequencer (orbitBinding, orbit, Ids::orbitPrefix, withLate (orbit.getParamDescriptors(), orbit.getLateParamDescriptors()));
    for (int i = 0; i < numParts; ++i)
    {
        auto& r = routing[(size_t) i];
        const auto& p = *parts[(size_t) i];
        r.idxChannel = registry.indexOf (p.id ("midich"));
        r.idxQuant = registry.indexOf (p.id ("quant"));
        r.idxChord = registry.indexOf (p.id ("chord"));
        r.idxMpe = registry.indexOf (p.id ("mpe"));
        r.idxMpeBend = registry.indexOf (p.id ("mpe.bend"));
        r.idxMpePress = registry.indexOf (p.id ("mpe.press"));
        r.idxMpeTimbre = registry.indexOf (p.id ("mpe.timbre"));
        r.idxPressAmt = registry.indexOf (p.id ("mpe.pressamt"));
        r.idxTimbreAmt = registry.indexOf (p.id ("mpe.timbreamt"));
    }
    syncSourceIndex = registry.indexOf (Ids::syncSource);
    syncSmoothIndex = registry.indexOf (Ids::syncSmoothing);
    clockOutIndex = registry.indexOf (Ids::clockOut);
    notesOutIndex = registry.indexOf (Ids::notesOut);
    perfBendIndex = registry.indexOf (Ids::perfBend);
    perfModIndex = registry.indexOf (Ids::perfMod);
    editPartIndex = registry.indexOf (Ids::editPart);
    stepDrumsIndex = registry.indexOf (Ids::stepDrums);
    euclidDrumsIndex = registry.indexOf (Ids::euclidDrums);
    orbitDrumsIndex = registry.indexOf (Ids::orbitDrums);
    keyRootIndex = registry.indexOf (Ids::keyRoot);
    spaceOutIndex = registry.indexOf (Ids::spaceOut);
    monoCheckIndex = registry.indexOf (Ids::spaceMonoCheck);
    keyScaleIndex = registry.indexOf (Ids::keyScale);

    snapshot.prepare (registry);
    for (auto& p : parts) p->setSnapshot (&snapshot); // 0.7: the matrix writes modulated values into it
    static_assert (MorphSpace::numParts == numParts, "morph space parts");
    morphSpace.bind (registry, snapshot, { parts[0].get(), parts[1].get(), parts[2].get(), parts[3].get() }); // p16
    static_assert (MotionRecorder::numParts == numParts, "motion recorder parts");
    motionRec.bind (registry, snapshot); // p22
    perfPads.bind (registry);            // p25
    harmonyBrain.bind (registry);        // p24
    lockStorage.reserve (1024);
    seqOut.locks = &lockStorage;
    seqOut.maxLocks = 1024;
    mapper.publish (map);
    prepare (48000.0, 512);
}

void Core::bindSequencer (SeqBinding& b, ISequencer& s, const std::string& prefix, const std::vector<ParamDescriptor>& descriptors)
{
    b.seq = &s;
    b.index.clear();
    for (const auto& d : descriptors) b.index.push_back (registry.indexOf (prefix + d.id));
    b.last.assign (b.index.size(), NAN);
}

void Core::pullSequencerParams (SeqBinding& b) noexcept
{
    for (size_t i = 0; i < b.index.size(); ++i)
        if (const float v = snapshot[b.index[i]]; b.index[i] >= 0 && ! (v == b.last[i]))
        {
            b.last[i] = v;
            b.seq->setParam ((int) i, v);
        }
}

void Core::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = std::max (1, maxBlockSize);
    const int chunk = std::min (maxChunk, maxBlock);
    for (int i = 0; i < numParts; ++i)
    {
        partL[(size_t) i].assign ((size_t) chunk, 0.0f);
        partR[(size_t) i].assign ((size_t) chunk, 0.0f);
    }
    drumL.assign ((size_t) chunk, 0.0f);
    drumR.assign ((size_t) chunk, 0.0f);
    for (auto& p : parts) p->prepare (sr, chunk);
    morphSpace.prepare (sr, chunk); // p16: the morph lanes
    motionRec.prepare (sr);         // p22
    perfPads.prepare (sr);          // p25
    harmonyBrain.prepare (sr);      // p24
    drums.prepare (sr, chunk);
    mix.prepare (sr, chunk);
    stepSeq.prepare (sr, maxBlock);
    euclid.prepare (sr, maxBlock);
    arp.prepare (sr, maxBlock);
    orbit.prepare (sr, maxBlock);
    clockIn.prepare (sr);
    clockGen.prepare (sr);
    tapeMachine.prepare (sr, chunk);
    masterRack.prepare (sr, chunk);
    for (auto& t : tapeOut) t.assign ((size_t) chunk, 0.0f);
    std::fill (stepParamLast.begin(), stepParamLast.end(), NAN);
    for (auto* b : { &euclidBinding, &arpBinding, &orbitBinding }) std::fill (b->last.begin(), b->last.end(), NAN);
    reset();
    prepareEffects();
}

bool Core::prepareEffects()
{
    bool any = masterRack.prepareSelected (registry);
    for (auto& p : parts) any = p->fxRack().prepareSelected (registry) || any;
    return any;
}

bool Core::effectsWaiting() const noexcept
{
    if (masterRack.isWaiting()) return true;
    for (const auto& p : parts)
        if (p->fxRack().isWaiting()) return true;
    return false;
}

void Core::reset() noexcept
{
    for (auto& p : parts) p->reset();
    morphSpace.reset();
    motionRec.reset(); // p22
    perfPads.reset();  // p25
    harmonyBrain.reset(); // p24
    drums.reset();
    mix.reset();
    tapeMachine.resetDsp();
    masterRack.reset();
    stepSeq.reset();
    euclid.reset();
    arp.reset();
    orbit.reset();
    clockIn.reset();
    clockGen.reset();
    for (auto& m : mpeIn) m.reset();
    for (auto& s : liveShaper) s.reset();
    for (auto& s : seqShaper) s.reset();
    snapshot.clearLocks();
    internalPpq = 0.0;
    internalWasRunning = false;
    sampleTime = 0;
    numRouted = 0;
    midiOut.clear();
}

void Core::queueNote (int note, float velocity, bool on) noexcept
{
    uiNotes.push ({ note, velocity, on });
}

void Core::queueGridEvent (int part, const MidiEvent& e) noexcept
{
    if (part >= 0 && part < numParts) gridEvents.push ({ part, e });
}

void Core::refreshShapers (int p) noexcept
{
    auto& r = routing[(size_t) p];
    const bool quant = snapshot.getBool (r.idxQuant);
    const int chord = snapshot.getInt (r.idxChord);
    int fr = 0;
    uint16_t fm = 0;
    if (harmonyBrain.followSet (p, fr, fm)) // p24: the part follows the harmony brain (chords build in its key)
    {
        const int kr = harmonyBrain.keyRoot(), ks = harmonyBrain.keyScale();
        const uint16_t km = ks == scales::Chromatic ? (uint16_t) 0xfff : scales::mask (ks);
        liveShaper[(size_t) p].setMask (true, chord, fr, fm, kr, km);
        seqShaper[(size_t) p].setMask (true, chord, fr, fm, kr, km);
        return;
    }
    liveShaper[(size_t) p].set (quant, chord, blockRoot, blockScale);
    seqShaper[(size_t) p].set (quant, chord, blockRoot, blockScale);
}

// ---------------------------------------------------------------- routing
void Core::addRouted (const MidiEvent& e, int part) noexcept
{
    if (numRouted < (int) routed.size()) routed[(size_t) numRouted++] = { e, part };
    if (part >= 0 && part < numParts) // p24: the harmony brain hears what reaches the parts (not the drum kit)
    {
        if (e.isNoteOn()) harmonyBrain.noteOn (part, e.data1, e.data2);
        else if (e.isNoteOff()) harmonyBrain.noteOff (part, e.data1);
    }
}

void Core::routeLive (const MidiEvent& e) noexcept
{
    // keyboard / host / device MIDI: to every part listening on its channel (MPE parts listen to all).
    // The drum channel plays the drum kit; omni parts ignore it, a part set to that channel hears it too.
    const bool drumCh = e.channel() == drumChannel;
    if (drumCh) addRouted (e, drumPart);
    for (int p = 0; p < numParts; ++p)
    {
        const auto& r = routing[(size_t) p];
        if (! (r.mpe || (r.channel == 0 && ! drumCh) || r.channel == e.channel())) continue;
        routeToPart (e, p);
    }
}

void Core::routeToPart (const MidiEvent& e, int p) noexcept
{
    const auto& r = routing[(size_t) p];
    const bool toArp = arp.enabled() && arp.targetPart() == p + 1 && ! r.mpe;
    auto emit = [&] (const MidiEvent& x) {
        if (toArp && (x.isNoteOn() || x.isNoteOff())) arp.noteInput (x);
        else addRouted (x, p);
    };
    if (r.mpe) { emit (e); return; } // MPE notes keep their channel and pitch (per-note expression)
    if (harmonyBrain.followMode (p) != ohpe::harmony::FollowOff && e.isNoteOn()) refreshShapers (p); // p24: the chord heard just now
    liveShaper[(size_t) p].process (e, emit);
}

void Core::routeGenerated (const MidiEvent& e, bool shape, bool toDrums) noexcept
{
    if (toDrums) // 0.4: the sequencer plays the drum kit (notes = pads from "drum.base")
    {
        auto d = e;
        d.status = (uint8_t) ((d.status & 0xf0) | ((drumChannel - 1) & 0x0f));
        addRouted (d, drumPart);
        if (notesOutOn) midiOut.add (d);
        return;
    }
    // sequencer events address their part by channel (1..4)
    const int p = std::clamp (e.channel(), 1, numParts) - 1;
    auto emit = [&] (const MidiEvent& x) {
        addRouted (x, p);
        if (notesOutOn)
        {
            auto o = x;
            const int ch = routing[(size_t) p].channel > 0 ? routing[(size_t) p].channel : p + 1;
            o.status = (uint8_t) ((o.status & 0xf0) | ((ch - 1) & 0x0f));
            midiOut.add (o);
        }
    };
    if (shape)
    {
        if (harmonyBrain.followMode (p) != ohpe::harmony::FollowOff && e.isNoteOn()) refreshShapers (p); // p24
        seqShaper[(size_t) p].process (e, emit);
    }
    else emit (e);
}

void Core::dispatch (const RoutedEvent& r) noexcept
{
    if (r.event.isNoteOn()) lastNoteOn.store (r.event.data1, std::memory_order_relaxed);
    if (r.part == drumPart) drums.handleMidi (r.event);
    else mpeIn[(size_t) r.part].handle (r.event, *parts[(size_t) r.part]); // note starts retrigger the rack (Part::noteOn)
}

void Core::applyLocks (const std::vector<ParamLockEvent>& locks, size_t& next, int upTo) noexcept
{
    bool changed = false;
    while (next < locks.size() && locks[next].sampleOffset <= upTo)
    {
        const auto& l = locks[next++];
        if (l.clearAll) snapshot.clearLocks();
        else snapshot.setLock (l.paramIndex, l.value);
        changed = true;
    }
    if (changed)
    {
        snapshot.pull (registry);
        morphSpace.afterPull(); // p16
        for (auto& p : parts) p->pullParams (snapshot);
        drums.pullParams (snapshot);
        mix.pullParams (snapshot);
        pullPhase2Params();
    }
}

void Core::pullPhase2Params() noexcept
{
    tapeMachine.pullParams (snapshot);
    masterRack.pullParams (snapshot); // 0.7 (part racks: Part::pullParams)
}

void Core::renderTapeChunk (SourceBuffers& src, int pos, int len) noexcept
{
    TapeInputs tin;
    tin.left[tapesource::input] = inputL != nullptr ? inputL + pos : nullptr;
    tin.right[tapesource::input] = inputR != nullptr ? inputR + pos : nullptr;
    for (int s = 0; s < 4; ++s)
    {
        tin.left[(size_t) (tapesource::part1 + s)] = src.left[(size_t) (source::part1 + s)];
        tin.right[(size_t) (tapesource::part1 + s)] = src.right[(size_t) (source::part1 + s)];
    }
    tin.left[tapesource::drum] = src.left[(size_t) source::drum];
    tin.right[tapesource::drum] = src.right[(size_t) source::drum];

    std::array<float*, 2 * TapeMachine::numTracks> outs {};
    for (size_t c = 0; c < outs.size(); ++c) outs[c] = tapeOut[c].data();
    tapeMachine.process (tin, outs.data(), len);
    for (int t = 0; t < TapeMachine::numTracks; ++t)
    {
        src.left[(size_t) (source::tape1 + t)] = tapeOut[(size_t) (2 * t)].data();
        src.right[(size_t) (source::tape1 + t)] = tapeOut[(size_t) (2 * t + 1)].data();
        if (const int target = tapeMachine.directOut (t); target > 0)
            mix.addDirect (tapeOut[(size_t) (2 * t)].data(), tapeOut[(size_t) (2 * t + 1)].data(), target);
    }
    if (tapeCapture != nullptr)
        for (size_t c = 0; c < outs.size(); ++c)
            if (float* d = (*tapeCapture)[c]) std::memcpy (d + pos, outs[c], sizeof (float) * (size_t) len);
}

// ---------------------------------------------------------------- transport
Transport Core::makeTransport (const Transport& host, const MidiEvent* midi, int numMidi, int numSamples) noexcept
{
    Transport tr = host;
    const bool run = snapshot.getBool (seqRunIndex);
    const int source = snapshot.getInt (syncSourceIndex);

    // MIDI clock input is always followed (tempo display), used when selected
    clockIn.setSmoothing (snapshot[syncSmoothIndex]);
    for (int i = 0; i < numMidi; ++i)
        if (midiclock::isClockMessage (midi[i])) clockIn.handle (midi[i], sampleTime + midi[i].sampleOffset);

    if (host.hostDriven) // plug-in host or Link (standalone): ppq from outside
    {
        tr.playing = host.playing && run;
        internalWasRunning = false;
    }
    else if (source == SyncMidiClock)
    {
        tr = clockIn.transportAt (sampleTime, snapshot[tempoIndex]); // tempo parameter until the clock's tempo is known
        tr.playing = tr.playing && run;
        internalWasRunning = false;
    }
    else
    {
        if (run && ! internalWasRunning) internalPpq = 0.0;
        internalWasRunning = run;
        tr.bpm = snapshot[tempoIndex];
        tr.playing = run;
        tr.ppqAtBlockStart = internalPpq;
        tr.hostDriven = false;
        if (run) internalPpq += numSamples * tr.bpm / 60.0 / sr;
    }
    return tr;
}

// ---------------------------------------------------------------- process
void Core::process (const OutputBuffers& outputs, int numSamples, const MidiEvent* midi, int numMidi, const Transport& host) noexcept
{
    // 1. parameters for this block, the transport (p22: the motion lanes need its position before the parts read them)
    snapshot.pull (registry);
    const Transport tr = makeTransport (host, midi, numMidi, numSamples);
    motionRec.process (tr, numSamples); // p22: recorded knob movements over the snapshot
    morphSpace.afterPull(); // p16: the morph values over the parts' own
    for (auto& p : parts) p->pullParams (snapshot);
    for (int p = 0; p < numParts; ++p) // p22: the lanes as matrix sources Motion 1 .. 4
        for (int l = 0; l < MotionRecorder::numLanes; ++l)
            parts[(size_t) p]->modMatrix().setSourceValue (mod::SrcMotion1 + l, motionRec.sourceValue (p, l));
    perfPads.pullParams (snapshot); // p25
    for (auto& p : parts) // p25: the pads as matrix sources (as of the last block)
    {
        p->modMatrix().setSourceValue (mod::SrcPadSweep, perfPads.sweepSource());
        p->modMatrix().setSourceValue (mod::SrcPadHeld, perfPads.heldSource());
    }
    drums.pullParams (snapshot);
    mix.setTempo (bpmForUi.load (std::memory_order_relaxed));
    mix.pullParams (snapshot);
    pullPhase2Params();
    for (size_t i = 0; i < stepParamIndex.size(); ++i)
        if (const float v = snapshot[stepParamIndex[i]]; stepParamIndex[i] >= 0 && ! (v == stepParamLast[i]))
        {
            stepParamLast[i] = v;
            stepSeq.setParam ((int) i, v);
        }
    pullSequencerParams (euclidBinding);
    pullSequencerParams (arpBinding);
    pullSequencerParams (orbitBinding);
    if (arpWasOn && (! arp.enabled() || arp.targetPart() != arpLastPart)) arp.releaseAll();
    arpWasOn = arp.enabled();
    arpLastPart = arp.targetPart();

    const int root = snapshot.getInt (keyRootIndex), scale = snapshot.getInt (keyScaleIndex);
    blockRoot = root;
    blockScale = scale;
    harmonyBrain.beginBlock (snapshot, tr.bpm, numSamples, root, scale); // p24: memory, follow modes
    for (int p = 0; p < numParts; ++p) // p24: the brain as matrix sources Chord Root / Chord Change / Tension
        for (int k = 0; k < HarmonyBrain::numSources; ++k)
            parts[(size_t) p]->modMatrix().setSourceValue (mod::SrcChordRoot + k, harmonyBrain.sourceValue (k));
    for (int i = 0; i < numParts; ++i)
    {
        auto& r = routing[(size_t) i];
        r.channel = std::clamp (snapshot.getInt (r.idxChannel), 0, 16);
        r.mpe = snapshot.getBool (r.idxMpe);
        mpeIn[(size_t) i].set (r.mpe, snapshot.getInt (r.idxMpeBend), snapshot.getInt (r.idxMpePress), snapshot.getInt (r.idxMpeTimbre),
                               r.idxPressAmt >= 0 ? snapshot[r.idxPressAmt] : 1.0f, r.idxTimbreAmt >= 0 ? snapshot[r.idxTimbreAmt] : 1.0f);
        refreshShapers (i); // scale / chord, p24: or the harmony brain's set
    }
    clockOutOn = snapshot.getBool (clockOutIndex);
    if (const int mon = spaceOutIndex >= 0 ? snapshot.getInt (spaceOutIndex) : 0; mon != lastMonitor) // 1.0: headphones / speakers
    {
        lastMonitor = mon;
        for (auto& p : parts) p->setMonitorMode (mon);
        morphSpace.setMonitorMode (mon); // p16
        masterRack.setMonitorMode (mon);
    }
    const bool monoCheck = monoCheckIndex >= 0 && snapshot.getBool (monoCheckIndex);
    notesOutOn = snapshot.getBool (notesOutIndex);
    midiOut.clear();

    // 2. transport (host / Link, MIDI clock or internal clock; made in step 1)
    ppqForUi.store (tr.ppqAtBlockStart, std::memory_order_relaxed);
    bpmForUi.store (tr.bpm, std::memory_order_relaxed);
    playingForUi.store (tr.playing, std::memory_order_relaxed);

    // 3. live MIDI: on-screen keyboard (offset 0), host / device MIDI minus mapped controls and
    //    clock messages -> parts by channel (through scale / chord and the arpeggiator)
    numRouted = 0;
    UiNote un;
    while (uiNotes.pop (un))
        routeLive (un.on ? MidiEvent::noteOn (1, un.note, std::clamp ((int) (un.velocity * 127.0f), 1, 127), 0)
                         : MidiEvent::noteOff (1, un.note, 0));
    // p14: the on-screen wheels - a changed value goes out as pitch bend / CC 1 on the edit part's channel
    if (perfBendIndex >= 0 && perfModIndex >= 0)
    {
        const int edit = editPartIndex >= 0 ? std::clamp ((int) std::lround (snapshot[editPartIndex]) - 1, 0, numParts - 1) : 0;
        const int ch = routing[(size_t) edit].channel > 0 ? routing[(size_t) edit].channel : 1;
        if (const float b = snapshot[perfBendIndex]; ! (b == perfBendSent))
        {
            perfBendSent = b;
            const int v = std::clamp ((int) std::lround (8192.0f + std::clamp (b, -1.0f, 1.0f) * 8191.0f), 0, 16383);
            MidiEvent e;
            e.status = (uint8_t) (0xe0 | ((ch - 1) & 0x0f));
            e.data1 = (uint8_t) (v & 0x7f);
            e.data2 = (uint8_t) ((v >> 7) & 0x7f);
            routeLive (e);
        }
        if (const float m = snapshot[perfModIndex]; ! (m == perfModSent))
        {
            perfModSent = m;
            routeLive (MidiEvent::controller (ch, 1, std::clamp ((int) std::lround (m * 127.0f), 0, 127), 0));
        }
    }
    for (int i = 0; i < numMidi; ++i)
        if (midi[i].status < 0xf0 && ! mapper.handleMidi (midi[i]))
            routeLive (midi[i]);
    GridEvent ge; // p24: the on-screen grid keyboard plays one part
    while (gridEvents.pop (ge))
    {
        const auto& r = routing[(size_t) ge.part];
        if (r.mpe) { addRouted (ge.event, ge.part); continue; }
        if (! ge.event.isNoteOn() && ! ge.event.isNoteOff()) continue;
        auto x = ge.event;
        x.status = (uint8_t) ((x.status & 0xf0) | (((r.channel > 0 ? r.channel : 1) - 1) & 0x0f));
        x.sampleOffset = 0;
        routeToPart (x, ge.part);
    }

    // 4. sequencers: their notes address parts by channel
    seqOut.clear();
    stepSeq.setDrumsMode (snapshot.getBool (stepDrumsIndex));
    stepSeq.process (tr, numSamples, seqOut);
    euclidOut.clear();
    euclid.process (tr, numSamples, euclidOut);
    const bool stepToDrums = snapshot.getBool (stepDrumsIndex), euclidToDrums = snapshot.getBool (euclidDrumsIndex),
               orbitToDrums = snapshot.getBool (orbitDrumsIndex);
    orbitOut.clear();
    orbit.setDrumsMode (orbitToDrums);
    orbit.process (tr, numSamples, orbitOut);
    arpOut.clear();
    arp.process (tr, numSamples, arpOut);
    for (const auto& e : seqOut.events) routeGenerated (e, true, stepToDrums);
    for (const auto& e : euclidOut.events) routeGenerated (e, true, euclidToDrums);
    for (const auto& e : orbitOut.events)
    {
        if (e.channel() > OrbitSequencer::lanePartChannelOffset) // 0.5: a lane with its own target part
        {
            auto x = e;
            x.status = (uint8_t) ((x.status & 0xf0) | ((e.channel() - OrbitSequencer::lanePartChannelOffset - 1) & 0x0f));
            routeGenerated (x, true, false);
        }
        else
            routeGenerated (e, true, orbitToDrums);
    }
    for (const auto& e : arpOut.events) routeGenerated (e, false); // already shaped on the way in

    // stable sort by time (insertion sort: lists are short and mostly ordered)
    for (int i = 1; i < numRouted; ++i)
    {
        const auto e = routed[(size_t) i];
        int j = i - 1;
        while (j >= 0 && routed[(size_t) j].event.sampleOffset > e.event.sampleOffset) { routed[(size_t) j + 1] = routed[(size_t) j]; --j; }
        routed[(size_t) j + 1] = e;
    }

    // 5. render in chunks split at event times
    size_t nextLock = 0;
    int nextEvent = 0;
    int pos = 0;
    while (pos < numSamples)
    {
        applyLocks (lockStorage, nextLock, pos);
        while (nextEvent < numRouted && routed[(size_t) nextEvent].event.sampleOffset <= pos)
            dispatch (routed[(size_t) nextEvent++]);

        int end = std::min (numSamples, pos + (int) partL[0].size());
        if (nextEvent < numRouted) end = std::min (end, std::max (pos + 1, routed[(size_t) nextEvent].event.sampleOffset));
        if (nextLock < lockStorage.size()) end = std::min (end, std::max (pos + 1, lockStorage[nextLock].sampleOffset));
        const int len = end - pos;

        SourceBuffers src;
        const double chunkPpq = tr.ppqAtBlockStart + (double) pos * tr.bpm / 60.0 / sr;
        masterRack.setTempo (tr.bpm > 0.0 ? tr.bpm : 120.0);
        masterRack.setTransportPosition (chunkPpq, tr.playing); // 1.0: tempo-synced motion (Space 3D)
        for (int p = 0; p < numParts; ++p)
        {
            parts[(size_t) p]->setTransport (tr.bpm, chunkPpq, tr.playing); // 0.7: tempo-synced LFOs
            morphSpace.render (p, partL[(size_t) p].data(), partR[(size_t) p].data(), len, tr.bpm, chunkPpq, tr.playing); // p16: part + morph lanes
            src.left[(size_t) (source::part1 + p)] = partL[(size_t) p].data();
            src.right[(size_t) (source::part1 + p)] = partR[(size_t) p].data();
            scopeTap.offer (p, partL[(size_t) p].data(), partR[(size_t) p].data(), len); // 0.7: display scope (after the rack)
        }
        drums.render (drumL.data(), drumR.data(), len);
        src.left[(size_t) source::drum] = drumL.data();
        src.right[(size_t) source::drum] = drumR.data();
        renderTapeChunk (src, pos, len);
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o)
        {
            out.left[(size_t) o] = outputs.left[(size_t) o] != nullptr ? outputs.left[(size_t) o] + pos : nullptr;
            out.right[(size_t) o] = outputs.right[(size_t) o] != nullptr ? outputs.right[(size_t) o] + pos : nullptr;
        }
        perfPads.setTransport (tr.bpm, chunkPpq, tr.playing); // p25: quantised pads
        mix.process (src, out, len);
        if (monoCheck && out.left[0] != nullptr && out.right[0] != nullptr) // 1.0: mono check of the main output
            for (int i = 0; i < len; ++i)
                out.left[0][i] = out.right[0][i] = 0.5f * (out.left[0][i] + out.right[0][i]);
        scopeTap.offer (ScopeTap::Master, out.left[0], out.right[0], len); // 0.7: display scope
        pos = end;
    }
    while (nextEvent < numRouted) dispatch (routed[(size_t) nextEvent++]);
    applyLocks (lockStorage, nextLock, std::numeric_limits<int>::max()); // swung steps may lock after the block end

    // 6. MIDI clock out (follows the transport in use)
    if (clockOutOn) clockGen.process (tr, numSamples, midiOut);
    else clockGen.reset();
    midiOut.sortByTime();

    int voices = 0;
    inputL = inputR = nullptr;
    for (auto& p : parts) voices += p->activeVoiceCount();
    voices += drums.activeVoiceCount();
    voicesActive.store (voices, std::memory_order_relaxed);
    sampleTime += numSamples;
}

// ---------------------------------------------------------------- state
std::string Core::saveState (const std::string& uiJson) const
{
    state::Sections s;
    s["midimap"] = map.toJson();
    s["sequencer"] = stepSeq.toJson();
    s["orbit"] = orbit.toJson();
    s["tape"] = tapeMachine.settingsJson();
    for (int p = 0; p < numParts; ++p)
        if (morphSpace.anyCorner (p)) { s["morph"] = morphSpace.toJson(); break; } // p16
    if (motionRec.anyLane()) s["motion"] = motionRec.toJson(); // p22
    if (harmonyBrain.anyMemory()) s["harmony"] = harmonyBrain.toJson(); // p24
    if (! uiJson.empty()) s["ui"] = uiJson;
    for (const auto& [name, text] : extraSections)
        if (! text.empty() && s.find (name) == s.end()) s[name] = text;
    return state::saveState (registry, s);
}

bool Core::loadState (const std::string& json, std::string* uiJsonOut)
{
    auto r = state::loadState (json, registry, true);
    if (! r.ok) return false;
    map.clear();
    if (auto it = r.sections.find ("midimap"); it != r.sections.end()) map.fromJson (it->second);
    mapper.publish (map);
    if (auto it = r.sections.find ("sequencer"); it != r.sections.end()) stepSeq.fromJson (it->second);
    else stepSeq.clearPattern();
    extraSections.clear();
    for (const auto& [name, text] : r.sections)
        if (name != "midimap" && name != "sequencer" && name != "ui" && name != "tape" && name != "orbit" && name != "morph" && name != "motion" && name != "harmony") extraSections[name] = text;
    if (auto it = r.sections.find ("morph"); it != r.sections.end()) morphSpace.fromJson (it->second); // p16
    else morphSpace.clearAll();
    if (auto it = r.sections.find ("motion"); it != r.sections.end()) motionRec.fromJson (it->second); // p22
    else motionRec.clearAll();
    if (auto it = r.sections.find ("harmony"); it != r.sections.end()) harmonyBrain.fromJson (it->second); // p24
    else harmonyBrain.requestClear();
    if (auto it = r.sections.find ("tape"); it != r.sections.end()) tapeMachine.loadSettingsJson (it->second);
    if (auto it = r.sections.find ("orbit"); it != r.sections.end()) orbit.fromJson (it->second);
    else orbit.clearPattern();
    prepareEffects(); // 0.7: effect types of the loaded racks
    if (uiJsonOut != nullptr)
    {
        auto it = r.sections.find ("ui");
        *uiJsonOut = it != r.sections.end() ? it->second : std::string();
    }
    return true;
}
} // namespace ohpe
