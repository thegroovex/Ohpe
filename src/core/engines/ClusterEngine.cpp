// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ClusterEngine.h"

namespace ohpe
{
namespace cluster
{
namespace
{
// semitone offsets per slot; the first n slots sound when the stack has n voices
constexpr int shapes[numShapes][maxStack] = {
    { 0, 0, 0, 0, 0, 0, 0 },        // Unison
    { 0, 12, -12, 0, 12, -12, 0 },  // Octave
    { 0, 7, 12, 0, 7, 12, -12 },    // Fifth
    { 0, 4, 7, 12, 0, 4, 7 },       // Major
    { 0, 3, 7, 12, 0, 3, 7 },       // Minor
    { 0, 2, 7, 12, 0, 2, 7 },       // Sus2
    { 0, 5, 7, 12, 0, 5, 7 },       // Sus4
    { 0, 4, 7, 11, 12, 4, 7 },      // Major 7
    { 0, 3, 7, 10, 12, 3, 7 },      // Minor 7
    { 0, 4, 7, 10, 12, 4, 7 },      // Dominant 7
    { 0, 4, 7, 14, 12, 4, 7 },      // Add 9
    { 0, 5, 10, 15, 0, 5, 10 },     // Fourths
};
// detune (relative to the maximum) and stereo position of each slot
constexpr float detuneOffset[maxStack] = { 0.0f, -1.0f, 1.0f, -0.58f, 0.58f, -0.19f, 0.19f };
constexpr float panPosition[maxStack] = { 0.0f, -1.0f, 1.0f, -0.66f, 0.66f, -0.33f, 0.33f };
} // namespace

int chordInterval (int shape, int slot) noexcept
{
    return shapes[std::clamp (shape, 0, numShapes - 1)][std::clamp (slot, 0, maxStack - 1)];
}

void Voice::prepare (float sampleRate) noexcept
{
    prepareBase (sampleRate);
    filterL.prepare (sampleRate);
    filterR.prepare (sampleRate);
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive)
    {
        rnd.seed ((uint32_t) (midiNote * 7919 + 101));
        osc[0].reset (0.0f);
        for (int k = 1; k < maxStack; ++k) osc[k].reset (rnd.next01()); // free phases: no flanging attack
        for (int k = 0; k < maxStack; ++k) driftValue[k] = driftTarget[k] = 0.0f;
        filterL.reset();
        filterR.reset();
        cutoffOct = -1.0f;
    }
}

void Voice::kill() noexcept
{
    killBase();
    filterL.reset();
    filterR.reset();
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    const int count = std::clamp (p.stackVoices, 1, maxStack);
    const int shape = std::clamp (p.chord, 0, numShapes - 1);

    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);

        // detune: fine near zero, up to +-55 cents for the outer pair
        const float maxCents = 55.0f * std::pow (dsp::clamp01 (p.detune), 1.6f);
        const float driftCents = 7.0f * dsp::clamp01 (p.drift);
        float freq[maxStack], gL[maxStack], gR[maxStack];
        float power = 0.0f;
        for (int k = 0; k < count; ++k)
        {
            // slow random drift per oscillator
            if (rnd.next01() < 0.02f) driftTarget[k] = rnd.nextBipolar();
            driftValue[k] += (driftTarget[k] - driftValue[k]) * 0.01f;
            const float cents = maxCents * detuneOffset[k] + driftCents * driftValue[k] * (k == 0 ? 0.0f : 1.0f);
            freq[k] = dsp::midiToHz (pitch + (float) shapes[shape][k] + cents * 0.01f);
            const float level = k == 0 ? 1.0f : p.sideLevel;
            // fade out stack members that get too close to Nyquist
            const float rel = freq[k] / sr;
            const float nyq = std::clamp ((0.42f - rel) / 0.17f, 0.0f, 1.0f);
            float pl, pr;
            dsp::panGains (panPosition[k] * dsp::clamp01 (p.spread), pl, pr);
            gL[k] = level * nyq * pl;
            gR[k] = level * nyq * pr;
            power += level * level;
        }
        const float norm = 1.41421356f / std::sqrt (std::max (1.0f, power)); // applied before the filter: no jumps
        for (int k = 0; k < count; ++k) { gL[k] *= norm; gR[k] *= norm; }

        float octaves = p.keyTrack * (pitch - 60.0f) / 12.0f + c.modEnvAmount * modEnv.getLevel() * 5.0f;
        if (c.lfoDest == LfoDest::Timbre) octaves += lfoValue * 2.0f;
        // cutoff in octaves above 20 Hz, glided over ~4 ms so jumps never ring the filter hard
        const float target = std::clamp (std::log2 (2.0f * std::pow (500.0f, dsp::clamp01 (p.tone))) + octaves, 0.0f, 9.97f);
        cutoffOct = cutoffOct < 0.0f ? target : cutoffOct + (target - cutoffOct) * (1.0f - std::exp (-(float) n / (sr * 0.004f)));
        const float cutoff = 20.0f * std::exp2 (cutoffOct);
        filterL.set (cutoff, 0.92f * p.resonance);
        filterR.set (cutoff, 0.92f * p.resonance);

        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = 0.3f * velocityGain (c) * lfoAmp (c);

        for (int i = 0; i < n; ++i)
        {
            float sL = 0.0f, sR = 0.0f;
            for (int k = 0; k < count; ++k)
            {
                const float v = osc[k].next (freq[k], sr, p.shape);
                sL += v * gL[k];
                sR += v * gR[k];
            }
            const float yL = filterL.process (sL * 0.7f, dsp::Svf::LowPass);
            const float yR = filterR.process (sR * 0.7f, dsp::Svf::LowPass);
            modEnv.next();
            const float env = ampEnv.next() * gain;
            l[done + i] += yL * env * panL * 1.41421356f;
            r[done + i] += yR * env * panR * 1.41421356f;
        }
        done += n;
    }
}
} // namespace cluster

ClusterEngine::ClusterEngine()
{
    for (auto& v : voices) v.shared = &shared;
}

std::vector<ParamDescriptor> ClusterEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("detune", "Detune", "Verstimmung", 0.0f, 1.0f, 0.3f));
    d.push_back (continuous ("spread", "Spread", "Breite", 0.0f, 1.0f, 0.6f));
    d.push_back (continuous ("tone", "Tone", "Klang", 0.0f, 1.0f, 0.7f));
    d.push_back (choice ("chord", "Chord", "Akkord",
                         { "Unison", "Octave", "Fifth", "Major", "Minor", "Sus2", "Sus4", "Major 7", "Minor 7", "Dominant 7", "Add 9", "Fourths" },
                         { "Unisono", "Oktave", "Quinte", "Dur", "Moll", "Sus2", "Sus4", "Dur 7", "Moll 7", "Dominant 7", "Add 9", "Quarten" }, 0));
    d.push_back (integer ("stack", "Stack Voices", "Stapelstimmen", 1, cluster::maxStack, 7));
    d.push_back (continuous ("shape", "Shape", "Form", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("side", "Side Level", "Seitenpegel", 0.0f, 1.0f, 0.7f));
    d.push_back (continuous ("reso", "Resonance", "Resonanz", 0.0f, 1.0f, 0.1f));
    d.push_back (continuous ("keytrack", "Key Track", "Key-Tracking", 0.0f, 1.0f, 0.5f));
    d.push_back (continuous ("drift", "Drift", "Drift", 0.0f, 1.0f, 0.2f));
    return d;
}

void ClusterEngine::setParam (int i, float v) noexcept
{
    using namespace cluster;
    switch (i)
    {
        case Detune: shared.detune = v; break;
        case Spread: shared.spread = v; break;
        case Tone: shared.tone = v; break;
        case Chord: shared.chord = (int) v; break;
        case StackVoices: shared.stackVoices = (int) v; break;
        case Shape: shared.shape = v; break;
        case SideLevel: shared.sideLevel = v; break;
        case Resonance: shared.resonance = v; break;
        case KeyTrack: shared.keyTrack = v; break;
        case Drift: shared.drift = v; break;
        default: break;
    }
}
} // namespace ohpe
