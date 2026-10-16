// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FmEngine.h"

#include <cstring>

namespace ohpe
{
namespace fm
{
namespace
{
Algorithm makeAlgorithm (std::initializer_list<std::pair<int, int>> links, std::initializer_list<int> carriers)
{
    Algorithm a;
    std::memset (&a, 0, sizeof (a));
    for (const auto& [src, dst] : links) a.mod[dst][src] = 1.0f;
    const float g = 1.0f / (float) carriers.size();
    for (int c : carriers) a.out[c] = g;
    return a;
}

std::array<Algorithm, numAlgorithms> buildAlgorithms()
{
    // operator numbers are 0-based here (0 = op 1); a link {src, dst} means src modulates dst
    return {
        makeAlgorithm ({ { 5, 4 }, { 4, 3 }, { 3, 2 }, { 2, 1 }, { 1, 0 } }, { 0 }),               // 1: one tall stack
        makeAlgorithm ({ { 5, 4 }, { 4, 3 }, { 2, 1 }, { 1, 0 } }, { 0, 3 }),                       // 2: two stacks of three
        makeAlgorithm ({ { 5, 4 }, { 3, 2 }, { 1, 0 } }, { 0, 2, 4 }),                              // 3: three pairs
        makeAlgorithm ({ { 5, 4 }, { 4, 0 }, { 3, 0 }, { 2, 1 }, { 1, 0 } }, { 0 }),               // 4: branches into one carrier
        makeAlgorithm ({ { 5, 0 }, { 4, 0 }, { 3, 0 }, { 2, 0 }, { 1, 0 } }, { 0 }),               // 5: five modulators, one carrier
        makeAlgorithm ({ { 5, 4 }, { 5, 3 }, { 5, 2 }, { 5, 1 }, { 5, 0 } }, { 0, 1, 2, 3, 4 }),   // 6: one modulator, five carriers
        makeAlgorithm ({ { 5, 4 }, { 4, 3 } }, { 0, 1, 2, 3 }),                                    // 7: stack + three sines
        makeAlgorithm ({}, { 0, 1, 2, 3, 4, 5 }),                                                  // 8: additive (organ)
    };
}
} // namespace

const std::array<Algorithm, numAlgorithms>& algorithms() noexcept
{
    static const auto table = buildAlgorithms();
    return table;
}

Algorithm morphAlgorithm (float pos) noexcept
{
    pos = std::clamp (pos, 0.0f, (float) (numAlgorithms - 1));
    const int i = std::min ((int) pos, numAlgorithms - 2);
    const float t = pos - (float) i;
    const auto& a = algorithms()[(size_t) i];
    const auto& b = algorithms()[(size_t) i + 1];
    Algorithm m;
    for (int d = 0; d < numOps; ++d)
    {
        for (int s = 0; s < numOps; ++s) m.mod[d][s] = a.mod[d][s] + (b.mod[d][s] - a.mod[d][s]) * t;
        m.out[d] = a.out[d] + (b.out[d] - a.out[d]) * t;
    }
    return m;
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive)
    {
        phase.fill (0.0f);
        fb1 = fb2 = 0.0f;
    }
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    const Algorithm& a = p.algo;
    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);
        const float f0 = dsp::midiToHz (pitch);

        // modulation index: macro * mod envelope (amount) * LFO timbre; up to ~4 pi radians
        float idx = p.index;
        const float envAmt = std::abs (c.modEnvAmount);
        const float envShape = c.modEnvAmount >= 0.0f ? modEnv.getLevel() : 1.0f - modEnv.getLevel();
        idx *= (1.0f - envAmt) + envAmt * envShape;
        if (c.lfoDest == LfoDest::Timbre) idx *= std::max (0.0f, 1.0f + lfoValue);
        idx *= std::max (0.0f, 1.0f + expr.timbre + expr.index); // per-note expression (MPE; 0.8: FM index target)
        if (p.velocityIndex > 0.0f) idx *= 1.0f - p.velocityIndex + p.velocityIndex * 1.6f * velocity * velocity; // 0.8
        const float depth = idx * 4.0f * dsp::pi;

        std::array<float, numOps> inc;
        for (int o = 0; o < numOps; ++o)
        {
            const float ratio = o == 0 ? p.ratio[0] : p.ratio[(size_t) o] * p.modRatio;
            inc[(size_t) o] = std::min (0.49f, f0 * ratio / sr);
        }
        const float fbAmt = p.feedback * 1.5f;

        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = 0.35f * velocityGain (c) * lfoAmp (c);

        for (int i = 0; i < n; ++i)
        {
            float opOut[numOps];
            for (int o = numOps - 1; o >= 0; --o)
            {
                float pm = 0.0f;
                for (int s = o + 1; s < numOps; ++s)
                    pm += a.mod[o][s] * opOut[s];
                pm *= depth;
                if (o == numOps - 1) pm += fbAmt * 0.5f * (fb1 + fb2);
                opOut[o] = std::sin (dsp::twoPi * phase[(size_t) o] + pm) * p.level[(size_t) o];
                phase[(size_t) o] += inc[(size_t) o];
                if (phase[(size_t) o] >= 1.0f) phase[(size_t) o] -= 1.0f;
            }
            fb2 = fb1;
            fb1 = opOut[numOps - 1];
            float sum = 0.0f;
            for (int o = 0; o < numOps; ++o) sum += a.out[o] * opOut[o];
            modEnv.next();
            const float out = sum * ampEnv.next() * gain;
            l[done + i] += out * panL;
            r[done + i] += out * panR;
        }
        done += n;
    }
}
} // namespace fm

FmEngine::FmEngine()
{
    for (auto& v : voices) v.shared = &shared;
}

std::vector<ParamDescriptor> FmEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("algo", "Algorithm", "Algorithmus", 0.0f, 7.0f, 0.0f));
    d.push_back (continuous ("modratio", "Mod Ratio", "Mod.-Verhältnis", 0.25f, 8.0f, 1.0f, ParamUnit::None, 0.4f));
    d.push_back (continuous ("index", "Index", "Index", 0.0f, 1.0f, 0.35f));
    d.push_back (continuous ("feedback", "Feedback", "Feedback", 0.0f, 1.0f, 0.0f));
    const float ratios[] = { 1.0f, 1.0f, 2.0f, 3.0f, 1.0f, 4.0f };
    const float levels[] = { 1.0f, 0.8f, 0.8f, 0.6f, 0.6f, 0.5f };
    for (int o = 0; o < fm::numOps; ++o)
    {
        const auto n = std::to_string (o + 1);
        d.push_back (continuous ("op" + n + "ratio", "Op " + n + " Ratio", "Op " + n + " Verhältnis", 0.25f, 16.0f,
                                 ratios[o], ParamUnit::None, 0.4f));
    }
    for (int o = 0; o < fm::numOps; ++o)
    {
        const auto n = std::to_string (o + 1);
        d.push_back (continuous ("op" + n + "level", "Op " + n + " Level", "Op " + n + " Pegel", 0.0f, 1.0f, levels[o]));
    }
    d.push_back (continuous ("velidx", "Velocity > Index", "Anschlag > Index", 0.0f, 1.0f, 0.0f)); // 0.8 (late)
    return d;
}

void FmEngine::setParam (int i, float v) noexcept
{
    using namespace fm;
    if (i == Algo)
    {
        if (v != shared.algorithm) { shared.algorithm = v; shared.algo = morphAlgorithm (v); }
    }
    else if (i == ModRatio) shared.modRatio = v;
    else if (i == Index) shared.index = v;
    else if (i == Feedback) shared.feedback = v;
    else if (i >= Op1Ratio && i <= Op6Ratio) shared.ratio[(size_t) (i - Op1Ratio)] = v;
    else if (i >= Op1Level && i <= Op6Level) shared.level[(size_t) (i - Op1Level)] = v;
    else if (i == VelocityIndex) shared.velocityIndex = dsp::clamp01 (v);
}
} // namespace ohpe
