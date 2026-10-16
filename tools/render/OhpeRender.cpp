// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// ohpe_render: renders presets offline to WAV files and prints level / spectrum metrics (one JSON
// object per line). Core-only (no JUCE): the factory samples are read from WAV copies of the
// embedded FLAC files (tools/samples/build_samples.py --wav-cache DIR writes them).
//
//   ohpe_render [--out DIR] [--bank resources/samples/bank.json --wavdir DIR] [--part N]
//               [--seconds S] [--bpm B] [--limit] [--hold S] [--beats N] [--tail S] preset.ohpepreset ...
//
// --hold S: part presets hold one note (bass, lead) or a chord for S seconds instead of their phrase (movement check).
// --notes N,N,.. / --vel V: with --hold, the held notes and their velocity (click checks at other pitches).
// --beats N / --tail S: shorten the phrase and its tail (the factory level tests render 4 beats + 1 s).
//
// Part presets play the phrase of their main category (render::phraseFor); project presets run
// their sequencers for --seconds (default 16). The master limiter is off, so peaks are honest (--limit keeps a project's limiter, as heard).

#include "OhpeCore.h"
#include "preset/Preset.h"
#include "sample/FactoryBank.h"
#include "../render/PresetRender.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using namespace ohpe;

namespace
{
uint32_t rd32 (const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }
uint16_t rd16 (const unsigned char* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

/** Minimal WAV reader (PCM 16 / 24 / 32 bit, float 32). */
SamplePtr readWav (const fs::path& path)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return nullptr;
    std::vector<unsigned char> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (d.size() < 44 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0) return nullptr;
    int fmt = 0, chans = 0, bits = 0;
    uint32_t rate = 0;
    size_t pos = 12;
    const unsigned char* data = nullptr;
    size_t dataLen = 0;
    while (pos + 8 <= d.size())
    {
        const uint32_t len = rd32 (&d[pos + 4]);
        if (std::memcmp (&d[pos], "fmt ", 4) == 0)
        {
            fmt = rd16 (&d[pos + 8]);
            chans = rd16 (&d[pos + 10]);
            rate = rd32 (&d[pos + 12]);
            bits = rd16 (&d[pos + 22]);
            if (fmt == 0xfffe && len >= 26) fmt = rd16 (&d[pos + 32]);
        }
        else if (std::memcmp (&d[pos], "data", 4) == 0)
        {
            data = &d[pos + 8];
            dataLen = std::min<size_t> (len, d.size() - pos - 8);
        }
        pos += 8 + len + (len & 1);
    }
    if (data == nullptr || chans < 1 || rate == 0) return nullptr;
    const int bytes = bits / 8;
    const size_t frames = dataLen / (size_t) (bytes * chans);
    std::vector<std::vector<float>> ch ((size_t) std::min (chans, 2), std::vector<float> (frames));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < (int) ch.size(); ++c)
        {
            const unsigned char* s = data + (i * (size_t) chans + (size_t) c) * (size_t) bytes;
            float v = 0.0f;
            if (fmt == 3 && bits == 32) std::memcpy (&v, s, 4);
            else if (bits == 16) v = (float) (int16_t) rd16 (s) / 32768.0f;
            else if (bits == 24) v = (float) ((int32_t) ((s[0] << 8) | (s[1] << 16) | ((uint32_t) s[2] << 24)) >> 8) / 8388608.0f;
            else if (bits == 32) v = (float) (int32_t) rd32 (s) / 2147483648.0f;
            ch[(size_t) c][i] = v;
        }
    return std::make_shared<SampleData> (std::move (ch), (double) rate, path.string(), path.stem().string());
}

void writeWav (const fs::path& path, const render::Audio& a)
{
    std::ofstream f (path, std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t frames = (uint32_t) a.size();
    f.write ("RIFF", 4); w32 (36 + frames * 8); f.write ("WAVEfmt ", 8);
    w32 (16); w16 (3); w16 (2); w32 ((uint32_t) a.sampleRate); w32 ((uint32_t) a.sampleRate * 8); w16 (8); w16 (32);
    f.write ("data", 4); w32 (frames * 8);
    for (size_t i = 0; i < a.size(); ++i) { f.write ((const char*) &a.l[i], 4); f.write ((const char*) &a.r[i], 4); }
}

std::string base (const std::string& file)
{
    const auto p = fs::path (file);
    return p.stem().string();
}

struct Bank
{
    std::map<int, SamplePtr> studio; ///< pad -> recording of kit 0
};

/** Loads bank.json (instruments, kits 1..n) and the studio kit (NN-name.wav) from the WAV cache. */
Bank loadBank (const fs::path& bankFile, const fs::path& wavdir)
{
    Bank b;
    if (wavdir.empty()) return b;
    for (const auto& e : fs::directory_iterator (wavdir))
    {
        const auto name = e.path().filename().string();
        if (name.size() > 3 && std::isdigit ((unsigned char) name[0]) && std::isdigit ((unsigned char) name[1]) && name[2] == '-')
            if (auto s = readWav (e.path())) b.studio[std::stoi (name.substr (0, 2)) - 1] = s;
    }
    if (bankFile.empty()) return b;
    std::ifstream f (bankFile);
    const auto j = nlohmann::json::parse (f, nullptr, false);
    if (! j.is_object()) { std::cerr << "cannot read " << bankFile << "\n"; return b; }
    for (int i = 1; i < factory::numInstruments; ++i)
    {
        const auto id = factory::instrumentId (i);
        if (! j["instruments"].contains (id)) continue;
        std::vector<sampler::ZoneSpec> zones;
        for (const auto& z : j["instruments"][id])
        {
            sampler::ZoneSpec s;
            s.sample = readWav (wavdir / (base (z.value ("file", "")) + ".wav"));
            if (s.sample == nullptr) { std::cerr << "missing " << z.value ("file", "") << "\n"; continue; }
            s.keyLo = z.value ("lo", 0); s.keyHi = z.value ("hi", 127);
            s.velLo = z.value ("vlo", 1); s.velHi = z.value ("vhi", 127);
            s.rootKey = z.value ("root", 60);
            s.loopStart = z.value ("ls", (int64_t) -1); s.loopEnd = z.value ("le", (int64_t) -1);
            zones.push_back (s);
        }
        factory::setInstrument (i, std::move (zones));
    }
    for (int k = 1; k < factory::numKits; ++k)
    {
        const auto id = factory::kitId (k);
        if (! j["kits"].contains (id)) continue;
        for (const auto& [padKey, file] : j["kits"][id].items())
            if (auto s = readWav (wavdir / (base (file.get<std::string>()) + ".wav")))
                factory::setKitSample (k, std::stoi (padKey) - 1, s);
    }
    return b;
}
} // namespace

int main (int argc, char** argv)
{
    fs::path out, bankFile, wavdir;
    int part = 0;
    double seconds = 16.0, bpm = 100.0, hold = 0.0, beats = 1.0e9, tail = 1.0e9;
    bool limiter = false;
    std::vector<std::string> files;
    std::vector<int> notes;
    int vel = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--out") out = next();
        else if (a == "--bank") bankFile = next();
        else if (a == "--wavdir") wavdir = next();
        else if (a == "--part") part = std::clamp (std::stoi (next()) - 1, 0, Core::numParts - 1);
        else if (a == "--seconds") seconds = std::stod (next());
        else if (a == "--bpm") bpm = std::stod (next());
        else if (a == "--limit") limiter = true;
        else if (a == "--hold") hold = std::stod (next());
        else if (a == "--beats") beats = std::stod (next()); // phrase limit, as the level tests render it
        else if (a == "--tail") tail = std::stod (next());
        else if (a == "--notes") // with --hold: the held notes, comma separated (MIDI numbers)
        {
            std::stringstream ns (next());
            for (std::string t; std::getline (ns, t, ',');) notes.push_back (std::stoi (t));
        }
        else if (a == "--vel") vel = std::stoi (next()); // with --hold: their velocity
        else files.push_back (a);
    }
    const auto bank = loadBank (bankFile, wavdir);
    std::cerr << "factory bank: " << factory::numLoadedInstruments() << " instruments, " << factory::numLoadedKitSamples()
              << " kit recordings, " << bank.studio.size() << " studio pads\n";
    if (! out.empty()) fs::create_directories (out);

    int failures = 0;
    for (const auto& file : files)
    {
        std::ifstream f (file);
        std::stringstream ss;
        ss << f.rdbuf();
        preset::Preset p;
        std::string err;
        if (! preset::fromJson (ss.str(), p, &err)) { std::cerr << file << ": " << err << "\n"; ++failures; continue; }

        auto core = std::make_unique<Core>();
        core->prepare (48000.0, 256);
        for (const auto& [pad, s] : bank.studio) core->drumKit().setFactorySample (pad, s);
        const bool project = p.meta.kind == preset::Kind::Project;
        const auto audio = project ? render::renderProject (*core, p, seconds, limiter) : render::renderPart (*core, p, part, bpm, beats, tail, hold, notes, vel);
        const auto m = render::measure (audio);
        if (! out.empty()) writeWav (out / (base (file) + ".wav"), audio);

        nlohmann::json j;
        j["file"] = base (file);
        j["name"] = p.meta.name;
        j["kind"] = project ? "project" : "part";
        j["category"] = render::primaryCategory (p.meta);
        j["engine"] = p.meta.engine;
        j["rms"] = std::round (m.rmsDb * 10) / 10;
        j["active"] = std::round (m.activeRmsDb * 10) / 10;
        j["peak"] = std::round (m.peakDb * 10) / 10;
        j["dc"] = m.dc;
        j["tail"] = std::round (m.tailDb * 10) / 10;
        j["hf"] = std::round (m.hfDb * 10) / 10;
        j["nonfinite"] = m.nonFinite;
        j["denormals"] = m.denormals;
        std::cout << j.dump() << std::endl;
    }
    return failures == 0 ? 0 : 1;
}
