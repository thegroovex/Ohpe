// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Host test (p80 "host matrix"): loads the built plug-in (VST3, and the Audio Unit on macOS) through JUCE's own
// hosting code, the way a DAW does, and checks what a host relies on. Every check fails the run on a crash, a
// NaN / Inf / runaway sample, a hang (watchdog) or a mismatch.
//
//   usage: ohpe_hosttest <Ohpe.vst3 | Ohpe.component> [--section <name>] [--source <repository root>]
//
// Sections (default "basic"; "all" runs every one in turn):
//   basic       bundle scan, instrument, buses, every host parameter ID of tests/core/golden/param_ids_v1.txt is
//               exposed (no hash collisions), idle silence, a note plays, state in a second instance
//   lifecycle   create / prepare / play / release / destroy repeatedly, prepare twice, release + prepare again
//   state       save / restore round trip: byte-identical getStateInformation over instances (all four parts,
//               presets per part, tape audio, mixer / FX / tape parameters), restore into the same instance
//   automation  a sample of parameter IDs ramped while notes play, sample-accurate ramps (1-sample blocks),
//               automation reaches the sound and the saved state, deterministic
//   bounce      offline (non-realtime) renders: two instances and two passes of one instance are bit-identical
//   rates       44.1 / 48 / 96 kHz x blocks 32 .. 2048, odd and changing block sizes, 1-sample blocks
//   bypass      the host bypass parameter: silent while bypassed, plays again afterwards
//   latency     reported latency per sample rate, constant while playing, no sound before it
//   multi       several instances at once, processed on their own threads: each sounds like it does alone
//   editor      processing with no editor, while the editor opens and closes (audio on its own thread)
//   transport   host transport start / stop / loop / relocate with tempo changes: the metronome clicks on the
//               host's beats (sample-accurate), silence while stopped

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <vector>

namespace
{
int failures = 0;
constexpr float maxSample = 4.0f; // +12 dBFS: anything louder is a runaway (Ohpe has a limiter on its output)

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  [ OK ] " : "  [FAIL] ") << what << std::endl;
    if (! condition) ++failures;
}

void info (const juce::String& what) { std::cout << "         " << what << std::endl; }

void pump (int ms)
{
    if (auto* mm = juce::MessageManager::getInstance())
        mm->runDispatchLoopUntil (ms);
}

// ---------------------------------------------------------------------------------------------- host
struct Host
{
    juce::AudioPluginFormatManager formats;
    juce::PluginDescription desc;
    juce::String format;
    std::vector<juce::String> ids; // golden host parameter IDs (string IDs of the plug-in)

    bool scan (const juce::File& file)
    {
        formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
       #if JUCE_PLUGINHOST_AU && JUCE_MAC
        formats.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
       #endif
        juce::OwnedArray<juce::PluginDescription> types;
        for (auto* f : formats.getFormats())
            if (f->fileMightContainThisPluginType (file.getFullPathName()))
                f->findAllTypesForFile (types, file.getFullPathName());
        check (types.size() == 1, "plug-in found in the bundle (" + juce::String (types.size()) + ")");
        if (types.isEmpty()) return false;
        desc = *types[0];
        format = desc.pluginFormatName;
        info ("format " + format + ", " + desc.fileOrIdentifier);
        return true;
    }

    std::unique_ptr<juce::AudioPluginInstance> create (double rate = 48000.0, int block = 512, bool prepare = true, bool allBuses = true)
    {
        juce::String error;
        auto p = formats.createPluginInstance (desc, rate, block, error);
        if (p == nullptr)
        {
            check (false, "instance created " + error);
            return nullptr;
        }
        if (allBuses) p->enableAllBuses();
        if (prepare)
        {
            p->setRateAndBufferSizeDetails (rate, block);
            p->prepareToPlay (rate, block);
        }
        return p;
    }
};

// host parameter by the plug-in's string ID: the host sees the hashed ID (VST3 / AU, "Studio One compatible" mask)
struct ParamMap
{
    std::map<juce::String, juce::AudioProcessorParameter*> byHostId;

    explicit ParamMap (juce::AudioPluginInstance& p)
    {
        for (auto* prm : p.getParameters())
            if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (prm))
                byHostId[h->getParameterID()] = prm;
    }

    static juce::String hostId (const juce::String& id, bool masked)
    {
        auto h = (juce::uint32) id.hashCode();
        if (masked) h &= 0x7fffffffu;
        return juce::String (h);
    }

    juce::AudioProcessorParameter* operator() (const juce::String& id) const
    {
        for (bool masked : { true, false })
            if (auto it = byHostId.find (hostId (id, masked)); it != byHostId.end()) return it->second;
        return nullptr;
    }
};

void setParam (juce::AudioProcessorParameter* p, float v)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    p->endChangeGesture();
}

void setPlain (const ParamMap& pm, const juce::String& id, float normalised)
{
    auto* p = pm (id);
    check (p != nullptr, "parameter " + id + " reachable");
    setParam (p, normalised);
}

// ---------------------------------------------------------------------------------------------- rendering
struct Event
{
    juce::int64 at;
    juce::MidiMessage msg;
};

struct Render
{
    juce::AudioBuffer<float> out; // every output channel, the whole render
    bool finite = true;
    juce::int64 badAt = -1;
    float peak = 0.0f;

    float rms (int ch, juce::int64 from, juce::int64 to) const
    {
        from = juce::jlimit<juce::int64> (0, out.getNumSamples(), from);
        to = juce::jlimit<juce::int64> (from, out.getNumSamples(), to);
        if (to <= from) return 0.0f;
        return out.getRMSLevel (ch, (int) from, (int) (to - from));
    }
    float mainPeak (juce::int64 from = 0, juce::int64 to = -1) const
    {
        if (to < 0) to = out.getNumSamples();
        from = juce::jlimit<juce::int64> (0, out.getNumSamples(), from);
        to = juce::jlimit<juce::int64> (from, out.getNumSamples(), to);
        if (to <= from) return 0.0f;
        return juce::jmax (out.getMagnitude (0, (int) from, (int) (to - from)), out.getMagnitude (1, (int) from, (int) (to - from)));
    }
};

using BlockSize = std::function<int (juce::int64 pos, int index)>;
using BlockHook = std::function<void (juce::int64 pos, int n)>;

BlockSize fixed (int n) { return [n] (juce::int64, int) { return n; }; }

/** Renders `total` samples in blocks, MIDI events at their exact sample, `hook` before every block (automation,
    playhead). Checks every output sample for NaN / Inf / runaway values. */
Render render (juce::AudioPluginInstance& p, juce::int64 total, BlockSize blockSize, std::vector<Event> events = {},
               BlockHook hook = {}, bool keep = true)
{
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.at < b.at; });
    const int outs = juce::jmax (2, p.getTotalNumOutputChannels());
    const int chans = juce::jmax (outs, p.getTotalNumInputChannels());
    Render r;
    r.out.setSize (keep ? outs : 2, keep ? (int) total : 1);
    r.out.clear();
    juce::AudioBuffer<float> io (chans, 8192);
    juce::MidiBuffer midi;
    size_t ev = 0;
    juce::int64 pos = 0;
    int index = 0;
    while (pos < total)
    {
        const int n = (int) juce::jmin<juce::int64> (total - pos, juce::jlimit (1, 8192, blockSize (pos, index++)));
        if (hook) hook (pos, n);
        midi.clear();
        while (ev < events.size() && events[ev].at < pos + n)
        {
            midi.addEvent (events[ev].msg, (int) juce::jmax<juce::int64> (0, events[ev].at - pos));
            ++ev;
        }
        juce::AudioBuffer<float> view (io.getArrayOfWritePointers(), chans, n);
        view.clear();
        p.processBlock (view, midi);
        for (int ch = 0; ch < outs; ++ch)
        {
            const float* d = view.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float v = d[i];
                if (! std::isfinite (v) || std::abs (v) > maxSample)
                {
                    if (r.finite) r.badAt = pos + i;
                    r.finite = false;
                }
                else if (ch < 2) r.peak = juce::jmax (r.peak, std::abs (v));
            }
            if (keep) r.out.copyFrom (ch, (int) pos, d, n);
        }
        pos += n;
    }
    return r;
}

std::vector<Event> chord (juce::int64 at, juce::int64 length, std::initializer_list<int> channels = { 1, 2, 3, 4 }, int note = 60)
{
    std::vector<Event> e;
    int k = 0;
    for (int ch : channels)
    {
        const int n = note + 4 * k++;
        e.push_back ({ at, juce::MidiMessage::noteOn (ch, n, (juce::uint8) 100) });
        if (length > 0) e.push_back ({ at + length, juce::MidiMessage::noteOff (ch, n) });
    }
    return e;
}

/** Silent blocks while the message loop runs (timers, parameter triggers, background loading). */
void settle (juce::AudioPluginInstance& p, int blocks = 25, int block = 256)
{
    juce::AudioBuffer<float> io (juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels())), block);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        io.clear();
        midi.clear();
        p.processBlock (io, midi);
        pump (8);
    }
}

bool sameAudio (const Render& a, const Render& b, juce::String& where)
{
    if (a.out.getNumChannels() != b.out.getNumChannels() || a.out.getNumSamples() != b.out.getNumSamples())
    {
        where = "different sizes";
        return false;
    }
    for (int ch = 0; ch < a.out.getNumChannels(); ++ch)
        for (int i = 0; i < a.out.getNumSamples(); ++i)
            if (! (a.out.getSample (ch, i) == b.out.getSample (ch, i)))
            {
                where = "channel " + juce::String (ch) + " sample " + juce::String (i) + ": " + juce::String (a.out.getSample (ch, i), 9)
                        + " vs " + juce::String (b.out.getSample (ch, i), 9);
                return false;
            }
    return true;
}

juce::MemoryBlock stateOf (juce::AudioPluginInstance& p)
{
    juce::MemoryBlock m;
    p.getStateInformation (m);
    return m;
}

juce::String firstDifference (const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    const size_t n = juce::jmin (a.getSize(), b.getSize());
    size_t i = 0;
    while (i < n && a[i] == b[i]) ++i;
    if (i == n && a.getSize() == b.getSize()) return "identical";
    auto excerpt = [i] (const juce::MemoryBlock& m) {
        const size_t from = i > 60 ? i - 60 : 0, to = juce::jmin (m.getSize(), i + 60);
        juce::String s;
        for (size_t k = from; k < to; ++k)
        {
            const char c = m[k];
            s << (c >= 32 && c < 127 ? juce::String::charToString ((juce::juce_wchar) (unsigned char) c) : juce::String ("."));
        }
        return s;
    };
    return "sizes " + juce::String ((int) a.getSize()) + " / " + juce::String ((int) b.getSize()) + ", first difference at byte "
           + juce::String ((int) i) + "\n           A: " + excerpt (a) + "\n           B: " + excerpt (b);
}

juce::var stateJson (const juce::MemoryBlock& m)
{
    const char* d = static_cast<const char*> (m.getData());
    size_t n = 0;
    while (n < m.getSize() && d[n] != 0) ++n;
    return juce::JSON::parse (juce::String::fromUTF8 (d, (int) n));
}

bool hasTapeAudio (const juce::MemoryBlock& m)
{
    const char* d = static_cast<const char*> (m.getData());
    size_t n = 0;
    while (n < m.getSize() && d[n] != 0) ++n;
    return m.getSize() > n + 1000;
}

void dumpOnMismatch (const juce::String& tag, const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    if (auto* dir = std::getenv ("OHPE_HOSTTEST_DUMP"))
    {
        juce::File (juce::String (dir) + "." + tag + ".A.bin").replaceWithData (a.getData(), a.getSize());
        juce::File (juce::String (dir) + "." + tag + ".B.bin").replaceWithData (b.getData(), b.getSize());
    }
}

/** Parameter IDs that are actions (dialogs, files, library, recording, network sync) rather than sound settings. */
bool isAction (const juce::String& id)
{
    for (auto* prefix : { "lib.", "bounce.", "tape.edit.", "tape.tx.", "ui.", "sync.", "voice." })
        if (id.startsWith (prefix)) return true;
    for (auto* exact : { "tape.rec", "tapemem.go", "harmony.take", "harmony.clear", "metro.tap", "tape.loopsnap" })
        if (id == exact) return true;
    for (auto* suffix : { ".motion.rec", ".motion.clear", ".voice.map", ".retrig" })
        if (id.endsWith (suffix)) return true;
    return false;
}

// ---------------------------------------------------------------------------------------------- sections
struct Context
{
    Host host;
    juce::File source;
};

void basic (Context& c)
{
    auto& h = c.host;
    check (h.desc.name == "Ohpe", "name is Ohpe (" + h.desc.name + ")");
    check (h.desc.isInstrument, "registered as an instrument");
    auto plugin = h.create (48000.0, 512, false, false);
    if (plugin == nullptr) return;
    check (plugin->acceptsMidi(), "accepts MIDI");
    check (plugin->getBusCount (false) == 9, "9 output buses (main + 8 aux): " + juce::String (plugin->getBusCount (false)));
    check (plugin->getBusCount (true) == 1, "1 input bus (tape recording): " + juce::String (plugin->getBusCount (true)));
    check (plugin->hasEditor(), "has an editor");

    int params = 0;
    for (auto* p : plugin->getParameters())
        if (! p->getName (64).startsWith ("MIDI CC") && p->getName (64) != "Bypass") ++params;
    check (params >= (int) h.ids.size(), juce::String (params) + " parameters exposed (" + juce::String ((int) h.ids.size()) + " IDs in v1)");

    // every v1 ID reachable by its host ID, no two IDs share a host ID (a collision would mix up automation lanes)
    ParamMap pm (*plugin);
    std::set<juce::String> hostIds;
    int missing = 0;
    juce::String firstMissing;
    for (const auto& id : h.ids)
    {
        hostIds.insert (ParamMap::hostId (id, true));
        if (pm (id) == nullptr && missing++ == 0) firstMissing = id;
    }
    check (missing == 0, "every v1 parameter ID reachable by its host ID" + (missing ? " (missing " + juce::String (missing) + ", first " + firstMissing + ")" : juce::String()));
    check (hostIds.size() == h.ids.size(), "no host ID collisions (" + juce::String ((int) hostIds.size()) + " unique)");

    plugin->enableAllBuses();
    plugin->prepareToPlay (48000.0, 512);
    settle (*plugin);
    auto idle = render (*plugin, 48000 / 4, fixed (512));
    check (idle.finite && idle.peak == 0.0f, "silent while idle (peak " + juce::String (idle.peak) + ")");
    auto note = render (*plugin, 48000, fixed (512), chord (0, 24000, { 1 }));
    check (note.finite && note.peak > 0.01f, "a note plays (peak " + juce::String (note.peak) + ")");

    auto state = stateOf (*plugin);
    check (state.getSize() > 1000, "state saved (" + juce::String ((int) state.getSize()) + " bytes)");
    auto second = h.create();
    if (second != nullptr)
    {
        second->setStateInformation (state.getData(), (int) state.getSize());
        settle (*second);
        auto again = stateOf (*second);
        check (again == state, "state restored identically in a second instance: " + firstDifference (state, again));
        if (! (again == state)) dumpOnMismatch ("basic", state, again);
    }
    plugin->releaseResources();
}

void lifecycle (Context& c)
{
    auto& h = c.host;
    bool ok = true;
    for (int i = 0; i < 8; ++i)
    {
        auto p = h.create (i % 2 ? 44100.0 : 48000.0, i % 3 ? 256 : 512, true, i % 2 == 0);
        if (p == nullptr) { ok = false; break; }
        auto r = render (*p, 9000, fixed (i % 3 ? 256 : 512), chord (0, 4000, { 1, 2 }), {}, false);
        ok = ok && r.finite;
        p->releaseResources();
    }
    check (ok, "8 x create / prepare / play / release / destroy");

    ok = true;
    for (int i = 0; i < 5; ++i)
        ok = ok && h.create (48000.0, 512, false) != nullptr;
    check (ok, "5 x create / destroy without prepareToPlay");

    if (auto p = h.create())
    {
        p->prepareToPlay (96000.0, 1024); // again, other settings, without release (some hosts do)
        auto a = render (*p, 20000, fixed (1024), chord (0, 9000), {}, false);
        p->releaseResources();
        p->releaseResources(); // twice
        p->prepareToPlay (44100.0, 128);
        auto b = render (*p, 20000, fixed (128), chord (0, 9000), {}, false);
        p->releaseResources();
        check (a.finite && b.finite && a.peak > 0.01f && b.peak > 0.01f, "prepare twice, release twice, prepare again: plays");
    }

    // several instances alive, destroyed in a different order
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> many;
    for (int i = 0; i < 4; ++i) many.push_back (h.create());
    many.erase (many.begin() + 1);
    many.erase (many.begin() + 2);
    many.clear();
    check (true, "4 instances alive, destroyed out of order");
}

void state (Context& c)
{
    auto& h = c.host;
    auto a = h.create();
    if (a == nullptr) return;
    ParamMap pm (*a);

    // start from the fixture with tape audio (a session saved with a recorded tape)
    const auto fixture = c.source.getChildFile ("tests/formats/fixtures/state/plugin-state-v1-with-tape.bin");
    juce::MemoryBlock fx;
    check (fixture.loadFileAsData (fx), "fixture state with tape loaded (" + fixture.getFileName() + ")");
    a->setStateInformation (fx.getData(), (int) fx.getSize());
    settle (*a);

    // a preset in every part (the host-mappable "next preset" trigger on the edit part)
    auto* part = pm ("ui.part");
    auto* next = pm ("lib.next");
    check (part != nullptr && next != nullptr, "ui.part / lib.next reachable");
    for (int k = 0; k < 4 && part != nullptr && next != nullptr; ++k)
    {
        setParam (part, (float) k / 3.0f);
        settle (*a, 10);
        for (int step = 0; step <= k; ++step) // a different preset per part
        {
            setParam (next, 1.0f);
            settle (*a, 12);
            setParam (next, 0.0f);
            settle (*a, 12);
        }
    }
    setParam (part, 1.0f / 3.0f); // edit part 2

    // sound settings in all four parts, the mixer, master FX and the tape
    juce::StringArray touched { "p1.tune", "p2.amp.a", "p3.lfo.rate", "p4.vol", "mix.ch3.pan", "mix.ch2.senda",
                                "mfx1.hyper.retrig", "tape.speed", "tape.wow", "tape.tr2.level", "metro.level", "key.root" };
    for (int i = 0; i < touched.size(); ++i)
        if (auto* p = pm (touched[i])) setParam (p, 0.13f + 0.061f * (float) i);
    settle (*a, 40);
    render (*a, 24000, fixed (512), chord (0, 12000), {}, false);
    settle (*a, 20);

    const auto s1 = stateOf (*a);
    const auto json = stateJson (s1);
    check (json.isObject() && json["format"].toString() == "ohpe-state", "state JSON readable");
    const auto presets = json["sections"]["ui"]["presets"];
    int named = 0;
    std::set<juce::String> distinct;
    if (auto* arr = presets.getArray())
        for (auto& u : *arr)
            if (u.toString().isNotEmpty()) { ++named; distinct.insert (u.toString()); }
    check (named == 4, "a preset in each of the four parts (" + juce::String (named) + ")");
    check (distinct.size() >= 2, "different presets in the parts (" + juce::String ((int) distinct.size()) + " distinct)");
    check (hasTapeAudio (s1), "tape audio in the state (" + juce::String ((int) s1.getSize()) + " bytes)");
    int inJson = 0;
    if (auto* o = json["params"].getDynamicObject())
        for (int i = 0; i < touched.size(); ++i)
            if (o->hasProperty (touched[i])) ++inJson;
    check (inJson >= touched.size() - 1, "host-set parameters in the state (" + juce::String (inJson) + " of " + juce::String (touched.size()) + ")");

    // round trip over fresh instances: byte-identical
    auto b = h.create();
    b->setStateInformation (s1.getData(), (int) s1.getSize());
    settle (*b, 40);
    const auto s2 = stateOf (*b);
    check (s2 == s1, "restored in a second instance: getStateInformation identical: " + firstDifference (s1, s2));
    if (! (s2 == s1)) dumpOnMismatch ("state12", s1, s2);

    auto c3 = h.create (48000.0, 256); // another block size and only the main bus: the session must not change
    c3->releaseResources();
    c3->disableNonMainBuses();
    c3->prepareToPlay (48000.0, 256);
    c3->setStateInformation (s2.getData(), (int) s2.getSize());
    settle (*c3, 40);
    const auto s3 = stateOf (*c3);
    check (s3 == s1, "restored in a third instance (256-sample blocks, main bus only): identical: " + firstDifference (s1, s3));
    if (! (s3 == s1)) dumpOnMismatch ("state13", s1, s3);

    // back into the first instance (undo / reload in the same session), after playing
    render (*a, 24000, fixed (512), chord (0, 12000), {}, false);
    a->setStateInformation (s1.getData(), (int) s1.getSize());
    settle (*a, 40);
    const auto s4 = stateOf (*a);
    check (s4 == s1, "restored into the same instance: identical: " + firstDifference (s1, s4));

    // the restored instance plays and the host sees the restored parameter values
    auto r = render (*b, 48000, fixed (512), chord (0, 24000));
    check (r.finite && r.peak > 0.001f, "restored instance plays (peak " + juce::String (r.peak) + ")");
    ParamMap pmb (*b);
    int same = 0;
    for (int i = 0; i < touched.size(); ++i)
        if (auto* pa = pm (touched[i]))
            if (auto* pb = pmb (touched[i]))
                if (std::abs (pa->getValue() - pb->getValue()) < 1.0e-4f) ++same;
    check (same == touched.size(), "host parameter values restored (" + juce::String (same) + " of " + juce::String (touched.size()) + ")");

    // an empty / damaged state never takes the instance down
    a->setStateInformation (nullptr, 0);
    const char junk[] = "{\"format\":\"ohpe-state\",\"version\":1,\"params\":{\"p1.vol\":";
    a->setStateInformation (junk, (int) sizeof (junk));
    settle (*a);
    auto after = render (*a, 24000, fixed (512), chord (0, 12000), {}, false);
    check (after.finite, "damaged / empty state: instance keeps running");
}

void automation (Context& c)
{
    auto& h = c.host;
    auto p = h.create();
    if (p == nullptr) return;
    ParamMap pm (*p);
    settle (*p);

    // a sample of the parameter IDs (sound settings, not actions), ramped 0 -> 1 -> default over 2048 samples while
    // notes play in all four parts
    std::vector<juce::String> sample;
    std::vector<juce::String> sound;
    for (const auto& id : h.ids)
        if (! isAction (id)) sound.push_back (id);
    for (size_t i = 0; i < sound.size(); i += 41) sample.push_back (sound[i]);
    for (auto* must : { "p1.vol", "p1.tune", "p2.analog.cutoff", "p3.engine", "p4.glide", "tempo", "mix.ch1.vol", "tape.play", "tape.speed" })
        sample.push_back (must);

    int bad = 0, unreachable = 0;
    juce::String firstBad;
    juce::int64 total = 0;
    for (const auto& id : sample)
    {
        auto* prm = pm (id);
        if (prm == nullptr) { ++unreachable; continue; }
        const float def = prm->getDefaultValue();
        auto r = render (*p, 2048, fixed (32), chord (0, 1800), [prm, def] (juce::int64 pos, int) {
            const float t = (float) pos / 2048.0f;
            prm->setValue (t < 0.5f ? t * 2.0f : 1.0f - (t - 0.5f) * 2.0f * (1.0f - def));
        }, false);
        prm->setValueNotifyingHost (def);
        total += 2048;
        if (! r.finite && bad++ == 0) firstBad = id + " at sample " + juce::String (r.badAt);
    }
    settle (*p, 10);
    check (unreachable == 0, "sampled parameter IDs reachable (" + juce::String ((int) sample.size()) + " IDs)");
    check (bad == 0, juce::String ((int) sample.size()) + " parameters ramped while playing: no NaN / Inf / runaway" + (bad ? " (" + juce::String (bad) + " bad, first " + firstBad + ")" : juce::String()));

    // sample-accurate ramps: the volume of part 1 automated per sample (1-sample blocks) - deterministic over two
    // fresh instances, and it reaches the sound
    auto rampRender = [&h] (bool rampDown, int blockSize) {
        auto q = h.create();
        ParamMap qm (*q);
        settle (*q);
        auto* vol = qm ("p1.vol");
        const float start = vol != nullptr ? vol->getValue() : 0.0f;
        Render r = render (*q, 24000, fixed (blockSize), chord (0, 0, { 1 }), [vol, start, rampDown] (juce::int64 pos, int) {
            if (vol == nullptr || ! rampDown) return;
            const float t = juce::jlimit (0.0f, 1.0f, (float) (pos - 4800) / 9600.0f);
            vol->setValue (start * (1.0f - t));
        });
        return r;
    };
    auto flat = rampRender (false, 1);
    auto down1 = rampRender (true, 1);
    auto down1b = rampRender (true, 1);
    auto down64 = rampRender (true, 64);
    juce::String where;
    check (down1.finite && down64.finite && flat.finite, "per-sample automation: finite and bounded");
    check (sameAudio (down1, down1b, where), "per-sample automation renders identically twice " + where);
    const float tailFlat = flat.rms (0, 19200, 24000), tailDown = down1.rms (0, 19200, 24000), tail64 = down64.rms (0, 19200, 24000);
    const float headFlat = flat.rms (0, 1000, 4800), headDown = down1.rms (0, 1000, 4800);
    check (tailFlat > 1.0e-3f && tailDown < tailFlat * 0.25f, "volume ramp reaches the sound (tail rms " + juce::String (tailDown, 5) + " vs " + juce::String (tailFlat, 5) + ")");
    check (std::abs (headDown - headFlat) <= 1.0e-6f + headFlat * 1.0e-3f, "nothing changes before the ramp starts (sample-accurate)");
    check (std::abs (tail64 - tailDown) <= 0.05f * tailFlat + 1.0e-5f, "1-sample and 64-sample automation end at the same level");

    // automation lands in the saved state, and in a restored instance
    auto* tune = pm ("p1.tune");
    if (tune != nullptr)
    {
        setParam (tune, 0.8f);
        settle (*p, 5);
        const auto s = stateOf (*p);
        const auto v1 = stateJson (s)["params"]["p1.tune"];
        setParam (tune, 0.3f);
        settle (*p, 5);
        const auto v2 = stateJson (stateOf (*p))["params"]["p1.tune"];
        check (! v1.isVoid() && ! v2.isVoid() && ! ((double) v1 == (double) v2), "automated value in the saved state (" + v1.toString() + " / " + v2.toString() + ")");
        auto q = h.create();
        q->setStateInformation (s.getData(), (int) s.getSize());
        settle (*q);
        ParamMap qm (*q);
        check (qm ("p1.tune") != nullptr && std::abs (qm ("p1.tune")->getValue() - 0.8f) < 1.0e-3f, "automated value restored for the host (" + juce::String (qm ("p1.tune") ? qm ("p1.tune")->getValue() : -1.0f) + ")");
    }
}

/** One offline bounce: the fixture session with tape playing, notes in all parts, 3 s, every output. */
Render bounceOnce (juce::AudioPluginInstance& p, const juce::MemoryBlock& session)
{
    p.setNonRealtime (true); // takes effect with the next prepareToPlay (VST3 process mode "offline")
    p.setStateInformation (session.getData(), (int) session.getSize());
    settle (p, 30);
    p.releaseResources();
    p.prepareToPlay (48000.0, 512); // a host prepares (resets) the instance before every bounce
    std::vector<Event> ev = chord (0, 60000);
    auto more = chord (72000, 30000, { 1, 3 }, 67);
    ev.insert (ev.end(), more.begin(), more.end());
    auto r = render (p, 144000, fixed (512), ev);
    p.setNonRealtime (false);
    return r;
}

void bounce (Context& c)
{
    auto& h = c.host;
    auto a = h.create();
    auto b = h.create();
    if (a == nullptr || b == nullptr) return;
    // the session: the fixture with tape audio, the tape playing, some sound settings
    juce::MemoryBlock fx;
    c.source.getChildFile ("tests/formats/fixtures/state/plugin-state-v1-with-tape.bin").loadFileAsData (fx);
    a->setStateInformation (fx.getData(), (int) fx.getSize());
    ParamMap pm (*a);
    if (auto* tp = pm ("tape.play")) setParam (tp, 1.0f);
    if (auto* ch = pm ("p2.analog.cutoff")) setParam (ch, 0.3f);
    settle (*a, 20);
    const auto session = stateOf (*a);

    auto r1 = bounceOnce (*a, session);
    auto r2 = bounceOnce (*b, session);
    auto r3 = bounceOnce (*a, session);
    juce::String where;
    check (r1.finite && r2.finite && r3.finite, "offline bounces finite and bounded");
    check (r1.peak > 0.01f, "offline bounce has sound (peak " + juce::String (r1.peak) + ")");
    check (sameAudio (r1, r2, where), "two instances bounce bit-identically (" + juce::String (r1.out.getNumChannels()) + " channels) " + where);
    check (sameAudio (r1, r3, where), "two bounces of one instance bit-identical " + where);
}

void rates (Context& c)
{
    auto& h = c.host;
    auto p = h.create (44100.0, 512, true, false); // the main bus only, as most hosts start
    if (p == nullptr) return;
    settle (*p);
    std::map<int, float> rmsAt;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        bool ok = true;
        float quietest = 1.0f;
        for (int block : { 32, 64, 128, 256, 512, 1024, 2048 })
        {
            p->releaseResources();
            p->setRateAndBufferSizeDetails (rate, block);
            p->prepareToPlay (rate, block);
            const auto len = (juce::int64) (rate * 0.3);
            auto r = render (*p, len, fixed (block), chord (0, len / 2, { 1 }), {}, false);
            ok = ok && r.finite;
            quietest = juce::jmin (quietest, r.peak);
            if (! r.finite) info ("bad at " + juce::String (rate) + " Hz / " + juce::String (block) + ": sample " + juce::String (r.badAt));
        }
        check (ok && quietest > 0.01f, juce::String (rate / 1000.0, 1) + " kHz, blocks 32 .. 2048: plays, finite (quietest peak " + juce::String (quietest) + ")");

        // odd and changing block sizes inside one prepare (max 2048), and 1-sample blocks
        p->releaseResources();
        p->prepareToPlay (rate, 2048);
        static const int odd[] = { 1, 7, 31, 33, 100, 333, 1000, 2047, 2048, 17, 999, 3, 512, 1023, 65 };
        auto r = render (*p, (juce::int64) (rate * 0.5), [] (juce::int64, int i) { return odd[(size_t) i % std::size (odd)]; },
                         chord (0, (juce::int64) (rate * 0.25), { 1, 2 }));
        check (r.finite && r.peak > 0.01f, juce::String (rate / 1000.0, 1) + " kHz, odd / changing block sizes (1 .. 2048): plays, finite");
        auto one = render (*p, 4800, fixed (1), chord (0, 2400, { 1 }), {}, false);
        check (one.finite && one.peak > 0.001f, juce::String (rate / 1000.0, 1) + " kHz, 1-sample blocks: plays, finite");

        // loudness of the same note at this rate (the sound must not depend on the rate)
        p->releaseResources();
        p->prepareToPlay (rate, 512);
        settle (*p, 4);
        auto ref = render (*p, (juce::int64) (rate * 0.5), fixed (512), chord (0, (juce::int64) (rate * 0.4), { 1 }));
        rmsAt[(int) rate] = ref.rms (0, 0, ref.out.getNumSamples());
    }
    const float r44 = rmsAt[44100], r96 = rmsAt[96000];
    check (r44 > 0.0f && r96 > 0.0f && std::abs (std::log10 (r96 / r44) * 20.0f) < 3.0f,
           "same note about as loud at 44.1 and 96 kHz (" + juce::String (juce::Decibels::gainToDecibels (r44), 1) + " / " + juce::String (juce::Decibels::gainToDecibels (r96), 1) + " dB)");
}

void bypass (Context& c)
{
    auto& h = c.host;
    auto p = h.create();
    if (p == nullptr) return;
    settle (*p);
    auto* by = p->getBypassParameter();
    check (by != nullptr, "host bypass parameter present");
    if (by == nullptr) return;
    auto before = render (*p, 24000, fixed (512), chord (0, 0, { 1 }));
    check (before.peak > 0.01f, "plays before bypass");
    setParam (by, 1.0f);
    auto during = render (*p, 24000, fixed (512), chord (4800, 9600, { 2 }));
    check (during.finite && during.mainPeak (2048) == 0.0f, "bypassed: silent (instrument, no input) (peak " + juce::String (during.mainPeak (2048)) + ")");
    setParam (by, 0.0f);
    auto after = render (*p, 24000, fixed (512), chord (0, 12000, { 1, 2, 3 }));
    check (after.finite && after.peak > 0.01f, "bypass off: plays again (peak " + juce::String (after.peak) + ")");
    // toggled every block while playing
    int k = 0;
    auto flicker = render (*p, 24000, fixed (256), chord (0, 0, { 1, 2 }), [by, &k] (juce::int64, int) { by->setValue ((k++ % 2) ? 1.0f : 0.0f); });
    by->setValue (0.0f);
    check (flicker.finite, "bypass toggled every block: finite and bounded");
}

void latency (Context& c)
{
    auto& h = c.host;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto p = h.create (rate, 512);
        if (p == nullptr) return;
        settle (*p);
        const int lat = p->getLatencySamples();
        info (juce::String (rate / 1000.0, 1) + " kHz: latency " + juce::String (lat) + " samples (" + juce::String (1000.0 * lat / rate, 2) + " ms)");
        check (lat >= 0 && lat < (int) (rate * 0.05), "latency reported and plausible at " + juce::String (rate / 1000.0, 1) + " kHz: " + juce::String (lat));
        bool constant = true;
        auto r = render (*p, (juce::int64) rate, fixed (512), chord (0, (juce::int64) (rate * 0.5)), [&p, lat, &constant] (juce::int64, int) {
            constant = constant && p->getLatencySamples() == lat;
        });
        pump (50);
        constant = constant && p->getLatencySamples() == lat;
        check (constant, "latency constant while playing");
        // no sound before the reported latency (otherwise a host's compensation puts notes early)
        auto q = h.create (rate, 512);
        settle (*q);
        auto onset = render (*q, 8192, fixed (512), chord (0, 0, { 1 }));
        int first = -1;
        for (int i = 0; i < onset.out.getNumSamples() && first < 0; ++i)
            if (onset.out.getSample (0, i) != 0.0f || onset.out.getSample (1, i) != 0.0f) first = i;
        info ("first sound at sample " + juce::String (first));
        check (first >= lat, "no sound before the reported latency (first sample " + juce::String (first) + ", latency " + juce::String (lat) + ")");
        check (r.finite, "finite and bounded");
    }
}

void multi (Context& c)
{
    auto& h = c.host;
    constexpr int count = 4;
    auto evFor = [] (int i) { return chord (0, 30000, { 1 + i % 4 }, 48 + 7 * i); };
    // each instance alone
    std::vector<Render> solo;
    for (int i = 0; i < count; ++i)
    {
        auto p = h.create();
        settle (*p);
        p->releaseResources();
        p->prepareToPlay (48000.0, 512);
        solo.push_back (render (*p, 48000, fixed (512), evFor (i)));
    }
    // all at once, each on its own thread (hosts process instances in parallel)
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> inst;
    for (int i = 0; i < count; ++i)
    {
        inst.push_back (h.create());
        settle (*inst.back());
        inst.back()->releaseResources();
        inst.back()->prepareToPlay (48000.0, 512);
    }
    std::vector<Render> together ((size_t) count);
    std::vector<std::thread> threads;
    for (int i = 0; i < count; ++i)
        threads.emplace_back ([&, i] { together[(size_t) i] = render (*inst[(size_t) i], 48000, fixed (512), evFor (i)); });
    for (auto& t : threads) t.join();
    bool finite = true, same = true;
    juce::String where;
    for (int i = 0; i < count; ++i)
    {
        finite = finite && together[(size_t) i].finite && together[(size_t) i].peak > 0.01f;
        juce::String w;
        if (! sameAudio (solo[(size_t) i], together[(size_t) i], w)) { same = false; where = "instance " + juce::String (i) + ": " + w; }
    }
    check (finite, juce::String (count) + " instances processed in parallel: all play, finite");
    check (same, "each instance sounds exactly as it does alone (no shared state) " + where);
    // states stay separate
    ParamMap pm0 (*inst[0]);
    if (auto* t = pm0 ("p1.tune")) setParam (t, 0.9f);
    settle (*inst[0], 4);
    const auto s0 = stateOf (*inst[0]), s1 = stateOf (*inst[1]);
    check (! (s0 == s1), "a change in one instance stays in that instance");
}

void editor (Context& c)
{
    auto& h = c.host;
    auto p = h.create();
    if (p == nullptr) return;
    settle (*p);
    auto none = render (*p, 24000, fixed (512), chord (0, 12000));
    check (none.finite && none.peak > 0.01f, "plays with no editor");

    // the audio runs on its own thread while the editor opens and closes on the message thread
    std::atomic<bool> run { true }, bad { false };
    std::atomic<float> peak { 0.0f };
    std::atomic<int> blocks { 0 };
    std::thread audio ([&] {
        int k = 0;
        while (run.load())
        {
            auto r = render (*p, 2048, fixed (256), chord (0, 1500, { 1 + k % 4 }, 55 + k % 12), {}, false);
            ++k;
            if (! r.finite) bad.store (true);
            peak.store (juce::jmax (peak.load(), r.peak));
            blocks.fetch_add (8);
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
    });
    bool opened = true;
    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorAndMakeActive());
        opened = opened && ed != nullptr && ed->getWidth() > 100 && ed->getHeight() > 100;
        if (ed != nullptr)
        {
            ed->addToDesktop (0);
            ed->setVisible (true);
            pump (700);
            ed->setVisible (false);
            ed->removeFromDesktop();
        }
        ed.reset();
        pump (300);
    }
    pump (200);
    run.store (false);
    audio.join();
    check (opened, "editor opens (3 times), sized");
    check (! bad.load(), "processing while the editor opens / closes: finite and bounded (" + juce::String (blocks.load()) + " blocks)");
    check (peak.load() > 0.01f, "plays while the editor opens / closes");
    auto after = render (*p, 24000, fixed (512), chord (0, 12000));
    check (after.finite && after.peak > 0.01f, "plays after the editor closed");
}

// host transport the test scripts block by block
struct ScriptedPlayHead final : juce::AudioPlayHead
{
    juce::AudioPlayHead::PositionInfo pos;
    juce::Optional<PositionInfo> getPosition() const override { return pos; }
};

void transport (Context& c)
{
    auto& h = c.host;
    constexpr double rate = 48000.0;
    constexpr int block = 480;
    // the script: stopped, play at 120, tempo change to 90, a 4-beat loop twice, stop, play at 140 from beat 16,
    // relocate back to beat 2
    struct Segment { juce::int64 until; bool playing; double bpm; };
    const std::vector<Segment> script { { 24000, false, 120.0 },   // 0.5 s stopped
                                        { 120000, true, 120.0 },   // play from beat 0
                                        { 216000, true, 90.0 },    // tempo change
                                        { 240000, false, 90.0 },   // stop
                                        { 336000, true, 140.0 } }; // play again at 140 from beat 16
    const juce::int64 total = 384000;
    const double loopStart = 3.0, loopEnd = 5.0; // looping while 24000 <= sample < 216000

    auto run = [&] (bool clicks, std::vector<juce::int64>* expected) {
        auto p = h.create (rate, block);
        ParamMap pm (*p);
        if (clicks)
        {
            setPlain (pm, "metro.on", 1.0f);
            setPlain (pm, "metro.plugin", 1.0f);
            setPlain (pm, "metro.level", 1.0f);
        }
        settle (*p);
        p->releaseResources();
        p->prepareToPlay (rate, block);
        ScriptedPlayHead ph;
        p->setPlayHead (&ph);
        double ppq = 0.0;
        bool wasPlaying = false, looped = false;
        int loops = 0;
        auto r = render (*p, total, fixed (block), {}, [&] (juce::int64 at, int n) {
            Segment seg = script.back();
            for (auto& s : script) if (at < s.until) { seg = s; break; }
            if (at >= script.back().until) seg = { total, true, 140.0 };
            if (seg.playing && ! wasPlaying && at >= 240000) ppq = 16.0; // play again from beat 16
            if (at == 360000 - (360000 % block)) ppq = 2.0;             // relocate back
            if (seg.playing && at >= 24000 && at < 216000 && loops < 3 && ppq >= loopEnd)
            {
                ppq = loopStart + (ppq - loopEnd);
                ++loops;
                looped = true;
            }
            juce::AudioPlayHead::PositionInfo pi;
            pi.setBpm (seg.bpm);
            pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            pi.setIsPlaying (seg.playing);
            pi.setIsLooping (at >= 24000 && at < 216000 && loops < 3);
            pi.setLoopPoints (juce::AudioPlayHead::LoopPoints { loopStart, loopEnd });
            pi.setPpqPosition (ppq);
            pi.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
            pi.setTimeInSamples (at);
            pi.setTimeInSeconds ((double) at / rate);
            ph.pos = pi;
            // the beats in this block, as the host sees them
            const double bpb = seg.bpm / 60.0 / rate;
            if (expected != nullptr && seg.playing)
                for (double b = std::ceil (ppq - 1.0e-9); b < ppq + n * bpb - 1.0e-9; b += 1.0)
                    expected->push_back (at + (juce::int64) std::ceil ((b - ppq) / bpb - 1.0e-6));
            if (seg.playing) ppq += n * bpb;
            wasPlaying = seg.playing;
        });
        p->setPlayHead (nullptr);
        info (juce::String (loops) + " loop jumps");
        if (clicks) check (looped, "the host loop jumped back while playing");
        return std::make_pair (r, p->getLatencySamples());
    };

    std::vector<juce::int64> expected;
    auto [withClicks, lat] = run (true, &expected);
    auto [without, lat2] = run (false, nullptr);
    juce::ignoreUnused (lat2);
    check (withClicks.finite && without.finite, "transport start / stop / loop / tempo changes: finite and bounded");

    // the clicks = difference of the two renders; onsets after at least 40 ms of silence
    std::vector<juce::int64> onsets;
    juce::int64 quietSince = -100000;
    for (int i = 0; i < withClicks.out.getNumSamples(); ++i)
    {
        const float d = std::abs (withClicks.out.getSample (0, i) - without.out.getSample (0, i))
                      + std::abs (withClicks.out.getSample (1, i) - without.out.getSample (1, i));
        if (d > 1.0e-4f)
        {
            if (i - quietSince > 1920) onsets.push_back (i);
            quietSince = i;
        }
    }
    // clicks never sit closer than 40 ms in this script except right after a loop / relocate jump
    std::vector<juce::int64> exp;
    for (auto e : expected)
        if (exp.empty() || e - exp.back() > 1920) exp.push_back (e);
    info (juce::String ((int) exp.size()) + " beats expected, " + juce::String ((int) onsets.size()) + " clicks found, latency " + juce::String (lat));
    int matched = 0;
    juce::int64 worst = 0;
    for (auto e : exp)
    {
        juce::int64 best = 1 << 30;
        for (auto o : onsets) best = juce::jmin (best, std::abs (o - (e + lat)));
        if (best <= 2) ++matched;
        worst = juce::jmax (worst, best);
    }
    check (! exp.empty() && matched == (int) exp.size() && onsets.size() == exp.size(),
           "metronome clicks on the host's beats through tempo changes, loops, stop / start (" + juce::String (matched) + " of "
           + juce::String ((int) exp.size()) + " within 2 samples, worst " + juce::String (worst) + ")");
    bool silentStopped = true;
    for (auto o : onsets)
        if ((o >= 2000 + lat && o < 24000) || (o >= 216000 + 4000 && o < 240000)) silentStopped = false;
    check (silentStopped, "no clicks while the host is stopped");
}

struct Section
{
    const char* name;
    void (*fn) (Context&);
    int timeoutSeconds;
};

const Section sections[] = { { "basic", basic, 300 },         { "lifecycle", lifecycle, 300 }, { "state", state, 300 },
                             { "automation", automation, 600 }, { "bounce", bounce, 600 },     { "rates", rates, 600 },
                             { "bypass", bypass, 300 },        { "latency", latency, 300 },    { "multi", multi, 300 },
                             { "editor", editor, 300 },        { "transport", transport, 300 } };
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::cerr << "usage: ohpe_hosttest <Ohpe.vst3 | Ohpe.component> [--section name|all] [--source repo]" << std::endl;
        return 2;
    }
    juce::String which = "basic";
    juce::File source;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        const juce::String key (argv[i]);
        if (key == "--section") which = argv[i + 1];
        else if (key == "--source") source = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
    }
    if (source == juce::File())
    {
        // the repository root above the working directory (CTest runs in the build tree)
        for (auto dir = juce::File::getCurrentWorkingDirectory(); dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
            if (dir.getChildFile ("tests/core/golden/param_ids_v1.txt").existsAsFile()) { source = dir; break; }
    }

    Context c;
    c.source = source;
    const juce::File plugin (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    std::cout << "Ohpe host test: " << plugin.getFullPathName() << ", section " << which << std::endl;
    if (! c.host.scan (plugin)) return 1;

    juce::StringArray lines;
    source.getChildFile ("tests/core/golden/param_ids_v1.txt").readLines (lines);
    for (auto& l : lines)
        if (l.trim().isNotEmpty() && ! l.trim().startsWith ("#")) c.host.ids.push_back (l.trim());
    check (c.host.ids.size() > 1000, "v1 parameter IDs read (" + juce::String ((int) c.host.ids.size()) + ") from " + source.getFullPathName());

    bool any = false;
    for (const auto& s : sections)
    {
        if (which != "all" && which != s.name) continue;
        any = true;
        std::cout << "== " << s.name << " (" << c.host.format << ")" << std::endl;
        std::atomic<bool> done { false };
        const int limit = s.timeoutSeconds;
        const juce::String name (s.name);
        std::thread watchdog ([&done, limit, name] {
            for (int t = 0; t < limit * 10 && ! done.load(); ++t) std::this_thread::sleep_for (std::chrono::milliseconds (100));
            if (! done.load())
            {
                std::cout << "  [FAIL] section " << name << " hangs (watchdog " << limit << " s)" << std::endl;
                std::_Exit (3);
            }
        });
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        s.fn (c);
        done.store (true);
        watchdog.join();
        info (juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) + " s");
    }
    check (any, "section " + which + " exists");
    std::cout << (failures == 0 ? "ALL HOST CHECKS PASSED" : "HOST CHECKS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
