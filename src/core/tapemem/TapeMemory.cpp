// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TapeMemory.h"

#include "../OhpeCore.h"
#include "../engines/SamplerEngine.h"
#include "../model/FormatVersions.h"
#include "../model/JsonParse.h"
#include "../model/ParamRegistry.h"
#include "../sample/Slicer.h"
#include "../wavetable/TableFile.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <numeric>

namespace ohpe::tapemem
{
void registerParams (ParamRegistry& reg)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d, const char* group) { d.group = group; reg.add (std::move (d)); };
    add (choice (Core::Ids::tapememLen, "Memory Length", "Gedächtnislänge", { "5 s", "10 s", "20 s", "30 s" },
                 { "5 s", "10 s", "20 s", "30 s" }, Len30s),
         "tapemem");
    add (choice (Core::Ids::tapememSrc, "Memory Source", "Gedächtnisquelle", { "Main Out", "Part 1", "Part 2", "Part 3", "Part 4" },
                 { "Hauptausgang", "Part 1", "Part 2", "Part 3", "Part 4" }, SourceMain),
         "tapemem");
    add (choice (Core::Ids::tapememMode, "Capture Mode", "Merk-Modus", { "Chromatic", "Slices" }, { "Chromatisch", "Slices" }, ModeChromatic),
         "tapemem");
    add (choice (Core::Ids::tapememTo, "Capture Target", "Merk-Ziel", { "Edit Part", "Part 1", "Part 2", "Part 3", "Part 4" },
                 { "Bearbeiteter Part", "Part 1", "Part 2", "Part 3", "Part 4" }, TargetEditPart),
         "tapemem");
    add (toggle (Core::Ids::tapememGo, "Capture", "Merken", false), "remote");
}

double lengthSeconds (int length) noexcept
{
    static const double s[] = { 5.0, 10.0, 20.0, 30.0 };
    return s[std::clamp (length, 0, (int) numLengths - 1)];
}

// ---------------------------------------------------------------- ring buffer
void RingBuffer::prepare (double sampleRate, double seconds)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    guard = (int64_t) std::ceil (guardSeconds * rate);
    const auto n = (size_t) std::max<int64_t> (1, (int64_t) std::ceil (std::clamp (seconds, 0.1, maxSeconds) * rate) + guard);
    if (bufL.size() != n)
    {
        bufL.assign (n, 0);
        bufR.assign (n, 0);
        bufL.shrink_to_fit();
        bufR.shrink_to_fit();
    }
    writePos.store (0, std::memory_order_relaxed);
    filled.store (0, std::memory_order_relaxed);
    clearRequest.store (false, std::memory_order_relaxed);
    peakHold.store (0.0f, std::memory_order_relaxed);
}

int64_t RingBuffer::available() const noexcept
{
    if (clearRequest.load (std::memory_order_acquire)) return 0;
    return std::max<int64_t> (0, filled.load (std::memory_order_acquire) - guard);
}

bounce::Audio RingBuffer::snapshot (double seconds) const
{
    bounce::Audio a;
    a.rate = rate;
    const int64_t cap = (int64_t) bufL.size();
    const int64_t n = std::min<int64_t> (available(), (int64_t) std::llround (std::max (0.0, seconds) * rate));
    if (cap == 0 || n <= 0) return a;
    const int64_t end = writePos.load (std::memory_order_acquire);
    const int64_t start = end - n;
    a.l.resize ((size_t) n);
    a.r.resize ((size_t) n);
    for (int64_t i = 0; i < n; ++i)
    {
        const auto at = (size_t) ((start + i) % cap);
        a.l[(size_t) i] = fromPcm (bufL[at]);
        a.r[(size_t) i] = fromPcm (bufR[at]);
    }
    // frames the audio thread overwrote while copying (more than the guard ahead) are dropped from the front
    const int64_t overrun = writePos.load (std::memory_order_acquire) - end - guard;
    if (overrun > 0)
    {
        const auto drop = (std::ptrdiff_t) std::min<int64_t> (overrun, n);
        a.l.erase (a.l.begin(), a.l.begin() + drop);
        a.r.erase (a.r.begin(), a.r.begin() + drop);
    }
    return a;
}

// ---------------------------------------------------------------- analysis
float detectPitch (const float* x, int64_t n, double rate, float* confidence)
{
    if (confidence != nullptr) *confidence = 0.0f;
    if (x == nullptr || rate <= 0.0) return 0.0f;
    const int tauMin = std::max (2, (int) (rate / 2000.0));
    const int tauMax = (int) (rate / 50.0);
    const int w = (int) std::min<int64_t> (n - tauMax - 2, (int64_t) std::max (1024, tauMax * 2));
    if (w < 256 || tauMax <= tauMin) return 0.0f;
    std::vector<float> d ((size_t) tauMax + 2, 0.0f);
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        double s = 0.0;
        for (int i = 0; i < w; ++i)
        {
            const double v = (double) x[i] - (double) x[i + tau];
            s += v * v;
        }
        d[(size_t) tau] = (float) s;
    }
    // cumulative mean normalised difference
    std::vector<float> c (d.size(), 1.0f);
    double run = 0.0;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        run += d[(size_t) tau];
        c[(size_t) tau] = run > 0.0 ? (float) (d[(size_t) tau] * tau / run) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
        if (c[(size_t) tau] < 0.15f)
        {
            while (tau + 1 <= tauMax && c[(size_t) tau + 1] < c[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (c[(size_t) tau] < c[(size_t) best]) best = tau;
        if (c[(size_t) best] > 0.35f) return 0.0f;
    }
    const float a = c[(size_t) best - 1], b = c[(size_t) best], e = c[(size_t) best + 1];
    const float den = a - 2.0f * b + e;
    const double shift = std::abs (den) > 1.0e-9f ? std::clamp (0.5 * (a - e) / den, -0.5, 0.5) : 0.0;
    if (confidence != nullptr) *confidence = std::clamp (1.0f - b, 0.0f, 1.0f);
    return (float) (rate / ((double) best + shift));
}

namespace
{
std::vector<float> monoOf (const bounce::Audio& a, int64_t start, int64_t end)
{
    start = std::clamp<int64_t> (start, 0, a.frames());
    end = std::clamp<int64_t> (end, start, a.frames());
    std::vector<float> m ((size_t) (end - start));
    for (int64_t i = start; i < end; ++i)
        m[(size_t) (i - start)] = 0.5f * (a.l[(size_t) i] + (i < (int64_t) a.r.size() ? a.r[(size_t) i] : a.l[(size_t) i]));
    return m;
}

SamplePtr toSample (const bounce::Audio& a, const std::string& name)
{
    if (a.frames() < 64) return nullptr;
    auto r = a.r.size() == a.l.size() ? a.r : a.l;
    return std::make_shared<SampleData> (std::vector<std::vector<float>> { a.l, std::move (r) }, a.rate, std::string(), name);
}

double energy (const bounce::Audio& a, int64_t s, int64_t e)
{
    double sum = 0.0;
    for (int64_t i = s; i < e; ++i) sum += (double) a.l[(size_t) i] * a.l[(size_t) i] + (double) a.r[(size_t) i] * a.r[(size_t) i];
    return sum;
}
} // namespace

std::vector<int64_t> detectOnsets (const bounce::Audio& a)
{
    if (a.frames() < 64) return { 0 };
    const auto s = toSample (a, {});
    sample::SliceSettings set;
    set.sensitivity = sliceSensitivity;
    auto on = sample::detectTransients (*s, set);
    if (on.empty() || on.front() != 0) on.insert (on.begin(), 0);
    return on;
}

float detectMaterialPitch (const bounce::Audio& a, const std::vector<int64_t>& onsets, float* confidence)
{
    if (confidence != nullptr) *confidence = 0.0f;
    const auto seg = strongestSegment (a, onsets);
    const int64_t attack = (int64_t) (0.03 * a.rate);
    const int64_t s = std::min (seg.first + attack, seg.second);
    const auto mono = monoOf (a, s, seg.second);
    const int64_t win = (int64_t) (a.rate / 50.0) * 3 + 8;
    std::vector<float> found;
    float confSum = 0.0f;
    for (int k = 0; k < 7; ++k)
    {
        const int64_t at = (int64_t) mono.size() * k / 8;
        if (at + win > (int64_t) mono.size()) break;
        float c = 0.0f;
        const float hz = detectPitch (mono.data() + at, (int64_t) mono.size() - at, a.rate, &c);
        if (hz > 0.0f && c >= 0.7f) { found.push_back (hz); confSum += c; }
    }
    if (found.empty()) return 0.0f;
    std::sort (found.begin(), found.end());
    if (confidence != nullptr) *confidence = confSum / (float) found.size();
    return found[found.size() / 2];
}

std::pair<int64_t, int64_t> strongestSegment (const bounce::Audio& a, const std::vector<int64_t>& onsets)
{
    const int64_t n = a.frames();
    const int64_t maxLen = (int64_t) (maxChromaticSeconds * a.rate);
    std::vector<int64_t> b;
    for (auto o : onsets)
        if (o > 0 && o < n) b.push_back (o);
    b.insert (b.begin(), 0);
    b.push_back (n);
    std::pair<int64_t, int64_t> best { 0, std::min (n, maxLen) };
    double bestE = -1.0;
    for (size_t i = 0; i + 1 < b.size(); ++i)
    {
        const int64_t s = b[i], e = std::min (b[i + 1], s + maxLen);
        if (e - s < 64) continue;
        const double en = energy (a, s, e);
        if (en > bestE) { bestE = en; best = { s, e }; }
    }
    return best;
}

bounce::Audio resample (const bounce::Audio& a, double step)
{
    bounce::Audio out;
    out.rate = a.rate;
    if (step <= 0.0 || a.frames() < 2) return a;
    const auto n = (size_t) std::floor ((double) (a.frames() - 1) / step);
    out.l.resize (n);
    out.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double pos = (double) i * step;
        out.l[i] = readHermite (a.l.data(), a.frames(), pos);
        out.r[i] = readHermite (a.r.data(), (int64_t) a.r.size(), pos);
    }
    return out;
}

Capture build (bounce::Audio a, int mode, const std::string& name)
{
    Capture c;
    c.mode = std::clamp (mode, 0, (int) numModes - 1);
    c.name = name;
    if (a.r.size() != a.l.size()) a.r = a.l;
    for (auto* ch : { &a.l, &a.r })
        for (float& x : *ch)
            if (! std::isfinite (x)) x = 0.0f;
    if (bounce::isSilent (a)) return c;
    bounce::trimSilence (a);
    if (a.frames() < 64) return c;
    const auto onsets = detectOnsets (a);
    if (c.mode == ModeSlices)
    {
        bounce::normalise (a);
        bounce::fadeEdges (a);
        c.slices = (int) onsets.size();
        c.sample = toSample (a, name);
        return c;
    }
    float conf = 0.0f;
    const float hz = detectMaterialPitch (a, onsets, &conf);
    const auto seg = strongestSegment (a, onsets);
    bounce::Audio s;
    s.rate = a.rate;
    s.l.assign (a.l.begin() + seg.first, a.l.begin() + seg.second);
    s.r.assign (a.r.begin() + seg.first, a.r.begin() + seg.second);
    if (hz > 0.0f)
    {
        const double note = 69.0 + 12.0 * std::log2 ((double) hz / 440.0);
        c.root = std::clamp ((int) std::lround (note), 0, 127);
        c.cents = (float) ((c.root - note) * 100.0);
        c.pitchHz = hz;
        if (std::abs (c.cents) > 0.5f) s = resample (s, std::pow (2.0, c.cents / 1200.0)); // step > 1 raises
    }
    bounce::trimSilence (s);
    bounce::normalise (s);
    bounce::fadeEdges (s, 0.005);
    c.sample = toSample (s, name);
    return c;
}

bool apply (Core& core, int part, const Capture& c, const bounce::Setter& set)
{
    if (part < 0 || part >= Core::numParts || c.sample == nullptr || ! set) return false;
    auto* e = dynamic_cast<SamplerEngine*> (core.part (part).engine (EngineType::Sampler));
    if (e == nullptr) return false;
    const auto p = "p" + std::to_string (part + 1) + ".";
    set (p + "engine", (float) EngineType::Sampler);
    set (p + "sampler.start", 0.0f);
    set (p + "sampler.reverse", 0.0f);
    set (p + "sampler.loop", (float) sampler::LoopOff);
    if (c.mode == ModeSlices)
    {
        set (p + "sampler.slicesens", sliceSensitivity);
        set (p + "sampler.slicebase", (float) sliceBaseKey);
        set (p + "sampler.mode", (float) sampler::ModeSlices);
        e->setSample (c.sample, sliceBaseKey);
    }
    else
    {
        set (p + "sampler.mode", (float) sampler::ModeZones);
        set (p + "sampler.root", (float) c.root);
        e->setSample (c.sample, c.root);
    }
    return true;
}

// ---------------------------------------------------------------- state
std::string saveCaptures (const std::vector<Stored>& captures)
{
    using nlohmann::json;
    json parts = json::object();
    for (const auto& s : captures)
    {
        const auto& smp = s.capture.sample;
        if (smp == nullptr || s.part < 0 || s.part >= Core::numParts || smp->numFrames() <= 0) continue;
        const int ch = std::clamp (smp->numChannels(), 1, 2);
        const auto frames = smp->numFrames();
        std::vector<uint8_t> bytes ((size_t) (frames * ch * 2));
        size_t k = 0;
        for (int64_t i = 0; i < frames; ++i)
            for (int c = 0; c < ch; ++c)
            {
                const float v = std::clamp (smp->channel (c)[i], -1.0f, 1.0f) * 32767.0f;
                const auto q = (uint16_t) (int16_t) std::lround (v);
                bytes[k++] = (uint8_t) (q & 0xff);
                bytes[k++] = (uint8_t) (q >> 8);
            }
        parts["p" + std::to_string (s.part + 1)] = { { "mode", s.capture.mode },
                                                     { "root", s.capture.root },
                                                     { "slices", s.capture.slices },
                                                     { "name", s.capture.name },
                                                     { "rate", smp->sampleRate() },
                                                     { "channels", ch },
                                                     { "frames", frames },
                                                     { "pcm16", usertable::base64Encode (bytes) } };
    }
    if (parts.empty()) return {};
    return json { { "format", "ohpe-tapememory" }, { "version", formats::tapeMemory }, { "parts", parts } }.dump();
}

std::vector<Stored> restoreCaptures (const std::string& text)
{
    std::vector<Stored> out;
    if (text.empty()) return out;
    try
    {
        const auto j = jsonutil::parse (text);
        if (! j.is_object()) return out;
        if (j.contains ("format") && j["format"] != "ohpe-tapememory") return out;
        // version: absent = 0 (read as 1); newer = the fields this build knows
        const auto parts = j.find ("parts");
        if (parts == j.end() || ! parts->is_object()) return out;
        for (int p = 0; p < Core::numParts; ++p)
        {
            const auto it = parts->find ("p" + std::to_string (p + 1));
            if (it == parts->end() || ! it->is_object()) continue;
            const auto& e = *it;
            auto num = [&e] (const char* k, double def) { return e.contains (k) && e[k].is_number() ? e[k].get<double>() : def; };
            const int ch = (int) num ("channels", 2);
            const double rate = num ("rate", 48000.0);
            if (ch < 1 || ch > 2 || ! (rate >= 8000.0 && rate <= 384000.0) || ! e.contains ("pcm16") || ! e["pcm16"].is_string()) continue;
            const auto bytes = usertable::base64Decode (e["pcm16"].get<std::string>());
            int64_t frames = (int64_t) bytes.size() / (2 * ch);
            frames = std::min<int64_t> (frames, (int64_t) num ("frames", (double) frames));
            if (frames < 64) continue;
            std::vector<std::vector<float>> data ((size_t) ch, std::vector<float> ((size_t) frames));
            size_t k = 0;
            for (int64_t i = 0; i < frames; ++i)
                for (int c = 0; c < ch; ++c, k += 2)
                    data[(size_t) c][(size_t) i] = (float) (int16_t) (uint16_t) (bytes[k] | (bytes[k + 1] << 8)) / 32767.0f;
            if (ch == 1) data.push_back (data[0]);
            Stored s;
            s.part = p;
            s.capture.mode = std::clamp ((int) num ("mode", 0), 0, (int) numModes - 1);
            s.capture.root = std::clamp ((int) num ("root", 60), 0, 127);
            s.capture.slices = std::max (0, (int) num ("slices", 0));
            s.capture.name = e.contains ("name") && e["name"].is_string() ? e["name"].get<std::string>() : std::string ("Capture");
            s.capture.sample = std::make_shared<SampleData> (std::move (data), rate, std::string(), s.capture.name);
            out.push_back (std::move (s));
        }
    }
    catch (...)
    {
        out.clear();
    }
    return out;
}
} // namespace ohpe::tapemem
