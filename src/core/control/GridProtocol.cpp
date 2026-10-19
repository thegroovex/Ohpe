// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GridProtocol.h"

#include "../harmony/HarmonyBrain.h"
#include "../seq/ScaleChord.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace ohpe::gridctl
{
int colourCode (Colour c, bool blink) noexcept
{
    int v = 0;
    switch (c)
    {
        case Colour::RedDim: v = 1; break;
        case Colour::Red: v = 4; break;
        case Colour::AmberDim: v = 7; break;
        case Colour::Amber: v = 10; break;
        case Colour::YellowDim: v = 13; break;
        case Colour::Yellow: v = 16; break;
        case Colour::GreenDim: v = 19; break;
        case Colour::Green: v = 22; break;
        default: return 0;
    }
    return blink ? v + 1 : v;
}

int padColourCode (Colour c) noexcept
{
    switch (c)
    {
        case Colour::Red: return 5;
        case Colour::RedDim: return 7;
        case Colour::Amber: return 9;
        case Colour::AmberDim: return 11;
        case Colour::Yellow: return 13;
        case Colour::YellowDim: return 15;
        case Colour::Green: return 21;
        case Colour::GreenDim: return 23;
        default: return 0;
    }
}

Colour padColourOf (int code) noexcept
{
    for (int k = 1; k <= (int) Colour::Green; ++k)
        if (padColourCode ((Colour) k) == code) return (Colour) k;
    return Colour::Off;
}

int buttonCode (bool on, bool dimWhenOff, bool blink) noexcept
{
    if (on) return blink ? 5 : 4;
    return dimWhenOff ? (blink ? 2 : 1) : 0;
}

// ---------------------------------------------------------------- display
std::string fold (const std::string& s)
{
    std::string out;
    out.reserve (s.size());
    for (size_t i = 0; i < s.size();)
    {
        const auto c = (unsigned char) s[i];
        if (c < 0x80)
        {
            out += (c >= 0x20 && c < 0x7f) ? (char) c : ' ';
            ++i;
            continue;
        }
        // decode one UTF-8 sequence
        int len = (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
        unsigned cp = len == 2 ? (c & 0x1fu) : len == 3 ? (c & 0x0fu) : len == 4 ? (c & 0x07u) : 0u;
        bool ok = len > 1 && i + (size_t) len <= s.size();
        for (int k = 1; ok && k < len; ++k)
        {
            const auto cc = (unsigned char) s[i + (size_t) k];
            if ((cc & 0xc0) != 0x80) ok = false;
            cp = (cp << 6) | (cc & 0x3fu);
        }
        i += ok ? (size_t) len : 1;
        if (! ok) { out += '?'; continue; }
        switch (cp)
        {
            case 0xe4: out += "ae"; break;
            case 0xf6: out += "oe"; break;
            case 0xfc: out += "ue"; break;
            case 0xc4: out += "Ae"; break;
            case 0xd6: out += "Oe"; break;
            case 0xdc: out += "Ue"; break;
            case 0xdf: out += "ss"; break;
            case 0xb7: case 0x2022: case 0x2027: out += '.'; break;   // middle dot, bullet
            case 0x2013: case 0x2014: case 0x2212: out += '-'; break; // dashes, minus
            case 0x266f: out += '#'; break;                           // sharp sign
            case 0x266d: out += 'b'; break;                           // flat sign
            case 0xb0: out += 'o'; break;                             // degree
            case 0xa0: case 0x2009: case 0x202f: out += ' '; break;   // spaces
            case 0x2192: out += '>'; break;
            case 0x2190: out += '<'; break;
            case 0xe9: case 0xe8: out += 'e'; break;
            case 0xe1: case 0xe0: out += 'a'; break;
            default: out += '?'; break;
        }
    }
    return out;
}

std::string fitLine (const std::string& utf8)
{
    auto s = fold (utf8);
    s.resize ((size_t) lineLength, ' ');
    return s;
}

namespace
{
std::string fitCell (const std::string& folded, int width, bool centre)
{
    std::string s = folded.substr (0, (size_t) width);
    const int pad = width - (int) s.size();
    if (pad <= 0) return s;
    const int left = centre ? pad / 2 : 0;
    return std::string ((size_t) left, ' ') + s + std::string ((size_t) (pad - left), ' ');
}
} // namespace

std::string cellLine (const std::array<std::string, numCells>& cells)
{
    std::string line;
    for (int seg = 0; seg < 4; ++seg)
    {
        // at most cellWidth - 1 characters in an 8-character cell: neighbouring cells never touch
        line += fitCell (fold (cells[(size_t) seg * 2]).substr (0, cellWidth - 1), cellWidth, true);
        line += ' ';
        line += fitCell (fold (cells[(size_t) seg * 2 + 1]).substr (0, cellWidth - 1), cellWidth, true);
    }
    return fitLine (line);
}

std::string segmentLine (const std::array<std::string, 4>& segments)
{
    std::string line;
    for (const auto& s : segments) line += fitCell (fold (s), lineLength / 4, false);
    return fitLine (line);
}

std::string shortName (const std::string& utf8, int width)
{
    std::string s = fold (utf8);
    // trim
    while (! s.empty() && s.back() == ' ') s.pop_back();
    while (! s.empty() && s.front() == ' ') s.erase (s.begin());
    if ((int) s.size() <= width) return s;
    // split into words, drop the spaces, then shorten the longest word one letter at a time
    std::vector<std::string> words;
    std::string w;
    for (char c : s)
    {
        if (c == ' ' || c == '-')
        {
            if (! w.empty()) words.push_back (w);
            w.clear();
        }
        else
            w += c;
    }
    if (! w.empty()) words.push_back (w);
    if (words.empty()) return s.substr (0, (size_t) width);
    // a leading "Ch 1" / "Kanal 1" / "Track 3" / "Pad 12" ... (the page already says which one)
    auto isNumber = [] (const std::string& x) { return ! x.empty() && std::all_of (x.begin(), x.end(), [] (char c) { return c >= '0' && c <= '9'; }); };
    static const char* prefixes[] = { "Ch", "Channel", "Kanal", "Track", "Spur", "Pad", "Part", "Lane", "Slot" };
    for (const char* pre : prefixes)
    {
        const std::string p (pre);
        if (words.size() >= 3 && words[0] == p && isNumber (words[1])) { words.erase (words.begin(), words.begin() + 2); break; }
        if (words.size() >= 2 && words[0].size() > p.size() && words[0].compare (0, p.size(), p) == 0 && isNumber (words[0].substr (p.size())))
        {
            words.erase (words.begin());
            break;
        }
    }
    auto total = [&words] {
        size_t n = 0;
        for (const auto& x : words) n += x.size();
        return (int) n;
    };
    if (total() <= width)
    {
        std::string r;
        for (const auto& x : words) r += x;
        return r;
    }
    while (total() > width)
    {
        size_t longest = 0;
        for (size_t k = 1; k < words.size(); ++k)
            if (words[k].size() > words[longest].size()) longest = k;
        if (words[longest].size() <= 1) break;
        // drop a lower-case vowel after the first letter if there is one, else the last letter
        auto& x = words[longest];
        size_t cut = std::string::npos;
        for (size_t k = x.size() - 1; k >= 1; --k)
        {
            if (std::strchr ("aeiou", x[k]) != nullptr) { cut = k; break; }
            if (k == 1) break;
        }
        x.erase (cut != std::string::npos ? cut : x.size() - 1, 1);
    }
    std::string r;
    for (const auto& x : words) r += x;
    return r.substr (0, (size_t) width);
}

std::string shortValue (const std::string& utf8, int width)
{
    std::string s = fold (utf8);
    while (! s.empty() && s.back() == ' ') s.pop_back();
    while (! s.empty() && s.front() == ' ') s.erase (s.begin());
    if ((int) s.size() <= width) return s;
    s.erase (std::remove (s.begin(), s.end(), ' '), s.end());
    return s.substr (0, (size_t) width);
}

std::vector<uint8_t> displayLineSysex (int line, const std::string& utf8)
{
    const auto text = fitLine (utf8);
    std::vector<uint8_t> m { 0xf0, 0x47, 0x7f, 0x15, (uint8_t) (0x18 + std::clamp (line, 0, numLines - 1)), 0x00, 0x45, 0x00 };
    for (char c : text) m.push_back ((uint8_t) (c & 0x7f));
    m.push_back (0xf7);
    return m;
}

std::vector<uint8_t> clearLineSysex (int line)
{
    return { 0xf0, 0x47, 0x7f, 0x15, (uint8_t) (0x1c + std::clamp (line, 0, numLines - 1)), 0x00, 0x00, 0xf7 };
}

std::vector<uint8_t> aftertouchModeSysex (bool poly)
{
    return { 0xf0, 0x47, 0x7f, 0x15, 0x5c, 0x00, 0x01, (uint8_t) (poly ? 0x01 : 0x00), 0xf7 };
}

// ---------------------------------------------------------------- p53: input, generation, mode, splash
int encoderOfCc (int c) noexcept
{
    if (c >= cc::firstEncoder && c < cc::firstEncoder + numCells) return c - cc::firstEncoder;
    if (c == cc::masterEncoder) return touchNoteMaster;
    if (c == cc::swingEncoder) return touchNoteSwing;
    if (c == cc::tempoEncoder) return touchNoteTempo;
    return -1;
}

Decoded decode (int status, int data1, int data2) noexcept
{
    const int type = status & 0xf0;
    Decoded d;
    if (type == 0x90 || type == 0x80)
    {
        const bool on = type == 0x90 && data2 > 0;
        if (const int pad = padOfNote (data1); pad >= 0) return { Input::Pad, pad, on ? data2 : 0 };
        if (const int e = encoderOfTouchNote (data1); e >= 0) return { Input::EncoderTouch, e, on ? 1 : 0 };
        if (data1 == touchNoteStrip) return { Input::StripTouch, 0, on ? 1 : 0 };
        return d;
    }
    if (type == 0xa0)
    {
        if (const int pad = padOfNote (data1); pad >= 0) return { Input::Pressure, pad, data2 };
        return d;
    }
    if (type == 0xd0) return { Input::Pressure, -1, data1 };
    if (type == 0xe0) return { Input::Strip, 0, (data2 & 0x7f) << 7 | (data1 & 0x7f) };
    if (type == 0xb0)
    {
        if (const int e = encoderOfCc (data1); e >= 0) return { Input::EncoderTurn, e, relativeDelta (data2) };
        return { Input::Button, data1, data2 };
    }
    return d;
}

std::vector<uint8_t> identityRequestSysex() { return { 0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7 }; }

Generation identityGeneration (const uint8_t* m, size_t n) noexcept
{
    if (m == nullptr || n < 8 || m[0] != 0xf0 || m[1] != 0x7e || m[3] != 0x06 || m[4] != 0x02) return Generation::Unknown;
    if (m[5] == 0x47) return n >= 8 && m[6] == 0x15 ? Generation::First : Generation::Unknown; // one-byte maker id, model
    if (m[5] == 0x00 && n >= 9 && m[6] == 0x21 && m[7] == 0x1d) return Generation::Later;     // three-byte maker id
    return Generation::Unknown;
}

std::vector<uint8_t> userModeSysex (Generation g, bool user)
{
    if (g == Generation::First) return { 0xf0, 0x47, 0x7f, 0x15, 0x62, 0x00, 0x01, (uint8_t) (user ? 0x01 : 0x00), 0xf7 };
    if (g == Generation::Later) return { 0xf0, 0x00, 0x21, 0x1d, 0x01, 0x01, 0x0a, (uint8_t) (user ? 0x01 : 0x00), 0xf7 };
    return {};
}

std::array<std::string, numLines> splashLines (int stage, bool german)
{
    auto centred = [] (const std::string& t) {
        const int pad = std::max (0, (lineLength - (int) t.size()) / 2);
        return fitLine (std::string ((size_t) pad, ' ') + t);
    };
    const std::string state = stage <= 0 ? (german ? "verbindet ..." : "connecting ...") : (german ? "verbunden" : "connected");
    return { fitLine (""), centred ("O H P E"), centred (state), centred ("designed by groovex") };
}

std::string deviceKey (const std::string& portName)
{
    std::string s;
    for (char c : portName) s += (char) std::tolower ((unsigned char) c);
    std::string key, word;
    auto flush = [&] {
        static const char* const drop[] = { "in", "out", "input", "output", "user", "live", "port", "midi" };
        bool skip = word.empty();
        for (auto* d : drop) skip = skip || word == d;
        // "midiin2" / "midiout2" / "midi2" (the Windows prefix of a device's further ports)
        if (! skip && word.rfind ("midi", 0) == 0)
        {
            auto rest = word.substr (4);
            if (rest.rfind ("in", 0) == 0) rest = rest.substr (2);
            else if (rest.rfind ("out", 0) == 0) rest = rest.substr (3);
            skip = ! rest.empty() && std::all_of (rest.begin(), rest.end(), [] (char ch) { return std::isdigit ((unsigned char) ch) != 0; });
        }
        if (! skip) key += word;
        word.clear();
    };
    for (char c : s)
    {
        if (std::isalnum ((unsigned char) c)) word += c;
        else flush();
    }
    flush();
    return key;
}

// ---------------------------------------------------------------- pad layout
int padLayoutNote (int layout, int col, int row, int octave, const PadKey& key, bool inKey, bool fixed) noexcept
{
    const int base = 12 * (std::clamp (octave, 0, 7) + 1);
    return harmony::gridNote (std::clamp (layout, 0, (int) harmony::numLayouts - 1), col, row, base, key.root, key.scale, inKey, fixed);
}

PadRole padRole (int note, const PadKey& key, uint16_t chordMask, bool pressed) noexcept
{
    if (pressed) return PadRole::Pressed;
    const int pc = ((note % 12) + 12) % 12;
    if (pc == ((key.root % 12) + 12) % 12) return PadRole::Root;
    if (((chordMask >> pc) & 1u) != 0) return PadRole::ChordTone;
    return scales::inScale (note, key.root, key.scale) ? PadRole::InKey : PadRole::Outside;
}

Colour roleColour (PadRole r) noexcept
{
    switch (r)
    {
        case PadRole::Pressed: return Colour::Green;
        case PadRole::Root: return Colour::Amber;
        case PadRole::ChordTone: return Colour::GreenDim;
        case PadRole::InKey: return Colour::YellowDim;
        default: return Colour::Off;
    }
}
} // namespace ohpe::gridctl
