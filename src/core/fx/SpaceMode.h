// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p41: the 3D switch - one three-way control per part, "pN.space.mode" = Off / Headphones / Speakers.
//
// It is a front for two older parameters that stay the stored truth (presets, sessions, the audio engine):
//   "pN.space.on"  - the part's 3D stage on / off,
//   "space.out"    - the 3D monitoring of the whole instrument: binaural for headphones, or for speakers (constant-power
//                    panning without interaural delay or head / pinna colouration, see Space3D.h).
// Off = space.on off; Headphones = space.on on + space.out Headphones; Speakers = space.on on + space.out Speakers.
// Monitoring is how the listener hears the instrument, so choosing Headphones or Speakers on one part switches the
// monitoring of every part that has 3D on (each part's switch then shows the new mode).
//
// The mode is a host parameter of its own so it can be automated and MIDI-mapped as one control (encoder 1 steps
// through the three states). Link keeps the three in step on the message thread:
//   * a moved mode (host, MIDI, encoder) writes space.on / space.out;
//   * changed space.on / space.out (a preset, a session, the monitoring page, automation of the older IDs) win and the
//     modes follow. deriveAll() sets the modes from the truth after every load, so a preset older than the switch
//     (no "space.mode" in it) never looks like a move of the switch.

#include "../model/ParamRegistry.h"

#include <array>
#include <cmath>
#include <string>

namespace ohpe::spacemode
{
enum Mode { Off = 0, Headphones, Speakers, numModes };

inline std::string partPrefix (int part) { return "p" + std::to_string (part + 1) + "."; }
inline std::string modeId (int part) { return partPrefix (part) + "space.mode"; }
inline std::string onId (int part) { return partPrefix (part) + "space.on"; }
inline constexpr const char* outId = "space.out";

/** The mode the stored values stand for. */
inline int derive (bool on, int out) noexcept { return on ? (out >= 1 ? Speakers : Headphones) : Off; }

/** Sets the mode of `part` (or of every part for part < 0) from space.on / space.out (registry only). */
inline void deriveAll (ParamRegistry& reg, int numParts, int part = -1)
{
    const int out = (int) std::lround (reg.get (outId));
    for (int p = 0; p < numParts; ++p)
        if (part < 0 || p == part)
        {
            const int idx = reg.indexOf (modeId (p));
            if (idx >= 0) reg.set (idx, (float) derive (reg.get (onId (p)) >= 0.5f, out));
        }
}

/**
    Keeps the modes and the stored values in step (message thread, ~20 Hz). `get (id)` reads the current plain value,
    `set (id, plain)` writes one (the plug-in: a host gesture + the registry, so automation and the UI follow).
*/
template <int MaxParts>
class Link
{
public:
    /** Forget what was seen (the next update only derives the modes). */
    void resync() noexcept { primed = false; }

    template <class Get, class Set>
    void update (int numParts, Get&& get, Set&& set)
    {
        numParts = numParts < MaxParts ? numParts : MaxParts;
        std::array<int, MaxParts> on {}, mode {};
        auto read = [&] (int& out) {
            out = (int) std::lround (get (std::string (outId)));
            for (int p = 0; p < numParts; ++p)
            {
                on[(size_t) p] = get (onId (p)) >= 0.5f ? 1 : 0;
                mode[(size_t) p] = (int) std::lround (get (modeId (p)));
            }
        };
        int out = 0;
        read (out);
        if (primed)
        {
            bool truthMoved = out != lastOut;
            for (int p = 0; p < numParts; ++p) truthMoved = truthMoved || on[(size_t) p] != lastOn[(size_t) p];
            if (! truthMoved)
            {
                bool wrote = false;
                for (int p = 0; p < numParts; ++p)
                {
                    const int m = mode[(size_t) p];
                    if (m == lastMode[(size_t) p] || m < Off || m >= numModes) continue;
                    if (m == Off) set (onId (p), 0.0f);
                    else
                    {
                        set (onId (p), 1.0f);
                        if (out != m - 1) set (std::string (outId), (float) (m - 1));
                        out = m - 1;
                    }
                    wrote = true;
                }
                if (wrote) read (out);
            }
        }
        for (int p = 0; p < numParts; ++p)
        {
            const int d = derive (on[(size_t) p] != 0, out);
            if (mode[(size_t) p] != d) set (modeId (p), (float) d);
            lastMode[(size_t) p] = d;
            lastOn[(size_t) p] = on[(size_t) p];
        }
        lastOut = out;
        primed = true;
    }

private:
    std::array<int, MaxParts> lastOn {}, lastMode {};
    int lastOut = 0;
    bool primed = false;
};
} // namespace ohpe::spacemode
