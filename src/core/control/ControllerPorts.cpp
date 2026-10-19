// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ControllerPorts.h"
#include "GridProtocol.h"

#include <algorithm>
#include <cctype>

namespace ohpe::gridctl
{
// ---------------------------------------------------------------- ownership
Ownership& Ownership::shared()
{
    static Ownership table;
    return table;
}

void Ownership::enrol (const void* who)
{
    const std::lock_guard<std::mutex> l (lock);
    for (const auto& e : order)
        if (e.who == who) return;
    order.insert (order.begin(), { who, false }); // the oldest claim: a new instance does not take over by itself
    ++gen;
}

void Ownership::remove (const void* who)
{
    const std::lock_guard<std::mutex> l (lock);
    const auto n = order.size();
    order.erase (std::remove_if (order.begin(), order.end(), [who] (const Entry& e) { return e.who == who; }), order.end());
    if (order.size() != n) ++gen;
}

void Ownership::setEnabled (const void* who, bool on)
{
    const std::lock_guard<std::mutex> l (lock);
    for (auto& e : order)
        if (e.who == who && e.enabled != on) { e.enabled = on; ++gen; }
}

void Ownership::claim (const void* who)
{
    const std::lock_guard<std::mutex> l (lock);
    const auto it = std::find_if (order.begin(), order.end(), [who] (const Entry& e) { return e.who == who; });
    if (it == order.end() || it + 1 == order.end()) return;
    const Entry e = *it;
    order.erase (it);
    order.push_back (e);
    ++gen;
}

const void* Ownership::ownerLocked() const
{
    for (auto it = order.rbegin(); it != order.rend(); ++it)
        if (it->enabled) return it->who;
    return nullptr;
}

bool Ownership::owns (const void* who) const
{
    const std::lock_guard<std::mutex> l (lock);
    return who != nullptr && ownerLocked() == who;
}

bool Ownership::ownedElsewhere (const void* who) const
{
    const std::lock_guard<std::mutex> l (lock);
    const auto* o = ownerLocked();
    return o != nullptr && o != who;
}

const void* Ownership::owner() const
{
    const std::lock_guard<std::mutex> l (lock);
    return ownerLocked();
}

uint64_t Ownership::generation() const
{
    const std::lock_guard<std::mutex> l (lock);
    return gen;
}

int Ownership::size() const
{
    const std::lock_guard<std::mutex> l (lock);
    return (int) order.size();
}

// ---------------------------------------------------------------- port names
namespace
{
std::string lower (std::string s)
{
    for (auto& c : s) c = (char) std::tolower ((unsigned char) c);
    return s;
}

std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

void replaceAll (std::string& s, const std::string& from, const std::string& to)
{
    for (size_t p = s.find (from); p != std::string::npos; p = s.find (from, p + to.size())) s.replace (p, from.size(), to);
}
} // namespace

bool portMatches (const std::string& name, const std::string& pattern)
{
    const auto n = lower (name);
    size_t start = 0;
    while (start <= pattern.size())
    {
        size_t end = pattern.find_first_of (";,", start);
        if (end == std::string::npos) end = pattern.size();
        const auto part = lower (trim (pattern.substr (start, end - start)));
        if (! part.empty() && n.find (part) != std::string::npos) return true;
        start = end + 1;
    }
    return false;
}

std::string portKey (const std::string& name)
{
    auto s = lower (name);
    replaceAll (s, "midiin", "midi");
    replaceAll (s, "midiout", "midi");
    // split into words of letters / digits, drop the direction words
    std::string key, word;
    auto flush = [&] {
        if (! word.empty() && word != "in" && word != "out" && word != "input" && word != "output") key += word;
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

PortPick autoPorts (const std::vector<std::string>& inputs, const std::vector<std::string>& outputs, const std::string& pattern)
{
    PortPick first;
    for (int i = 0; i < (int) inputs.size(); ++i)
    {
        if (! portMatches (inputs[(size_t) i], pattern)) continue;
        if (first.in < 0) first.in = i;
        const auto key = portKey (inputs[(size_t) i]);
        for (int o = 0; o < (int) outputs.size(); ++o)
            if (portMatches (outputs[(size_t) o], pattern) && portKey (outputs[(size_t) o]) == key) return { i, o };
    }
    for (int o = 0; o < (int) outputs.size() && first.out < 0; ++o)
        if (portMatches (outputs[(size_t) o], pattern)) first.out = o;
    return first.found() ? first : PortPick {};
}
std::vector<PortPick> autoPortCandidates (const std::vector<std::string>& inputs, const std::vector<std::string>& outputs,
                                         const std::string& pattern)
{
    std::vector<PortPick> c;
    for (int i = 0; i < (int) inputs.size(); ++i)
    {
        if (! portMatches (inputs[(size_t) i], pattern)) continue;
        const auto key = portKey (inputs[(size_t) i]);
        for (int o = 0; o < (int) outputs.size(); ++o)
            if (portMatches (outputs[(size_t) o], pattern) && portKey (outputs[(size_t) o]) == key) { c.push_back ({ i, o }); break; }
    }
    return c;
}

int siblingPort (const std::vector<std::string>& ports, const std::string& name)
{
    const auto key = deviceKey (name);
    if (key.empty()) return -1;
    for (int i = 0; i < (int) ports.size(); ++i)
        if (ports[(size_t) i] != name && deviceKey (ports[(size_t) i]) == key) return i;
    return -1;
}
} // namespace ohpe::gridctl
