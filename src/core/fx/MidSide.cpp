// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidSide.h"

#include <cmath>

namespace ohpe
{
std::vector<ParamDescriptor> MidSideEffect::getParamDescriptors() const
{
    using namespace pd;
    // registry order = page order on the display (after "type"): width / levels / balance, bass mono / shift, tilts.
    // The first four are what the modulation matrix moves ("FX k Param 1-4"): all continuous.
    return {
        continuous ("width", "Stereo Width", "Stereobreite", 0.0f, 2.0f, 1.0f, ParamUnit::Percent),
        continuous ("mid", "Mid Level", "Mitte-Pegel", -24.0f, 12.0f, 0.0f, ParamUnit::Decibel),
        continuous ("side", "Side Level", "Seite-Pegel", -24.0f, 12.0f, 0.0f, ParamUnit::Decibel),
        continuous ("balance", "Balance", "Balance", -1.0f, 1.0f, 0.0f, ParamUnit::Percent),
        toggle ("bassmono", "Bass Mono", "Bass mono", false),
        continuous ("monofreq", "Mono Below", "Mono unter", 40.0f, 500.0f, 120.0f, ParamUnit::Hertz, 0.5f),
        continuous ("haas", "Micro-Shift", "Mikroversatz", 0.0f, 1.0f, 0.0f),
        continuous ("haastime", "Shift Time", "Versatzzeit", 0.001f, maxHaasSeconds, 0.012f, ParamUnit::Seconds, 0.6f),
        continuous ("midtilt", "Mid Tilt", "Mitte-Tilt", -6.0f, 6.0f, 0.0f, ParamUnit::Decibel),
        continuous ("sidetilt", "Side Tilt", "Seite-Tilt", -6.0f, 6.0f, 0.0f, ParamUnit::Decibel),
    };
}

void MidSideEffect::tiltGains (float db, float& low, float& high) noexcept
{
    // +db = brighter: highs up by db/2, lows down by db/2 (exactly 1 at 0 dB)
    if (db == 0.0f) { low = high = 1.0f; return; }
    low = std::pow (10.0f, -db / 40.0f);
    high = std::pow (10.0f, db / 40.0f);
}

void MidSideEffect::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? (float) sampleRate : 48000.0f;
    delayBuf.assign ((size_t) (maxHaasSeconds * sr) + 4, 0.0f);
    for (auto* s : { &midGain, &sideGain, &balL, &balR, &monoMix, &haasAmt })
        s->prepare (sr, 20.0f);
    haasDelay.prepare (sr, 60.0f);
    for (auto* t : { &midTiltF, &sideTiltF })
    {
        t->lp.setCutoff (pivotHz, sr);
        t->low.prepare (sr, 20.0f);
        t->high.prepare (sr, 20.0f);
    }
    for (auto& f : sideHp) f.prepare (sr);
    for (auto& f : sideHp) f.set (monoHz, 0.299f); // Butterworth Q
    reset();
}

void MidSideEffect::reset() noexcept
{
    std::fill (delayBuf.begin(), delayBuf.end(), 0.0f);
    writePos = 0;
    for (auto& f : sideHp) f.reset();
    midTiltF.lp.reset();
    sideTiltF.lp.reset();
    snapSmoothers();
    active = false;
}

void MidSideEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Width: width = std::clamp (v, 0.0f, 2.0f); break;
        case Mid: midDb = std::clamp (v, -24.0f, 12.0f); break;
        case Side: sideDb = std::clamp (v, -24.0f, 12.0f); break;
        case BassMono: bassMono = v >= 0.5f; break;
        case MonoFreq:
            monoHz = std::clamp (v, 20.0f, 1000.0f);
            for (auto& f : sideHp) f.set (monoHz, 0.299f);
            break;
        case Balance: balance = std::clamp (v, -1.0f, 1.0f); break;
        case Haas: haas = dsp::clamp01 (v); break;
        case MidTilt: midTilt = std::clamp (v, -6.0f, 6.0f); break;
        case SideTilt: sideTilt = std::clamp (v, -6.0f, 6.0f); break;
        case HaasTime: haasTime = std::clamp (v, 0.001f, maxHaasSeconds); break;
        default: break;
    }
}

bool MidSideEffect::isNeutral() const noexcept
{
    return width == 1.0f && midDb == 0.0f && sideDb == 0.0f && ! bassMono && balance == 0.0f && haas == 0.0f && midTilt == 0.0f
           && sideTilt == 0.0f;
}

bool MidSideEffect::settled() const noexcept
{
    auto near = [] (const dsp::Smoother& s, float target) { return std::abs (s.value() - target) < 1.0e-6f; };
    return near (midGain, 1.0f) && near (sideGain, 1.0f) && near (balL, 1.0f) && near (balR, 1.0f) && near (monoMix, 0.0f)
           && near (haasAmt, 0.0f) && near (midTiltF.low, 1.0f) && near (midTiltF.high, 1.0f) && near (sideTiltF.low, 1.0f)
           && near (sideTiltF.high, 1.0f);
}

void MidSideEffect::snapSmoothers() noexcept
{
    midGain.reset (dsp::dbToGain (midDb));
    sideGain.reset (dsp::dbToGain (sideDb) * width);
    balL.reset (balance > 0.0f ? 1.0f - balance : 1.0f);
    balR.reset (balance < 0.0f ? 1.0f + balance : 1.0f);
    monoMix.reset (bassMono ? 1.0f : 0.0f);
    haasAmt.reset (haas);
    haasDelay.reset (haasTime * sr);
    float lo, hi;
    tiltGains (midTilt, lo, hi);
    midTiltF.low.reset (lo);
    midTiltF.high.reset (hi);
    tiltGains (sideTilt, lo, hi);
    sideTiltF.low.reset (lo);
    sideTiltF.high.reset (hi);
}

void MidSideEffect::process (float* l, float* r, int n) noexcept
{
    if (delayBuf.empty() || n <= 0) return;
    if (isNeutral() && (! active || settled()))
    {
        if (active) { snapSmoothers(); active = false; }
        return; // exact bypass
    }
    if (! active)
    {
        // leaving the bypass: fresh filter states, smoothers glide from neutral to the settings
        for (auto& f : sideHp) f.reset();
        midTiltF.lp.reset();
        sideTiltF.lp.reset();
        std::fill (delayBuf.begin(), delayBuf.end(), 0.0f);
        active = true;
    }

    midGain.setTarget (dsp::dbToGain (midDb));
    sideGain.setTarget (dsp::dbToGain (sideDb) * width);
    balL.setTarget (balance > 0.0f ? 1.0f - balance : 1.0f);
    balR.setTarget (balance < 0.0f ? 1.0f + balance : 1.0f);
    monoMix.setTarget (bassMono ? 1.0f : 0.0f);
    haasAmt.setTarget (haas);
    haasDelay.setTarget (haasTime * sr);
    float lo, hi;
    tiltGains (midTilt, lo, hi);
    midTiltF.low.setTarget (lo);
    midTiltF.high.setTarget (hi);
    tiltGains (sideTilt, lo, hi);
    sideTiltF.low.setTarget (lo);
    sideTiltF.high.setTarget (hi);
    // a glide that has arrived lands exactly on its target (width 0 = exactly mono, 100 % = exact)
    for (auto* sm : { &midGain, &sideGain, &balL, &balR, &monoMix, &haasAmt, &haasDelay, &midTiltF.low, &midTiltF.high,
                      &sideTiltF.low, &sideTiltF.high })
        sm->settle (1.0e-6f);

    const int len = (int) delayBuf.size();
    for (int i = 0; i < n; ++i)
    {
        float m = 0.5f * (l[i] + r[i]);
        float s = 0.5f * (l[i] - r[i]);

        // bass mono: remove the side signal below the crossover (cross-faded on / off)
        const float mono = monoMix.next();
        const float hp = sideHp[1].process (sideHp[0].process (s, dsp::Svf::HighPass), dsp::Svf::HighPass);
        s += (hp - s) * mono;

        m = midTiltF.process (m);
        s = sideTiltF.process (s);

        // micro-shift: delayed mid into the side (fractional delay, linear interpolation)
        delayBuf[(size_t) writePos] = m;
        const float amt = haasAmt.next();
        const float d = std::clamp (haasDelay.next(), 1.0f, (float) (len - 2));
        float rp = (float) writePos - d;
        if (rp < 0.0f) rp += (float) len;
        const int i0 = (int) rp;
        const int i1 = i0 + 1 < len ? i0 + 1 : 0;
        const float frac = rp - (float) i0;
        const float delayed = delayBuf[(size_t) i0] + (delayBuf[(size_t) i1] - delayBuf[(size_t) i0]) * frac;
        if (++writePos >= len) writePos = 0;

        m *= midGain.next();
        s = s * sideGain.next() + delayed * amt * 0.7f;

        l[i] = dsp::flush ((m + s) * balL.next());
        r[i] = dsp::flush ((m - s) * balR.next());
    }
}
} // namespace ohpe
