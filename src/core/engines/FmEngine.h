// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PolyEngine.h"

#include <array>

namespace ohpe
{
namespace fm
{
constexpr int numOps = 6;
constexpr int numAlgorithms = 8;

/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Algo = 0, ModRatio, Index, Feedback, // macros 1-4
    Op1Ratio, Op2Ratio, Op3Ratio, Op4Ratio, Op5Ratio, Op6Ratio,
    Op1Level, Op2Level, Op3Level, Op4Level, Op5Level, Op6Level,
    VelocityIndex, // 0.8 (late parameter, registered at the end of the layout)
    numParams
};

/**
    One algorithm = which operator modulates which, and which operators are heard.
    mod[dst][src] = 1 means operator src phase-modulates operator dst. Modulators always have
    a higher index than the operator they modulate, so one pass from op 6 down to op 1 is exact.
    Operator 6 (index 5) has the feedback loop.
*/
struct Algorithm
{
    float mod[numOps][numOps];
    float out[numOps];
};

/** The eight algorithm tables (index 0..7). The Algorithm macro morphs smoothly between neighbours. */
const std::array<Algorithm, numAlgorithms>& algorithms() noexcept;

/** Blends algorithm floor(pos) with the next one by the fractional part of pos (0..7). */
Algorithm morphAlgorithm (float pos) noexcept;

struct Shared
{
    float algorithm = 0.0f, modRatio = 1.0f, index = 0.35f, feedback = 0.0f;
    std::array<float, numOps> ratio { 1.0f, 1.0f, 2.0f, 3.0f, 1.0f, 4.0f };
    std::array<float, numOps> level { 1.0f, 0.8f, 0.8f, 0.6f, 0.6f, 0.5f };
    Algorithm algo = morphAlgorithm (0.0f);
    float velocityIndex = 0.0f; ///< 0.8: how much the velocity scales the index (0 = not at all)
};

/** p35, tests only: true renders with the library sine (the reference form) instead of the polynomial one. */
extern bool useLibrarySine;

class Voice : public VoiceBase
{
public:
    const Shared* shared = nullptr;

    void prepare (float sampleRate) noexcept { prepareBase (sampleRate); }
    void start (int note, float vel, bool legato, const VoiceCommon& c) noexcept;
    void release() noexcept { releaseBase(); }
    void kill() noexcept { killBase(); }
    void render (float* l, float* r, int n, const VoiceCommon& c) noexcept;

private:
    template <bool librarySine>
    void renderImpl (float* l, float* r, int n, const VoiceCommon& c) noexcept;

    std::array<float, numOps> phase {};
    float fb1 = 0.0f, fb2 = 0.0f;
};
} // namespace fm

/** Six-operator phase-modulation engine with eight morphable algorithms. */
class FmEngine final : public PolyEngine<fm::Voice>
{
public:
    FmEngine();
    EngineInfo info() const noexcept override { return { "fm", "FM", "FM", "fm" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void setParam (int localIndex, float value) noexcept override;
    int firstLate08Param() const noexcept override { return fm::VelocityIndex; }

    const fm::Shared& getShared() const noexcept { return shared; }

private:
    fm::Shared shared;
};
} // namespace ohpe
