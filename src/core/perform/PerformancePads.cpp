// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PerformancePads.h"

#include "../dsp/DspUtil.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
namespace
{
constexpr double fadeSeconds = 0.004;   ///< pad in / out
constexpr double throwSeconds = 0.25;   ///< freeze: how long the mix flows into the reverb
constexpr double releaseRt60 = 3.0;     ///< freeze: decay after release
constexpr float frozenFeedback = 0.99995f;
constexpr float throwFeedback = 0.93f;
constexpr int baseLineLen[4] = { 1427, 1871, 2357, 2861 }; ///< at 48 kHz (about 30 .. 60 ms, mutually prime)
const std::vector<std::string> lengthsEn { "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };
const std::vector<std::string> lengthsDe { "1/8", "1/4", "1/2", "1 Takt", "2 Takte", "4 Takte" };
} // namespace

const char* PerformancePads::padId (int pad) noexcept
{
    static const char* ids[numPads] = { "pads.stutter", "pads.stop", "pads.sweep", "pads.freeze" };
    return ids[std::clamp (pad, 0, numPads - 1)];
}

void PerformancePads::registerParams (ParamRegistry& r)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d) { d.group = "pads"; r.add (std::move (d)); };
    add (toggle ("pads.stutter", "Pad Stutter", "Pad Stottern", false));
    add (toggle ("pads.stop", "Pad Tape Stop", "Pad Bandstopp", false));
    add (toggle ("pads.sweep", "Pad Filter Sweep", "Pad Filter-Sweep", false));
    add (toggle ("pads.freeze", "Pad Reverb Freeze", "Pad Hall-Freeze", false));
    add (choice ("pads.quant", "Pad Quantise", "Pad-Quantisierung", { "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar" },
                 { "Aus", "1/16", "1/8", "1/4", "1/2", "1 Takt" }, 1));
    add (choice ("pads.stutter.div", "Stutter Length", "Stotter-Länge", { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" },
                 { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 2));
    add (choice ("pads.stop.len", "Tape Stop Time", "Bandstopp-Dauer", { lengthsEn.begin(), lengthsEn.begin() + 5 },
                 { lengthsDe.begin(), lengthsDe.begin() + 5 }, 2));
    add (choice ("pads.sweep.mode", "Sweep Filter", "Sweep-Filter", { "Low-pass", "High-pass" }, { "Tiefpass", "Hochpass" }, 0));
    add (choice ("pads.sweep.len", "Sweep Time", "Sweep-Dauer", lengthsEn, lengthsDe, 3));
    add (continuous ("pads.sweep.res", "Sweep Resonance", "Sweep-Resonanz", 0.0f, 1.0f, 0.45f, ParamUnit::Percent));
    add (continuous ("pads.freeze.mix", "Freeze Level", "Freeze-Pegel", 0.0f, 1.0f, 0.6f, ParamUnit::Percent));
}

double PerformancePads::quantBeats (int c) noexcept
{
    static const double t[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };
    return t[std::clamp (c, 0, 5)];
}
double PerformancePads::stutterBeats (int c) noexcept
{
    static const double t[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    return t[std::clamp (c, 0, 5)];
}
double PerformancePads::lengthBeats (int c) noexcept
{
    static const double t[] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    return t[std::clamp (c, 0, 5)];
}

void PerformancePads::bind (const ParamRegistry& r)
{
    for (int p = 0; p < numPads; ++p) padIdx[(size_t) p] = r.indexOf (padId (p));
    quantIdx = r.indexOf ("pads.quant");
    divIdx = r.indexOf ("pads.stutter.div");
    stopLenIdx = r.indexOf ("pads.stop.len");
    sweepModeIdx = r.indexOf ("pads.sweep.mode");
    sweepLenIdx = r.indexOf ("pads.sweep.len");
    sweepResIdx = r.indexOf ("pads.sweep.res");
    freezeMixIdx = r.indexOf ("pads.freeze.mix");
}

void PerformancePads::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    fadeStep = (float) (1.0 / (fadeSeconds * sr));
    const auto slice = (size_t) std::ceil (maxSliceSeconds * sr) + 1;
    sliceL.assign (slice, 0.0f);
    sliceR.assign (slice, 0.0f);
    const auto ring = (size_t) std::ceil ((maxStopSeconds + 0.5) * sr);
    ringL.assign (ring, 0.0f);
    ringR.assign (ring, 0.0f);
    for (int k = 0; k < numLines; ++k)
    {
        lineLen[(size_t) k] = std::max (16, (int) std::lround (baseLineLen[k] * sr / 48000.0));
        lines[(size_t) k].assign ((size_t) lineLen[(size_t) k], 0.0f);
    }
    for (auto& f : svf) f.prepare ((float) sr);
    reset();
}

void PerformancePads::reset() noexcept
{
    state.fill (Idle);
    capturing = false;
    sliceLen = capPos = playPos = 0;
    stMix = stopMix = 0.0f;
    writeCount = 0;
    readPos = stopElapsed = 0.0;
    speed = 1.0f;
    sweepPos = 0.0f;
    for (auto& f : svf) f.reset();
    for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
    linePos.fill (0);
    damp.fill (0.0f);
    inGate = feedback = freezeEnv = 0.0f;
    ringing = false;
    for (auto& s : uiStates) s.store (Idle, std::memory_order_relaxed);
    uiSweepPos.store (0.0f, std::memory_order_relaxed);
    uiSpeed.store (1.0f, std::memory_order_relaxed);
    uiPhase.store (0.0f, std::memory_order_relaxed);
    uiFreeze.store (0.0f, std::memory_order_relaxed);
}

void PerformancePads::pullParams (const ParamSnapshot& s) noexcept
{
    for (int p = 0; p < numPads; ++p) want[(size_t) p] = padIdx[(size_t) p] >= 0 && s.getBool (padIdx[(size_t) p]);
    if (quantIdx >= 0) quant = s.getInt (quantIdx);
    if (divIdx >= 0) div = s.getInt (divIdx);
    if (stopLenIdx >= 0) stopLen = s.getInt (stopLenIdx);
    if (sweepModeIdx >= 0) sweepMode = s.getInt (sweepModeIdx);
    if (sweepLenIdx >= 0) sweepLen = s.getInt (sweepLenIdx);
    if (sweepResIdx >= 0) sweepRes = dsp::clamp01 (s[sweepResIdx]);
    if (freezeMixIdx >= 0) freezeMix = dsp::clamp01 (s[freezeMixIdx]);
}

void PerformancePads::setTransport (double tempo, double position, bool isPlaying) noexcept
{
    bpm = tempo > 0.0 && std::isfinite (tempo) ? tempo : 120.0;
    ppq = std::isfinite (position) ? position : 0.0;
    playing = isPlaying;
}

bool PerformancePads::isActive() const noexcept
{
    for (int p = 0; p < numPads; ++p)
        if (want[(size_t) p] || state[(size_t) p] != Idle) return true;
    return stMix > 0.0f || stopMix > 0.0f || sweepPos > 0.0f || ringing;
}

float PerformancePads::heldSource() const noexcept
{
    for (auto s : state)
        if (s == Engaged) return 1.0f;
    return 0.0f;
}

bool PerformancePads::onGrid (double at, double grid) const noexcept
{
    const double step = bpm / 60.0 / sr / grid;
    const double ph = at / grid;
    return ph - std::floor (ph) < step;
}

void PerformancePads::engage (int pad) noexcept
{
    const double beat = 60.0 / bpm * sr; // samples per quarter note
    switch (pad)
    {
        case Stutter:
            sliceLen = (int) std::clamp (std::lround (stutterBeats (div) * beat), 64L, (long) sliceL.size() - 1);
            capPos = playPos = 0;
            capturing = true;
            break;
        case TapeStop:
            writeCount = 0;
            readPos = stopElapsed = 0.0;
            stopDuration = std::clamp (lengthBeats (stopLen) * beat, 0.05 * sr, maxStopSeconds * sr);
            speed = 1.0f;
            break;
        case Sweep:
            if (sweepPos <= 0.0f)
                for (auto& f : svf) f.reset();
            break;
        case Freeze:
            throwElapsed = 0.0;
            ringing = true;
            break;
        default: break;
    }
    state[(size_t) pad] = Engaged;
}

float PerformancePads::freezeTick (float inL, float inR, float& outR) noexcept
{
    std::array<float, numLines> o;
    for (int k = 0; k < numLines; ++k) o[(size_t) k] = lines[(size_t) k][(size_t) linePos[(size_t) k]];
    const float h[numLines] = { 0.5f * (o[0] + o[1] + o[2] + o[3]), 0.5f * (o[0] - o[1] + o[2] - o[3]),
                                0.5f * (o[0] + o[1] - o[2] - o[3]), 0.5f * (o[0] - o[1] - o[2] + o[3]) };
    const float in[numLines] = { inL, inR, inR, -inL };
    const float dampCoef = feedback >= 0.999f ? 0.0f : 0.25f; // frozen: no damping, the sound stays as caught
    for (int k = 0; k < numLines; ++k)
    {
        auto& d = damp[(size_t) k];
        d = dsp::flush (d + (h[k] - d) * (1.0f - dampCoef));
        const float w = std::clamp (d * feedback + in[k] * inGate * 0.5f, -4.0f, 4.0f);
        lines[(size_t) k][(size_t) linePos[(size_t) k]] = dsp::flush (w);
        if (++linePos[(size_t) k] >= lineLen[(size_t) k]) linePos[(size_t) k] = 0;
    }
    outR = 0.5f * (o[1] - o[3]);
    return 0.5f * (o[0] + o[2]);
}

void PerformancePads::process (float* left, float* right, int n) noexcept
{
    if (left == nullptr || right == nullptr || n <= 0) return;
    const double dPpq = bpm / 60.0 / sr;
    const double beat = 60.0 / bpm * sr;
    const double qGrid = quantBeats (quant);
    const float sweepUp = (float) (1.0 / std::clamp (lengthBeats (sweepLen) * beat, 0.02 * sr, 32.0 * sr));
    const float sweepDown = 4.0f * sweepUp;
    const float releaseFeedback = (float) std::pow (10.0, -3.0 * (0.25 * (baseLineLen[0] + baseLineLen[1] + baseLineLen[2] + baseLineLen[3]) / 48000.0)
                                                              / releaseRt60);
    const float gateStep = (float) (1.0 / (0.01 * sr));
    const int fadeLen = std::max (8, std::min ((int) (0.0015 * sr), sliceLen / 4));
    float peak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        // pad states: pending until the grid line, released at once
        const double at = ppq + (double) i * dPpq;
        for (int p = 0; p < numPads; ++p)
        {
            auto& s = state[(size_t) p];
            const bool w = want[(size_t) p];
            if (w && (s == Idle || s == Releasing)) s = Pending;
            if (! w && (s == Pending || s == Engaged)) s = Releasing;
            if (s == Pending && (qGrid <= 0.0 || ! playing || onGrid (at, qGrid))) engage (p);
        }

        float l = left[i], r = right[i];

        // 1. stutter
        if (state[Stutter] != Idle || stMix > 0.0f)
        {
            const bool on = state[Stutter] == Engaged;
            stMix = on ? std::min (1.0f, stMix + fadeStep) : std::max (0.0f, stMix - fadeStep);
            float wl = l, wr = r;
            if (on && capturing)
            {
                const float win = std::min (1.0f, (float) std::min (capPos, sliceLen - 1 - capPos) / (float) fadeLen);
                sliceL[(size_t) capPos] = l;
                sliceR[(size_t) capPos] = r;
                wl = l * win;
                wr = r * win;
                if (++capPos >= sliceLen) { capturing = false; playPos = 0; }
            }
            else if (sliceLen > 0 && ! capturing)
            {
                const float win = std::min (1.0f, (float) std::min (playPos, sliceLen - 1 - playPos) / (float) fadeLen);
                wl = sliceL[(size_t) playPos] * win;
                wr = sliceR[(size_t) playPos] * win;
                if (++playPos >= sliceLen) playPos = 0;
            }
            l += (wl - l) * stMix;
            r += (wr - r) * stMix;
            if (state[Stutter] == Releasing && stMix <= 0.0f) { state[Stutter] = Idle; capturing = false; }
        }

        // 2. tape stop
        if (state[TapeStop] != Idle || stopMix > 0.0f)
        {
            const bool on = state[TapeStop] == Engaged;
            stopMix = on ? std::min (1.0f, stopMix + fadeStep) : std::max (0.0f, stopMix - fadeStep);
            float wl = l, wr = r;
            if (on)
            {
                const auto N = (long long) ringL.size();
                if (speed > 0.0f)
                {
                    ringL[(size_t) (writeCount % N)] = l;
                    ringR[(size_t) (writeCount % N)] = r;
                    ++writeCount;
                }
                const long long i0 = std::min ((long long) readPos, std::max (0LL, writeCount - 1));
                const long long i1 = std::min (i0 + 1, std::max (0LL, writeCount - 1));
                const float fr = (float) (readPos - std::floor (readPos));
                const float g = std::min (1.0f, speed * 6.0f);
                wl = g * (ringL[(size_t) (i0 % N)] + (ringL[(size_t) (i1 % N)] - ringL[(size_t) (i0 % N)]) * fr);
                wr = g * (ringR[(size_t) (i0 % N)] + (ringR[(size_t) (i1 % N)] - ringR[(size_t) (i0 % N)]) * fr);
                readPos += speed;
                stopElapsed += 1.0;
                speed = (float) std::max (0.0, 1.0 - stopElapsed / stopDuration);
            }
            else
                wl = wr = 0.0f; // fading out of a stopped tape: towards the live mix
            l += (wl - l) * stopMix;
            r += (wr - r) * stopMix;
            if (state[TapeStop] == Releasing && stopMix <= 0.0f) { state[TapeStop] = Idle; speed = 1.0f; }
        }

        // 3. filter sweep
        if (state[Sweep] != Idle || sweepPos > 0.0f)
        {
            const bool on = state[Sweep] == Engaged;
            sweepPos = on ? std::min (1.0f, sweepPos + sweepUp) : std::max (0.0f, sweepPos - sweepDown);
            if (--coeffCountdown <= 0)
            {
                coeffCountdown = 16;
                const float s = sweepPos;
                const float hz = sweepMode == 0 ? 20000.0f * std::pow (150.0f / 20000.0f, s) : 20.0f * std::pow (8000.0f / 20.0f, s);
                const float res = 0.9f * sweepRes * std::min (1.0f, s * 4.0f);
                for (auto& f : svf) f.set (hz, res);
            }
            const int mode = sweepMode == 0 ? dsp::Svf::LowPass : dsp::Svf::HighPass;
            const float wl = svf[0].process (l, mode), wr = svf[1].process (r, mode);
            const float amt = std::min (1.0f, sweepPos * 12.0f);
            l += (wl - l) * amt;
            r += (wr - r) * amt;
            if (state[Sweep] == Releasing && sweepPos <= 0.0f) state[Sweep] = Idle;
        }

        // 4. reverb freeze (added under the dry mix)
        if (ringing)
        {
            const bool on = state[Freeze] == Engaged;
            const bool throwing = on && throwElapsed < throwSeconds;
            if (on) throwElapsed += 1.0 / sr;
            inGate = throwing ? std::min (1.0f, inGate + gateStep) : std::max (0.0f, inGate - gateStep);
            const float fbTarget = throwing ? throwFeedback : on ? frozenFeedback : releaseFeedback;
            feedback += (fbTarget - feedback) * (8.0f * gateStep); // about 1 ms: the freeze closes right after the throw
            float fr = 0.0f;
            const float fl = freezeTick (l, r, fr);
            const float level = 0.7f * freezeMix;
            l += fl * level;
            r += fr * level;
            peak = std::max (peak, std::max (std::abs (fl), std::abs (fr)));
        }

        left[i] = l;
        right[i] = r;
    }

    if (ringing)
    {
        freezeEnv = std::max (peak, freezeEnv * 0.5f);
        if (state[Freeze] != Engaged && state[Freeze] != Pending && freezeEnv < 1.0e-5f && inGate <= 0.0f)
        {
            ringing = false;
            state[Freeze] = Idle;
            for (auto& ln : lines) std::fill (ln.begin(), ln.end(), 0.0f);
            damp.fill (0.0f);
            feedback = 0.0f;
            freezeEnv = 0.0f;
        }
    }
    else if (state[Freeze] == Releasing)
        state[Freeze] = Idle;

    for (int p = 0; p < numPads; ++p) uiStates[(size_t) p].store ((int) state[(size_t) p], std::memory_order_relaxed);
    uiSweepPos.store (sweepPos, std::memory_order_relaxed);
    uiSpeed.store (state[TapeStop] == Engaged ? speed : 1.0f, std::memory_order_relaxed);
    uiPhase.store (state[Stutter] == Engaged && ! capturing && sliceLen > 0 ? (float) playPos / (float) sliceLen : 0.0f, std::memory_order_relaxed);
    uiFreeze.store (ringing ? std::min (1.0f, freezeEnv * 4.0f) : 0.0f, std::memory_order_relaxed);
}
} // namespace ohpe
