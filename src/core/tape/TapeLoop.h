// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

namespace ohpe::tapeloop
{
/**
    p42: setting the tape loop by hand - loop in / loop out at the playhead (while the tape plays too), the snap
    grid, dragging an edge, halving / doubling, and an undo history of the loop range.

    Pure functions on seconds; no threads, no parameters. The plug-in reads "tape.loopin" / "tape.loopout" /
    "tape.loopsnap", calls these and writes the result back (host-notified). The tape machine itself only plays
    the range (TapeMachine: loop on and out > in).

    Grid: the tape counts bars from its start in 4/4 at the project tempo (the readout's "BAR 3.2" = bar 3, beat 2),
    so a beat is 60 / bpm seconds and a bar four beats. Snapping goes to the NEAREST line - a key pressed a little
    late while the tape plays still lands on the downbeat.

    Rules: in >= 0, out <= the tape length, out - in >= the minimum length (minLength(): 0.1 s free, one grid step
    with the grid on). Setting an edge on the wrong side of the other one keeps the loop's length and moves the
    other edge along (set in after out: out = in + the old length); dragging an edge stops at the other one.
*/
enum class Snap { Off = 0, Beat, Bar };

constexpr double minFreeSeconds = 0.1;
constexpr int beatsPerBar = 4;

struct Range
{
    double in = 0.0, out = 0.0;
    bool operator== (const Range& o) const noexcept { return in == o.in && out == o.out; }
    bool operator!= (const Range& o) const noexcept { return ! (*this == o); }
    double length() const noexcept { return out - in; }
    bool valid() const noexcept { return out > in; }
};

/** Seconds of one grid step (beat / bar at `bpm`), 0 when off. */
double gridSeconds (Snap s, double bpm) noexcept;
/** `seconds` on the nearest grid line (unchanged when off). */
double snapTime (double seconds, Snap s, double bpm) noexcept;
/** The shortest loop: one grid step with the grid on (never longer than the tape), minFreeSeconds when off. */
double minLength (Snap s, double bpm, double tapeLength) noexcept;
/** The length a fresh loop gets when the other edge is not set yet: one bar on the grid, 2 s free. */
double defaultLength (Snap s, double bpm) noexcept;

/** Loop in at `at` (snapped); keeps the loop's length when it would pass loop out. */
Range setIn (Range r, double at, double tapeLength, Snap s, double bpm) noexcept;
/** Loop out at `at` (snapped); keeps the loop's length when it would pass loop in. */
Range setOut (Range r, double at, double tapeLength, Snap s, double bpm) noexcept;
/** Dragging one edge (true = in) to `at` (snapped): it stops one minimum length before the other edge. */
Range dragEdge (Range r, bool inEdge, double at, double tapeLength, Snap s, double bpm) noexcept;
/** Moves the whole loop by `delta` seconds (its start snapped, the length kept), inside the tape. */
Range shift (Range r, double delta, double tapeLength, Snap s, double bpm) noexcept;
/** Half / double the length from loop in (never shorter than the minimum, never past the tape's end). */
Range halve (Range r, double tapeLength, Snap s, double bpm) noexcept;
Range doubled (Range r, double tapeLength, Snap s, double bpm) noexcept;
/** Makes any range legal (order, minimum length, inside the tape). `movedIn`: the edge that was changed last is
    kept and the other one gives way (true = in was changed). */
Range sanitise (Range r, double tapeLength, double minLen, bool movedIn) noexcept;

/** Undo / redo of the loop range (newest last). A change is pushed as the range BEFORE it. */
class History
{
public:
    static constexpr size_t maxSteps = 64;
    void push (Range before);
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    /** Returns the range to restore; `current` goes to the redo side. False if there is nothing. */
    bool undo (Range current, Range& restore);
    bool redo (Range current, Range& restore);
    void clear() noexcept { undoStack.clear(); redoStack.clear(); }
    size_t undoSize() const noexcept { return undoStack.size(); }

private:
    std::vector<Range> undoStack, redoStack;
};

/**
    Watches the loop range from every source (keys, drags, encoders, MIDI, host) on the message thread. One burst of
    changes (an encoder turn, a drag) becomes one undo step once it rests for `settleSeconds`; a change that breaks
    the rules is corrected (the edge that moved wins) and the corrected range is returned for writing back.
*/
class Tracker
{
public:
    static constexpr double settleSeconds = 0.6;
    /** Starts from `r` without an undo step (state loaded, first call). */
    void reset (Range r) noexcept { last = r; burstStart = r; inBurst = false; started = true; }
    /** Called regularly with the current range; returns true and fills `fixed` when it must be corrected. */
    bool observe (Range current, double nowSeconds, double tapeLength, double minLen, Range& fixed);
    /** A change made in one go (a key): one undo step at once. */
    void commit (Range before, Range after);
    /** Ends a burst now (before undo / redo, so the last turn is its own step). */
    void flush();
    /** Undo / redo through the history (ends a burst first); `restore` is the range to write. */
    bool undo (Range current, Range& restore);
    bool redo (Range current, Range& restore);
    History& history() noexcept { return hist; }
    const History& history() const noexcept { return hist; }
    Range lastSeen() const noexcept { return last; }

private:
    void note (Range current); ///< a change not observed yet joins the burst
    History hist;
    Range last, burstStart;
    bool inBurst = false, started = false;
    double lastChange = 0.0;
};
} // namespace ohpe::tapeloop
