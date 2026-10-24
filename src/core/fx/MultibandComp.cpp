// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MultibandComp.h"

namespace ohpe
{
namespace
{
const char* const bandKeys[MultibandCompEffect::numBands] = { "low", "mid", "high" };
const char* const bandEn[MultibandCompEffect::numBands] = { "Low", "Mid", "High" };
const char* const bandDe[MultibandCompEffect::numBands] = { "Tief", "Mitte", "Hoch" };
constexpr float kneeDb = 4.0f, maxLiftDb = 24.0f;
} // namespace

MultibandCompEffect::MultibandCompEffect()
{
    const auto d = defaults();
    for (int i = 0; i < numParams; ++i) setParam (i, d[(size_t) i]);
    reset();
}

std::array<float, MultibandCompEffect::numParams> MultibandCompEffect::defaults()
{
    std::array<float, numParams> v {};
    v[Depth] = 0.4f; v[Time] = 1.0f; v[Upward] = 1.0f; v[Downward] = 1.0f; v[Input] = 0.0f; v[Output] = 0.0f;
    v[LowSplit] = 120.0f; v[HighSplit] = 2500.0f;
    const float down[3] = { -30.0f, -30.0f, -32.0f }, up[3] = { -42.0f, -42.0f, -44.0f };
    const float atk[3] = { 2.0f, 1.0f, 0.5f }, rel[3] = { 150.0f, 100.0f, 70.0f };
    for (int b = 0; b < numBands; ++b)
    {
        v[(size_t) bandParam (b, DownThresh)] = down[b];
        v[(size_t) bandParam (b, UpThresh)] = up[b];
        v[(size_t) bandParam (b, DownRatio)] = 60.0f;
        v[(size_t) bandParam (b, UpRatio)] = 4.0f;
        v[(size_t) bandParam (b, Attack)] = atk[b];
        v[(size_t) bandParam (b, Release)] = rel[b];
        v[(size_t) bandParam (b, Gain)] = 0.0f;
        v[(size_t) bandParam (b, Solo)] = 0.0f;
    }
    return v;
}

const std::vector<MultibandCompEffect::Style>& MultibandCompEffect::styles()
{
    static const std::vector<Style> list = [] {
        std::vector<Style> s;
        auto add = [&s] (const char* en, const char* de, auto&& edit) {
            Style st { en, de, defaults() };
            edit (st.values);
            s.push_back (st);
        };
        auto band = [] (std::array<float, numParams>& v, int b, int k, float x) { v[(size_t) bandParam (b, k)] = x; };
        // everything at once: hard down, hard up, fast - the dense, loud sound
        add ("Squash", "Squash", [&] (auto& v) { v[Depth] = 0.6f; v[Output] = -2.0f; });
        // a gentle bus glue: low ratios, slower, little lift
        add ("Glue", "Glue", [&] (auto& v) {
            v[Depth] = 0.35f; v[Time] = 2.5f; v[Upward] = 0.3f; v[Downward] = 0.8f;
            for (int b = 0; b < numBands; ++b) { band (v, b, DownThresh, -22.0f); band (v, b, UpThresh, -50.0f); band (v, b, DownRatio, 4.0f); band (v, b, UpRatio, 2.0f); }
        });
        // lifted lows and highs, the mids held back
        add ("Air & Weight", "Luft & Gewicht", [&] (auto& v) {
            v[Depth] = 0.45f; v[Upward] = 0.8f; v[Downward] = 0.6f;
            band (v, 0, Gain, 2.0f); band (v, 1, Gain, -1.0f); band (v, 2, Gain, 2.5f); band (v, 2, UpThresh, -38.0f);
        });
        // slower attacks let the transient through first, the body is held down: punch
        add ("Punch", "Punch", [&] (auto& v) {
            v[Depth] = 0.5f; v[Upward] = 0.3f;
            const float atk[3] = { 8.0f, 5.0f, 2.0f }, rel[3] = { 80.0f, 60.0f, 50.0f };
            for (int b = 0; b < numBands; ++b) { band (v, b, Attack, atk[b]); band (v, b, Release, rel[b]); }
        });
        // mostly upward: quiet details and tails come forward, peaks barely touched
        add ("Density", "Dichte", [&] (auto& v) {
            v[Depth] = 0.5f; v[Downward] = 0.4f;
            for (int b = 0; b < numBands; ++b) { band (v, b, UpThresh, -36.0f); band (v, b, UpRatio, 6.0f); }
        });
        // a light touch for keys and pads
        add ("Gentle", "Sanft", [&] (auto& v) { v[Depth] = 0.2f; v[Time] = 1.5f; v[Upward] = 0.5f; v[Downward] = 0.5f; });
        return s;
    }();
    return list;
}

std::vector<ParamDescriptor> MultibandCompEffect::getParamDescriptors() const
{
    using namespace pd;
    const auto d = defaults();
    const std::string en = "Multiband Comp ", de = "Multiband-Kompressor ";
    std::vector<ParamDescriptor> v {
        continuous ("depth", en + "Depth", de + "Tiefe", 0.0f, 1.0f, d[Depth], ParamUnit::Percent),
        continuous ("time", en + "Time", de + "Zeit", 0.0f, 10.0f, d[Time], ParamUnit::Percent, 0.45f),
        continuous ("upward", en + "Upward", de + "Aufwärts", 0.0f, 1.0f, d[Upward], ParamUnit::Percent),
        continuous ("downward", en + "Downward", de + "Abwärts", 0.0f, 1.0f, d[Downward], ParamUnit::Percent),
        continuous ("input", en + "In", de + "Eingang", -24.0f, 24.0f, d[Input], ParamUnit::Decibel),
        continuous ("output", en + "Out", de + "Ausgang", -24.0f, 24.0f, d[Output], ParamUnit::Decibel),
        continuous ("lowsplit", en + "Low Split", de + "Trennung tief", 40.0f, 1000.0f, d[LowSplit], ParamUnit::Hertz, 0.5f),
        continuous ("highsplit", en + "High Split", de + "Trennung hoch", 1000.0f, 12000.0f, d[HighSplit], ParamUnit::Hertz, 0.5f),
    };
    for (int b = 0; b < numBands; ++b)
    {
        const std::string k = bandKeys[b], e = en + bandEn[b] + " ", g = de + bandDe[b] + " ";
        auto at = [&] (int p) { return d[(size_t) bandParam (b, p)]; };
        v.push_back (continuous (k + "down", e + "Down Threshold", g + "Schwelle abwärts", -60.0f, 0.0f, at (DownThresh), ParamUnit::Decibel));
        v.push_back (continuous (k + "up", e + "Up Threshold", g + "Schwelle aufwärts", -80.0f, 0.0f, at (UpThresh), ParamUnit::Decibel));
        v.push_back (continuous (k + "downratio", e + "Down Ratio", g + "Ratio abwärts", 1.0f, 100.0f, at (DownRatio), ParamUnit::None, 0.3f));
        v.push_back (continuous (k + "upratio", e + "Up Ratio", g + "Ratio aufwärts", 1.0f, 100.0f, at (UpRatio), ParamUnit::None, 0.3f));
        v.push_back (continuous (k + "attack", e + "Attack", g + "Attack", 0.1f, 200.0f, at (Attack), ParamUnit::Milliseconds, 0.35f));
        v.push_back (continuous (k + "release", e + "Release", g + "Release", 5.0f, 2000.0f, at (Release), ParamUnit::Milliseconds, 0.4f));
        v.push_back (continuous (k + "gain", e + "Gain", g + "Pegel", -24.0f, 24.0f, at (Gain), ParamUnit::Decibel));
        v.push_back (toggle (k + "solo", e + "Solo", g + "Solo", false));
    }
    return v;
}

float MultibandCompEffect::bandCurveDb (float lvl, float downTh, float downRatio, float upTh, float upRatio, float downAmt, float upAmt) noexcept
{
    const float down = compStaticGainDb (lvl, downTh, downRatio, kneeDb) * downAmt;
    // the lift: the downward curve mirrored at the up threshold, capped, faded out towards the noise floor
    float lift = std::min (maxLiftDb, -compStaticGainDb (2.0f * upTh - lvl, upTh, upRatio, kneeDb));
    lift *= std::clamp ((lvl + 90.0f) / 12.0f, 0.0f, 1.0f) * upAmt;
    return down + lift;
}

void MultibandCompEffect::setSplits() noexcept
{
    constexpr float q = 0.70710678f;
    for (int c = 0; c < 2; ++c)
    {
        for (auto* z : { &lowA[(size_t) c], &lowB[(size_t) c], &lowHpA[(size_t) c], &lowHpB[(size_t) c] }) z->set (lowSplit, q, sr);
        for (auto* z : { &midA[(size_t) c], &midB[(size_t) c], &highA[(size_t) c], &highB[(size_t) c], &lowAp[(size_t) c] }) z->set (highSplit, q, sr);
    }
}

void MultibandCompEffect::setTimes (int b) noexcept
{
    auto& bd = bands[(size_t) b];
    bd.det.setTimes (std::max (0.01f, bd.attackMs * time), std::max (1.0f, bd.releaseMs * time));
}

void MultibandCompEffect::prepare (double sampleRate, int)
{
    sr = (float) sampleRate;
    depthG.prepare (sr, 0.02f);
    inG.prepare (sr, 0.02f);
    outG.prepare (sr, 0.02f);
    safetyRel = std::exp (-1.0f / (0.05f * sr));
    ceiling = dsp::dbToGain (safetyCeilingDb);
    for (int b = 0; b < numBands; ++b)
    {
        bands[(size_t) b].det.prepare (sr);
        bands[(size_t) b].gain.prepare (sr, 0.02f);
        setTimes (b);
    }
    setSplits();
    reset();
}

void MultibandCompEffect::reset() noexcept
{
    for (auto* arr : { &lowA, &lowB, &lowHpA, &lowHpB, &midA, &midB, &highA, &highB, &lowAp })
        for (auto& z : *arr) z.reset();
    for (auto& b : bands) { b.det.reset(); b.gain.v = b.gain.t; }
    depthG.v = depthG.t;
    inG.v = inG.t;
    outG.v = outG.t;
    safetyEnv = 0.0f;
    for (auto& a : levelUi) a.store (-100.0f);
    for (auto& a : gainUi) a.store (0.0f);
    safetyUi.store (0.0f);
}

void MultibandCompEffect::setParam (int i, float v) noexcept
{
    switch (i)
    {
        case Depth: depthG.t = dsp::clamp01 (v); return;
        case Time: time = std::clamp (v, 0.0f, 10.0f); for (int b = 0; b < numBands; ++b) setTimes (b); return;
        case Upward: upward = dsp::clamp01 (v); return;
        case Downward: downward = dsp::clamp01 (v); return;
        case Input: inG.t = dsp::dbToGain (v); return;
        case Output: outG.t = dsp::dbToGain (v); return;
        case LowSplit: lowSplit = std::clamp (v, 20.0f, 2000.0f); setSplits(); return;
        case HighSplit: highSplit = std::clamp (v, 500.0f, 16000.0f); setSplits(); return;
        default: break;
    }
    if (i < firstBandParam || i >= numParams) return;
    const int b = (i - firstBandParam) / numBandParams, k = (i - firstBandParam) % numBandParams;
    auto& bd = bands[(size_t) b];
    switch (k)
    {
        case DownThresh: bd.downTh = v; break;
        case UpThresh: bd.upTh = v; break;
        case DownRatio: bd.downRatio = std::max (1.0f, v); break;
        case UpRatio: bd.upRatio = std::max (1.0f, v); break;
        case Attack: bd.attackMs = std::max (0.01f, v); setTimes (b); break;
        case Release: bd.releaseMs = std::max (1.0f, v); setTimes (b); break;
        case Gain: bd.gain.t = dsp::dbToGain (v); break;
        case Solo: bd.solo = v >= 0.5f; break;
        default: break;
    }
}

void MultibandCompEffect::process (float* l, float* r, int n) noexcept
{
    float* io[2] = { l, r };
    const bool anySolo = bands[0].solo || bands[1].solo || bands[2].solo;
    std::array<float, numBands> peak {}, lastGain {};
    float worstSafety = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const float in = inG.next();
        std::array<std::array<float, numBands>, 2> band {};
        for (int c = 0; c < 2; ++c)
        {
            const float x = io[c][i] * in;
            auto& la = lowA[(size_t) c]; auto& lb = lowB[(size_t) c];
            la.tick (x); lb.tick (la.lp);
            auto& ha = lowHpA[(size_t) c]; auto& hb = lowHpB[(size_t) c];
            ha.tick (x); hb.tick (ha.hp);
            const float low = lb.lp, rest = hb.hp;
            // the low band through the high split's LR4 all-pass (= a 2nd-order all-pass, Butterworth Q): the three
            // bands sum to all-pass(low split) x all-pass(high split) - flat in level, phase-coherent
            auto& ap = lowAp[(size_t) c];
            ap.tick (low);
            const float lowAligned = low - 2.0f * ap.k * ap.bp;
            auto& ma = midA[(size_t) c]; auto& mb = midB[(size_t) c];
            ma.tick (rest); mb.tick (ma.lp);
            auto& xa = highA[(size_t) c]; auto& xb = highB[(size_t) c];
            xa.tick (rest); xb.tick (xa.hp);
            band[(size_t) c][0] = lowAligned;
            band[(size_t) c][1] = mb.lp;
            band[(size_t) c][2] = xb.hp;
        }
        const float depth = depthG.next();
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < numBands; ++b)
        {
            auto& bd = bands[(size_t) b];
            const float pk = std::max (std::abs (band[0][(size_t) b]), std::abs (band[1][(size_t) b]));
            peak[(size_t) b] = std::max (peak[(size_t) b], pk);
            const float dTh = bd.downTh, dR = bd.downRatio, uTh = bd.upTh, uR = bd.upRatio, dA = downward, uA = upward;
            const float gDb = bd.det.next (pk, [=] (float db) { return bandCurveDb (db, dTh, dR, uTh, uR, dA, uA); });
            lastGain[(size_t) b] = gDb;
            const float wet = std::exp (gDb * 0.11512925f) * bd.gain.next(); // 10^(dB/20)
            const float g = (anySolo && ! bd.solo) ? 0.0f : 1.0f + depth * (wet - 1.0f); // DEPTH = wet / dry per band
            outL += band[0][(size_t) b] * g;
            outR += band[1][(size_t) b] * g;
        }
        const float o = outG.next();
        outL *= o;
        outR *= o;
        // output safety: instant attack, 50 ms release - no sample above the ceiling, nothing happens below it
        const float a = std::max (std::abs (outL), std::abs (outR));
        safetyEnv = a > safetyEnv ? a : dsp::flush (a + (safetyEnv - a) * safetyRel);
        const float sg = safetyEnv > ceiling ? ceiling / safetyEnv : 1.0f;
        worstSafety = std::min (worstSafety, sg);
        l[i] = outL * sg;
        r[i] = outR * sg;
    }
    for (int b = 0; b < numBands; ++b)
    {
        levelUi[(size_t) b].store (dsp::gainToDb (peak[(size_t) b]), std::memory_order_relaxed);
        gainUi[(size_t) b].store (lastGain[(size_t) b], std::memory_order_relaxed);
    }
    safetyUi.store (-dsp::gainToDb (worstSafety), std::memory_order_relaxed);
}
} // namespace ohpe
