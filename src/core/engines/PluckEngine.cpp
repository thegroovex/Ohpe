// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluckEngine.h"

#include <complex>
#include <cstring>

namespace ohpe
{
namespace pluck
{
namespace
{
constexpr float modeRatio[Voice::numModes] = { 1.0f, 3.99f, 9.97f, 2.76f, 5.40f };
constexpr float modeGain[Voice::numModes] = { 1.0f, 0.45f, 0.18f, 0.22f, 0.12f };

/** Decay time (T60, seconds) of the string fundamental for a damping value and frequency. */
inline float stringT60 (float damping, float freq) noexcept
{
    const float base = 0.12f + 14.0f * std::pow (1.0f - dsp::clamp01 (damping), 2.5f);
    return base * std::clamp (std::pow (261.63f / freq, 0.35f), 0.35f, 2.5f);
}
} // namespace

void Voice::prepare (float sampleRate)
{
    prepareBase (sampleRate);
    int size = 1;
    while (size < (int) (sampleRate / minFreq) + 8) size <<= 1;
    line.assign ((size_t) size, 0.0f);
    burst.assign ((size_t) size, 0.0f);
    mask = size - 1;
    body1.prepare (sampleRate);
    body2.prepare (sampleRate);
    body3.prepare (sampleRate);
    clearState();
}

void Voice::clearState() noexcept
{
    if (! line.empty()) std::memset (line.data(), 0, sizeof (float) * line.size());
    writePos = 0;
    apX1 = apY1 = lpState = hpState = 0.0f;
    for (int m = 0; m < numModes; ++m) modeY1[m] = modeY2[m] = 0.0f;
    malletPos = malletLength = 0;
    body1.reset();
    body2.reset();
    body3.reset();
}

void Voice::kill() noexcept
{
    killBase();
    clearState();
}

void Voice::start (int midiNote, float vel, bool legato, const VoiceCommon& c) noexcept
{
    const bool wasActive = isActive();
    startBase (midiNote, vel, legato, c);
    if (! wasActive) clearState();
    noise.seed ((uint32_t) (midiNote * 7919 + 17));
    const float hardness = dsp::clamp01 (shared->excite * (0.6f + 0.4f * vel));
    const float freq = std::clamp (dsp::midiToHz ((float) midiNote + c.pitchOffset), minFreq, sr / 6.0f);
    exciteString (freq, hardness);
    // mallet: raised-cosine pulse, 0.4 ms (hard) .. 6 ms (soft)
    malletLength = std::max (2, (int) (sr * (0.0004f + 0.0056f * (1.0f - hardness))));
    malletPos = 0;
}

void Voice::exciteString (float freq, float hardness) noexcept
{
    if (line.empty()) return;
    const int period = std::clamp ((int) (sr / freq), 2, mask - 4);
    // noise burst through a one-pole low-pass (excitation brightness) ...
    const float fc = std::min (0.45f * sr, 150.0f * std::pow (2.0f, hardness * 7.5f));
    const float a = std::exp (-dsp::twoPi * fc / sr);
    float lp = 0.0f;
    for (int k = 0; k < period; ++k)
    {
        lp = (1.0f - a) * noise.nextBipolar() + a * lp;
        burst[(size_t) k] = lp;
    }
    // ... minus a delayed copy: a comb at the pick position (harmonics with a node there are notched)
    const int pick = std::clamp ((int) std::round (shared->pickPosition * (float) period), 1, period - 1);
    for (int k = period - 1; k >= pick; --k) burst[(size_t) k] -= burst[(size_t) (k - pick)];
    float mean = 0.0f, peakAbs = 1.0e-6f;
    for (int k = 0; k < period; ++k) mean += burst[(size_t) k];
    mean /= (float) period;
    for (int k = 0; k < period; ++k) peakAbs = std::max (peakAbs, std::abs (burst[(size_t) k] - mean));
    const float scale = 0.9f / peakAbs;
    // into the window the loop reads during the next period (added: a re-pluck keeps the string ringing)
    const int start = writePos - period;
    for (int k = 0; k < period; ++k)
        line[(size_t) ((start + k) & mask)] += (burst[(size_t) k] - mean) * scale;
}

void Voice::render (float* l, float* r, int numSamples, const VoiceCommon& c) noexcept
{
    const Shared& p = *shared;
    const float mat = dsp::clamp01 (p.material);
    const float wString = 1.0f - mat, wMallet = mat;
    const bool doString = wString > 0.001f, doMallet = wMallet > 0.001f;

    int done = 0;
    while (done < numSamples && isActive())
    {
        const int n = std::min (controlRate, numSamples - done);
        const float pitch = controlTick (c, n);
        const float f0 = std::clamp (dsp::midiToHz (pitch), minFreq, sr / 6.0f);
        const float w0 = dsp::twoPi * f0 / sr;
        const float cw = std::cos (w0), sw = std::sin (w0);

        // brightness: macro + mod envelope + LFO timbre
        float bright = p.brightness + c.modEnvAmount * modEnv.getLevel() * 0.5f;
        if (c.lfoDest == LfoDest::Timbre) bright += lfoValue * 0.3f;
        bright = dsp::clamp01 (bright);

        // ---- string loop: N (integer) + all-pass + loss filter (one-pole low-pass and a DC blocker at
        //      f0 / 20, which lets the fundamental keep its decay time even when the string is dark)
        //      = exactly one period at f0; loop gain set from the decay time, capped below 1 at every harmonic
        const float fc = std::min (0.45f * sr, f0 * (2.0f + 40.0f * bright * bright));
        const float a = std::exp (-dsp::twoPi * fc / sr);
        const float hp = std::exp (-dsp::twoPi * f0 * 0.05f / sr);
        const float hpGain = 0.5f * (1.0f + hp);
        auto loss = [&] (float w) {
            const std::complex<float> z1 = std::polar (1.0f, -w);
            return (1.0f - a) / (1.0f - a * z1) * hpGain * (1.0f - z1) / (1.0f - hp * z1);
        };
        const auto h1 = loss (w0);
        const float lossDelay = -std::arg (h1) / w0;
        float maxMag = std::abs (h1);
        for (int k = 2; k <= 4 && (float) k * w0 < dsp::pi; ++k) maxMag = std::max (maxMag, std::abs (loss ((float) k * w0)));
        for (float rel = 0.03f; rel < 1.0f; rel *= 1.5f) maxMag = std::max (maxMag, std::abs (loss (rel * w0))); // peak below f0
        const float period = sr / f0;
        const int N = std::clamp ((int) std::floor (period - lossDelay - 0.5f), 1, mask - 2);
        const float d = period - lossDelay - (float) N; // 0.5 .. 1.5 normally
        const float T = std::tan ((1.0f - d) * w0 * 0.5f);
        const float eta = std::clamp (T / (sw - cw * T), -0.99f, 0.99f);
        const float t60 = stringT60 (p.damping, f0);
        const float loopGain = std::min (std::pow (0.001f, 1.0f / (f0 * t60)) / std::abs (h1), 0.99995f / maxMag);

        // ---- mallet modes
        float mA1[numModes], mA2[numModes], mIn[numModes];
        const float t60m = (0.1f + 4.0f * std::pow (1.0f - dsp::clamp01 (p.damping), 2.0f)) * std::clamp (std::pow (261.63f / f0, 0.3f), 0.4f, 2.0f);
        const float hard = dsp::clamp01 (p.excite);
        for (int m = 0; m < numModes; ++m)
        {
            const float wm = w0 * modeRatio[m];
            if (! doMallet || wm > dsp::twoPi * 0.45f) { mA1[m] = mA2[m] = mIn[m] = 0.0f; continue; }
            const float rr = std::pow (0.001f, 1.0f / (t60m / std::pow (modeRatio[m], 0.7f) * sr));
            mA1[m] = 2.0f * rr * std::cos (wm);
            mA2[m] = -rr * rr;
            mIn[m] = std::sin (wm) * modeGain[m] * std::pow (0.15f + 0.85f * hard, std::log2 (modeRatio[m]));
        }
        const float malletNorm = 2.0f / (float) std::max (1, malletLength);

        // ---- body: three resonances scaled by body size
        const float fb = 85.0f * std::pow (2.0f, 2.0f * dsp::clamp01 (p.bodySize));
        body1.set (fb, 0.85f);
        body2.set (fb * 2.23f, 0.8f);
        body3.set (fb * 4.7f, 0.7f);
        const float bodyK = 2.0f - 1.96f * 0.8f; // ~ normalises the band-pass peak gain
        const float bodyAmt = dsp::clamp01 (p.body);

        float panL, panR;
        lfoPan (c, panL, panR);
        const float gain = 0.45f * velocityGain (c) * lfoAmp (c);

        for (int i = 0; i < n; ++i)
        {
            float x = 0.0f;
            if (doString)
            {
                const float s = line[(size_t) ((writePos - N) & mask)];
                const float ap = eta * s + apX1 - eta * apY1;
                apX1 = s;
                apY1 = dsp::flush (ap);
                const float lpPrev = lpState;
                lpState = dsp::flush ((1.0f - a) * ap + a * lpState);
                hpState = dsp::flush (hpGain * (lpState - lpPrev) + hp * hpState);
                line[(size_t) (writePos & mask)] = dsp::flush (hpState * loopGain);
                x += wString * s;
            }
            ++writePos;
            if (doMallet)
            {
                float exc = 0.0f;
                if (malletPos < malletLength)
                {
                    exc = malletNorm * 0.5f * (1.0f - std::cos (dsp::twoPi * (float) malletPos / (float) malletLength));
                    ++malletPos;
                }
                float sum = 0.0f;
                for (int m = 0; m < numModes; ++m)
                {
                    const float y = mA1[m] * modeY1[m] + mA2[m] * modeY2[m] + mIn[m] * exc;
                    modeY2[m] = modeY1[m];
                    modeY1[m] = dsp::flush (y);
                    sum += y;
                }
                x += wMallet * sum * 0.8f;
            }
            if (bodyAmt > 0.0f)
            {
                const float b = body1.process (x, dsp::Svf::BandPass) + 0.7f * body2.process (x, dsp::Svf::BandPass)
                              + 0.4f * body3.process (x, dsp::Svf::BandPass);
                x = x * (1.0f - 0.4f * bodyAmt) + b * bodyK * bodyAmt * 1.2f;
            }
            modEnv.next();
            const float out = x * ampEnv.next() * gain;
            l[done + i] += out * panL;
            r[done + i] += out * panR;
        }
        writePos &= mask;
        done += n;
    }
}
} // namespace pluck

PluckEngine::PluckEngine()
{
    for (auto& v : voices) v.shared = &shared;
}

std::vector<ParamDescriptor> PluckEngine::getParamDescriptors() const
{
    using namespace pd;
    std::vector<ParamDescriptor> d;
    d.push_back (continuous ("material", "Material", "Material", 0.0f, 1.0f, 0.0f));
    d.push_back (continuous ("damping", "Damping", "Dämpfung", 0.0f, 1.0f, 0.35f));
    d.push_back (continuous ("bright", "Brightness", "Helligkeit", 0.0f, 1.0f, 0.55f));
    d.push_back (continuous ("body", "Body", "Korpus", 0.0f, 1.0f, 0.3f));
    d.push_back (continuous ("excite", "Excitation", "Anregung", 0.0f, 1.0f, 0.6f));
    d.push_back (continuous ("pickpos", "Pick Position", "Anschlagpunkt", 0.02f, 0.5f, 0.18f));
    d.push_back (continuous ("bodysize", "Body Size", "Korpusgröße", 0.0f, 1.0f, 0.5f));
    return d;
}

void PluckEngine::setParam (int i, float v) noexcept
{
    using namespace pluck;
    switch (i)
    {
        case Material: shared.material = v; break;
        case Damping: shared.damping = v; break;
        case Brightness: shared.brightness = v; break;
        case Body: shared.body = v; break;
        case Excite: shared.excite = v; break;
        case PickPosition: shared.pickPosition = v; break;
        case BodySize: shared.bodySize = v; break;
        default: break;
    }
}
} // namespace ohpe
