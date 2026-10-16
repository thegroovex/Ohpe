// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FactoryBank.h"

#include "../drum/DrumKit.h"

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>

namespace ohpe::factory
{
namespace
{
struct Names
{
    const char* id;
    const char* en;
    const char* de;
};

// append-only: the index is stored in presets ("pN.sampler.inst", "pN.grain.inst")
constexpr std::array<Names, numInstruments> instruments { {
    { "builtin", "Built-in", "Eingebaut" },
    { "kalimba", "Kalimba", "Kalimba" },
    { "marimba", "Marimba", "Marimba" },
    { "vibraphone", "Vibraphone", "Vibrafon" },
    { "glockenspiel", "Glockenspiel", "Glockenspiel" },
    { "harp", "Harp", "Harfe" },
    { "harpsichord", "Harpsichord", "Cembalo" },
    { "recorder", "Recorder", "Blockflöte" },
    { "glass", "Singing Glass", "Klangglas" },
    { "zither", "Zither", "Zither" },
    { "bowedmetal", "Bowed Metal", "Gestrichenes Metall" },
    { "grandpiano", "Grand Piano", "Flügel" },          // 0.8
    { "uprightpiano", "Upright Piano", "Klavier" },     // 0.8
    { "tinepiano", "Tine Piano", "Tine-Piano" },        // 0.8: Ohpe's own modelled electric piano
    { "reedpiano", "Reed Piano", "Zungen-Piano" },      // 0.8: Ohpe's own modelled electric piano
} };

// append-only: the index is stored in presets ("drum.kit")
constexpr std::array<Names, numKits> kits { {
    { "studio", "Studio", "Studio" },
    { "tight", "Tight", "Trocken" },
    { "room", "Room", "Raum" },
    { "lofi", "Lo-Fi", "Lo-Fi" },
    { "hybrid", "Hybrid", "Hybrid" },
    { "boom", "Boom", "Boom" }, // 0.5: synthesised deep kick / sub kit
} };

struct Instrument
{
    std::shared_ptr<const sampler::SampleMap> map;
    SamplePtr grain;
    int grainRoot = 60;
};

std::mutex& writeLock()
{
    static std::mutex m;
    return m;
}

// written once under writeLock (message thread), never replaced or freed (process lifetime)
std::array<Instrument, numInstruments>& instrumentStore()
{
    static std::array<Instrument, numInstruments> s;
    return s;
}
std::array<std::atomic<const sampler::SampleMap*>, numInstruments> instrumentLive {};
std::array<std::atomic<const SampleData*>, numInstruments> grainLive {};
std::array<std::atomic<int>, numInstruments> grainRootLive {};

constexpr int numPads = drum::DrumKit::numPads;
std::array<std::array<SamplePtr, numPads>, numKits>& kitStore()
{
    static std::array<std::array<SamplePtr, numPads>, numKits> s;
    return s;
}
std::array<std::array<std::atomic<const SampleData*>, numPads>, numKits> kitLive {};
std::atomic<int> loadedInstruments { 0 }, loadedKitSamples { 0 };

template <size_t N>
const Names& entry (const std::array<Names, N>& list, int index) noexcept
{
    return list[(size_t) std::clamp (index, 0, (int) N - 1)];
}
} // namespace

const char* instrumentId (int i) noexcept { return entry (instruments, i).id; }
const char* instrumentNameEn (int i) noexcept { return entry (instruments, i).en; }
const char* instrumentNameDe (int i) noexcept { return entry (instruments, i).de; }
std::vector<std::string> instrumentNamesEn() { return instrumentNamesEn (0, numClassicInstruments); }
std::vector<std::string> instrumentNamesDe() { return instrumentNamesDe (0, numClassicInstruments); }
std::vector<std::string> instrumentNamesEn (int from, int to)
{
    std::vector<std::string> v;
    for (int i = std::max (0, from); i < std::min (to, numInstruments); ++i) v.emplace_back (instruments[(size_t) i].en);
    return v;
}
std::vector<std::string> instrumentNamesDe (int from, int to)
{
    std::vector<std::string> v;
    for (int i = std::max (0, from); i < std::min (to, numInstruments); ++i) v.emplace_back (instruments[(size_t) i].de);
    return v;
}
std::vector<std::string> keyboardChoiceEn()
{
    auto v = instrumentNamesEn (numClassicInstruments, numInstruments);
    v.insert (v.begin(), "Off");
    return v;
}
std::vector<std::string> keyboardChoiceDe()
{
    auto v = instrumentNamesDe (numClassicInstruments, numInstruments);
    v.insert (v.begin(), "Aus");
    return v;
}

const char* kitId (int i) noexcept { return entry (kits, i).id; }
const char* kitNameEn (int i) noexcept { return entry (kits, i).en; }
const char* kitNameDe (int i) noexcept { return entry (kits, i).de; }
std::vector<std::string> kitNamesEn()
{
    std::vector<std::string> v;
    for (const auto& n : kits) v.emplace_back (n.en);
    return v;
}
std::vector<std::string> kitNamesDe()
{
    std::vector<std::string> v;
    for (const auto& n : kits) v.emplace_back (n.de);
    return v;
}

void setInstrument (int index, std::vector<sampler::ZoneSpec> zones)
{
    if (index <= 0 || index >= numInstruments) return;
    zones.erase (std::remove_if (zones.begin(), zones.end(), [] (const sampler::ZoneSpec& z) { return z.sample == nullptr; }), zones.end());
    if (zones.empty()) return;
    std::lock_guard<std::mutex> lock (writeLock());
    auto& inst = instrumentStore()[(size_t) index];
    if (inst.map != nullptr) return; // first call wins (several plug-in instances)

    auto m = std::make_shared<sampler::SampleMap>();
    m->specs = std::move (zones);
    for (const auto& z : m->specs)
    {
        sampler::SampleMap::Zone zone;
        zone.sample = z.sample.get();
        zone.keyLo = std::clamp (std::min (z.keyLo, z.keyHi), 0, 127);
        zone.keyHi = std::clamp (std::max (z.keyLo, z.keyHi), 0, 127);
        zone.velLo = std::clamp (std::min (z.velLo, z.velHi), 1, 127);
        zone.velHi = std::clamp (std::max (z.velLo, z.velHi), 1, 127);
        zone.rootKey = z.rootKey < 0 ? -1 : std::clamp (z.rootKey, 0, 127);
        zone.loopStart = z.loopStart;
        zone.loopEnd = z.loopEnd;
        m->zones.push_back (zone);
    }
    m->sliceSource = m->specs.front().sample;
    m->slices = { 0 };
    m->builtIn = false;

    // Grain source: the loudest-layer zone whose root is closest to C4
    const sampler::ZoneSpec* best = nullptr;
    for (const auto& z : m->specs)
    {
        const int root = z.rootKey >= 0 ? z.rootKey : 60;
        const int bestRoot = best != nullptr ? (best->rootKey >= 0 ? best->rootKey : 60) : 0;
        if (best == nullptr || std::abs (root - 60) < std::abs (bestRoot - 60)
            || (std::abs (root - 60) == std::abs (bestRoot - 60) && z.velHi > best->velHi))
            best = &z;
    }
    inst.grain = best->sample;
    inst.grainRoot = best->rootKey >= 0 ? best->rootKey : 60;
    inst.map = m;
    grainRootLive[(size_t) index].store (inst.grainRoot, std::memory_order_relaxed);
    grainLive[(size_t) index].store (inst.grain.get(), std::memory_order_release);
    instrumentLive[(size_t) index].store (inst.map.get(), std::memory_order_release);
    ++loadedInstruments;
}

const sampler::SampleMap* instrumentMap (int index) noexcept
{
    if (index <= 0 || index >= numInstruments) return nullptr;
    return instrumentLive[(size_t) index].load (std::memory_order_acquire);
}

const SampleData* grainSource (int index, int& root) noexcept
{
    if (index <= 0 || index >= numInstruments) return nullptr;
    const auto* s = grainLive[(size_t) index].load (std::memory_order_acquire);
    root = grainRootLive[(size_t) index].load (std::memory_order_relaxed);
    return s;
}

void setKitSample (int kit, int pad, SamplePtr sample)
{
    if (kit <= 0 || kit >= numKits || pad < 0 || pad >= numPads || sample == nullptr) return;
    std::lock_guard<std::mutex> lock (writeLock());
    auto& slot = kitStore()[(size_t) kit][(size_t) pad];
    if (slot != nullptr) return; // first call wins
    slot = std::move (sample);
    kitLive[(size_t) kit][(size_t) pad].store (slot.get(), std::memory_order_release);
    ++loadedKitSamples;
}

const SampleData* kitSample (int kit, int pad) noexcept
{
    if (kit <= 0 || kit >= numKits || pad < 0 || pad >= numPads) return nullptr;
    return kitLive[(size_t) kit][(size_t) pad].load (std::memory_order_acquire);
}

SamplePtr kitSampleOwner (int kit, int pad)
{
    if (kit <= 0 || kit >= numKits || pad < 0 || pad >= numPads) return nullptr;
    std::lock_guard<std::mutex> lock (writeLock());
    return kitStore()[(size_t) kit][(size_t) pad];
}

int numLoadedInstruments() noexcept { return loadedInstruments.load(); }
int numLoadedKitSamples() noexcept { return loadedKitSamples.load(); }
} // namespace ohpe::factory
