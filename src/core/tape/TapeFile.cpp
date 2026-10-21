// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TapeFile.h"
#include "../model/FormatVersions.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe::tapefile
{
namespace
{
constexpr char fileMagic[8] = { 'O', 'H', 'P', 'E', 'T', 'P', 'F', '1' };
constexpr int fileVersion = formats::tapeFile;
// the tape audio blob (TapeMachine::saveAudio): "OHPETAPE", u32 version, u32 tracks, u32 frames per block,
// f64 sample rate, u64 length, then (u32 track, u32 page, float L[frames], float R[frames])..., u32 0xffffffff
constexpr size_t blobHeader = 8 + 4 + 4 + 4 + 8 + 8;
constexpr uint32_t endMarker = 0xffffffffu;
constexpr uint32_t maxHeader = 1u << 20;

template <typename T>
void put (std::vector<uint8_t>& out, const T& v)
{
    const auto* p = reinterpret_cast<const uint8_t*> (&v);
    out.insert (out.end(), p, p + sizeof (T));
}

template <typename T>
bool get (const uint8_t*& p, const uint8_t* end, T& v)
{
    if ((size_t) (end - p) < sizeof (T)) return false;
    std::memcpy (&v, p, sizeof (T));
    p += sizeof (T);
    return true;
}

/** Splits the file into its header and its audio blob. */
bool split (const void* data, size_t size, Settings& s, const uint8_t*& blob, size_t& blobSize)
{
    const auto* p = static_cast<const uint8_t*> (data);
    const auto* end = p + size;
    if (data == nullptr || size < 12 || std::memcmp (p, fileMagic, 8) != 0) return false;
    p += 8;
    uint32_t len = 0;
    if (! get (p, end, len) || len > maxHeader || (size_t) (end - p) < len) return false;
    const auto j = nlohmann::json::parse (std::string (reinterpret_cast<const char*> (p), len), nullptr, false);
    p += len;
    if (j.is_discarded() || ! j.is_object() || j.value ("format", std::string()) != "ohpe-tape") return false;
    if (j.value ("version", 0) < 1 || j.value ("version", 0) > fileVersion) return false;
    s.sampleRate = j.value ("sampleRate", 0.0);
    s.lengthSeconds = std::clamp (j.value ("lengthSeconds", TapeMachine::defaultLengthSeconds), TapeMachine::minLengthSeconds, TapeMachine::maxLengthSeconds);
    s.loopIn = std::max (0.0, j.value ("loopIn", 0.0));
    s.loopOut = std::max (0.0, j.value ("loopOut", 0.0));
    s.loop = j.value ("loop", false);
    s.editTrack = std::clamp (j.value ("editTrack", 1), 1, (int) TapeMachine::numTracks);
    if (! std::isfinite (s.sampleRate) || s.sampleRate < 0.0 || ! std::isfinite (s.loopIn) || ! std::isfinite (s.loopOut)) return false;
    blob = p;
    blobSize = (size_t) (end - p);
    return blobSize >= blobHeader;
}

/** Resamples the blob's audio from its rate to `rate` (linear interpolation); returns a new blob. */
bool resample (const uint8_t* blob, size_t size, double rate, std::vector<uint8_t>& out)
{
    const auto* p = blob;
    const auto* end = blob + size;
    p += 8;
    uint32_t version = 0, tracks = 0, frames = 0;
    double srcRate = 0.0;
    uint64_t length = 0;
    if (! get (p, end, version) || ! get (p, end, tracks) || tracks != (uint32_t) TapeMachine::numTracks || ! get (p, end, frames)
        || frames != (uint32_t) TapeBlock::frames || ! get (p, end, srcRate) || ! get (p, end, length) || srcRate <= 0.0)
        return false;
    constexpr size_t blockBytes = sizeof (float) * TapeBlock::frames;
    std::vector<std::vector<const uint8_t*>> pages ((size_t) tracks);
    for (;;)
    {
        uint32_t t = 0, page = 0;
        if (! get (p, end, t)) return false;
        if (t == endMarker) break;
        if (t >= tracks || ! get (p, end, page) || page > (1u << 24) || (size_t) (end - p) < 2 * blockBytes) return false;
        auto& list = pages[(size_t) t];
        if (list.size() <= page) list.resize ((size_t) page + 1, nullptr);
        list[page] = p;
        p += 2 * blockBytes;
    }
    const double ratio = srcRate / rate; // source frames per output frame
    auto fetch = [&] (size_t t, int64_t idx, int ch) {
        if (idx < 0) return 0.0f;
        const auto& list = pages[t];
        const size_t pg = (size_t) (idx >> TapeBlock::shift);
        if (pg >= list.size() || list[pg] == nullptr) return 0.0f;
        float v = 0.0f;
        std::memcpy (&v, list[pg] + (size_t) ch * blockBytes + sizeof (float) * (size_t) (idx & (TapeBlock::frames - 1)), sizeof (float));
        return v;
    };

    out.assign (blob, blob + 8);
    put (out, version);
    put (out, tracks);
    put (out, frames);
    put (out, rate);
    put (out, (uint64_t) std::ceil ((double) length / ratio));
    std::vector<float> l (TapeBlock::frames), r (TapeBlock::frames);
    for (size_t t = 0; t < (size_t) tracks; ++t)
    {
        const auto& list = pages[t];
        if (list.empty()) continue;
        const int64_t srcFrames = (int64_t) list.size() * TapeBlock::frames;
        const int64_t outPages = (int64_t) std::ceil ((double) srcFrames / ratio / TapeBlock::frames);
        for (int64_t op = 0; op < outPages; ++op)
        {
            const int64_t first = (int64_t) std::floor ((double) (op * TapeBlock::frames) * ratio);
            const int64_t last = (int64_t) std::ceil ((double) ((op + 1) * TapeBlock::frames) * ratio) + 1;
            bool any = false;
            for (int64_t sp = first >> TapeBlock::shift; sp <= (last >> TapeBlock::shift) && ! any; ++sp)
                any = sp < (int64_t) list.size() && list[(size_t) sp] != nullptr;
            if (! any) continue;
            bool silent = true;
            for (int i = 0; i < TapeBlock::frames; ++i)
            {
                const double pos = (double) (op * TapeBlock::frames + i) * ratio;
                const auto idx = (int64_t) pos;
                const float f = (float) (pos - (double) idx);
                l[(size_t) i] = fetch (t, idx, 0) + (fetch (t, idx + 1, 0) - fetch (t, idx, 0)) * f;
                r[(size_t) i] = fetch (t, idx, 1) + (fetch (t, idx + 1, 1) - fetch (t, idx, 1)) * f;
                silent = silent && l[(size_t) i] == 0.0f && r[(size_t) i] == 0.0f;
            }
            if (silent) continue;
            put (out, (uint32_t) t);
            put (out, (uint32_t) op);
            const auto* lp = reinterpret_cast<const uint8_t*> (l.data());
            const auto* rp = reinterpret_cast<const uint8_t*> (r.data());
            out.insert (out.end(), lp, lp + blockBytes);
            out.insert (out.end(), rp, rp + blockBytes);
        }
    }
    put (out, endMarker);
    return true;
}
} // namespace

std::vector<uint8_t> save (const TapeMachine& tape, Settings s)
{
    std::vector<uint8_t> audio;
    tape.saveAudio (audio);
    nlohmann::json j;
    j["format"] = "ohpe-tape";
    j["version"] = fileVersion;
    j["sampleRate"] = tape.sampleRate();
    j["lengthSeconds"] = tape.lengthSeconds();
    j["loopIn"] = s.loopIn;
    j["loopOut"] = s.loopOut;
    j["loop"] = s.loop;
    j["editTrack"] = std::clamp (s.editTrack, 1, (int) TapeMachine::numTracks);
    const auto header = j.dump();
    std::vector<uint8_t> out (fileMagic, fileMagic + 8);
    put (out, (uint32_t) header.size());
    out.insert (out.end(), header.begin(), header.end());
    out.insert (out.end(), audio.begin(), audio.end());
    return out;
}

bool readSettings (const void* data, size_t size, Settings& out)
{
    const uint8_t* blob = nullptr;
    size_t blobSize = 0;
    Settings s;
    if (! split (data, size, s, blob, blobSize)) return false;
    out = s;
    return true;
}

bool load (TapeMachine& tape, const void* data, size_t size, Settings& out)
{
    const uint8_t* blob = nullptr;
    size_t blobSize = 0;
    Settings s;
    if (! split (data, size, s, blob, blobSize) || tape.transport().recording) return false;
    std::vector<uint8_t> converted;
    const double rate = tape.sampleRate();
    if (s.sampleRate > 0.0 && rate > 0.0 && std::abs (s.sampleRate - rate) > 0.5)
    {
        if (! resample (blob, blobSize, rate, converted)) return false;
        blob = converted.data();
        blobSize = converted.size();
    }
    const double oldLength = tape.lengthSeconds();
    tape.setLengthSeconds (s.lengthSeconds);
    if (! tape.loadAudio (blob, blobSize, true))
    {
        tape.setLengthSeconds (oldLength);
        return false;
    }
    out = s;
    return true;
}
} // namespace ohpe::tapefile
