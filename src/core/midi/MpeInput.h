// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../engines/Part.h"
#include "../model/Events.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
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

    p50 Fingerboard mode / MPE HD (14/21-bit), receiver side, backwards compatible with plain MPE:
    - CC 87 is an LSB prefix: its value is kept per channel and the NEXT channel pressure, CC 74 or pitch bend on
      that channel uses it as 7 extra low bits (then it is cleared). Pressure / timbre become 14-bit
      (0x0000 .. 0x3F80, 0x3F80 = 1.0), the bend 21-bit. Without CC 87 the same MSB values mean the same thing.
    - NRPN 100 / 101 / 102 (CC 99 = 0, CC 98 = 100..102, CC 6 = value): low-pass cutoff of X (pitch) / Y (timbre) /
      Z (pressure) in 2 Hz units; the voices smooth each dimension with it (replaces the fixed smoothing).
    - RPN 0 (bend range) on a member channel sets the bend range of all member channels; RPN 6 (MPE configuration
      message) on channel 1 / 16 selects the lower / upper zone.
    "pN.mpe.onset": with "Pressure" the velocity is ignored (these controllers always send 127) and the note's level
    follows the pressure (0 = silent), with "Velocity x Pressure" both count. "pN.mpe.pcurve" shapes the pressure.
    "pN.mpe.hd": Off = CC 87 / (N)RPN are ignored (as before), Auto = used as soon as it arrives (default: plain
    MPE sounds exactly as before), On = the smoothing also runs with default cutoffs before any NRPN arrived.
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
              float timbreAmountIn = 1.0f) noexcept
    {
        if (mpeOn != mpe) reset();
        mpe = mpeOn;
        bendRange = (float) memberBendRange;
        pressTarget = pressureTarget;
        timbreTarget = timbreTargetIn;
        pressAmount = pressureAmount;
        timbreAmount = timbreAmountIn;
    }
    bool isMpe() const noexcept { return mpe; }

    /** p50: "pN.mpe.hd" (HdMode), "pN.mpe.onset" (Onset), "pN.mpe.pcurve" (-1..+1, 0 = linear pressure). */
    enum HdMode { HdOff = 0, HdAuto, HdOn, numHdModes };
    enum Onset { OnsetVelocity = 0, OnsetPressure, OnsetBoth, numOnsets };
    static std::vector<std::string> hdNamesEn() { return { "Off", "Auto", "On" }; }
    static std::vector<std::string> hdNamesDe() { return { "Aus", "Auto", "Ein" }; }
    static std::vector<std::string> onsetNamesEn() { return { "Velocity", "Pressure", "Velocity x Pressure" }; }
    static std::vector<std::string> onsetNamesDe() { return { "Anschlag", "Druck", "Anschlag x Druck" }; }
    /** Default cutoffs (Hz) of X / Y / Z until the controller sends its own (NRPN 100..102). */
    static constexpr float defaultCutoffHz[3] = { 120.0f, 60.0f, 250.0f };
    /** Status bits for the UI (statusBits()). */
    enum Status : int { StatusHdData = 1, StatusCutoffs = 2, StatusBendRange = 4, StatusUpperZone = 8 };

    void setHd (int hdModeIn, int onsetIn, float pressureCurve) noexcept
    {
        hdMode = std::clamp (hdModeIn, 0, (int) numHdModes - 1);
        onset = std::clamp (onsetIn, 0, (int) numOnsets - 1);
        pCurve = std::isfinite (pressureCurve) ? std::clamp (pressureCurve, -1.0f, 1.0f) : 0.0f;
    }
    int onsetMode() const noexcept { return onset; }

    /** p50: low-pass cutoff (Hz) the voices use for X / Y / Z (dim 0..2); 0 = the classic smoothing (mpe.smooth). */
    float cutoffHz (int dim) const noexcept
    {
        const int d = std::clamp (dim, 0, 2);
        const bool active = hdMode == HdOn || (hdMode == HdAuto && (hdSeen || cutoffSeen));
        if (! active) return 0.0f;
        return rxCutoff[(size_t) d] > 0.0f ? rxCutoff[(size_t) d] : defaultCutoffHz[d];
    }
    /** p50 (UI, any thread): Status bits seen since the last reset. */
    int statusBits() const noexcept { return status.load (std::memory_order_relaxed); }
    /** p50 (UI, any thread): bend range received with RPN 0 on a member channel (-1 = none). */
    int receivedBendRange() const noexcept { return statusBend.load (std::memory_order_relaxed); }
    /** p50 (UI, any thread): cutoff received for dim 0..2 in Hz (0 = none). */
    float receivedCutoff (int dim) const noexcept { return statusCutoff[(size_t) std::clamp (dim, 0, 2)].load (std::memory_order_relaxed); }
    /** p50 (tests): the member bend range in use. */
    float memberBendRange() const noexcept { return rxBendRange >= 0 && hdMode != HdOff ? (float) rxBendRange : bendRange; }

    void reset() noexcept
    {
        for (auto& c : channels) c = Channel {};
        hdSeen = cutoffSeen = false;
        rxCutoff = { 0.0f, 0.0f, 0.0f };
        rxBendRange = -1;
        masterCh = 0;
        status.store (0, std::memory_order_relaxed);
        statusBend.store (-1, std::memory_order_relaxed);
        for (auto& c : statusCutoff) c.store (0.0f, std::memory_order_relaxed);
    }

    /** Handles one channel message addressed to this part. */
    void handle (const MidiEvent& e, Part& part) noexcept
    {
        const int ch = e.channel() - 1;
        auto& c = channels[(size_t) ch];
        const bool member = mpe && ch != masterCh; // lower zone: channel 1 is the master channel (p50: upper: 16)
        const int type = e.type();
        const bool hd = hdMode != HdOff;

        if (e.isNoteOn())
        {
            // p50: with the pressure onset the velocity is ignored (the level follows the pressure)
            part.noteOn (e.data1, onset == OnsetPressure ? 1.0f : (float) e.data2 / 127.0f);
            c.addNote (e.data1);
            if (member || ! neutral (c) || onset != OnsetVelocity) part.noteExpression (e.data1, expression (c, member));
            return;
        }
        if (e.isNoteOff())
        {
            c.removeNote (e.data1);
            part.handleMidi (e);
            return;
        }
        if (type == 0xe0 && member)
        {
            c.bend = (float) e.pitchBendValue() + (hd ? takeLsb (c) : 0.0f); // p50: 21-bit with a CC 87 prefix
            update (c, part, true);
            return;
        }
        if (type == 0xd0)
        {
            c.pressure = hd ? hiRes (e.data1, takeLsb (c)) : (float) e.data1 / 127.0f; // p50: 14-bit with CC 87
            update (c, part, member);
            return;
        }
        if (type == 0xa0) // poly aftertouch: pressure of one note
        {
            Channel single = c;
            single.pressure = (float) e.data2 / 127.0f;
            part.noteExpression (e.data1, expression (single, member));
            return;
        }
        if (e.isController() && e.data1 == 74)
        {
            c.timbre = std::min (127.0f, (float) e.data2 + (hd ? takeLsb (c) : 0.0f)); // p50: 14-bit with CC 87
            update (c, part, member);
            return;
        }
        if (hd && e.isController() && controller (e, c, ch)) return; // p50: CC 87 prefix, RPN / NRPN
        if (type == 0xe0) c.lsb = -1; // p50: a master-channel bend used its prefix (the part's bend is 14-bit)
        part.handleMidi (e); // master bend, sustain, all notes off, ...
    }

private:
    struct Channel
    {
        float bend = 8192.0f;  ///< 14-bit units (p50: with the 7 extra bits of a CC 87 prefix as a fraction)
        float pressure = 0.0f; ///< 0..1
        float timbre = 64.0f;  ///< 0..127 (p50: fraction from CC 87)
        int lsb = -1;          ///< p50: pending CC 87 value (-1 = none)
        int paramMsb = 127, paramLsb = 127; ///< p50: selected (N)RPN (127 / 127 = null)
        bool rpn = false;
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
        if (member) x.pitch = (c.bend - 8192.0f) / 8192.0f * memberBendRange();
        // 0.7: the raw values for the modulation matrix (p50: through the pressure curve)
        const float press = shapePressure (c.pressure);
        x.pressure = press;
        x.slide = std::clamp ((c.timbre - 64.0f) / 63.0f, -1.0f, 1.0f);
        x.noteBend = member ? std::clamp ((c.bend - 8192.0f) / 8191.0f, -1.0f, 1.0f) : 0.0f;
        const bool pressOnset = onset != OnsetVelocity;
        if (pressOnset) x.amp = press; // p50: the finger's pressure is the note's level (0 = silent)
        // 0.8: target amounts (p50: with the pressure onset the level already follows the pressure)
        if (! (pressOnset && pressTarget == Amp)) apply (pressTarget, press * pressAmount, x);
        apply (timbreTarget, x.slide * timbreAmount, x);
        return x;
    }

    /** p50: pressure curve, 0 = linear, > 0 = softer start (exponent up to 3), < 0 = faster start (down to 1/3). */
    float shapePressure (float p) const noexcept
    {
        p = std::clamp (p, 0.0f, 1.0f);
        if (pCurve == 0.0f) return p;
        return std::pow (p, std::exp2 (pCurve * 1.5849625f)); // 2^(c * log2 3): 1/3 .. 3
    }

    /** p50: the pending CC 87 bits as a fraction of one MSB step (0 when there is none); clears them. */
    static float takeLsb (Channel& c) noexcept
    {
        if (c.lsb < 0) return 0.0f;
        const float f = (float) c.lsb / 128.0f;
        c.lsb = -1;
        return f;
    }
    /** p50: 7-bit MSB + fraction -> 0..1 (MSB 127 + 0 = 0x3F80 = exactly 1). */
    static float hiRes (int msb, float frac) noexcept { return std::clamp (((float) msb + frac) / 127.0f, 0.0f, 1.0f); }

    /** p50: CC 87 prefix, (N)RPN selection and data entry; true = consumed. */
    bool controller (const MidiEvent& e, Channel& c, int ch) noexcept
    {
        const int v = e.data2 & 0x7f;
        switch (e.data1)
        {
            case 87:
                c.lsb = v;
                if (! hdSeen) { hdSeen = true; status.fetch_or (StatusHdData, std::memory_order_relaxed); }
                return true;
            case 99: c.paramMsb = v; c.rpn = false; return true;
            case 98: c.paramLsb = v; c.rpn = false; return true;
            case 101: c.paramMsb = v; c.rpn = true; return true;
            case 100: c.paramLsb = v; c.rpn = true; return true;
            case 38: return c.paramMsb != 127 || c.paramLsb != 127; // data entry LSB: not needed here
            case 6: break;
            default: return false;
        }
        if (c.paramMsb == 127 && c.paramLsb == 127) return true; // null (N)RPN
        if (! c.rpn && c.paramMsb == 0 && c.paramLsb >= 100 && c.paramLsb <= 102)
        {
            const int d = c.paramLsb - 100;
            rxCutoff[(size_t) d] = v == 0 ? 20000.0f : 2.0f * (float) v; // 0 = no filter
            cutoffSeen = true;
            statusCutoff[(size_t) d].store (rxCutoff[(size_t) d], std::memory_order_relaxed);
            status.fetch_or (StatusCutoffs, std::memory_order_relaxed);
            return true;
        }
        if (c.rpn && c.paramMsb == 0 && c.paramLsb == 0) // bend range (semitones)
        {
            if (mpe && ch != masterCh)
            {
                rxBendRange = std::clamp (v, 0, 96);
                statusBend.store (rxBendRange, std::memory_order_relaxed);
                status.fetch_or (StatusBendRange, std::memory_order_relaxed);
                return true;
            }
            return false; // master channel: the part's own bend range applies
        }
        if (c.rpn && c.paramMsb == 0 && c.paramLsb == 6) // MPE configuration message: zone
        {
            if (ch == 0) masterCh = 0;
            else if (ch == 15) masterCh = v > 0 ? 15 : 0;
            rxBendRange = -1; // a new zone starts at the configured range
            statusBend.store (-1, std::memory_order_relaxed);
            if (masterCh == 15) status.fetch_or (StatusUpperZone, std::memory_order_relaxed);
            else status.fetch_and (~(int) StatusUpperZone, std::memory_order_relaxed);
            return true;
        }
        return false;
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
    // p50: MPE HD state
    int hdMode = HdAuto, onset = OnsetVelocity;
    float pCurve = 0.0f;
    bool hdSeen = false, cutoffSeen = false;
    std::array<float, 3> rxCutoff { 0.0f, 0.0f, 0.0f };
    int rxBendRange = -1;
    int masterCh = 0;
    std::atomic<int> status { 0 }, statusBend { -1 };
    std::array<std::atomic<float>, 3> statusCutoff {};
    bool mpe = false;
    float bendRange = 48.0f;
    int pressTarget = Amp, timbreTarget = Timbre;
    float pressAmount = 1.0f, timbreAmount = 1.0f;
};
} // namespace ohpe
