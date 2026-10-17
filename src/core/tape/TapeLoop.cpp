// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TapeLoop.h"

#include <algorithm>
#include <cmath>

namespace ohpe::tapeloop
{
namespace
{
double safeBpm (double bpm) noexcept { return std::isfinite (bpm) ? std::clamp (bpm, 20.0, 999.0) : 120.0; }
double safeLen (double len) noexcept { return std::isfinite (len) && len > 0.0 ? len : 0.0; }
double finiteOr (double v, double fallback) noexcept { return std::isfinite (v) ? v : fallback; }
bool same (double a, double b) noexcept { return std::abs (a - b) < 2.0e-3; } // 2 ms: host parameters round a little
} // namespace

double gridSeconds (Snap s, double bpm) noexcept
{
    const double beat = 60.0 / safeBpm (bpm);
    switch (s)
    {
        case Snap::Beat: return beat;
        case Snap::Bar: return beat * beatsPerBar;
        case Snap::Off: break;
    }
    return 0.0;
}

double snapTime (double seconds, Snap s, double bpm) noexcept
{
    seconds = std::max (0.0, finiteOr (seconds, 0.0));
    const double g = gridSeconds (s, bpm);
    if (g <= 0.0) return seconds;
    return std::round (seconds / g) * g;
}

double minLength (Snap s, double bpm, double tapeLength) noexcept
{
    const double g = gridSeconds (s, bpm);
    const double len = safeLen (tapeLength);
    double m = g > 0.0 ? g : minFreeSeconds;
    if (len > 0.0) m = std::min (m, std::max (minFreeSeconds, len));
    return m;
}

double defaultLength (Snap s, double bpm) noexcept
{
    const double g = gridSeconds (s, bpm);
    return g > 0.0 ? gridSeconds (Snap::Bar, bpm) : 2.0;
}

Range sanitise (Range r, double tapeLength, double minLen, bool movedIn) noexcept
{
    const double len = safeLen (tapeLength);
    minLen = std::max (minFreeSeconds, finiteOr (minLen, minFreeSeconds));
    if (len > 0.0) minLen = std::min (minLen, len);
    r.in = std::max (0.0, finiteOr (r.in, 0.0));
    r.out = std::max (0.0, finiteOr (r.out, 0.0));
    if (len > 0.0)
    {
        r.in = std::min (r.in, len - minLen);
        r.out = std::min (r.out, len);
    }
    if (r.out - r.in < minLen - 1.0e-9)
    {
        if (movedIn) r.out = r.in + minLen;    // in moved: out gives way
        else r.in = std::max (0.0, r.out - minLen); // out moved: in gives way
        if (r.out - r.in < minLen - 1.0e-9) r.out = r.in + minLen;
        if (len > 0.0 && r.out > len)
        {
            r.out = len;
            r.in = std::max (0.0, len - minLen);
        }
    }
    return r;
}

Range setIn (Range r, double at, double tapeLength, Snap s, double bpm) noexcept
{
    const double len = safeLen (tapeLength);
    const double m = minLength (s, bpm, len);
    double keep = r.valid() ? r.length() : defaultLength (s, bpm);
    keep = std::max (keep, m);
    Range n;
    n.in = snapTime (at, s, bpm);
    if (len > 0.0 && n.in > len - m) n.in = std::max (0.0, len - m);
    n.out = r.out;
    if (! r.valid() || n.out - n.in < m - 1.0e-9) n.out = n.in + keep; // passed loop out: the loop moves along
    if (len > 0.0) n.out = std::min (n.out, len);
    return sanitise (n, len, m, true);
}

Range setOut (Range r, double at, double tapeLength, Snap s, double bpm) noexcept
{
    const double len = safeLen (tapeLength);
    const double m = minLength (s, bpm, len);
    double keep = r.valid() ? r.length() : defaultLength (s, bpm);
    keep = std::max (keep, m);
    Range n;
    n.out = snapTime (at, s, bpm);
    if (len > 0.0 && n.out > len) n.out = len;
    if (n.out < m) n.out = m;
    n.in = r.in;
    if (! r.valid() || n.out - n.in < m - 1.0e-9) n.in = std::max (0.0, n.out - keep); // before loop in: the loop moves along
    return sanitise (n, len, m, false);
}

Range dragEdge (Range r, bool inEdge, double at, double tapeLength, Snap s, double bpm) noexcept
{
    const double len = safeLen (tapeLength);
    const double m = minLength (s, bpm, len);
    double v = snapTime (at, s, bpm);
    if (inEdge) r.in = std::min (v, r.out - m);
    else r.out = std::max (v, r.in + m);
    return sanitise (r, len, m, inEdge);
}

Range shift (Range r, double delta, double tapeLength, Snap s, double bpm) noexcept
{
    const double len = safeLen (tapeLength);
    const double l = std::max (r.length(), minLength (s, bpm, len));
    double in = snapTime (r.in + finiteOr (delta, 0.0), s, bpm);
    if (len > 0.0 && in + l > len) in = std::max (0.0, len - l);
    in = std::max (0.0, in);
    return sanitise ({ in, in + l }, len, minLength (s, bpm, len), true);
}

Range halve (Range r, double tapeLength, Snap s, double bpm) noexcept
{
    const double len = safeLen (tapeLength);
    const double m = minLength (s, bpm, len);
    if (! r.valid()) return sanitise (r, len, m, true);
    double l = r.length() * 0.5;
    if (const double g = gridSeconds (s, bpm); g > 0.0) l = std::max (g, std::round (l / g) * g);
    return sanitise ({ r.in, r.in + std::max (l, m) }, len, m, false);
}

Range doubled (Range r, double tapeLength, Snap s, double bpm) noexcept
{
    const double len = safeLen (tapeLength);
    const double m = minLength (s, bpm, len);
    if (! r.valid()) return sanitise (r, len, m, true);
    double out = r.in + r.length() * 2.0;
    if (len > 0.0) out = std::min (out, len);
    return sanitise ({ r.in, out }, len, m, false);
}

// ---------------------------------------------------------------- history
void History::push (Range before)
{
    undoStack.push_back (before);
    if (undoStack.size() > maxSteps) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

bool History::undo (Range current, Range& restore)
{
    if (undoStack.empty()) return false;
    restore = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back (current);
    return true;
}

bool History::redo (Range current, Range& restore)
{
    if (redoStack.empty()) return false;
    restore = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back (current);
    return true;
}

// ---------------------------------------------------------------- tracker
bool Tracker::observe (Range current, double now, double tapeLength, double minLen, Range& fixed)
{
    if (! started) { reset (current); return false; }
    bool corrected = false;
    if (! same (current.in, last.in) || ! same (current.out, last.out))
    {
        if (! inBurst) { burstStart = last; inBurst = true; }
        lastChange = now;
        const bool movedIn = ! same (current.in, last.in);
        const Range s = sanitise (current, tapeLength, minLen, movedIn);
        if (! same (s.in, current.in) || ! same (s.out, current.out))
        {
            fixed = s;
            current = s;
            corrected = true;
        }
        last = current;
    }
    else if (inBurst && now - lastChange >= settleSeconds)
        flush();
    return corrected;
}

void Tracker::flush()
{
    if (inBurst && (! same (last.in, burstStart.in) || ! same (last.out, burstStart.out))) hist.push (burstStart);
    inBurst = false;
}

void Tracker::commit (Range before, Range after)
{
    note (before);
    flush();
    if (! same (before.in, after.in) || ! same (before.out, after.out)) hist.push (before);
    last = after;
    started = true;
}

void Tracker::note (Range current)
{
    if (same (current.in, last.in) && same (current.out, last.out)) return;
    if (! inBurst) { burstStart = last; inBurst = true; }
    last = current;
}

bool Tracker::undo (Range current, Range& restore)
{
    note (current);
    flush();
    if (! hist.undo (current, restore)) return false;
    last = restore;
    return true;
}

bool Tracker::redo (Range current, Range& restore)
{
    note (current);
    flush();
    if (! hist.redo (current, restore)) return false;
    last = restore;
    return true;
}
} // namespace ohpe::tapeloop
