// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TableFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ohpe::usertable
{
namespace
{
constexpr char magic[6] = { 'O', 'H', 'P', 'E', 'W', 'T' };
constexpr uint16_t version = 1;

void put32 (std::vector<uint8_t>& b, uint32_t v)
{
    for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) ((v >> (8 * i)) & 0xff));
}

void putFloat (std::vector<uint8_t>& b, float f)
{
    uint32_t v;
    std::memcpy (&v, &f, 4);
    put32 (b, v);
}

uint32_t get32 (const uint8_t* p) noexcept
{
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

float getFloat (const uint8_t* p) noexcept
{
    const uint32_t v = get32 (p);
    float f;
    std::memcpy (&f, &v, 4);
    return std::isfinite (f) ? f : 0.0f;
}
} // namespace

std::vector<uint8_t> encode (const UserTable& t)
{
    std::vector<uint8_t> b;
    const auto& name = t.name();
    b.reserve (26 + name.size() + (size_t) t.numFrames() * frameSize * 4 + t.formants().size() * 60);
    b.insert (b.end(), magic, magic + 6);
    b.push_back ((uint8_t) (version & 0xff));
    b.push_back ((uint8_t) (version >> 8));
    put32 (b, (uint32_t) frameSize);
    put32 (b, (uint32_t) t.numFrames());
    put32 (b, (uint32_t) t.formants().size());
    put32 (b, (uint32_t) name.size());
    b.insert (b.end(), name.begin(), name.end());
    for (int f = 0; f < t.numFrames(); ++f)
    {
        const float* fr = t.frame (f, 0);
        for (int i = 0; i < frameSize; ++i) putFloat (b, fr[i]);
    }
    for (const auto& fm : t.formants())
    {
        for (float v : fm.freq) putFloat (b, v);
        for (float v : fm.gainDb) putFloat (b, v);
        for (float v : fm.bandwidth) putFloat (b, v);
    }
    return b;
}

bool isTableFile (const uint8_t* data, size_t size) noexcept
{
    return data != nullptr && size >= 24 && std::memcmp (data, magic, 6) == 0;
}

UserTablePtr decode (const uint8_t* data, size_t size, std::string* error)
{
    auto fail = [&] (const char* e) -> UserTablePtr {
        if (error != nullptr) *error = e;
        return nullptr;
    };
    if (! isTableFile (data, size)) return fail ("The file is not an Ohpe wavetable.");
    const uint16_t ver = (uint16_t) (data[6] | (data[7] << 8));
    if (ver < 1) return fail ("The file is not an Ohpe wavetable.");
    const uint32_t fsize = get32 (data + 8), frames = get32 (data + 12), formants = get32 (data + 16), nameBytes = get32 (data + 20);
    if (fsize != (uint32_t) frameSize || frames > (uint32_t) maxFrames || formants > (uint32_t) maxFormantFrames || nameBytes > 4096
        || (frames == 0 && formants == 0))
        return fail ("The wavetable file is damaged.");
    const size_t need = 24 + (size_t) nameBytes + (size_t) frames * frameSize * 4 + (size_t) formants * 60;
    if (size < need) return fail ("The wavetable file is damaged.");
    std::string name (reinterpret_cast<const char*> (data + 24), nameBytes);
    const uint8_t* p = data + 24 + nameBytes;
    std::vector<float> wave ((size_t) frames * frameSize);
    for (auto& v : wave) // p63: a damaged file may hold NaN / Inf / huge values
    {
        v = getFloat (p);
        v = std::isfinite (v) ? std::clamp (v, -16.0f, 16.0f) : 0.0f;
        p += 4;
    }
    std::vector<FormantFrame> fm (formants);
    for (auto& f : fm)
    {
        for (auto& v : f.freq) { v = getFloat (p); p += 4; }
        for (auto& v : f.gainDb) { v = getFloat (p); p += 4; }
        for (auto& v : f.bandwidth) { v = getFloat (p); p += 4; }
    }
    auto t = UserTable::create (std::move (name), wave, std::move (fm));
    if (t == nullptr) return fail ("The wavetable file is damaged.");
    return t;
}

int wavFrameSizeHint (const uint8_t* data, size_t size)
{
    if (data == nullptr || size < 12 || std::memcmp (data, "RIFF", 4) != 0 || std::memcmp (data + 8, "WAVE", 4) != 0) return 0;
    int blockAlign = 0, cueSpacing = 0, clm = 0;
    size_t pos = 12;
    while (pos + 8 <= size)
    {
        const uint32_t len = get32 (data + pos + 4);
        const uint8_t* body = data + pos + 8;
        const size_t avail = std::min ((size_t) len, size - pos - 8);
        if (std::memcmp (data + pos, "fmt ", 4) == 0 && avail >= 14)
            blockAlign = (int) (body[12] | (body[13] << 8));
        else if (std::memcmp (data + pos, "clm ", 4) == 0)
        {
            const std::string text (reinterpret_cast<const char*> (body), avail);
            const auto mark = text.find ("<!>");
            if (mark != std::string::npos)
            {
                int v = 0;
                for (size_t i = mark + 3; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) v = v * 10 + (text[i] - '0');
                if (v >= 16 && v <= 65536) clm = v;
            }
        }
        else if (std::memcmp (data + pos, "cue ", 4) == 0 && avail >= 4)
        {
            const uint32_t count = get32 (body);
            if (count >= 2 && avail >= 4 + (size_t) count * 24)
            {
                std::vector<int64_t> offs;
                for (uint32_t i = 0; i < count; ++i) offs.push_back ((int64_t) get32 (body + 4 + i * 24 + 20)); // dwSampleOffset
                std::sort (offs.begin(), offs.end());
                const int64_t d = offs[1] - offs[0];
                bool even = d >= 16;
                for (size_t i = 2; i < offs.size() && even; ++i) even = offs[i] - offs[i - 1] == d;
                if (even) cueSpacing = (int) d;
            }
        }
        pos += 8 + (size_t) len + (len & 1);
    }
    (void) blockAlign;
    return clm > 0 ? clm : cueSpacing;
}

bool looksLikeWavetableSet (int64_t n, int hint) noexcept
{
    if (hint > 0) return n >= hint;
    return n >= frameSize && n % frameSize == 0 && n / frameSize <= maxFrames;
}

std::vector<float> framesFromSet (const float* x, int64_t n, int fs)
{
    if (fs <= 0) fs = frameSize;
    if (x == nullptr || n < fs)
    {
        if (x == nullptr || n < 16) return {};
        fs = (int) n; // one short cycle
    }
    const int64_t total = n / fs;
    const int frames = (int) std::min<int64_t> (total, maxFrames);
    std::vector<float> out ((size_t) frames * frameSize);
    for (int f = 0; f < frames; ++f)
    {
        const int64_t srcFrame = frames > 1 ? (int64_t) std::llround ((double) f * (double) (total - 1) / (double) (frames - 1)) : 0;
        const float* src = x + srcFrame * fs;
        float* dst = out.data() + (size_t) f * frameSize;
        if (fs == frameSize) { std::copy_n (src, frameSize, dst); continue; }
        for (int i = 0; i < frameSize; ++i) // periodic cubic Hermite (the cycle wraps)
        {
            const double pos = (double) i * (double) fs / (double) frameSize;
            const int64_t k = (int64_t) pos;
            const float t = (float) (pos - (double) k);
            auto s = [&] (int64_t j) { return src[((j % fs) + fs) % fs]; };
            const float xm1 = s (k - 1), x0 = s (k), x1 = s (k + 1), x2 = s (k + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            dst[i] = ((c3 * t + c2) * t + c1) * t + x0;
        }
    }
    return out;
}

std::string base64Encode (const std::vector<uint8_t>& b)
{
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve ((b.size() + 2) / 3 * 4);
    for (size_t i = 0; i < b.size(); i += 3)
    {
        const uint32_t v = ((uint32_t) b[i] << 16) | (i + 1 < b.size() ? (uint32_t) b[i + 1] << 8 : 0u) | (i + 2 < b.size() ? (uint32_t) b[i + 2] : 0u);
        out.push_back (chars[(v >> 18) & 63]);
        out.push_back (chars[(v >> 12) & 63]);
        out.push_back (i + 1 < b.size() ? chars[(v >> 6) & 63] : '=');
        out.push_back (i + 2 < b.size() ? chars[v & 63] : '=');
    }
    return out;
}

std::vector<uint8_t> base64Decode (const std::string& s)
{
    auto val = [] (char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<uint8_t> out;
    out.reserve (s.size() / 4 * 3);
    uint32_t acc = 0;
    int bits = 0;
    for (char c : s)
    {
        const int v = val (c);
        if (v < 0) continue; // padding, whitespace
        acc = (acc << 6) | (uint32_t) v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back ((uint8_t) ((acc >> bits) & 0xff));
        }
    }
    return out;
}
} // namespace ohpe::usertable
