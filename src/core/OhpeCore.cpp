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
    tapeMachine.bind (registry);
    for (int i = 0; i < numParts; ++i) inserts[(size_t) i].bind (registry, parts[(size_t) i]->prefix() + "fx.");
    stepSeq.setRegistry (&registry);
    tempoIndex = registry.indexOf (Ids::tempo);
    seqRunIndex = registry.indexOf (Ids::seqRun);
    for (const auto& d : stepSeq.getParamDescriptors())
        stepParamIndex.push_back (registry.indexOf (std::string (Ids::stepPrefix) + d.id));
    stepParamLast.assign (stepParamIndex.size(), NAN);

    snapshot.prepare (registry);
    lockStorage.reserve (1024);
    seqOut.locks = &lockStorage;
    seqOut.maxLocks = 1024;
    mapper.publish (map);
    prepare (48000.0, 512);
}

void Core::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = std::max (1, maxBlockSize);
    const int chunk = std::min (maxChunk, maxBlock);
    partL.assign ((size_t) chunk, 0.0f);
    partR.assign ((size_t) chunk, 0.0f);
    for (auto& p : parts) p->prepare (sr, chunk);
    mix.prepare (sr, chunk);
    stepSeq.prepare (sr, maxBlock);
    tapeMachine.prepare (sr, chunk);
    for (auto& f : inserts) f.prepare (sr, chunk);
    for (auto& t : tapeOut) t.assign ((size_t) chunk, 0.0f);
    std::fill (stepParamLast.begin(), stepParamLast.end(), NAN);
    reset();
}

void Core::reset() noexcept
{
    for (auto& p : parts) p->reset();
    mix.reset();
    tapeMachine.resetDsp();
    for (auto& f : inserts) f.reset();
    stepSeq.reset();
    snapshot.clearLocks();
    internalPpq = 0.0;
    internalWasRunning = false;
}

void Core::queueNote (int note, float velocity, bool on) noexcept
{
    uiNotes.push ({ note, velocity, on });
}

void Core::dispatch (const MidiEvent& e) noexcept
{
    if (e.isNoteOn()) lastNoteOn.store (e.data1, std::memory_order_relaxed);
    // phase 1: one part, omni. Later: channel N -> part N (MPE: zone -> part).
    parts[0]->handleMidi (e);
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
        for (auto& p : parts) p->pullParams (snapshot);
        mix.pullParams (snapshot);
        pullPhase2Params();
    }
}

void Core::pullPhase2Params() noexcept
{
    tapeMachine.pullParams (snapshot);
    for (auto& f : inserts) f.pullParams (snapshot);
}

void Core::renderTapeChunk (SourceBuffers& src, int pos, int len) noexcept
{
    inserts[0].process (partL.data(), partR.data(), len); // FX slot of part 1 (more parts: their own buffers)

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

void Core::process (const OutputBuffers& outputs, int numSamples, const MidiEvent* midi, int numMidi, const Transport& host) noexcept
{
    // 1. parameters for this block
    snapshot.pull (registry);
    for (auto& p : parts) p->pullParams (snapshot);
    mix.pullParams (snapshot);
    pullPhase2Params();
    for (size_t i = 0; i < stepParamIndex.size(); ++i)
        if (const float v = snapshot[stepParamIndex[i]]; ! (v == stepParamLast[i]))
        {
            stepParamLast[i] = v;
            stepSeq.setParam ((int) i, v);
        }

    // 2. transport (host or internal clock)
    Transport tr = host;
    const bool run = snapshot.getBool (seqRunIndex);
    if (host.hostDriven)
        tr.playing = host.playing && run;
    else
    {
        if (run && ! internalWasRunning) internalPpq = 0.0;
        internalWasRunning = run;
        tr.bpm = snapshot[tempoIndex];
        tr.playing = run;
        tr.ppqAtBlockStart = internalPpq;
        if (run) internalPpq += numSamples * tr.bpm / 60.0 / sr;
    }
    ppqForUi.store (tr.ppqAtBlockStart, std::memory_order_relaxed);

    // 3. sequencer events
    seqOut.clear();
    stepSeq.process (tr, numSamples, seqOut);

    // 4. merge MIDI: on-screen keyboard (offset 0), host MIDI (minus mapped / learned), sequencer
    merged.clear();
    UiNote un;
    while (uiNotes.pop (un))
        merged.add (un.on ? MidiEvent::noteOn (1, un.note, std::clamp ((int) (un.velocity * 127.0f), 1, 127), 0)
                          : MidiEvent::noteOff (1, un.note, 0));
    for (int i = 0; i < numMidi; ++i)
        if (! mapper.handleMidi (midi[i]))
            merged.add (midi[i]);
    for (const auto& e : seqOut.events) merged.add (e);
    merged.sortByTime();

    // 5. render in chunks split at event times
    size_t nextLock = 0;
    int nextEvent = 0;
    int pos = 0;
    while (pos < numSamples)
    {
        applyLocks (lockStorage, nextLock, pos);
        while (nextEvent < merged.size() && merged[nextEvent].sampleOffset <= pos)
            dispatch (merged[nextEvent++]);

        int end = std::min (numSamples, pos + (int) partL.size());
        if (nextEvent < merged.size()) end = std::min (end, std::max (pos + 1, merged[nextEvent].sampleOffset));
        if (nextLock < lockStorage.size()) end = std::min (end, std::max (pos + 1, lockStorage[nextLock].sampleOffset));
        const int len = end - pos;

        parts[0]->render (partL.data(), partR.data(), len);

        SourceBuffers src;
        src.left[(size_t) source::part1] = partL.data();
        src.right[(size_t) source::part1] = partR.data();
        renderTapeChunk (src, pos, len);
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o)
        {
            out.left[(size_t) o] = outputs.left[(size_t) o] != nullptr ? outputs.left[(size_t) o] + pos : nullptr;
            out.right[(size_t) o] = outputs.right[(size_t) o] != nullptr ? outputs.right[(size_t) o] + pos : nullptr;
        }
        mix.process (src, out, len);
        pos = end;
    }
    while (nextEvent < merged.size()) dispatch (merged[nextEvent++]);
    applyLocks (lockStorage, nextLock, std::numeric_limits<int>::max()); // swung steps may lock after the block end

    int voices = 0;
    inputL = inputR = nullptr;
    for (auto& p : parts) voices += p->activeVoiceCount();
    voicesActive.store (voices, std::memory_order_relaxed);
}

std::string Core::saveState (const std::string& uiJson) const
{
    state::Sections s;
    s["midimap"] = map.toJson();
    s["sequencer"] = stepSeq.toJson();
    s["tape"] = tapeMachine.settingsJson();
    if (! uiJson.empty()) s["ui"] = uiJson;
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
    if (auto it = r.sections.find ("tape"); it != r.sections.end()) tapeMachine.loadSettingsJson (it->second);
    if (uiJsonOut != nullptr)
    {
        auto it = r.sections.find ("ui");
        *uiJsonOut = it != r.sections.end() ? it->second : std::string();
    }
    return true;
}
} // namespace ohpe
