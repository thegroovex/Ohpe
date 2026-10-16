// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "IEffect.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ohpe
{
/**
    p19: "Resonator" / "Resonanzkörper" - a physically modelled body that any sound can excite. Used twice with the
    same parameter block:
      * the per-part stage "pN.reso.*" (Part: right after the engines and the layer, before the part's volume and FX rack;
        switched by "pN.reso.on"), which follows the part's keyboard (KeyState) and its MPE expression,
      * FX rack type "Resonator" (InsertSlot::Resonator, "pN.fxK.resonator.*", "mfxK.resonator.*"): in a part rack it
        follows that part's keyboard too, on the master rack it stays at its fixed tuning.

    Models (the input is the excitation: plucks, noise, pads ... whatever the engine plays):
      * String - two coupled waveguides (a slightly detuned pair exchanging energy through a lossless rotation at the
        bridge: beating and the two-stage decay of real strings), a first-order allpass cascade for stiffness
        (inharmonicity, stretched partials), a frequency-dependent loss filter (decay / material), the excitation
        point as a comb on the input (position);
      * Plate - a modal bank of 48 modes of a simply supported rectangular plate (proportions = size, mode pattern
        warped by inharmonicity), each mode a damped complex rotation, excited at the position, picked up at two points
        (natural stereo); drive bends the modes upward with the energy (tension, gong-like), without adding energy;
      * Open Tube / Closed Tube - a bore waveguide pair: open at both ends (all harmonics) or closed at one end (odd
        harmonics, an octave lower loop with inverting reflection), radiation loss set by the bore width (size);
      * every model can add up to six sympathetic strings, driven by the body's output and tuned to the played note
        (octaves / fifths), the held chord, or the fixed tuning.

    Stability: every recursion is passive (loop gains < 1, rotations norm-preserving, in-loop saturation |f(x)| <= |x|),
    parameters are clamped (the line delay never gets shorter than the interpolator needs, dispersion is reduced at high
    pitches), states are flushed (no denormals), and a guard resets the body if a state ever leaves its range. The wet
    signal passes a peak limiter and a soft clipper (|wet| <= 1). The drive saturations use first-order antiderivative
    anti-aliasing (no oversampling needed: the only nonlinearities are smooth and run on band-limited signals).
    mix 0 is an exact bypass (the buffer is not touched).

    CPU: one body per part (it is the instrument's body that all voices share, as with a real soundboard), so the cost
    does not grow with the voice count: String ~2 interpolated waveguides + 6 short loops, Plate 48 two-multiply
    rotations per sample. No allocation after prepare().
*/
class ResonatorEffect final : public IEffect
{
public:
    /// Descriptor order (append-only: local IDs are part of the parameter IDs).
    enum Param
    {
        Model = 0, Follow, Tune, Size, Material, Decay, Position, Inharm, Sympathetic, Width, Drive, Mix, PressAmt, SlideAmt,
        numParams
    };
    enum ModelType { ModelString = 0, ModelPlate, ModelOpenTube, ModelClosedTube, numModels };
    enum FollowMode { FollowFixed = 0, FollowNote, FollowChord, numFollowModes };

    static constexpr int numModes = 48;    ///< plate modes
    static constexpr int maxSympathetic = 6;
    static constexpr int controlRate = 16; ///< samples per coefficient update

    ResonatorEffect() = default;

    EffectInfo info() const noexcept override { return { "resonator", "Resonator", "Resonanzkörper", "resonator" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setParam (int localIndex, float plainValue) noexcept override;
    void process (float* left, float* right, int numSamples) noexcept override;
    double tailSeconds() const noexcept override { return 8.0; }
    /** Rack "FX k Param 1-4": tune, material, decay, position. */
    int modulationParam (int j) const noexcept override;
    void setKeys (const KeyState* k) noexcept override { keys = k; }

    /** Fundamental the body is tuned to right now (Hz) - tests, UI. */
    float currentPitchHz() const noexcept { return pitchHz; }
    /** Tunings of the sympathetic strings (Hz, 0 = inactive) - tests. */
    float sympatheticHz (int i) const noexcept { return i >= 0 && i < maxSympathetic ? symHz[(size_t) i] : 0.0f; }
    int activeSympathetic() const noexcept { return numSymActive; }
    /** Frequency of plate mode k (Hz, 0 = off) - tests. */
    float plateModeHz (int k) const noexcept { return k >= 0 && k < numModes ? modeHz[(size_t) k] : 0.0f; }
    /** True after the guard had to reset the body (tests: must never happen). */
    int guardResets() const noexcept { return guardCount; }

    // ---- UI (any thread) ----
    float uiLevel() const noexcept { return uiLevelValue.load (std::memory_order_relaxed); }
    float uiPitch() const noexcept { return uiPitchValue.load (std::memory_order_relaxed); }
    float uiExcitePosition() const noexcept { return uiPosValue.load (std::memory_order_relaxed); }
    int uiModel() const noexcept { return uiModelValue.load (std::memory_order_relaxed); }

private:
    /** Power-of-two delay line with Hermite (4-point) and linear reads. Delays in samples, >= 1 / >= 2. */
    struct Line
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void prepare (int minLength);
        void clear() noexcept;
        void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float hermite (float d) const noexcept;
        float linear (float d) const noexcept;
    };
    /** First-order antiderivative anti-aliased tanh (k = input gain; returns tanh (k x) / k averaged). */
    struct Adaa
    {
        float x1 = 0.0f;
        float process (float x, float k) noexcept;
    };
    /** One waveguide loop: line -> dispersion allpasses -> loss one-pole -> sign -> saturation -> (+ input) -> line. */
    struct Guide
    {
        Line line;
        float delay = 100.0f, dDelay = 0.0f;
        float apC = 0.0f, dApC = 0.0f;
        std::array<float, 4> apX {}, apY {};
        float lossG = 0.9f, dLossG = 0.0f, lossA = 0.0f, dLossA = 0.0f, lossZ = 0.0f;
        float sign = 1.0f;
        Adaa sat;
        float last = 0.0f; ///< last loop output (bridge signal)
        void clear() noexcept;
        float loop() noexcept; ///< reads and filters (returns the reflected wave; call write after)
    };
    struct Sym // sympathetic string: line + loss one-pole
    {
        Line line;
        float delay = 100.0f, dDelay = 0.0f, g = 0.9f, a = 0.2f, z = 0.0f, inGain = 0.0f, gl = 0.7f, gr = 0.7f;
        bool active = false;
    };

    void control (int n) noexcept;            ///< coefficient targets for the next n samples
    void updatePlate (float f0, float t60, float bright, float pos) noexcept;
    void setGuide (Guide& gd, float f0, float t60, float bright, bool closed, float inh, float extraDelay, int n) noexcept;
    float basePitchNote() const noexcept;
    void chooseSympathetic (float note) noexcept;
    void resetState() noexcept;

    // parameters (plain values)
    int model = ModelString, follow = FollowNote;
    float tune = 0.0f, size = 0.5f, material = 0.55f, decay = 2.0f, position = 0.3f, inharm = 0.1f, sympathetic = 0.35f,
          width = 0.6f, drive = 0.0f, mix = 0.5f, pressAmt = 0.5f, slideAmt = 0.5f;

    const KeyState* keys = nullptr;

    // state
    float sr = 48000.0f;
    int countdown = 0;
    bool fresh = true, bypassed = true, snapMix = true; ///< snapMix: the first block after reset() starts at the mix (no glide)
    float mixSm = 0.0f, mixCoeff = 0.01f;
    float pitchHz = 130.81f, logPitch = -1.0f, posSm = 0.3f, pressSm = 0.0f, excGain = 1.0f, dExcGain = 0.0f;
    std::array<Guide, 2> guide;
    float rotC = 1.0f, rotS = 0.0f, panW = 0.6f, inNorm = 0.5f, outNorm = 1.0f, radiate = 0.0f, radX1[2] {}, satAmt = 0.0f;
    Line excLine;
    float excTap = 10.0f, dExcTap = 0.0f;
    Adaa inSat;
    std::array<Sym, maxSympathetic> sym;
    std::array<float, maxSympathetic> symHz {};
    int numSymActive = 0;
    float symLevel = 0.0f, symCouple = 0.0f;
    // plate
    std::array<float, numModes> mRe {}, mIm {}, mCr {}, mCi {}, mIn {}, mOutL {}, mOutR {}, modeHz {}, modeRatio {};
    std::array<int, numModes> modeM {}, modeN {};
    float plateShapeSize = -1.0f, plateShapeInh = -1.0f, plateEnv = 0.0f;
    int numModesActive = 0;
    // output
    float dcX[2] {}, dcY[2] {}, dcCoeff = 0.999f, limEnv = 0.0f, limRelease = 0.999f, compEnv = 0.0f, compAttack = 0.01f, compRelease = 0.001f;
    int guardCount = 0;
    float levelAcc = 0.0f;
    int levelCount = 0;
    std::vector<float> wetL, wetR;

    std::atomic<float> uiLevelValue { 0.0f }, uiPitchValue { 130.81f }, uiPosValue { 0.3f };
    std::atomic<int> uiModelValue { 0 };
};
} // namespace ohpe
