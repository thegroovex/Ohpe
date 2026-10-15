// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Shared checks for the phase-2 engine tests (Wavetable, Pluck, Cluster).

#include "OhpeCore.h"
#include "TestHelpers.h"
#include "dsp/DspUtil.h"
#include "engines/EngineFactory.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <complex>
#include <cstring>
#include <functional>

namespace ohpe::test::kit
{
/** Zero-crossing frequency after two passes of a band-pass (Q 6) centred on the expected pitch. */
inline double bandFrequency (const std::vector<float>& v, double sr, size_t from, size_t to, double expected)
{
    const double w = 2.0 * 3.14159265358979323846 * expected / sr, q = 6.0;
    const double alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
    const double b0 = alpha / a0, b2 = -alpha / a0, a1 = -2.0 * std::cos (w) / a0, a2 = (1.0 - alpha) / a0;
    std::vector<float> x (v.begin(), v.begin() + (long) to);
    for (int pass = 0; pass < 2; ++pass)
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& s : x)
        {
            const double y = b0 * s + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = s; y2 = y1; y1 = y;
            s = (float) y;
        }
    }
    return measureFrequency (x, sr, from, to);
}

/** Magnitude spectrum (4-term Blackman-Harris window, sidelobes < -92 dB) of v[from .. from + n), n a power of two. */
inline std::vector<double> spectrum (const std::vector<float>& v, size_t from, size_t n)
{
    std::vector<std::complex<double>> x (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double ph = 2.0 * 3.14159265358979323846 * (double) i / (double) n;
        x[i] = v[from + i] * (0.35875 - 0.48829 * std::cos (ph) + 0.14128 * std::cos (2.0 * ph) - 0.01168 * std::cos (3.0 * ph));
    }
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (x[i], x[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto a = x[i + k], b = x[i + k + len / 2] * w;
                x[i + k] = a + b;
                x[i + k + len / 2] = a - b;
                w *= wl;
            }
        }
    }
    std::vector<double> mag (n / 2);
    for (size_t i = 0; i < n / 2; ++i) mag[i] = std::abs (x[i]);
    return mag;
}

/** Energy ratio (dB) of everything that is not within +-6 bins of a harmonic of f0, versus the harmonics. */
inline double inharmonicDb (const std::vector<float>& v, double sr, double f0, size_t from = 8192, size_t n = 16384)
{
    const auto mag = spectrum (v, from, n);
    const double binHz = sr / (double) n;
    double harm = 1.0e-30, rest = 1.0e-30;
    for (size_t b = 2; b < mag.size(); ++b)
    {
        const double hz = (double) b * binHz;
        if (hz < 0.5 * f0) continue; // DC and sub-audio drift are not aliasing
        const double h = std::round (hz / f0);
        const bool onHarmonic = h >= 1.0 && std::abs (hz - h * f0) <= 6.0 * binHz;
        (onHarmonic ? harm : rest) += mag[b] * mag[b];
    }
    return 10.0 * std::log10 (rest / harm);
}

/** Sum of squared differences between two renders, relative to their energy. */
inline double difference (const Stereo& a, const Stereo& b)
{
    double d = 0.0, e = 1.0e-12;
    for (size_t i = 0; i < a.size(); ++i)
    {
        d += std::pow (a.l[i] - b.l[i], 2.0) + std::pow (a.r[i] - b.r[i], 2.0);
        e += (double) a.l[i] * a.l[i] + (double) b.l[i] * b.l[i];
    }
    return d / e;
}

inline void prepare (ISynthEngine& e, VoiceCommon c = plainCommon())
{
    e.prepare (sampleRate, 512);
    e.setCommon (c);
    e.setPolyphony (16);
}

inline void setAll (ISynthEngine& e, bool toMax)
{
    const auto d = e.getParamDescriptors();
    for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, toMax ? d[i].maxValue : d[i].minValue);
}

inline void setDefaults (ISynthEngine& e)
{
    const auto d = e.getParamDescriptors();
    for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
}

// ------------------------------------------------------------------------------------------ checks

inline void checkSilentWhenIdle (EngineType type)
{
    auto e = createEngine (type);
    REQUIRE (e != nullptr);
    prepare (*e);
    setDefaults (*e);
    auto out = renderEngine (*e, 9600);
    CHECK (peak (out.l) == 0.0f);
    CHECK (peak (out.r) == 0.0f);
    CHECK (e->activeVoiceCount() == 0);
    // also after a note has fully ended
    e->noteOn (60, 1.0f);
    renderEngine (*e, 2400);
    e->noteOff (60);
    renderEngine (*e, 48000 * 2);
    CHECK (e->activeVoiceCount() == 0);
    CHECK (peak (renderEngine (*e, 4800).l) == 0.0f);
}

inline void checkExtremes (EngineType type)
{
    for (int pass = 0; pass < 2; ++pass)
    {
        auto e = createEngine (type);
        prepare (*e);
        setAll (*e, pass == 1);
        for (int n = 12; n <= 120; n += 9) e->noteOn (n, 1.0f);
        auto a = renderEngine (*e, 24000, 64);
        setAll (*e, pass == 0); // flip every parameter while notes sound
        auto b = renderEngine (*e, 24000, 333);
        e->allNotesOff (false);
        auto t = renderEngine (*e, 48000);
        CAPTURE (pass);
        CHECK (allFinite (a.l));
        CHECK (allFinite (a.r));
        CHECK (allFinite (b.l));
        CHECK (allFinite (t.r));
        CHECK (noDenormals (a.l));
        CHECK (noDenormals (b.r));
        CHECK (noDenormals (t.l));
        CHECK (peak (a.l) < 8.0f);
        CHECK (peak (b.r) < 8.0f);
    }
}

inline void checkReleaseToSilence (EngineType type)
{
    auto e = createEngine (type);
    prepare (*e);
    setDefaults (*e);
    e->noteOn (57, 0.8f);
    auto on = renderEngine (*e, 4800);
    CHECK (e->activeVoiceCount() == 1);
    CHECK (rms (on.l, 480) > 0.005f);
    e->noteOff (57);
    auto tail = renderEngine (*e, 48000);
    CHECK (e->activeVoiceCount() == 0);
    CHECK (peak (tail.l, 24000) == 0.0f);
    CHECK (noDenormals (tail.l));
}

inline void checkPolyphony (EngineType type)
{
    auto e = createEngine (type);
    prepare (*e);
    setDefaults (*e);
    for (int n = 0; n < 16; ++n) e->noteOn (40 + n * 2, 0.8f);
    auto out = renderEngine (*e, 4800);
    CHECK (e->activeVoiceCount() == 16);
    CHECK (allFinite (out.l));
    e->allNotesOff (true);
    CHECK (e->activeVoiceCount() == 0);
}

/** The first four descriptors are the macros: valid ranges, and each one audibly changes the sound. */
inline void checkMacros (EngineType type, const std::function<void (ISynthEngine&)>& setup, int note = 48)
{
    auto probe = createEngine (type);
    const auto d = probe->getParamDescriptors();
    REQUIRE (d.size() >= (size_t) ISynthEngine::numMacros);
    for (int m = 0; m < ISynthEngine::numMacros; ++m)
    {
        CAPTURE (d[(size_t) m].id);
        CHECK (d[(size_t) m].minValue < d[(size_t) m].maxValue);
        CHECK (d[(size_t) m].defaultValue >= d[(size_t) m].minValue);
        CHECK (d[(size_t) m].defaultValue <= d[(size_t) m].maxValue);
        CHECK (! d[(size_t) m].nameEn.empty());
        CHECK (! d[(size_t) m].nameDe.empty());
        auto render = [&] (float v) {
            auto e = createEngine (type);
            prepare (*e);
            setDefaults (*e);
            setup (*e);
            e->setParam (m, v);
            e->noteOn (note, 0.9f);
            return renderEngine (*e, 24000);
        };
        const auto lo = render (d[(size_t) m].minValue), hi = render (d[(size_t) m].maxValue);
        CHECK (allFinite (lo.l));
        CHECK (allFinite (hi.l));
        CHECK (difference (lo, hi) > 0.01);
    }
}

/** 16 voices, 2 s of audio: must render well faster than real time. */
inline double checkCpu (EngineType type, const std::function<void (ISynthEngine&)>& setup, double maxRealtimeFraction = 0.5)
{
    auto e = createEngine (type);
    prepare (*e);
    setDefaults (*e);
    setup (*e);
    for (int n = 0; n < 16; ++n) e->noteOn (36 + n * 3, 0.8f);
    std::vector<float> l (256), r (256);
    const int blocks = (int) (2.0 * sampleRate / 256.0);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        std::memset (l.data(), 0, sizeof (float) * 256);
        std::memset (r.data(), 0, sizeof (float) * 256);
        e->render (l.data(), r.data(), 256);
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double fraction = secs / 2.0;
    MESSAGE (std::string (engineTypeInfo (type).id) << ": 16 voices use " << fraction * 100.0 << " % of real time");
    CHECK (e->activeVoiceCount() == 16);
    CHECK (fraction < maxRealtimeFraction);
    return fraction;
}

/** Engine parameters survive a state round trip through the core, and both cores sound the same. */
inline void checkStateRoundTrip (EngineType type)
{
    Core a;
    auto& reg = a.params();
    const std::string prefix = std::string ("p1.") + engineTypeInfo (type).id + ".";
    reg.set ("p1.engine", (float) (int) type);
    int touched = 0;
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        if (d.id.rfind (prefix, 0) != 0) continue;
        reg.setNormalised (i, d.toNormalised (d.defaultValue) > 0.5f ? 0.23f : 0.77f);
        ++touched;
    }
    CHECK (touched >= ISynthEngine::numMacros);
    const auto json = a.saveState ("{}");

    Core b;
    REQUIRE (b.loadState (json, nullptr));
    for (int i = 0; i < reg.size(); ++i)
    {
        CAPTURE (reg.descriptor (i).id);
        CHECK (b.params().get (i) == reg.get (i));
    }

    auto play = [] (Core& core) {
        core.prepare (sampleRate, 480);
        std::array<std::vector<float>, numOutputPairs> l, r;
        OutputBuffers out;
        for (int o = 0; o < numOutputPairs; ++o)
        {
            l[(size_t) o].assign (480, 0.0f);
            r[(size_t) o].assign (480, 0.0f);
            out.left[(size_t) o] = l[(size_t) o].data();
            out.right[(size_t) o] = r[(size_t) o].data();
        }
        std::vector<float> rec;
        const MidiEvent on = MidiEvent::noteOn (1, 60, 100, 0);
        for (int blk = 0; blk < 20; ++blk)
        {
            core.process (out, 480, blk == 0 ? &on : nullptr, blk == 0 ? 1 : 0, Transport {});
            rec.insert (rec.end(), l[0].begin(), l[0].end());
        }
        return rec;
    };
    const auto ra = play (a), rb = play (b);
    CHECK (rms (ra) > 0.001f);
    CHECK (ra == rb);
}
} // namespace ohpe::test::kit
