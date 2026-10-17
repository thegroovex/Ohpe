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
        line += fitCell (fitCell (fold (cells[(size_t) seg * 2]), cellWidth - 1, false), cellWidth, true);
        line += ' ';
        line += fitCell (fitCell (fold (cells[(size_t) seg * 2 + 1]), cellWidth - 1, false), cellWidth, true);
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
        if (c == ' ' || c == '-' || c == '/' || c == '.')
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

// ---------------------------------------------------------------- pad layout
int padLayoutNote (int layout, int col, int row, int octave, const PadKey& key) noexcept
{
    const int base = 12 * (std::clamp (octave, 0, 7) + 1);
    return harmony::gridNote (std::clamp (layout, 0, (int) harmony::numLayouts - 1), col, row, base, key.root, key.scale);
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
