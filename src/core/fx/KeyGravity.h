// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ohpe
{
/**
    p66: Key Gravity ("Tonart-Gravitation") - a spectral stage per part, right after the engines. Every partial is
    pulled toward the nearest pitch of the target set (the key / scale, the held chord or both, in every octave).

    How: a stereo STFT (Hann, 4x overlap; left and right packed into one complex FFT), peak picking with phase-locked
    regions (each peak carries the bins around it), the true frequency of every peak from its phase advance, a frequency
    remap of each region to the target pitch (partly, by the amount and the curve), partial tracks that glide to a new
    target when the chord changes, formant keep (the spectral envelope stays where it was), transients pass with their
    own phases. At high amounts the bins between the target pitches fade and the energy that lands on a target note
    takes that note's running phase: noise and drums turn into pitched texture in key.

    Amount 0 (and no latency asked by another part) is a bit-exact passthrough that does no work at all. The stage has
    latency (one frame: 512 / 1024 / 2048 samples at 48 kHz for Low Latency / Normal / High, scaled with the sample
    rate); the core asks every part for the largest active latency and every part (active or not) delays to it, so all
    parts stay in time and the host is told one latency.

    Real-time safe: all memory is allocated in prepare(); process() takes any block size (1 sample too).
*/
class KeyGravity
{
public:
    enum Target { TargetScale = 0, TargetChord, TargetChordScale, numTargets };
    enum Quality { QualityLow = 0, QualityNormal, QualityHigh, numQualities };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // ---- parameters (audio thread, any time)
    void setAmount (float a) noexcept;      ///< 0 .. 1 (0 = off)
    void setTarget (int t) noexcept;        ///< Target
    void setGlide (float seconds) noexcept; ///< how fast partials flow to a new target
    void setCurve (float c) noexcept;       ///< -1 (gentle) .. +1 (magnetic)
    void setFormant (float f) noexcept;     ///< 0 .. 1 formant keep
    void setMix (float m) noexcept;         ///< 0 .. 1
    void setQuality (int q) noexcept;       ///< Quality (a change restarts the analysis)
    /** Absolute pitch-class masks (bit 0 = C): the key's scale and the held chord. */
    void setTargets (uint16_t scaleMask, uint16_t chordMask) noexcept;
    /** The latency the whole instrument runs at (core: the largest active one); 0 = none. */
    void setAlignLatency (int samples) noexcept;

    /** Processes in place. */
    void process (float* l, float* r, int n) noexcept;

    // ---- state
    bool active() const noexcept { return running; }
    /** The latency this stage needs while it runs (0 when off). */
    int wantedLatency() const noexcept { return running ? frameLatency (quality) : 0; }
    /** The latency of a quality at the prepared sample rate. */
    int frameLatency (int q) const noexcept;
    /** True when process() is a passthrough right now (no work, bit-exact). */
    bool idle() const noexcept { return ! running && alignNow == 0 && alignFrom == 0 && alignWanted == 0; }
    /** The mask partials are pulled to (absolute pitch classes) for the current target. */
    uint16_t targetMask() const noexcept;

    // ---- pure helpers (tests)
    /** Signed distance in semitones from a MIDI pitch to the nearest target pitch (chord tones win by `chordBonus`). */
    static float distanceToTarget (float midi, uint16_t mask, uint16_t chordMask, float chordBonus) noexcept;
    /** Pull fraction 0 .. 1 for an amount and a curve. */
    static float pull (float amount, float curve) noexcept;

    static constexpr int maxPeaks = 320;

private:
    void startAnalysis() noexcept;
    void processFrame() noexcept;
    float stftSample (float xl, float xr, float& yr) noexcept;
    void fft (float* re, float* im, int n, bool inverse) const noexcept;

    double sr = 48000.0;
    int maxFrame = 0;
    int ringSize = 0, ringMask = 0;

    // parameters
    float amount = 0.0f, glideSec = 0.15f, curve = 0.0f, formant = 0.5f, mix = 1.0f, mixSmoothed = 1.0f;
    int target = TargetChordScale, quality = QualityNormal, activeQuality = QualityNormal;
    uint16_t scaleMask = 0xab5, chordMask = 0x091;

    // run state
    bool running = false, stopping = false, ringsLive = false;
    int zeroSamples = 0, offHoldSamples = 0;
    float wetGain = 0.0f, wetStep = 0.0f;
    int warmup = 0;

    // alignment (dry / wet rings)
    std::vector<float> dryL, dryR, wetL, wetR;
    int ringPos = 0;
    int alignWanted = 0, alignNow = 0, alignFrom = 0;
    int fadeLeft = 0, fadeLen = 1;

    // STFT
    int N = 1024, hop = 256, lat = 1024, fifoLat = 768, rover = 768;
    std::vector<float> inL, inR, outL, outR, accL, accR, window;
    std::vector<float> re, im;               // FFT buffers (maxFrame)
    std::vector<float> twRe, twIm;           // twiddles for maxFrame
    std::vector<int> bitrev;                 // for the current N
    std::vector<float> mag, env, curPhase, binMidi, prevPhase, prevMag, aRe, aIm, bRe, bIm, oaRe, oaIm, obRe, obIm, binW;
    std::vector<float> envPrefix;
    std::array<double, 128> notePhase {};
    float prevEnergy = 0.0f;
    bool havePrev = false;

    struct Track { float midi = 0.0f, tgt = 0.0f, outPhase = 0.0f; };
    std::array<Track, maxPeaks> prevTracks {}, curTracks {};
    int numPrev = 0;
    std::array<int, maxPeaks> peakBin {};
};
} // namespace ohpe
