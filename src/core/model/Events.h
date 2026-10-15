// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ohpe
{
/** A raw short MIDI message with a sample offset inside the current block. */
struct MidiEvent
{
    int sampleOffset = 0;
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;

    int channel() const noexcept { return (status & 0x0f) + 1; } ///< 1..16
    int type() const noexcept { return status & 0xf0; }
    bool isNoteOn() const noexcept { return type() == 0x90 && data2 > 0; }
    bool isNoteOff() const noexcept { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
    bool isController() const noexcept { return type() == 0xb0; }
    bool isPitchBend() const noexcept { return type() == 0xe0; }
    int pitchBendValue() const noexcept { return (int) data1 | ((int) data2 << 7); } ///< 0..16383

    static MidiEvent noteOn (int ch, int note, int vel, int offset = 0)
    {
        return { offset, (uint8_t) (0x90 | ((ch - 1) & 0x0f)), (uint8_t) (note & 0x7f), (uint8_t) (vel & 0x7f) };
    }
    static MidiEvent noteOff (int ch, int note, int offset = 0)
    {
        return { offset, (uint8_t) (0x80 | ((ch - 1) & 0x0f)), (uint8_t) (note & 0x7f), 0 };
    }
    static MidiEvent controller (int ch, int cc, int value, int offset = 0)
    {
        return { offset, (uint8_t) (0xb0 | ((ch - 1) & 0x0f)), (uint8_t) (cc & 0x7f), (uint8_t) (value & 0x7f) };
    }
};

/** Fixed-capacity event list (no allocation): sequencers write into it, the core reads it. */
template <int Capacity>
class EventList
{
public:
    bool add (const MidiEvent& e) noexcept
    {
        if (count >= Capacity) return false;
        events[(size_t) count++] = e;
        return true;
    }
    void clear() noexcept { count = 0; }
    int size() const noexcept { return count; }
    const MidiEvent& operator[] (int i) const noexcept { return events[(size_t) i]; }
    const MidiEvent* begin() const noexcept { return events.data(); }
    const MidiEvent* end() const noexcept { return events.data() + count; }

    /** Stable sort by sample offset (insertion sort; lists are short). */
    void sortByTime() noexcept
    {
        for (int i = 1; i < count; ++i)
        {
            auto e = events[(size_t) i];
            int j = i - 1;
            while (j >= 0 && events[(size_t) j].sampleOffset > e.sampleOffset)
            {
                events[(size_t) j + 1] = events[(size_t) j];
                --j;
            }
            events[(size_t) j + 1] = e;
        }
    }

private:
    std::array<MidiEvent, (size_t) Capacity> events {};
    int count = 0;
};

/** Host / internal transport state handed to sequencers every block. */
struct Transport
{
    double bpm = 120.0;
    double ppqAtBlockStart = 0.0; ///< quarter notes since song start
    bool playing = false;
    bool hostDriven = false; ///< true when the host provides position, false = internal clock
};
} // namespace ohpe
