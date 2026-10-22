// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p68: Voice Control ("Stimme führt Klang") - a live audio input analysed in real time into modulation sources:
// level, vowel position (F1 / F2 on an A-E-I-O-U map), brightness, noisiness (sibilants / fricatives), pitch
// (optional) and an onset trigger. Not a vocoder: the voice only modulates, it is never heard through the synth
// (the optional monitor is off by default).
//
// Analysis: the input is low-passed and decimated to about 11-12 kHz, a 16 ms Hann window (centre about 8 ms behind
// the input) hops every ~2.7 ms; LPC (order 12, autocorrelation + Levinson) and the roots of the predictor polynomial
// give the formants F1 / F2. Brightness, noisiness and onsets run at the full rate. Real-time safe: fixed buffers,
// no allocation after prepare().

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ohpe::voice
{
/** Index of each analysed value (also the order of the matrix sources Voice Level ... Onset). */
enum Value
{
    Level = 0, ///< 0 below the gate .. 1 at about -6 dBFS
    VowelX,    ///< bipolar: back (U / O, low F2) -1 .. +1 front (I / E, high F2); 0 = centre / rest
    VowelY,    ///< bipolar: closed (I / U, low F1) -1 .. +1 open (A, high F1); 0 = centre / rest
    Brightness,///< share of the energy above ~2 kHz (0 dark .. 1 bright)
    Noisiness, ///< sibilance / fricative / unvoiced (0 tonal .. 1 noise)
    Pitch,     ///< sung note C2 (0) .. C6 (1), only with pitch tracking on; held between notes
    Onset,     ///< 1 at a new syllable / attack, decaying to 0 in ~80 ms
    numValues
};

/** The five vowels of the vowel map. */
enum Vowel { VowelA = 0, VowelE, VowelI, VowelO, VowelU, numVowels };

/** Map position (0..1 each; the sources use 2 * pos - 1) of a formant pair (Hz). */
void formantsToMap (float f1, float f2, float& x, float& y) noexcept;
/** Nearest vowel of a map position (0..1 each). */
int classifyVowel (float x, float y) noexcept;
/** Centre of vowel v on the map; reference formants (adult average) of vowel v. */
void vowelCentre (int v, float& x, float& y) noexcept;
void vowelFormants (int v, float& f1, float& f2) noexcept;
const char* vowelName (int v) noexcept; ///< "A", "E", "I", "O", "U"

struct Settings
{
    float gainDb = 0.0f;    ///< input gain
    float gateDb = -50.0f;  ///< below this the voice counts as silent (level 0, the rest held, then released)
    float smoothMs = 20.0f; ///< smoothing of the continuous values
    bool pitch = false;     ///< pitch tracking (costs a little more CPU)
};

class VoiceAnalyser
{
public:
    static constexpr int lpcOrder = 12;
    static constexpr int maxWindow = 512;  ///< decimated samples
    static constexpr int pitchWindow = 256;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setSettings (const Settings& s) noexcept { settings = s; }

    /** Analyses n samples (mono or stereo; right may be nullptr; both nullptr = no input). Any n >= 0. */
    void process (const float* left, const float* right, int n) noexcept;

    /** The smoothed value v (Value) after the last process() call. */
    float value (int v) const noexcept { return v >= 0 && v < numValues ? out[(size_t) v] : 0.0f; }
    /** Unsmoothed formants of the last voiced frame (Hz, 0 = none yet). */
    float lastF1() const noexcept { return f1Raw; }
    float lastF2() const noexcept { return f2Raw; }
    /** Input level before the gate (dBFS, -120 = silence). */
    float inputDb() const noexcept { return inDb; }
    bool gateOpen() const noexcept { return open; }
    /** Analysis latency (window centre) in ms at the prepared rate. */
    float latencyMs() const noexcept { return latency; }

    // ---- UI feedback (any thread) ----
    void publishUi() noexcept;
    float uiValue (int v) const noexcept { return v >= 0 && v < numValues ? ui[(size_t) v].load (std::memory_order_relaxed) : 0.0f; }
    float uiInputDb() const noexcept { return uiDb.load (std::memory_order_relaxed); }
    bool uiGateOpen() const noexcept { return uiOpen.load (std::memory_order_relaxed); }

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double sr, double f, double q) noexcept;
        float run (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void analyseFrame() noexcept;
    void analysePitch() noexcept;

    Settings settings;
    double sr = 48000.0;
    int decim = 4, decimPhase = 0, win = 192, hop = 32, sinceHop = 0, hopCount = 0;
    float fsd = 12000.0f, latency = 8.0f;
    Biquad aa1, aa2;
    std::array<float, maxWindow> ring {}; ///< decimated, pre-emphasised history
    std::array<float, maxWindow> hann {};
    int ringPos = 0;
    float preLast = 0.0f;

    // full-rate features
    float hpBrightZ = 0.0f, hpBrightX = 0.0f, hpSibZ = 0.0f, hpSibX = 0.0f, cBright = 0.0f, cSib = 0.0f;
    float eAll = 0.0f, eBright = 0.0f, eSib = 0.0f, zcr = 0.0f, lastSample = 0.0f;
    float featAlpha = 0.0f;
    float envFast = 0.0f, envSlow = 0.0f, fastAtk = 0.0f, fastRel = 0.0f, slowAtk = 0.0f, slowRel = 0.0f;
    float onsetDecay = 0.0f, refractory = 0.0f, onset = 0.0f;
    float inDb = -120.0f;
    bool open = false;
    float closedFor = 0.0f; ///< seconds since the gate closed

    // frame results (targets)
    std::array<float, numValues> target {}, out {};
    float f1Raw = 0.0f, f2Raw = 0.0f;
    float voicing = 0.0f;

    std::array<std::atomic<float>, numValues> ui {};
    std::atomic<float> uiDb { -120.0f };
    std::atomic<bool> uiOpen { false };
};
} // namespace ohpe::voice
