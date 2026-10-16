// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p20: audio to wavetable - analysis, user tables, table files, state and presets
#include "EngineTestKit.h"
#include "OhpeCore.h"
#include "dsp/Fft.h"
#include "engines/VocalEngine.h"
#include "engines/WavetableEngine.h"
#include "preset/Preset.h"
#include "wavetable/AudioAnalysis.h"
#include "wavetable/InputCapture.h"
#include "wavetable/TableFile.h"
#include "wavetable/UserTableState.h"

#include <doctest/doctest.h>

#include <cstring>

using namespace ohpe;
using namespace ohpe::test;
using namespace ohpe::usertable;

namespace
{
constexpr double pi = 3.14159265358979323846;

std::vector<float> sine (double hz, double seconds, double sr = sampleRate, float amp = 0.5f)
{
    std::vector<float> v ((size_t) (seconds * sr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2.0 * pi * hz * (double) i / sr + 0.7);
    return v;
}

/** Band-limited saw (additive, every harmonic below Nyquist). */
std::vector<float> saw (double hz, double seconds, double sr = sampleRate)
{
    std::vector<float> v ((size_t) (seconds * sr), 0.0f);
    const int harmonics = (int) (0.5 * sr / hz) - 1;
    for (int h = 1; h <= harmonics; ++h)
        for (size_t i = 0; i < v.size(); ++i) v[i] += (float) (0.3 / h * std::sin (2.0 * pi * hz * h * (double) i / sr + 0.3 * h));
    return v;
}

/** Harmonic magnitudes 0 .. 1023 of one 2048-sample frame. */
std::vector<double> harmonics (const float* frame)
{
    std::vector<std::complex<double>> x ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i) x[(size_t) i] = frame[i];
    dsp::fft (x, -1);
    std::vector<double> m ((size_t) frameSize / 2);
    for (int h = 0; h < frameSize / 2; ++h) m[(size_t) h] = std::abs (x[(size_t) h]) * 2.0 / frameSize;
    return m;
}

double db (double r) { return 20.0 * std::log10 (std::max (1.0e-12, r)); }

UserTablePtr sawTable (int frames = 8)
{
    const auto s = saw (110.0, 1.0);
    AnalysisOptions o;
    o.frames = frames;
    const auto r = analyse (s.data(), (int64_t) s.size(), sampleRate, o);
    REQUIRE (r.ok);
    return UserTable::create ("Saw", r.frames, r.formants);
}
} // namespace

TEST_CASE ("user wavetable: pitch detection")
{
    for (double hz : { 55.0, 110.0, 220.0, 440.0, 1000.0 })
    {
        const auto s = sine (hz, 0.5);
        float clarity = 0.0f;
        const float f = detectPitch (s.data(), (int64_t) s.size(), sampleRate, 30.0f, 2000.0f, &clarity);
        CAPTURE (hz);
        CAPTURE (f);
        CHECK (std::abs (f - hz) / hz < 0.005);
        CHECK (clarity > 0.8f);
    }
    std::vector<float> noise (24000);
    uint32_t seed = 1;
    for (auto& v : noise) { seed = seed * 1664525u + 1013904223u; v = (float) ((seed >> 8) & 0xffff) / 32768.0f - 1.0f; }
    float clarity = 1.0f;
    detectPitch (noise.data(), (int64_t) noise.size(), sampleRate, 30.0f, 2000.0f, &clarity);
    CHECK (clarity < 0.8f);
}

TEST_CASE ("user wavetable: a sine gives pure sine frames")
{
    const auto s = sine (220.0, 1.0);
    AnalysisOptions o;
    o.frames = 16;
    const auto r = analyse (s.data(), (int64_t) s.size(), sampleRate, o);
    REQUIRE (r.ok);
    CHECK (r.pitched);
    CHECK (std::abs (r.pitchHz - 220.0f) < 1.0f);
    CHECK (r.numFrames == 16);
    const auto t = UserTable::create ("Sine", r.frames);
    REQUIRE (t != nullptr);
    REQUIRE (t->numFrames() == 16);
    for (int f = 0; f < t->numFrames(); ++f)
    {
        const auto m = harmonics (t->frame (f, 0));
        CAPTURE (f);
        CHECK (m[0] < 1.0e-4);                    // zero DC
        CHECK (std::abs (m[1] - 1.0) < 0.02);      // normalised fundamental
        double rest = 0.0;
        for (int h = 2; h < frameSize / 2; ++h) rest = std::max (rest, m[(size_t) h]);
        CHECK (db (rest) < -50.0);
        // phase alignment: the frame starts as a rising sine
        CHECK (std::abs (t->frame (f, 0)[0]) < 0.02f);
        CHECK (t->frame (f, 0)[frameSize / 4] > 0.95f);
    }
}

TEST_CASE ("user wavetable: a saw gives saw frames (1/h harmonics), aligned and alike")
{
    const auto t = sawTable (8);
    for (int f = 0; f < t->numFrames(); ++f)
    {
        const auto m = harmonics (t->frame (f, 0));
        CAPTURE (f);
        for (int h = 2; h <= 24; ++h)
        {
            CAPTURE (h);
            CHECK (std::abs (db (m[(size_t) h] / m[1]) - db (1.0 / h)) < 1.0);
        }
    }
    // phase-aligned frames of a steady sound are (nearly) the same wave
    double diff = 0.0, energy = 0.0;
    for (int i = 0; i < frameSize; ++i)
    {
        diff += std::pow (t->frame (0, 0)[i] - t->frame (7, 0)[i], 2.0);
        energy += std::pow (t->frame (0, 0)[i], 2.0);
    }
    CHECK (diff / energy < 0.01);
}

TEST_CASE ("user wavetable: a changing sound gives changing frames, smoothing evens them out")
{
    // sine for the first half, saw for the second
    auto s = sine (110.0, 1.0);
    const auto w = saw (110.0, 1.0);
    for (size_t i = s.size() / 2; i < s.size(); ++i) s[i] = w[i];
    AnalysisOptions o;
    o.frames = 32;
    const auto plain = analyse (s.data(), (int64_t) s.size(), sampleRate, o);
    o.smoothing = 1.0f;
    const auto smooth = analyse (s.data(), (int64_t) s.size(), sampleRate, o);
    REQUIRE (plain.ok);
    REQUIRE (smooth.ok);
    const auto a = UserTable::create ("a", plain.frames), b = UserTable::create ("b", smooth.frames);
    const auto first = harmonics (a->frame (0, 0)), last = harmonics (a->frame (31, 0));
    CHECK (db (first[3] / first[1]) < -40.0); // sine
    CHECK (db (last[3] / last[1]) > -15.0);   // saw
    // smoothing: the step between neighbouring frames in the middle gets smaller
    auto step = [] (const UserTable& t) {
        double mx = 0.0;
        for (int f = 1; f < t.numFrames(); ++f)
        {
            double d = 0.0;
            for (int i = 0; i < frameSize; ++i) d += std::pow (t.frame (f, 0)[i] - t.frame (f - 1, 0)[i], 2.0);
            mx = std::max (mx, d);
        }
        return mx;
    };
    CHECK (step (*b) < 0.5 * step (*a));
}

TEST_CASE ("user wavetable: mip levels are band-limited (no harmonics above their limit)")
{
    const auto t = sawTable (8);
    for (int lv = 0; lv < numLevels; ++lv)
    {
        const int limit = maxHarmonics >> lv;
        const auto m = harmonics (t->frame (3, lv));
        double above = 0.0;
        for (int h = limit + 1; h < frameSize / 2; ++h) above = std::max (above, m[(size_t) h]);
        CAPTURE (lv);
        CHECK (db (above) < -100.0);
        CHECK (t->frame (3, lv)[frameSize] == t->frame (3, lv)[0]); // guard sample
    }
}

TEST_CASE ("user wavetable: the engine plays a user table without aliasing, bounded, no NaN")
{
    const auto t = sawTable (16);
    for (int note : { 33, 57, 81, 96, 108 })
        for (float pos : { 0.0f, 0.6f })
        {
            WavetableEngine e;
            kit::prepare (e);
            e.setParam (wavetable::Position, pos);
            e.setParam (wavetable::Unison, 0.0f);
            e.setParam (wavetable::Tone, 1.0f);
            e.setParam (wavetable::Resonance, 0.0f);
            e.setParam (wavetable::KeyTrack, 0.0f);
            e.setUserTable (t);
            e.setUserSource (true);
            e.noteOn (note, 1.0f);
            auto out = renderEngine (e, 8192 + 16384);
            CHECK (e.playingUserTable() == t.get());
            const double d = kit::inharmonicDb (out.l, sampleRate, dsp::midiToHz ((float) note));
            CAPTURE (note);
            CAPTURE (d);
            CHECK (d < (note < 40 ? -65.0 : -80.0)); // the lowest note: linear read-out images of the dense spectrum
            CHECK (allFinite (out.l));
            CHECK (allFinite (out.r));
            CHECK (noDenormals (out.l));
            CHECK (peak (out.l) > 0.01f);
            CHECK (peak (out.l) < 2.0f);
        }
}

TEST_CASE ("user wavetable: user source off, or no table = the built-in table (neutral)")
{
    auto render = [] (bool userOn, bool withTable) {
        WavetableEngine e;
        kit::prepare (e);
        e.setParam (wavetable::Position, 0.4f);
        if (withTable) e.setUserTable (sawTable (8));
        e.setUserSource (userOn);
        e.noteOn (57, 1.0f);
        return renderEngine (e, 9600);
    };
    const auto builtIn = render (false, false);
    CHECK (kit::difference (builtIn, render (false, true)) < 1.0e-12);
    CHECK (kit::difference (builtIn, render (true, false)) < 1.0e-12);
    CHECK (kit::difference (builtIn, render (true, true)) > 0.01);
}

TEST_CASE ("user wavetable: one-frame tables and extreme settings stay finite")
{
    std::vector<float> one ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i) one[(size_t) i] = i < frameSize / 2 ? 1.0f : -1.0f; // square, not band-limited
    const auto t = UserTable::create ("One", one);
    REQUIRE (t != nullptr);
    CHECK (t->numFrames() == 1);
    WavetableEngine e;
    kit::prepare (e);
    kit::setAll (e, true);
    e.setUserTable (t);
    e.setUserSource (true);
    for (int n : { 0, 60, 127 }) e.noteOn (n, 1.0f);
    auto out = renderEngine (e, 24000);
    CHECK (allFinite (out.l));
    CHECK (allFinite (out.r));
    CHECK (peak (out.l) < 4.0f);
}

TEST_CASE ("user wavetable: silent, short, noisy and broken input")
{
    std::vector<float> silence (48000, 0.0f);
    auto r = analyse (silence.data(), (int64_t) silence.size(), sampleRate, {});
    CHECK_FALSE (r.ok);
    CHECK (r.error == "The sound is silent.");
    const auto shortSine = sine (50.0, 0.01);
    r = analyse (shortSine.data(), (int64_t) shortSine.size(), sampleRate, {});
    CHECK_FALSE (r.ok);
    CHECK (r.error == "The sound is too short.");

    std::vector<float> noise (48000);
    uint32_t seed = 7;
    for (auto& v : noise) { seed = seed * 1664525u + 1013904223u; v = (float) ((seed >> 8) & 0xffff) / 65536.0f - 0.5f; }
    noise[100] = NAN;
    noise[200] = INFINITY;
    AnalysisOptions o;
    o.frames = 256;
    r = analyse (noise.data(), (int64_t) noise.size(), sampleRate, o);
    REQUIRE (r.ok);
    CHECK (r.numFrames == 256);
    CHECK (allFinite (r.frames));
    const auto t = UserTable::create ("Noise", r.frames, r.formants);
    REQUIRE (t != nullptr);
    CHECK (allFinite (t->levelZero()));
    for (const auto& f : t->formants())
        for (int k = 0; k < numFormants; ++k)
        {
            CHECK (std::isfinite (f.freq[(size_t) k]));
            CHECK (std::isfinite (f.gainDb[(size_t) k]));
        }
    // cancel
    int calls = 0;
    r = analyse (noise.data(), (int64_t) noise.size(), sampleRate, o, [&] (float) { return ++calls < 3; });
    CHECK_FALSE (r.ok);
    CHECK (r.error == "Cancelled.");
}

TEST_CASE ("user wavetable: formant extraction finds the resonances of a synthetic vowel")
{
    // 110 Hz pulse train through three resonators (700, 1220, 2600 Hz)
    const double sr = sampleRate;
    std::vector<float> x (48000, 0.0f);
    const double F[3] = { 700.0, 1220.0, 2600.0 }, B[3] = { 80.0, 90.0, 120.0 }, G[3] = { 1.0, 0.5, 0.25 };
    for (int k = 0; k < 3; ++k)
    {
        const double r = std::exp (-pi * B[k] / sr), th = 2.0 * pi * F[k] / sr;
        const double a1 = -2.0 * r * std::cos (th), a2 = r * r;
        double y1 = 0.0, y2 = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double in = (i % 436) == 0 ? 1.0 : 0.0;
            const double y = G[k] * in - a1 * y1 - a2 * y2;
            y2 = y1;
            y1 = y;
            x[i] += (float) (y * 0.05);
        }
    }
    const auto fm = extractFormants (x.data(), (int64_t) x.size(), sr, 4);
    REQUIRE (fm.size() == 4);
    for (const auto& f : fm)
        for (double target : F)
        {
            double best = 1.0e9;
            for (int k = 0; k < numFormants; ++k) best = std::min (best, std::abs (f.freq[(size_t) k] - target) / target);
            CAPTURE (target);
            CHECK (best < 0.08);
        }
}

TEST_CASE ("user formants: the Vocal engine follows them, off = built-in vowels")
{
    const auto t = sawTable (8);
    REQUIRE (t->hasFormants());
    auto render = [&] (bool on) {
        VocalEngine e;
        kit::prepare (e);
        e.setParam (vocal::Vowel, 2.0f);
        e.setUserTable (t);
        e.setUserSource (on);
        e.noteOn (57, 1.0f);
        return renderEngine (e, 24000);
    };
    VocalEngine plain;
    kit::prepare (plain);
    plain.setParam (vocal::Vowel, 2.0f);
    plain.noteOn (57, 1.0f);
    const auto builtIn = renderEngine (plain, 24000);
    CHECK (kit::difference (builtIn, render (false)) < 1.0e-12);
    const auto user = render (true);
    CHECK (kit::difference (builtIn, user) > 0.01);
    CHECK (allFinite (user.l));
    CHECK (peak (user.l) < 2.0f);
}

TEST_CASE ("user wavetable: .ohpewt round trip, base64, damaged files")
{
    const auto t = sawTable (12);
    const auto bytes = encode (*t);
    CHECK (isTableFile (bytes.data(), bytes.size()));
    const auto back = decode (bytes);
    REQUIRE (back != nullptr);
    CHECK (back->name() == "Saw");
    REQUIRE (back->numFrames() == 12);
    float maxDiff = 0.0f;
    for (int f = 0; f < 12; ++f)
        for (int lv = 0; lv < numLevels; ++lv)
            for (int i = 0; i <= frameSize; ++i) maxDiff = std::max (maxDiff, std::abs (back->frame (f, lv)[i] - t->frame (f, lv)[i]));
    CHECK (maxDiff < 1.0e-5f);
    REQUIRE (back->formants().size() == t->formants().size());
    for (size_t f = 0; f < t->formants().size(); ++f)
        for (int k = 0; k < numFormants; ++k) CHECK (back->formants()[f].freq[(size_t) k] == doctest::Approx (t->formants()[f].freq[(size_t) k]));
    // the saved table saved again gives the same table (idempotent up to float rounding)
    const auto again = decode (encode (*back));
    REQUIRE (again != nullptr);
    CHECK (encode (*back).size() == bytes.size());
    float d2 = 0.0f;
    for (int i = 0; i < frameSize; ++i) d2 = std::max (d2, std::abs (again->frame (5, 0)[i] - t->frame (5, 0)[i]));
    CHECK (d2 < 1.0e-4f);

    CHECK (base64Decode (base64Encode (bytes)) == bytes);
    for (size_t n : { 0, 1, 2, 3, 4, 5 })
    {
        std::vector<uint8_t> v (n);
        for (size_t i = 0; i < n; ++i) v[i] = (uint8_t) (i * 77 + 3);
        CHECK (base64Decode (base64Encode (v)) == v);
    }
    std::string err;
    auto cut = bytes;
    cut.resize (cut.size() / 2);
    CHECK (decode (cut, &err) == nullptr);
    CHECK (err == "The wavetable file is damaged.");
    std::vector<uint8_t> junk (100, 0x41);
    CHECK (decode (junk, &err) == nullptr);
    CHECK (err == "The file is not an Ohpe wavetable.");
}

TEST_CASE ("user wavetable: single-cycle sets (2048 / other frame sizes, clm and cue hints)")
{
    // 4 frames of 2048: sine with rising harmonic 2
    std::vector<float> set (4 * 2048);
    for (int f = 0; f < 4; ++f)
        for (int i = 0; i < 2048; ++i)
            set[(size_t) (f * 2048 + i)] = (float) (std::sin (2.0 * pi * i / 2048.0) + 0.25 * f * std::sin (4.0 * pi * i / 2048.0));
    CHECK (looksLikeWavetableSet ((int64_t) set.size(), 0));
    CHECK_FALSE (looksLikeWavetableSet (48000, 0));
    CHECK (looksLikeWavetableSet (4096, 1024));
    auto frames = framesFromSet (set.data(), (int64_t) set.size(), 0);
    REQUIRE (frames.size() == 4 * 2048);
    auto t = UserTable::create ("Set", frames);
    const auto m0 = harmonics (t->frame (0, 0)), m3 = harmonics (t->frame (3, 0));
    CHECK (db (m0[2]) < -60.0);
    CHECK (m3[2] / m3[1] == doctest::Approx (0.75).epsilon (0.01));

    // frame size 1024 resampled to 2048
    std::vector<float> small (3 * 1024);
    for (int i = 0; i < 3 * 1024; ++i) small[(size_t) i] = (float) std::sin (2.0 * pi * (i % 1024) / 1024.0);
    frames = framesFromSet (small.data(), (int64_t) small.size(), 1024);
    REQUIRE (frames.size() == 3 * 2048);
    t = UserTable::create ("Small", frames);
    const auto ms = harmonics (t->frame (2, 0));
    CHECK (ms[1] == doctest::Approx (1.0).epsilon (0.01));
    CHECK (db (ms[2]) < -50.0);

    // more than 256 frames: 256 evenly picked
    std::vector<float> many (300 * 64, 0.5f);
    CHECK (framesFromSet (many.data(), (int64_t) many.size(), 64).size() == (size_t) maxFrames * frameSize);

    // RIFF with a "clm " chunk and with evenly spaced cue points
    auto riff = [] (const std::vector<std::pair<std::string, std::vector<uint8_t>>>& chunks) {
        std::vector<uint8_t> b = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E' };
        for (const auto& [id, body] : chunks)
        {
            b.insert (b.end(), id.begin(), id.end());
            const auto n = (uint32_t) body.size();
            for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) (n >> (8 * i)));
            b.insert (b.end(), body.begin(), body.end());
            if (n & 1) b.push_back (0);
        }
        return b;
    };
    const std::string clmText = "<!>1024 10000000 wavetable (made by hand)";
    auto clm = riff ({ { "fmt ", std::vector<uint8_t> (16, 0) }, { "clm ", std::vector<uint8_t> (clmText.begin(), clmText.end()) } });
    CHECK (wavFrameSizeHint (clm.data(), clm.size()) == 1024);
    std::vector<uint8_t> cue = { 3, 0, 0, 0 };
    for (int k = 0; k < 3; ++k)
    {
        std::vector<uint8_t> pt (24, 0);
        const uint32_t off = (uint32_t) (k * 512);
        for (int i = 0; i < 4; ++i) pt[(size_t) (20 + i)] = (uint8_t) (off >> (8 * i));
        cue.insert (cue.end(), pt.begin(), pt.end());
    }
    auto withCue = riff ({ { "cue ", cue } });
    CHECK (wavFrameSizeHint (withCue.data(), withCue.size()) == 512);
    auto none = riff ({ { "fmt ", std::vector<uint8_t> (16, 0) } });
    CHECK (wavFrameSizeHint (none.data(), none.size()) == 0);
}

TEST_CASE ("user wavetable: state keeps references; a missing file falls back to the built-in table")
{
    auto big = std::const_pointer_cast<UserTable> (sawTable (128)); // too big to embed
    big->path = "/nowhere/Big.ohpewt";
    auto small = std::const_pointer_cast<UserTable> (sawTable (16)); // embedded
    small->path = "/nowhere/Small.ohpewt";

    Core a;
    a.prepare (sampleRate, 256);
    setTable (a, 0, Target::Wavetable, big);
    setTable (a, 1, Target::Wavetable, small);
    setTable (a, 1, Target::Vocal, small);
    const auto saved = saveAssignments (a);
    CHECK (saved.find ("sameAs") != std::string::npos);

    Core b;
    b.prepare (sampleRate, 256);
    std::vector<std::string> asked;
    const auto r = restoreAssignments (b, saved, [&] (const std::string& path, const std::string& file) -> UserTablePtr {
        asked.push_back (file);
        CHECK (! path.empty());
        return nullptr; // nothing on this computer
    });
    CHECK (r.missing == 1);
    CHECK (r.embedded == 1);
    CHECK (r.loaded == 0);
    CHECK (asked.size() == 2);
    CHECK (tableOf (b, 0, Target::Wavetable) == nullptr);
    REQUIRE (tableOf (b, 1, Target::Wavetable) != nullptr);
    CHECK (tableOf (b, 1, Target::Wavetable)->numFrames() == 16);
    CHECK (tableOf (b, 1, Target::Wavetable)->path == "/nowhere/Small.ohpewt");
    CHECK (tableOf (b, 1, Target::Vocal) == tableOf (b, 1, Target::Wavetable));

    // part 1 still asks for its user table: plays the built-in one, finite and audible
    auto& reg = b.params();
    reg.set (reg.indexOf ("p1.engine"), (float) EngineType::Wavetable);
    reg.set (reg.indexOf ("p1.usertable.wt"), 1.0f);
    b.queueNote (60, 0.9f, true);
    std::vector<float> l (256), rr (256);
    float pk = 0.0f;
    bool finite = true;
    for (int blk = 0; blk < 100; ++blk)
    {
        OutputBuffers out;
        out.left[0] = l.data();
        out.right[0] = rr.data();
        b.process (out, 256, nullptr, 0, {});
        for (float v : l) { finite &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
    }
    CHECK (finite);
    CHECK (pk > 0.001f);

    // a resolver that finds the file by name wins over the embedded copy
    const auto r2 = restoreAssignments (b, saved, [&] (const std::string&, const std::string& file) -> UserTablePtr {
        return file == "Big.ohpewt" ? UserTablePtr (big) : nullptr;
    });
    CHECK (r2.loaded == 1);
    CHECK (r2.embedded == 1);
    CHECK (tableOf (b, 0, Target::Wavetable) == big);

    // garbage / empty clears everything
    restoreAssignments (b, "garbage", {});
    for (int p = 0; p < Core::numParts; ++p) CHECK (tableOf (b, p, Target::Wavetable) == nullptr);
}

TEST_CASE ("user wavetable: part presets carry the usertables block")
{
    Core a;
    auto t = std::const_pointer_cast<UserTable> (sawTable (8));
    t->path = "/nowhere/Part.ohpewt";
    setTable (a, 2, Target::Wavetable, t);
    auto p = preset::capturePart (a.params(), 2, { "With table" });
    p.userTables = partToJson (a, 2);
    preset::Preset back;
    REQUIRE (preset::fromJson (preset::toJson (p), back));
    REQUIRE (back.userTables.has_value());
    Core b;
    const auto r = partFromJson (b, 0, *back.userTables, {});
    CHECK (r.embedded == 1);
    REQUIRE (tableOf (b, 0, Target::Wavetable) != nullptr);
    CHECK (tableOf (b, 0, Target::Wavetable)->name() == "Saw");
    CHECK (partToJson (b, 1).empty());
}

TEST_CASE ("user wavetable: live input capture")
{
    InputCapture c;
    CHECK_FALSE (c.recording());
    c.arm (0.01, 48000.0); // 480 samples
    CHECK (c.recording());
    std::vector<float> l (256, 0.5f), r (256, -0.25f);
    c.push (l.data(), r.data(), 256);
    CHECK (c.progress() == doctest::Approx (256.0f / 480.0f));
    c.push (l.data(), nullptr, 256);
    CHECK_FALSE (c.recording());
    CHECK (c.finished());
    c.push (l.data(), r.data(), 256); // ignored after the end
    const auto audio = c.take();
    REQUIRE (audio.size() == 480);
    CHECK (audio[0] == doctest::Approx (0.125f));
    CHECK (audio[300] == doctest::Approx (0.5f));
    CHECK_FALSE (c.finished());
}
