// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Mixer.h"

#include <cstring>
#include <string>

namespace ohpe
{
namespace
{
std::string chId (int ch, const char* name) { return "mix.ch" + std::to_string (ch + 1) + "." + name; }

std::vector<std::string> outChoices (bool de)
{
    std::vector<std::string> c { de ? "Haupt" : "Main" };
    for (int i = 1; i <= numAuxOutputs; ++i) c.push_back ((de ? "Ausgang " : "Out ") + std::to_string (i));
    return c;
}
} // namespace

int Mixer::defaultSourceForChannel (int ch) noexcept
{
    // ch1-4 = parts 1-4, ch5 = drum, ch6-8 = tape tracks 1-3 (changeable per channel)
    if (ch < 4) return source::part1 + ch;
    if (ch == 4) return source::drum;
    return source::tape1 + (ch - 5);
}

void Mixer::registerParams (ParamRegistry& reg) const
{
    using namespace pd;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto n = std::to_string (ch + 1);
        const auto g = "mixer/ch" + n;
        auto add = [&] (ParamDescriptor d) { d.group = g; reg.add (std::move (d)); };
        add (integer (chId (ch, "src"), "Ch " + n + " Source", "Kanal " + n + " Quelle", 0, source::maxSources - 1,
                      defaultSourceForChannel (ch)));
        add (continuous (chId (ch, "vol"), "Ch " + n + " Volume", "Kanal " + n + " Lautstärke", -60.0f, 6.0f, 0.0f,
                         ParamUnit::Decibel, 2.0f));
        add (continuous (chId (ch, "pan"), "Ch " + n + " Pan", "Kanal " + n + " Panorama", -1.0f, 1.0f, 0.0f));
        add (toggle (chId (ch, "mute"), "Ch " + n + " Mute", "Kanal " + n + " Stumm", false));
        add (toggle (chId (ch, "solo"), "Ch " + n + " Solo", "Kanal " + n + " Solo", false));
        add (continuous (chId (ch, "eqlo"), "Ch " + n + " EQ Low", "Kanal " + n + " EQ Tiefen", -15.0f, 15.0f, 0.0f, ParamUnit::Decibel));
        add (continuous (chId (ch, "eqmid"), "Ch " + n + " EQ Mid", "Kanal " + n + " EQ Mitten", -15.0f, 15.0f, 0.0f, ParamUnit::Decibel));
        add (continuous (chId (ch, "eqhi"), "Ch " + n + " EQ High", "Kanal " + n + " EQ Höhen", -15.0f, 15.0f, 0.0f, ParamUnit::Decibel));
        add (continuous (chId (ch, "senda"), "Ch " + n + " Send A", "Kanal " + n + " Send A", 0.0f, 1.0f, 0.0f));
        add (continuous (chId (ch, "sendb"), "Ch " + n + " Send B", "Kanal " + n + " Send B", 0.0f, 1.0f, 0.0f));
        add (choice (chId (ch, "out"), "Ch " + n + " Output", "Kanal " + n + " Ausgang", outChoices (false), outChoices (true), 0));
    }
    auto addM = [&] (ParamDescriptor d) { d.group = "mixer/master"; reg.add (std::move (d)); };
    addM (continuous ("mix.master.vol", "Master Volume", "Master-Lautstärke", -60.0f, 6.0f, 0.0f, ParamUnit::Decibel, 2.0f));
    addM (toggle ("mix.master.limit", "Master Limiter", "Master-Limiter", true));
    addM (continuous ("mix.returna", "Return A (Delay)", "Return A (Delay)", 0.0f, 1.0f, 0.7f));
    addM (continuous ("mix.returnb", "Return B (Reverb)", "Return B (Hall)", 0.0f, 1.0f, 0.7f));

    for (auto d : delay.getParamDescriptors()) { d.id = "fxa.delay." + d.id; d.group = "fx/a"; reg.add (std::move (d)); }
    for (auto d : reverb.getParamDescriptors()) { d.id = "fxb.reverb." + d.id; d.group = "fx/b"; reg.add (std::move (d)); }
}

void Mixer::registerDynamicsParams (ParamRegistry& reg) const
{
    using namespace pd;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto n = std::to_string (ch + 1);
        auto d = continuous (chId (ch, "comp"), "Ch " + n + " Compressor", "Kanal " + n + " Kompressor", 0.0f, 1.0f, 0.0f);
        d.group = "mixer/ch" + n;
        reg.add (std::move (d));
    }
    auto addM = [&] (ParamDescriptor d) { d.group = "mixer/master"; reg.add (std::move (d)); };
    addM (toggle ("mix.master.comp", "Master Compressor", "Master-Kompressor", false));
    addM (continuous ("mix.master.comp.thresh", "Comp Threshold", "Kompressor Schwelle", -40.0f, 0.0f, -12.0f, ParamUnit::Decibel));
    addM (continuous ("mix.master.comp.ratio", "Comp Ratio", "Kompressor Ratio", 1.0f, 20.0f, 3.0f, ParamUnit::None, 0.4f));
    addM (continuous ("mix.master.comp.attack", "Comp Attack", "Kompressor Attack", 0.0001f, 0.1f, 0.01f, ParamUnit::Seconds, 0.4f));
    addM (continuous ("mix.master.comp.release", "Comp Release", "Kompressor Release", 0.01f, 2.0f, 0.15f, ParamUnit::Seconds, 0.4f));
    addM (continuous ("mix.master.comp.makeup", "Comp Makeup", "Kompressor Aufholverstärkung", 0.0f, 24.0f, 0.0f, ParamUnit::Decibel));
    addM (continuous ("mix.master.tapesat", "Master Tape Saturation", "Master-Bandsättigung", 0.0f, 1.0f, 0.0f));
    addM (continuous ("mix.master.ceiling", "Limiter Ceiling", "Limiter-Obergrenze", -12.0f, 0.0f, -0.3f, ParamUnit::Decibel));
}

void Mixer::registerLate05Params (ParamRegistry& reg) const
{
    using namespace pd;
    auto d = choice ("fxa.delay.sync", "Delay Sync", "Delay-Sync", { "Off", "1/16", "1/8T", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2" },
                     { "Aus", "1/16", "1/8T", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2" }, 0);
    d.group = "fx/a";
    reg.add (std::move (d));
}

double Mixer::delaySyncBeats (int choice) noexcept
{
    static constexpr double beats[] = { 0.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
    return beats[(size_t) std::clamp (choice, 0, 8)];
}

void Mixer::bind (const ParamRegistry& reg)
{
    static const char* names[numChannelParams] = { "src", "vol", "pan", "mute", "solo", "eqlo", "eqmid", "eqhi", "senda", "sendb", "out" };
    for (int ch = 0; ch < numChannels; ++ch)
        for (int p = 0; p < numChannelParams; ++p)
            channels[(size_t) ch].idx[(size_t) p] = reg.indexOf (chId (ch, names[p]));
    for (int ch = 0; ch < numChannels; ++ch) channels[(size_t) ch].compIdx = reg.indexOf (chId (ch, "comp"));
    dynIdx = { reg.indexOf ("mix.master.comp"), reg.indexOf ("mix.master.comp.thresh"), reg.indexOf ("mix.master.comp.ratio"),
               reg.indexOf ("mix.master.comp.attack"), reg.indexOf ("mix.master.comp.release"), reg.indexOf ("mix.master.comp.makeup"),
               reg.indexOf ("mix.master.tapesat"), reg.indexOf ("mix.master.ceiling") };
    masterIdx = { reg.indexOf ("mix.master.vol"), reg.indexOf ("mix.master.limit"), reg.indexOf ("mix.returna"), reg.indexOf ("mix.returnb") };
    delayIdx.clear();
    for (const auto& d : delay.getParamDescriptors()) delayIdx.push_back (reg.indexOf ("fxa.delay." + d.id));
    reverbIdx.clear();
    for (const auto& d : reverb.getParamDescriptors()) reverbIdx.push_back (reg.indexOf ("fxb.reverb." + d.id));
    delayLast.assign (delayIdx.size(), NAN);
    delaySyncIdx = reg.indexOf ("fxa.delay.sync");
    reverbLast.assign (reverbIdx.size(), NAN);
}

void Mixer::prepare (double sampleRate, int maxBlockSize)
{
    sr = (float) sampleRate;
    const auto n = (size_t) maxBlockSize;
    for (auto* v : { &chL, &chR, &sendAL, &sendAR, &sendBL, &sendBR, &masterL, &masterR }) v->assign (n, 0.0f);
    delay.prepare (sampleRate, maxBlockSize);
    reverb.prepare (sampleRate, maxBlockSize);
    limiter.prepare (sr);
    for (auto& c : channels)
    {
        c.gainL.prepare (sr, 15.0f);
        c.gainR.prepare (sr, 15.0f);
        c.comp.prepare (sr);
    }
    masterComp.prepare (sr);
    masterSat.prepare (sr);
    std::fill (delayLast.begin(), delayLast.end(), NAN);
    std::fill (reverbLast.begin(), reverbLast.end(), NAN);
    reset();
}

void Mixer::reset() noexcept
{
    for (auto& c : channels)
    {
        for (auto* b : { &c.low, &c.mid, &c.high })
            for (auto& f : *b) f.reset();
        c.gainL.reset (0.0f);
        c.gainR.reset (0.0f);
        c.comp.reset();
    }
    masterComp.reset();
    masterSat.reset();
    numDirects = 0;
    for (auto& r : reductions) r.store (0.0f);
    delay.reset();
    reverb.reset();
    limiter.reset();
    for (auto& m : meters) m.store (0.0f);
}

void Mixer::pullParams (const ParamSnapshot& s) noexcept
{
    for (auto& c : channels)
    {
        c.src = s.getInt (c.idx[Src]);
        const float db = s[c.idx[Vol]];
        c.gain = db <= -59.9f ? 0.0f : dsp::dbToGain (db);
        c.pan = s[c.idx[Pan]];
        c.mute = s.getBool (c.idx[Mute]);
        c.solo = s.getBool (c.idx[Solo]);
        c.sendA = s[c.idx[SendA]];
        c.sendB = s[c.idx[SendB]];
        c.out = s.getInt (c.idx[Out]);
        if (const float a = c.compIdx >= 0 ? dsp::clamp01 (s[c.compIdx]) : 0.0f; a != c.compAmount)
        {
            // compressor-lite: one amount knob -> threshold, ratio and half the makeup
            c.compAmount = a;
            const float thresh = -4.0f - 26.0f * a, ratio = 1.5f + 4.5f * a;
            c.comp.set (thresh, ratio, 8.0f, 160.0f, 0.5f * -thresh * (1.0f - 1.0f / ratio));
        }
        const float lo = s[c.idx[EqLow]], mi = s[c.idx[EqMid]], hi = s[c.idx[EqHigh]];
        if (lo != c.eqLow || mi != c.eqMid || hi != c.eqHigh || c.low[0].isFlat() != (std::abs (lo) < 0.01f))
        {
            c.eqLow = lo; c.eqMid = mi; c.eqHigh = hi;
            for (int k = 0; k < 2; ++k)
            {
                c.low[(size_t) k].setLowShelf (sr, 200.0f, lo);
                c.mid[(size_t) k].setPeak (sr, 1000.0f, mi);
                c.high[(size_t) k].setHighShelf (sr, 5000.0f, hi);
            }
        }
    }
    const float mdb = s[masterIdx[MasterVol]];
    masterGain = mdb <= -59.9f ? 0.0f : dsp::dbToGain (mdb);
    limiterOn = s.getBool (masterIdx[MasterLimit]);
    returnA = s[masterIdx[ReturnA]];
    returnB = s[masterIdx[ReturnB]];
    masterCompOn = dynIdx[MComp] >= 0 && s.getBool (dynIdx[MComp]);
    if (masterCompOn)
        masterComp.set (s[dynIdx[MThresh]], s[dynIdx[MRatio]], s[dynIdx[MAttack]] * 1000.0f, s[dynIdx[MRelease]] * 1000.0f, s[dynIdx[MMakeup]]);
    if (dynIdx[MTapeSat] >= 0 && s[dynIdx[MTapeSat]] != masterSat.getAmount()) masterSat.setAmount (s[dynIdx[MTapeSat]]);
    if (dynIdx[MCeiling] >= 0)
    {
        const float ceilDb = s[dynIdx[MCeiling]];
        limiter.setCeiling (std::abs (ceilDb + 0.3f) < 1.0e-4f ? 0.966f : dsp::dbToGain (ceilDb));
    }
    const double syncBeats = delaySyncIdx >= 0 ? delaySyncBeats (s.getInt (delaySyncIdx)) : 0.0;
    for (size_t i = 0; i < delayIdx.size(); ++i)
    {
        float v = s[delayIdx[i]];
        if (i == (size_t) DelayEffect::Time && syncBeats > 0.0) // 0.5: synced to the tempo
            v = (float) std::clamp (syncBeats * 60.0 / tempo, 0.01, 2.0);
        if (! (v == delayLast[i])) { delayLast[i] = v; delay.setParam ((int) i, v); }
    }
    for (size_t i = 0; i < reverbIdx.size(); ++i)
        if (const float v = s[reverbIdx[i]]; ! (v == reverbLast[i])) { reverbLast[i] = v; reverb.setParam ((int) i, v); }
}

void Mixer::addDirect (const float* l, const float* r, int target) noexcept
{
    if (l == nullptr || r == nullptr || numDirects >= (int) directs.size()) return;
    directs[(size_t) numDirects++] = { l, r, target };
}

void Mixer::process (const SourceBuffers& in, const OutputBuffers& out, int n) noexcept
{
    const size_t bytes = sizeof (float) * (size_t) n;
    for (int o = 0; o < numOutputPairs; ++o)
    {
        if (out.left[(size_t) o] != nullptr) std::memset (out.left[(size_t) o], 0, bytes);
        if (out.right[(size_t) o] != nullptr) std::memset (out.right[(size_t) o], 0, bytes);
    }
    for (auto* v : { &sendAL, &sendAR, &sendBL, &sendBR, &masterL, &masterR }) std::memset (v->data(), 0, bytes);

    bool anySolo = false;
    for (const auto& c : channels) anySolo |= c.solo;

    for (int ci = 0; ci < numChannels; ++ci)
    {
        auto& c = channels[(size_t) ci];
        const bool audible = ! c.mute && (! anySolo || c.solo);
        float pl, pr;
        dsp::panGains (c.pan, pl, pr);
        c.gainL.setTarget (audible ? c.gain * pl * 1.41421356f : 0.0f);
        c.gainR.setTarget (audible ? c.gain * pr * 1.41421356f : 0.0f);

        const bool validSrc = c.src > 0 && c.src < source::maxSources;
        const float* sl = validSrc ? in.left[(size_t) c.src] : nullptr;
        const float* sr2 = validSrc ? in.right[(size_t) c.src] : nullptr;
        if (sl == nullptr || sr2 == nullptr)
        {
            c.gainL.reset (c.gainL.value()); // keep state, nothing to mix
            meters[(size_t) ci].store (0.0f, std::memory_order_relaxed);
            continue;
        }

        float* dl = masterL.data();
        float* dr = masterR.data();
        if (c.out > 0 && c.out <= numAuxOutputs && out.left[(size_t) c.out] != nullptr && out.right[(size_t) c.out] != nullptr)
        {
            dl = out.left[(size_t) c.out];
            dr = out.right[(size_t) c.out];
        }

        const bool eq = ! (c.low[0].isFlat() && c.mid[0].isFlat() && c.high[0].isFlat());
        const bool comp = c.compAmount > 0.0f;
        if (comp)
        {
            // EQ -> compressor-lite into the channel scratch, then the fader below
            for (int i = 0; i < n; ++i)
            {
                float l = sl[i], r = sr2[i];
                if (eq)
                {
                    l = c.high[0].process (c.mid[0].process (c.low[0].process (l)));
                    r = c.high[1].process (c.mid[1].process (c.low[1].process (r)));
                }
                chL[(size_t) i] = l;
                chR[(size_t) i] = r;
            }
            c.comp.process (chL.data(), chR.data(), n);
            sl = chL.data();
            sr2 = chR.data();
        }
        reductions[(size_t) ci].store (comp ? c.comp.gainReductionDb() : 0.0f, std::memory_order_relaxed);
        const bool eqHere = eq && ! comp;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = sl[i], r = sr2[i];
            if (eqHere)
            {
                l = c.high[0].process (c.mid[0].process (c.low[0].process (l)));
                r = c.high[1].process (c.mid[1].process (c.low[1].process (r)));
            }
            l *= c.gainL.next();
            r *= c.gainR.next();
            dl[i] += l;
            dr[i] += r;
            sendAL[(size_t) i] += l * c.sendA;
            sendAR[(size_t) i] += r * c.sendA;
            sendBL[(size_t) i] += l * c.sendB;
            sendBR[(size_t) i] += r * c.sendB;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        }
        meters[(size_t) ci].store (peak, std::memory_order_relaxed);
    }

    // direct feeds (tape direct outs): aux output, or the main mix when that output is disabled
    for (int d = 0; d < numDirects; ++d)
    {
        const auto& f = directs[(size_t) d];
        float* dl = masterL.data();
        float* dr = masterR.data();
        if (f.target > 0 && f.target <= numAuxOutputs && out.left[(size_t) f.target] != nullptr && out.right[(size_t) f.target] != nullptr)
        {
            dl = out.left[(size_t) f.target];
            dr = out.right[(size_t) f.target];
        }
        for (int i = 0; i < n; ++i)
        {
            dl[i] += f.l[i];
            dr[i] += f.r[i];
        }
    }
    numDirects = 0;

    delay.process (sendAL.data(), sendAR.data(), n);
    reverb.process (sendBL.data(), sendBR.data(), n);
    const bool dynamics = masterCompOn || ! masterSat.isBypassed();
    if (! dynamics)
    {
        for (int i = 0; i < n; ++i)
        {
            masterL[(size_t) i] = (masterL[(size_t) i] + sendAL[(size_t) i] * returnA + sendBL[(size_t) i] * returnB) * masterGain;
            masterR[(size_t) i] = (masterR[(size_t) i] + sendAR[(size_t) i] * returnA + sendBR[(size_t) i] * returnB) * masterGain;
        }
        reductions[numChannels].store (0.0f, std::memory_order_relaxed);
    }
    else
    {
        // master chain: sum -> compressor -> tape saturation -> volume -> limiter
        for (int i = 0; i < n; ++i)
        {
            masterL[(size_t) i] = masterL[(size_t) i] + sendAL[(size_t) i] * returnA + sendBL[(size_t) i] * returnB;
            masterR[(size_t) i] = masterR[(size_t) i] + sendAR[(size_t) i] * returnA + sendBR[(size_t) i] * returnB;
        }
        if (masterCompOn) masterComp.process (masterL.data(), masterR.data(), n);
        masterSat.process (masterL.data(), masterR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            masterL[(size_t) i] *= masterGain;
            masterR[(size_t) i] *= masterGain;
        }
        reductions[numChannels].store (masterCompOn ? masterComp.gainReductionDb() : 0.0f, std::memory_order_relaxed);
    }
    if (limiterOn) limiter.process (masterL.data(), masterR.data(), n);

    float peak = 0.0f;
    float* ml = out.left[0];
    float* mr = out.right[0];
    for (int i = 0; i < n; ++i)
    {
        if (ml != nullptr) ml[i] += masterL[(size_t) i];
        if (mr != nullptr) mr[i] += masterR[(size_t) i];
        peak = std::max (peak, std::max (std::abs (masterL[(size_t) i]), std::abs (masterR[(size_t) i])));
    }
    meters[numChannels].store (peak, std::memory_order_relaxed);
}
} // namespace ohpe
