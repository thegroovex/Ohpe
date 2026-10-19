// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace ohpe::gridctl
{
/**
    p49: the Grid-Controller inside a plug-in. The plug-in opens the controller's own MIDI ports (bypassing the host),
    so only one Ohpe of the process may hold them.

    Ownership: every controller object of the process registers (it lives as long as its plug-in instance once the
    instance's window was opened); claim() makes it the most recent one (window opened, clicked, "Take over here").
    The owner is the most recently claimed registered controller that is enabled (profile on). Removing or disabling
    the owner hands the controller to the next most recent one. Message thread (locked anyway). No JUCE.
*/
class Ownership
{
public:
    /** The one table of the process (a static of the plug-in binary). */
    static Ownership& shared();

    void enrol (const void* who);
    void remove (const void* who);
    /** Enabled = wants the controller (profile on). */
    void setEnabled (const void* who, bool on);
    /** Most recent now: the owner if enabled. */
    void claim (const void* who);
    bool owns (const void* who) const;
    /** Someone else owns it now. */
    bool ownedElsewhere (const void* who) const;
    /** The owner (nullptr = none). */
    const void* owner() const;
    /** Counts every change (pollers see a hand-over without comparing everything). */
    uint64_t generation() const;
    int size() const;

private:
    struct Entry { const void* who; bool enabled; };
    const void* ownerLocked() const;
    mutable std::mutex lock;
    std::vector<Entry> order; ///< oldest claim first
    uint64_t gen = 0;
};

/** Matching a port name against the pattern: parts separated by ';' or ',', blanks around them ignored, any part found
    in the name (case does not matter). An empty pattern matches nothing. */
bool portMatches (const std::string& name, const std::string& pattern);
/** The pairing key of a port name: lower case, the words in / out / input / output dropped, "midiin" / "midiout" ->
    "midi", only letters and digits - an input and an output of one device get the same key. */
std::string portKey (const std::string& name);

struct PortPick
{
    int in = -1, out = -1; ///< indices into the lists, -1 = none
    bool found() const noexcept { return in >= 0 && out >= 0; }
};
/** The automatic choice: the first matching input with a matching output of the same key; else the first matching input
    and the first matching output; else none. */
PortPick autoPorts (const std::vector<std::string>& inputs, const std::vector<std::string>& outputs, const std::string& pattern);
} // namespace ohpe::gridctl
