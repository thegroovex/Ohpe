// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p68: Voice Control - a live input analysed into matrix sources (level, vowel X / Y, brightness, noisiness, pitch, onset).
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "mod/ModMatrix.h"
#include "preset/Preset.h"
#include "voice/VoiceAnalyser.h"
#include "voice/VoiceControl.h"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <vector>

using namespace ohpe;
using namespace ohpe::voice;

namespace
{
constexpr double pi = 3.14159265358979323846;

/** Two-pole resonator (formant). */
struct Res
{
    double a1, a2, g, y1 = 0.0, y2 = 0.0;
    Res (double sr, double f, double bw)
    {
        const double r = std::exp (-pi * bw / sr);
        a1 = -2.0 * r * std::cos (2.0 * pi * f / sr);
        a2 = r * r;
        g = 1.0 - r;
    }
    double run (double x) { const double y = g * x - a1 * y1 - a2 * y2; y2 = y1; y1 = y; return y; }
};

/** A synthetic sung vowel: a glottal pulse train (5 Hz vibrato) through four formants, peak `amp`. */
std::vector<float> vowelSignal (double sr, int vowel, double f0, double secs, double amp, double formantScale = 1.0)
{
    float f1, f2;
    vowelFormants (vowel, f1, f2);
    Res r1 (sr, f1 * formantScale, 80), r2 (sr, f2 * formantScale, 100), r3 (sr, 2600 * formantScale, 140), r4 (sr, 3500, 200);
    std::vector<float> o ((size_t) (sr * secs));
    double ph = 0.0, peak = 1.0e-9, glottal = 0.0;
    for (size_t i = 0; i < o.size(); ++i)
    {
        ph += f0 * (1.0 + 0.01 * std::sin (2.0 * pi * 5.0 * (double) i / sr)) / sr;
        double ex = 0.0;
        if (ph >= 1.0) { ph -= 1.0; ex = 1.0; }
        glottal = 0.97 * glottal + ex;
        const double y = r4.run (r3.run (r2.run (r1.run (glottal))));
        o[i] = (float) y;
        peak = std::max (peak, std::abs (y));
    }
    for (auto& s : o) s = (float) (s / peak * amp);
    return o;
}

void feed (VoiceAnalyser& a, const std::vector<float>& x, int block)
{
    for (size_t i = 0; i < x.size(); i += (size_t) block) a.process (x.data() + i, nullptr, (int) std::min<size_t> ((size_t) block, x.size() - i));
}

int heardVowel (const VoiceAnalyser& a) { return classifyVowel (0.5f * (a.value (VowelX) + 1.0f), 0.5f * (a.value (VowelY) + 1.0f)); }

bool allValuesFinite (const VoiceAnalyser& a)
{
    for (int v = 0; v < numValues; ++v)
        if (! std::isfinite (a.value (v))) return false;
    return true;
}
} // namespace

TEST_CASE ("p68 voice: vowel map and classification of the reference vowels")
{
    for (int v = 0; v < numVowels; ++v)
    {
        float x, y;
        vowelCentre (v, x, y);
        CHECK (classifyVowel (x, y) == v);
    }
    float x, y;
    formantsToMap (730.0f, 1150.0f, x, y); // A: open
    CHECK (y > 0.85f);
    formantsToMap (290.0f, 2250.0f, x, y); // I: closed, front
    CHECK (x > 0.85f);
    CHECK (y < 0.15f);
}

TEST_CASE ("p68 voice: synthetic vowels are classified (sample rates, voices, block sizes)")
{
    int ok = 0, total = 0;
    for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0 })
        for (double f0 : { 100.0, 140.0, 200.0 })
            for (int v = 0; v < numVowels; ++v)
            {
                VoiceAnalyser a;
                a.prepare (sr);
                Settings s;
                s.smoothMs = 10.0f;
                a.setSettings (s);
                feed (a, vowelSignal (sr, v, f0, 0.35, 0.3), 64);
                ok += heardVowel (a) == v ? 1 : 0;
                ++total;
            }
    MESSAGE ("vowel accuracy " << ok << " / " << total);
    CHECK ((double) ok / total >= 0.9);

    // higher formants (a smaller vocal tract, +12 %) still land on the right vowel most of the time
    int okHigh = 0;
    for (int v = 0; v < numVowels; ++v)
    {
        VoiceAnalyser a;
        a.prepare (48000.0);
        a.setSettings ({});
        feed (a, vowelSignal (48000.0, v, 220.0, 0.35, 0.3, 1.12), 128);
        okHigh += heardVowel (a) == v ? 1 : 0;
    }
    CHECK (okHigh >= 4);
}

TEST_CASE ("p68 voice: buffer size 1 gives the same result")
{
    for (int v : { (int) VowelA, (int) VowelI, (int) VowelU })
    {
        VoiceAnalyser a, b;
        a.prepare (44100.0);
        b.prepare (44100.0);
        const auto x = vowelSignal (44100.0, v, 130.0, 0.3, 0.3);
        feed (a, x, 1);
        feed (b, x, 512);
        CHECK (heardVowel (a) == v);
        CHECK (heardVowel (b) == v);
        CHECK (a.value (Level) == doctest::Approx (b.value (Level)).epsilon (0.1));
        CHECK (allValuesFinite (a));
    }
}

TEST_CASE ("p68 voice: level, brightness, noisiness, onset, pitch")
{
    const double sr = 48000.0;
    VoiceAnalyser a;
    a.prepare (sr);
    Settings s;
    s.pitch = true;
    a.setSettings (s);
    CHECK (a.latencyMs() <= 10.0f);

    // a sung A at 130.8 Hz (C3)
    const auto x = vowelSignal (sr, VowelA, 130.81, 0.4, 0.3);
    feed (a, x, 128);
    CHECK (a.gateOpen());
    CHECK (a.value (Level) > 0.5f);
    CHECK (a.value (Noisiness) < 0.3f);
    CHECK (a.value (Pitch) * 48.0f + 36.0f == doctest::Approx (48.0f).epsilon (0.012)); // within ~0.6 semitones

    // white noise (a long "sss"): noisy and bright
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> d (-0.2f, 0.2f);
    std::vector<float> n ((size_t) (sr * 0.3));
    for (auto& v : n) v = d (rng);
    VoiceAnalyser b;
    b.prepare (sr);
    feed (b, n, 256);
    CHECK (b.value (Noisiness) > 0.7f);
    CHECK (b.value (Brightness) > a.value (Brightness));

    // an onset: silence, then a syllable
    VoiceAnalyser c;
    c.prepare (sr);
    std::vector<float> silence ((size_t) (sr * 0.2), 0.0f);
    feed (c, silence, 64);
    CHECK (c.value (Onset) == 0.0f);
    float maxOnset = 0.0f;
    const auto syl = vowelSignal (sr, VowelO, 150.0, 0.05, 0.3);
    for (size_t i = 0; i < syl.size(); i += 64)
    {
        c.process (syl.data() + i, nullptr, (int) std::min<size_t> (64, syl.size() - i));
        maxOnset = std::max (maxOnset, c.value (Onset));
    }
    CHECK (maxOnset > 0.8f);
}

TEST_CASE ("p68 voice: silence, no input, NaN - everything rests at 0 and stays finite")
{
    for (double sr : { 8000.0, 44100.0, 192000.0 })
    {
        VoiceAnalyser a;
        a.prepare (sr);
        std::vector<float> zero (4096, 0.0f);
        feed (a, zero, 1);
        for (int v = 0; v < numValues; ++v) CHECK (a.value (v) == 0.0f);
        a.process (nullptr, nullptr, 4096);
        for (int v = 0; v < numValues; ++v) CHECK (a.value (v) == 0.0f);

        std::vector<float> bad (2048);
        for (size_t i = 0; i < bad.size(); ++i)
            bad[i] = i % 3 == 0 ? std::numeric_limits<float>::quiet_NaN() : (i % 3 == 1 ? std::numeric_limits<float>::infinity() : 1.0e30f);
        feed (a, bad, 100);
        CHECK (allValuesFinite (a));
        for (int v = 0; v < numValues; ++v) CHECK (std::abs (a.value (v)) <= 1.0f);
    }

    // after a pause the values go home (no input = the sound as programmed)
    VoiceAnalyser a;
    a.prepare (48000.0);
    feed (a, vowelSignal (48000.0, VowelI, 180.0, 0.3, 0.3), 256);
    CHECK (a.value (VowelX) > 0.5f);
    std::vector<float> silence (48000 * 2, 0.0f);
    feed (a, silence, 256);
    for (int v = 0; v < numValues; ++v) CHECK (std::abs (a.value (v)) < 0.02f);
}

TEST_CASE ("p68 voice: parameters appended after the p59 grid block; matrix sources 34 .. 40")
{
    auto owner = std::make_unique<Core>();
    const auto& reg = owner->params();
    const int base = reg.indexOf ("harmony.kb.fixed");
    REQUIRE (base >= 0);
    const char* ids[] = { "voice.on", "voice.gain", "voice.gate", "voice.smooth", "voice.pitch", "voice.monitor",
                          "p1.voice.map", "p2.voice.map", "p3.voice.map", "p4.voice.map" };
    for (int i = 0; i < 10; ++i) CHECK (reg.indexOf (ids[i]) == base + 1 + i);
    CHECK (reg.get ("voice.on") == 1.0f);
    CHECK (reg.get ("voice.monitor") == 0.0f); // no feedback by default
    CHECK (reg.get ("voice.pitch") == 0.0f);

    CHECK (mod::SrcVoiceLevel == 34);
    CHECK (mod::SrcVoiceOnset == 40);
    CHECK (mod::sourceTable().size() == (size_t) mod::numSources);
    const char* keys[] = { "voicelevel", "vowelx", "vowely", "voicebright", "voicenoise", "voicepitch", "voiceonset" };
    for (int k = 0; k < numValues; ++k)
    {
        CHECK (mod::sourceIndexOfKey (keys[k]) == mod::SrcVoiceLevel + k);
        CHECK (mod::isBipolar (mod::SrcVoiceLevel + k) == (k == VowelX || k == VowelY));
    }

    // the player's setup is not part of a project; the trigger not part of a part preset
    CHECK (preset::isExcludedFromProject ("voice.on"));
    CHECK (preset::isExcludedFromProject ("p2.voice.map"));
    CHECK (! preset::isExcludedFromProject ("p2.mod.s1.src"));
    owner->params().set ("p1.voice.map", 1.0f);
    const auto part = preset::capturePart (owner->params(), 0, {});
    CHECK (part.params.count ("voice.map") == 0);
}

TEST_CASE ("p68 voice: quick map places three routings, again only refreshes them, a full matrix is left alone")
{
    auto owner = std::make_unique<Core>();
    auto& reg = owner->params();
    reg.set ("p2.mod.s1.src", (float) mod::SrcLfo2); // a routing the user already has
    reg.set ("p2.mod.s1.dst", (float) mod::DstPitch);
    int placed = 0;
    auto apply = [&] { for (const auto& [id, v] : VoiceControl::quickMap (reg, 1, &placed)) REQUIRE (reg.set (id, v)); };
    apply();
    CHECK (placed == 3);
    CHECK ((int) reg.get ("p2.mod.s1.src") == mod::SrcLfo2);
    CHECK ((int) reg.get ("p2.mod.s2.src") == mod::SrcVoiceVowelY);
    CHECK ((int) reg.get ("p2.mod.s2.dst") == mod::destIndexOfKey ("timbre"));
    CHECK ((int) reg.get ("p2.mod.s3.src") == mod::SrcVoiceVowelX);
    CHECK ((int) reg.get ("p2.mod.s3.dst") == mod::destIndexOfKey ("morphx"));
    CHECK ((int) reg.get ("p2.mod.s4.dst") == mod::destIndexOfKey ("morphy"));
    reg.set ("p2.mod.s3.amt", 0.1f);
    apply();
    CHECK (placed == 3);
    CHECK ((int) reg.get ("p2.mod.s5.src") == mod::SrcOff); // no duplicates
    CHECK (reg.get ("p2.mod.s3.amt") == doctest::Approx (0.5f));

    for (int k = 1; k <= mod::numSlots; ++k)
    {
        reg.set ("p3.mod.s" + std::to_string (k) + ".src", (float) mod::SrcLfo3);
        reg.set ("p3.mod.s" + std::to_string (k) + ".dst", (float) mod::DstLevel);
    }
    const auto changes = VoiceControl::quickMap (reg, 2, &placed);
    CHECK (placed == 0);
    CHECK (changes.size() == 1); // only voice.on
}

TEST_CASE ("p68 voice: the core feeds the sources; off / no input = neutral; monitor off = the input is never heard")
{
    auto owner = std::make_unique<Core>();
    Core& core = *owner;
    constexpr int block = 256;
    core.prepare (48000.0, block);
    std::array<std::vector<float>, numOutputPairs> l, r;
    for (auto& v : l) v.assign (block, 0.0f);
    for (auto& v : r) v.assign (block, 0.0f);
    OutputBuffers out;
    for (int o = 0; o < numOutputPairs; ++o) { out.left[(size_t) o] = l[(size_t) o].data(); out.right[(size_t) o] = r[(size_t) o].data(); }
    const auto sig = vowelSignal (48000.0, VowelA, 120.0, 0.5, 0.3);
    double inputEnergyOut = 0.0;
    auto run = [&] (bool withInput, int blocks) {
        for (int b = 0; b < blocks; ++b)
        {
            const float* in = withInput ? sig.data() + (size_t) ((b * block) % ((int) sig.size() - block)) : nullptr;
            core.setAudioInput (in, in);
            core.process (out, block, nullptr, 0, Transport {});
            for (int i = 0; i < block; ++i) inputEnergyOut += (double) l[0][(size_t) i] * l[0][(size_t) i];
            CHECK (test::allFinite (l[0]));
        }
    };
    run (false, 10);
    for (int k = 0; k < numValues; ++k) CHECK (core.part (0).modMatrix().sourceValue (mod::SrcVoiceLevel + k) == 0.0f);
    inputEnergyOut = 0.0;
    run (true, 40); // nothing plays: with the monitor off the output stays silent although the input sings
    CHECK (inputEnergyOut < 1.0e-6);
    CHECK (core.part (0).modMatrix().sourceValue (mod::SrcVoiceLevel) > 0.3f);
    CHECK (core.part (3).modMatrix().sourceValue (mod::SrcVoiceVowelY) > 0.5f);
    CHECK (core.voiceControl().isOn());

    core.params().set ("voice.monitor", 1.0f);
    inputEnergyOut = 0.0;
    run (true, 4);
    CHECK (inputEnergyOut > 1.0e-3);
    core.params().set ("voice.monitor", 0.0f);

    core.params().set ("voice.on", 0.0f);
    run (true, 2);
    for (int k = 0; k < numValues; ++k) CHECK (core.part (1).modMatrix().sourceValue (mod::SrcVoiceLevel + k) == 0.0f);
}
