// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../engines/Part.h"
#include "../model/Events.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace ohpe
{
/**
    MIDI input of one part including MPE (MIDI Polyphonic Expression, lower zone: master
    channel 1, member channels 2-16). Per-note pitch bend, pressure (channel pressure or poly
    aftertouch) and timbre (CC 74) of each member channel are turned into per-note expression
    on the voices of that channel's notes. Pressure and timbre each go to a selectable target,
    scaled by an amount (0.8, -1..+1: negative amounts invert, e.g. pressure closes the filter).

    Without MPE, channel pressure and CC 74 of the part's channel act on all of its notes and
    everything else goes to Part::handleMidi as before. Audio thread, no allocation.
*/
class MpeInput
{
public:
    /** Targets of pressure / timbre ("pN.mpe.press", "pN.mpe.timbre"; append-only). */
    enum Target
    {
        Off = 0, Timbre, Amp, LfoDepth, Pan,
        Cutoff, Position, FmIndex, Density, Drive, Vibrato, Formant, Blend, // 0.8
        numTargets
    };

    static std::vector<std::string> targetNamesEn()
    {
        return { "Off", "Timbre", "Level", "LFO Depth", "Pan", "Cutoff", "Position", "FM Index", "Grain Density", "Drive",
                 "Vibrato", "Formant", "Layer Blend" };
    }
    static std::vector<std::string> targetNamesDe()
    {
        return { "Aus", "Klangfarbe", "Pegel", "LFO-Tiefe", "Panorama", "Cutoff", "Position", "FM-Index", "Grain-Dichte", "Drive",
                 "Vibrato", "Formant", "Layer-Mischung" };
    }

    void set (bool mpeOn, int memberBendRange, int pressureTarget, int timbreTargetIn, float pressureAmount = 1.0f,
              float timbreAmount = 1.0f) noexcept
    {
        if (mpeOn != mpe) reset();
        mpe = mpeOn;
        bendRange = (float) memberBendRange;
        pressTarget = pressureTarget;
        timbreTarget = timbreTargetIn;
        pressAmount = pressureAmount;
        timbreAmount = timbreAmount;
    }
    bool isMpe() const noexcept { return mpe; }

    void reset() noexcept
    {
        for (auto& c : channels) c = Channel {};
    }

    /** Handles one channel message addressed to this part. */
    void handle (const MidiEvent& e, Part& part) noexcept
    {
        const int ch = e.channel() - 1;
        auto& c = channels[(size_t) ch];
        const bool member = mpe && ch != 0; // lower zone: channel 1 is the master channel
        const int type = e.type();

        if (e.isNoteOn())
        {
            part.noteOn (e.data1, (float) e.data2 / 127.0f);
            c.addNote (e.data1);
            if (member || ! neutral (c)) part.noteExpression (e.data1, expression (c, member));
            return;
        }
        if (e.isNoteOff())
        {
            c.removeNote (e.data1);
            part.handleMidi (e);
            return;
        }
        if (type == 0xe0 && member) { c.bend = e.pitchBendValue(); update (c, part, true); return; }
        if (type == 0xd0) { c.pressure = (float) e.data1 / 127.0f; update (c, part, member); return; }
        if (type == 0xa0) // poly aftertouch: pressure of one note
        {
            Channel single = c;
            single.pressure = (float) e.data2 / 127.0f;
            part.noteExpression (e.data1, expression (single, member));
            return;
        }
        if (e.isController() && e.data1 == 74) { c.timbre = (float) e.data2; update (c, part, member); return; }
        part.handleMidi (e); // master bend, sustain, all notes off, ...
    }

private:
    struct Channel
    {
        int bend = 8192;
        float pressure = 0.0f;
        float timbre = 64.0f;
        std::array<int, 16> notes {};
        int numNotes = 0;

        void addNote (int n) noexcept
        {
            for (int i = 0; i < numNotes; ++i)
                if (notes[(size_t) i] == n) return;
            if (numNotes < (int) notes.size()) notes[(size_t) numNotes++] = n;
        }
        void removeNote (int n) noexcept
        {
            for (int i = 0; i < numNotes; ++i)
                if (notes[(size_t) i] == n) { notes[(size_t) i] = notes[(size_t) --numNotes]; return; }
        }
    };

    static bool neutral (const Channel& c) noexcept { return c.pressure == 0.0f && c.timbre == 64.0f; }

    NoteExpression expression (const Channel& c, bool member) const noexcept
    {
        NoteExpression x;
        if (member) x.pitch = ((float) c.bend - 8192.0f) / 8192.0f * bendRange;
        apply (pressTarget, c.pressure * pressAmount, x);
        apply (timbreTarget, std::clamp ((c.timbre - 64.0f) / 63.0f, -1.0f, 1.0f) * timbreAmount, x);
        return x;
    }

    static void apply (int target, float v, NoteExpression& x) noexcept
    {
        switch (target)
        {
            case Timbre: x.timbre += v; break;
            case Amp: x.amp *= std::clamp (1.0f + v, 0.0f, 2.0f); break;
            case LfoDepth: x.lfo += v; break;
            case Pan: x.pan = std::clamp (x.pan + v, -1.0f, 1.0f); break;
            case Cutoff: x.cutoff += v; break;
            case Position: x.position += v; break;
            case FmIndex: x.index += v; break;
            case Density: x.density += v; break;
            case Drive: x.drive = std::max (0.0f, x.drive + v); break;
            case Vibrato: x.vibrato = std::max (0.0f, x.vibrato + v); break;
            case Formant: x.formant += v; break;
            case Blend: x.blend += v; break;
            default: break;
        }
    }

    void update (const Channel& c, Part& part, bool member) noexcept
    {
        const auto x = expression (c, member);
        if (member)
            for (int i = 0; i < c.numNotes; ++i) part.noteExpression (c.notes[(size_t) i], x);
        else
            part.noteExpression (-1, x); // channel-wide: every note of the part
    }

    std::array<Channel, 16> channels {};
    bool mpe = false;
    float bendRange = 48.0f;
    int pressTarget = Amp, timbreTarget = Timbre;
    float pressAmount = 1.0f, timbreAmount = 1.0f;
};
} // namespace ohpe
