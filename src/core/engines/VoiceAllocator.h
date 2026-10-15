// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>

namespace ohpe
{
/**
    Assigns notes to voice slots. Policy: retrigger a slot already playing the same note,
    otherwise the first idle slot, otherwise steal the oldest released slot, otherwise the
    oldest held slot. Real-time safe (fixed arrays).
*/
template <int MaxVoices>
class VoiceAllocator
{
public:
    struct Slot
    {
        int note = -1;
        uint32_t age = 0;
        bool held = false;
        bool sounding = false;
    };

    void setPolyphony (int n) noexcept { polyphony = n < 1 ? 1 : (n > MaxVoices ? MaxVoices : n); }
    int getPolyphony() const noexcept { return polyphony; }

    /** Returns the slot to use for this note; `stolen` is true when a sounding voice is taken over. */
    int allocate (int note, bool& stolen) noexcept
    {
        stolen = false;
        int chosen = -1;
        for (int i = 0; i < polyphony; ++i)
            if (slots[i].sounding && slots[i].note == note) { chosen = i; stolen = true; break; }
        if (chosen < 0)
            for (int i = 0; i < polyphony; ++i)
                if (! slots[i].sounding) { chosen = i; break; }
        if (chosen < 0)
        {
            chosen = oldest (false);
            if (chosen < 0) chosen = oldest (true);
            stolen = true;
        }
        auto& s = slots[(size_t) chosen];
        s.note = note;
        s.age = ++clock;
        s.held = true;
        s.sounding = true;
        return chosen;
    }

    /** Marks held slots of this note as released; calls fn(slot) for each. */
    template <typename Fn>
    void release (int note, Fn&& fn) noexcept
    {
        for (int i = 0; i < MaxVoices; ++i)
            if (slots[i].held && slots[i].note == note) { slots[i].held = false; fn (i); }
    }

    /** The voice reports it has gone silent. */
    void voiceFinished (int slot) noexcept
    {
        slots[(size_t) slot].sounding = false;
        slots[(size_t) slot].held = false;
        slots[(size_t) slot].note = -1;
    }

    void resetAll() noexcept
    {
        for (auto& s : slots) s = Slot {};
    }

    const Slot& slot (int i) const noexcept { return slots[(size_t) i]; }

private:
    int oldest (bool held) const noexcept
    {
        int best = -1;
        for (int i = 0; i < polyphony; ++i)
            if (slots[i].sounding && slots[i].held == held && (best < 0 || slots[i].age < slots[(size_t) best].age))
                best = i;
        return best;
    }

    std::array<Slot, (size_t) MaxVoices> slots {};
    int polyphony = 8;
    uint32_t clock = 0;
};
} // namespace ohpe
