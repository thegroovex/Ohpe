// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Version.h"

#include <cctype>
#include <vector>

namespace ohpe::update
{
namespace
{
bool parseNumber (const std::string& s, int& out)
{
    if (s.empty() || s.size() > 9) return false;
    if (s.size() > 1 && s[0] == '0') return false; // no leading zeros
    int v = 0;
    for (char c : s)
    {
        if (! std::isdigit ((unsigned char) c)) return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

std::vector<std::string> split (const std::string& s, char sep)
{
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s)
    {
        if (c == sep) { parts.push_back (cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back (cur);
    return parts;
}

bool allDigits (const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (! std::isdigit ((unsigned char) c)) return false;
    return true;
}
} // namespace

bool Version::parse (const std::string& textIn, Version& out)
{
    std::string text = textIn;
    while (! text.empty() && std::isspace ((unsigned char) text.back())) text.pop_back();
    size_t start = 0;
    while (start < text.size() && std::isspace ((unsigned char) text[start])) ++start;
    text = text.substr (start);
    if (! text.empty() && (text[0] == 'v' || text[0] == 'V')) text = text.substr (1);
    if (const auto plus = text.find ('+'); plus != std::string::npos) text = text.substr (0, plus);
    std::string pre;
    if (const auto dash = text.find ('-'); dash != std::string::npos)
    {
        pre = text.substr (dash + 1);
        text = text.substr (0, dash);
        if (pre.empty()) return false;
        for (const auto& id : split (pre, '.'))
        {
            if (id.empty()) return false;
            for (char c : id)
                if (! (std::isalnum ((unsigned char) c) || c == '-')) return false;
        }
    }
    const auto nums = split (text, '.');
    if (nums.size() != 3) return false;
    Version v;
    if (! parseNumber (nums[0], v.major) || ! parseNumber (nums[1], v.minor) || ! parseNumber (nums[2], v.patch)) return false;
    v.pre = pre;
    v.ok = true;
    out = v;
    return true;
}

Version Version::of (const std::string& text)
{
    Version v;
    parse (text, v);
    return v;
}

std::string Version::toString() const
{
    std::string s = std::to_string (major) + "." + std::to_string (minor) + "." + std::to_string (patch);
    if (! pre.empty()) s += "-" + pre;
    return s;
}

int Version::compare (const Version& a, const Version& b)
{
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    if (a.pre == b.pre) return 0;
    if (a.pre.empty()) return 1; // a release is newer than its pre-releases
    if (b.pre.empty()) return -1;
    const auto pa = split (a.pre, '.'), pb = split (b.pre, '.');
    for (size_t i = 0; i < pa.size() && i < pb.size(); ++i)
    {
        const bool na = allDigits (pa[i]), nb = allDigits (pb[i]);
        if (na && nb)
        {
            if (pa[i].size() != pb[i].size()) return pa[i].size() < pb[i].size() ? -1 : 1;
            if (pa[i] != pb[i]) return pa[i] < pb[i] ? -1 : 1;
        }
        else if (na != nb)
            return na ? -1 : 1; // numeric identifiers sort before alphanumeric ones
        else if (pa[i] != pb[i])
            return pa[i] < pb[i] ? -1 : 1;
    }
    if (pa.size() != pb.size()) return pa.size() < pb.size() ? -1 : 1;
    return 0;
}
} // namespace ohpe::update
