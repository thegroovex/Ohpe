// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Resonator.h"

#include "../dsp/DspUtil.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace ohpe
{
namespace
{
constexpr float pi = 3.14159265358979f;
constexpr int numApStages = 4;
constexpr float minLineDelay = 2.0f;  // the Hermite read needs one newer sample than the one it starts at
constexpr float stateLimit = 1000.0f; // guard: a state beyond this means something went wrong -> reset
constexpr float peakExp = 0.4f;       // resonance peak gain ~ (loss per period)^-peakExp (0.5 = unity broadband gain)
constexpr float limitThreshold = 0.95f;
constexpr float compThreshold = 0.35f, compRatio = 4.0f; // the body's loudness control ahead of the peak limiter
constexpr float stringGain = 1.0f, tubeGain = 1.0f, plateGain = 1.0f, plateDrive = 2.5f; // output calibration per model

/** Phase delay (samples) of the first-order allpass (c + z^-1) / (1 + c z^-1) at w (radians / sample). */
float allpassDelay (float c, float w) noexcept
{
    const std::complex<float> z1 = std::polar (1.0f, -w);
    const auto h = (c + z1) / (1.0f + c * z1);
    float ph = -std::arg (h);
    if (ph < 0.0f) ph += 2.0f * pi;
    return ph / w;
}

/** Phase delay (samples) of the one-pole (1 - a) / (1 - a z^-1) at w. */
float onePoleDelay (float a, float w) noexcept { return std::atan2 (a * std::sin (w), 1.0f - a * std::cos (w)) / w; }

/** Deterministic -1..+1 per (m, n): the small imperfections of a real plate (no exactly degenerate mode pairs). */
float plateJitter (int m, int n) noexcept
{
    uint32_t h = (uint32_t) m * 0x9E3779B1u ^ (uint32_t) n * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return (float) (h & 0xffff) / 32767.5f - 1.0f;
}

double logCosh (double u) noexcept
{
    const double a = std::abs (u);
    return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531;
}

float noteToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
} // namespace

// ---------------------------------------------------------------------------------------------- building blocks
void ResonatorEffect::Line::prepare (int minLength)
{
    int n = 1;
    while (n < minLength + 8) n <<= 1;
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
    w = 0;
}

void ResonatorEffect::Line::clear() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    w = 0;
}

inline float ResonatorEffect::Line::hermite (float d) const noexcept
{
    // the value written d samples ago (d = 1: the newest), 4-point 3rd-order Hermite between d and d + 1
    const int i = (int) d;
    const float f = d - (float) i;
    const float y0 = buf[(size_t) ((w - i + 1) & mask)];
    const float y1 = buf[(size_t) ((w - i) & mask)];
    const float y2 = buf[(size_t) ((w - i - 1) & mask)];
    const float y3 = buf[(size_t) ((w - i - 2) & mask)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

inline float ResonatorEffect::Line::linear (float d) const noexcept
{
    const int i = (int) d;
    const float f = d - (float) i;
    const float a = buf[(size_t) ((w - i) & mask)];
    const float b = buf[(size_t) ((w - i - 1) & mask)];
    return a + (b - a) * f;
}

inline float ResonatorEffect::Adaa::process (float x, float k) noexcept
{
    const double u = (double) k * (double) x, u1 = (double) k * (double) x1;
    x1 = x;
    const double d = u - u1;
    const double y = std::abs (d) > 1.0e-5 ? (logCosh (u) - logCosh (u1)) / d : std::tanh (0.5 * (u + u1));
    return (float) (y / (double) k);
}

void ResonatorEffect::Guide::clear() noexcept
{
    line.clear();
    apX.fill (0.0f);
    apY.fill (0.0f);
    lossZ = 0.0f;
    sat.x1 = 0.0f;
    last = 0.0f;
}

inline float ResonatorEffect::Guide::loop() noexcept
{
    float y = line.hermite (delay);
    delay += dDelay;
    const float c = apC;
    apC += dApC;
    for (int k = 0; k < numApStages; ++k) // stiffness: y[n] = c x[n] + x[n-1] - c y[n-1]
    {
        const float o = c * (y - apY[(size_t) k]) + apX[(size_t) k];
        apX[(size_t) k] = y;
        apY[(size_t) k] = dsp::flush (o);
        y = o;
    }
    lossZ = dsp::flush (lossG * (1.0f - lossA) * y + lossA * lossZ); // DC gain lossG < 1, highs lose more
    lossG += dLossG;
    lossA += dLossA;
    return lossZ * sign;
}

// ---------------------------------------------------------------------------------------------- parameters
std::vector<ParamDescriptor> ResonatorEffect::getParamDescriptors() const
{
    using namespace pd;
    return {
        choice ("model", "Body", "Körper", { "String", "Plate", "Open Tube", "Closed Tube" },
                { "Saite", "Platte", "Offenes Rohr", "Gedacktes Rohr" }, ModelString),
        choice ("follow", "Tuning", "Stimmung", { "Fixed", "Note", "Chord" }, { "Fest", "Note", "Akkord" }, FollowNote),
        continuous ("tune", "Body Tune", "Körper-Stimmung", -24.0f, 24.0f, 0.0f, ParamUnit::Semitones),
        continuous ("size", "Size", "Größe", 0.0f, 1.0f, 0.5f, ParamUnit::Percent),
        continuous ("material", "Material", "Material", 0.0f, 1.0f, 0.55f, ParamUnit::Percent),
        continuous ("decay", "Decay", "Ausklang", 0.05f, 20.0f, 2.0f, ParamUnit::Seconds, 0.3f),
        continuous ("position", "Position", "Position", 0.0f, 1.0f, 0.3f, ParamUnit::Percent),
        continuous ("inharm", "Inharmonicity", "Inharmonizität", 0.0f, 1.0f, 0.1f, ParamUnit::Percent),
        continuous ("sympathetic", "Sympathetic Strings", "Resonanzsaiten", 0.0f, 1.0f, 0.35f, ParamUnit::Percent),
        continuous ("width", "Body Width", "Körper-Breite", 0.0f, 1.0f, 0.6f, ParamUnit::Percent),
        continuous ("drive", "Drive", "Drive", 0.0f, 1.0f, 0.0f, ParamUnit::Percent),
        continuous ("mix", "Body Mix", "Körper-Mischung", 0.0f, 1.0f, 0.5f, ParamUnit::Percent),
        continuous ("pressamt", "Pressure to Body", "Druck auf Körper", -1.0f, 1.0f, 0.5f),
        continuous ("slideamt", "Slide to Position", "Slide auf Position", -1.0f, 1.0f, 0.5f),
    };
}

int ResonatorEffect::modulationParam (int j) const noexcept
{
    static constexpr int map[4] = { Tune, Material, Decay, Position };
    return map[std::clamp (j, 0, 3)];
}

void ResonatorEffect::setParam (int localIndex, float v) noexcept
{
    if (! std::isfinite (v)) return;
    switch (localIndex)
    {
        case Model: model = std::clamp ((int) std::lround (v), 0, numModels - 1); break;
        case Follow: follow = std::clamp ((int) std::lround (v), 0, numFollowModes - 1); break;
        case Tune: tune = std::clamp (v, -24.0f, 24.0f); break;
        case Size: size = dsp::clamp01 (v); break;
        case Material: material = dsp::clamp01 (v); break;
        case Decay: decay = std::clamp (v, 0.05f, 20.0f); break;
        case Position: position = dsp::clamp01 (v); break;
        case Inharm: inharm = dsp::clamp01 (v); break;
        case Sympathetic: sympathetic = dsp::clamp01 (v); break;
        case Width: width = dsp::clamp01 (v); break;
        case Drive: drive = dsp::clamp01 (v); break;
        case Mix: mix = dsp::clamp01 (v); break;
        case PressAmt: pressAmt = std::clamp (v, -1.0f, 1.0f); break;
        case SlideAmt: slideAmt = std::clamp (v, -1.0f, 1.0f); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------- lifecycle
void ResonatorEffect::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? (float) sampleRate : 48000.0f;
    const int len = (int) std::ceil (sr / 15.0f) + 64; // lowest body pitch 16 Hz
    for (auto& g : guide) g.line.prepare (len);
    for (auto& s : sym) s.line.prepare (len);
    excLine.prepare (len);
    wetL.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    wetR.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    mixCoeff = 1.0f - std::exp (-1.0f / (0.02f * sr));
    dcCoeff = 1.0f - 2.0f * pi * 12.0f / sr;
    limRelease = std::exp (-1.0f / (0.12f * sr));
    compAttack = 1.0f - std::exp (-1.0f / (0.004f * sr));
    compRelease = 1.0f - std::exp (-1.0f / (0.2f * sr));
    reset();
}

void ResonatorEffect::resetState() noexcept
{
    for (auto& g : guide) g.clear();
    for (auto& s : sym) { s.line.clear(); s.z = 0.0f; }
    excLine.clear();
    inSat.x1 = 0.0f;
    mRe.fill (0.0f);
    mIm.fill (0.0f);
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
    radX1[0] = radX1[1] = 0.0f;
    limEnv = 0.0f;
    compEnv = 0.0f;
    plateEnv = 0.0f;
    levelAcc = 0.0f;
    levelCount = 0;
    fresh = true;
}

void ResonatorEffect::reset() noexcept
{
    resetState();
    mixSm = mix;
    snapMix = true;
    bypassed = mix <= 0.0f;
    uiLevelValue.store (0.0f, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------------------------- control
float ResonatorEffect::basePitchNote() const noexcept
{
    if (follow != FollowFixed && keys != nullptr && keys->lastNote >= 0)
        return (float) keys->lastNote + keys->bendSemitones + tune;
    return 48.0f + tune; // C3 (fixed tuning, or before the first note)
}

void ResonatorEffect::chooseSympathetic (float note) noexcept
{
    numSymActive = sympathetic > 0.0f ? 2 + (int) std::lround (size * 4.0f) : 0;
    std::array<float, maxSympathetic> notes {};
    static constexpr float offsets[maxSympathetic] = { 12.0f, 7.0f, 19.0f, 24.0f, -12.0f, 16.0f }; // octave, fifth ...
    if (follow == FollowChord && keys != nullptr)
    {
        if (keys->numHeld <= 0)
        {
            if (symHz[0] > 0.0f) return; // no keys down: the strings keep the last chord
            for (int i = 0; i < maxSympathetic; ++i) notes[(size_t) i] = note + offsets[i];
        }
        else
        {
            const int h = std::min (keys->numHeld, KeyState::maxHeld);
            for (int i = 0; i < maxSympathetic; ++i)
                notes[(size_t) i] = (float) keys->held[(size_t) (i % h)] + 12.0f * (float) (i / h) + keys->bendSemitones + tune;
        }
    }
    else
        for (int i = 0; i < maxSympathetic; ++i) notes[(size_t) i] = note + offsets[i];

    const float maxHz = sr / 7.0f;
    for (int i = 0; i < maxSympathetic; ++i)
    {
        float f = noteToHz (std::clamp (notes[(size_t) i], -60.0f, 200.0f));
        while (f > maxHz) f *= 0.5f;
        while (f < 30.0f) f *= 2.0f;
        symHz[(size_t) i] = f;
    }
}

void ResonatorEffect::setGuide (Guide& gd, float f0, float t60, float a, bool closed, float c, float extraDelay, int n) noexcept
{
    const float loopLen = closed ? sr / (2.0f * f0) : sr / f0;
    const float w = 2.0f * pi * f0 / sr;
    float lineDelay = 0.0f;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        lineDelay = loopLen - (float) numApStages * allpassDelay (c, w) - onePoleDelay (a, w) - extraDelay;
        if (lineDelay >= minLineDelay) break;
        c *= 0.5f; // high pitches: less dispersion / loss phase, so the loop still fits
        a *= 0.6f;
    }
    lineDelay = std::clamp (lineDelay, minLineDelay, (float) gd.line.mask - 4.0f);
    const float g = std::min (0.99995f, std::pow (10.0f, -3.0f * loopLen / (std::max (0.02f, t60) * sr)));
    gd.sign = closed ? -1.0f : 1.0f;
    const float inv = 1.0f / (float) std::max (1, n);
    if (fresh)
    {
        gd.delay = lineDelay; gd.apC = c; gd.lossG = g; gd.lossA = a;
        gd.dDelay = gd.dApC = gd.dLossG = gd.dLossA = 0.0f;
        return;
    }
    gd.dDelay = (lineDelay - gd.delay) * inv;
    gd.dApC = (c - gd.apC) * inv;
    gd.dLossG = (g - gd.lossG) * inv;
    gd.dLossA = (a - gd.lossA) * inv;
}

void ResonatorEffect::updatePlate (float f0, float t60, float bright, float pos) noexcept
{
    if (size != plateShapeSize || inharm != plateShapeInh) // the mode pattern: the lowest 48 of 9 x 9 (m, n)
    {
        plateShapeSize = size;
        plateShapeInh = inharm;
        const float asp = 1.0f + 1.8f * size, ia2 = 1.0f / (asp * asp);
        const float p = 0.7f + 0.6f * inharm; // 0.5 = the ideal plate, lower = closer to harmonic, higher = stretched
        std::array<float, 81> r {};
        std::array<int, 81> order {};
        for (int m = 1; m <= 9; ++m)
            for (int nn = 1; nn <= 9; ++nn)
            {
                const int i = (m - 1) * 9 + nn - 1;
                const float base = ((float) (m * m) + (float) (nn * nn) * ia2) / (1.0f + ia2);
                r[(size_t) i] = std::pow (base, p) * (1.0f + (i == 0 ? 0.0f : 0.0035f * plateJitter (m, nn)));
                order[(size_t) i] = i;
            }
        std::partial_sort (order.begin(), order.begin() + numModes, order.end(),
                           [&] (int x, int y) { return r[(size_t) x] < r[(size_t) y]; });
        for (int k = 0; k < numModes; ++k)
        {
            const int i = order[(size_t) k];
            modeM[(size_t) k] = i / 9 + 1;
            modeN[(size_t) k] = i % 9 + 1;
            modeRatio[(size_t) k] = r[(size_t) i];
        }
    }
    const float xe = 0.07f + 0.43f * pos, ye = 0.37f;               // exciter
    const float xl = 0.81f, yl = 0.67f, xr = 0.23f, yr = 0.43f;     // pickups (left / right)
    const float bend = 1.0f + drive * 0.05f * std::min (1.0f, plateEnv * 3.0f); // tension: louder = higher, no energy added
    const float hfDamp = (1.0f - bright) * 1.1f, tilt = (1.0f - bright) * 0.5f;
    const float maxHz = 0.45f * sr;
    int active = 0;
    for (int k = 0; k < numModes; ++k)
    {
        const float f = f0 * modeRatio[(size_t) k] * bend;
        if (f >= maxHz)
        {
            mCr[(size_t) k] = mCi[(size_t) k] = mIn[(size_t) k] = 0.0f;
            mRe[(size_t) k] = mIm[(size_t) k] = 0.0f;
            modeHz[(size_t) k] = 0.0f;
            continue;
        }
        ++active;
        modeHz[(size_t) k] = f;
        const float ratio = f0 / f;
        const float t = std::clamp (t60 * std::pow (ratio, hfDamp), 0.01f, 40.0f);
        const float rad = std::exp (-6.9077553f / (t * sr));
        const float w = 2.0f * pi * f / sr;
        mCr[(size_t) k] = rad * std::cos (w);
        mCi[(size_t) k] = rad * std::sin (w);
        const float m = (float) modeM[(size_t) k], nn = (float) modeN[(size_t) k];
        // every mode at unity broadband gain: a pluck or noise excites the plate at about its own level
        mIn[(size_t) k] = plateDrive * std::sin (m * pi * xe) * std::sin (nn * pi * ye) * std::sqrt (1.0f - rad * rad) * std::pow (ratio, tilt);
        mOutL[(size_t) k] = std::sin (m * pi * xl) * std::sin (nn * pi * yl);
        mOutR[(size_t) k] = std::sin (m * pi * xr) * std::sin (nn * pi * yr);
    }
    // modes above the limit are off: keep the active ones contiguous at the front (they are sorted by ratio)
    numModesActive = active;
}

void ResonatorEffect::control (int n) noexcept
{
    // keyboard, expression
    const float press = keys != nullptr ? dsp::clamp01 (keys->pressure) : 0.0f;
    const float slide = keys != nullptr ? std::clamp (keys->slide, -1.0f, 1.0f) : 0.0f;
    const float k30 = fresh ? 1.0f : 1.0f - std::exp (-(float) n / (0.03f * sr));
    pressSm += (press - pressSm) * k30;
    const float pressMod = pressSm * pressAmt;
    posSm += (dsp::clamp01 (position + 0.5f * slide * slideAmt) - posSm) * k30;

    const float note = std::clamp (basePitchNote(), -40.0f, 180.0f);
    const float targetLog = std::log2 (noteToHz (note));
    const float kPitch = fresh ? 1.0f : 1.0f - std::exp (-(float) n / (0.006f * sr));
    if (fresh || logPitch < 0.0f) logPitch = targetLog;
    logPitch += (targetLog - logPitch) * kPitch;
    const bool waveguide = model != ModelPlate;
    pitchHz = std::clamp (std::exp2 (logPitch), 16.0f, waveguide ? sr / 7.0f : 0.4f * sr);

    const float t60 = std::clamp (decay * std::exp2 (1.5f * pressMod), 0.03f, 40.0f); // pressure: + rings longer, - damps
    const float excTarget = 1.0f + 2.0f * std::max (0.0f, pressMod);                    // and excites harder
    const float inv = 1.0f / (float) std::max (1, n);
    dExcGain = fresh ? 0.0f : (excTarget - excGain) * inv;
    if (fresh) excGain = excTarget;
    satAmt = model == ModelPlate ? 0.0f : drive * 0.85f;

    if (waveguide)
    {
        const bool closed = model == ModelClosedTube;
        const bool tube = model != ModelString;
        float a, c, detuneCents;
        if (tube)
        {
            a = std::clamp (0.18f + 0.55f * (1.0f - material) + 0.2f * size, 0.0f, 0.93f); // radiation + wall loss
            c = 0.45f * inharm;                                                             // bore deviation
            detuneCents = 0.3f + 3.0f * size;
            radiate = 0.15f + 0.6f * size;
        }
        else
        {
            a = 0.85f * std::pow (1.0f - material, 1.6f) + 0.02f;
            c = -0.88f * inharm; // stiffness: upper partials sharp
            detuneCents = 0.5f + 6.0f * size;
            radiate = 0.0f;
        }
        const float extra = 0.5f * satAmt;
        setGuide (guide[0], pitchHz, t60, a, closed, c, extra, n);
        setGuide (guide[1], pitchHz * std::exp2 (detuneCents / 1200.0f), t60, a, closed, c, extra, n);
        const float g = guide[0].lossG + guide[0].dLossG * (float) n;
        // between unity broadband gain (sqrt (1 - g^2)) and unity peak gain (1 - g): tuned excitation (an engine
        // playing the note the body follows) rings strongly but not without bound, short plucks still speak
        inNorm = 1.4f * std::pow (std::max (1.0e-7f, 1.0f - g), 1.0f - peakExp) * (closed ? 1.4f : 1.0f);
        const float theta = 0.02f;
        rotC = std::cos (theta);
        rotS = std::sin (theta);
        outNorm = tube ? tubeGain : stringGain;
        // excitation point: comb at (4 % .. 50 %) of the loop (in the closed tube: of the half loop)
        const float loopLen = closed ? sr / (2.0f * pitchHz) : sr / pitchHz;
        const float tap = std::clamp ((0.04f + 0.46f * posSm) * loopLen, 1.0f, (float) excLine.mask - 4.0f) + 1.0f;
        dExcTap = fresh ? 0.0f : (tap - excTap) * inv;
        if (fresh) excTap = tap;
    }
    else
    {
        updatePlate (pitchHz, t60, material, posSm);
        outNorm = plateGain;
    }
    panW = width;

    // sympathetic strings
    chooseSympathetic (note);
    symLevel = sympathetic;
    symCouple = 0.6f * sympathetic;
    const float symT60 = std::min (40.0f, t60 * 1.3f + 0.3f);
    const float symA = 0.75f * std::pow (1.0f - material, 1.4f) + 0.05f;
    const float kSym = fresh ? 1.0f : 1.0f - std::exp (-(float) n / (0.025f * sr));
    for (int i = 0; i < maxSympathetic; ++i)
    {
        auto& s = sym[(size_t) i];
        if (i >= numSymActive)
        {
            if (s.active) { s.line.clear(); s.z = 0.0f; s.active = false; }
            continue;
        }
        const float f = symHz[(size_t) i];
        const float w = 2.0f * pi * f / sr;
        const float d = std::clamp (sr / f - onePoleDelay (symA, w), minLineDelay, (float) s.line.mask - 4.0f);
        if (! s.active || fresh) { s.delay = d; s.dDelay = 0.0f; }
        else s.dDelay = (s.delay + (d - s.delay) * kSym - s.delay) * inv;
        s.active = true;
        s.a = symA;
        s.g = std::min (0.99995f, std::pow (10.0f, -3.0f / (symT60 * f)));
        s.inGain = symCouple * std::pow (std::max (1.0e-7f, 1.0f - s.g), 1.0f - peakExp);
        const float pan = (i % 2 == 0 ? -1.0f : 1.0f) * (0.3f + 0.12f * (float) i) * width;
        dsp::panGains (pan, s.gl, s.gr);
    }

    uiPitchValue.store (pitchHz, std::memory_order_relaxed);
    uiPosValue.store (posSm, std::memory_order_relaxed);
    uiModelValue.store (model, std::memory_order_relaxed);
    fresh = false;
}

// ---------------------------------------------------------------------------------------------- audio
void ResonatorEffect::process (float* left, float* right, int numSamples) noexcept
{
    if (snapMix) // parameters set after reset() (a preset, a new slot type) apply at once
    {
        mixSm = mix;
        snapMix = false;
    }
    if (mix <= 0.0f && mixSm <= 0.0f) // exact bypass: the buffer is not touched
    {
        if (! bypassed) { bypassed = true; uiLevelValue.store (0.0f, std::memory_order_relaxed); }
        return;
    }
    if (bypassed) // coming back: start clean
    {
        resetState();
        bypassed = false;
    }
    const int maxBlock = (int) wetL.size();
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int len = std::min (maxBlock, numSamples - start);
        float* l = left + start;
        float* r = right + start;
        for (int s0 = 0; s0 < len; s0 += controlRate)
        {
            const int n = std::min (controlRate, len - s0);
            control (n);
            const bool driveIn = drive > 0.0f;
            const float kIn = 1.0f + 7.0f * drive * drive, inMakeup = 1.0f + 1.5f * drive;
            const bool plate = model == ModelPlate;
            const float gl0 = std::sqrt (0.5f * (1.0f + 0.7f * panW)), gr0 = std::sqrt (0.5f * (1.0f - 0.7f * panW));
            for (int i = s0; i < s0 + n; ++i)
            {
                const float in = 0.5f * (l[i] + r[i]);
                float e = in * excGain;
                excGain += dExcGain;
                if (driveIn) e = inSat.process (e, kIn) * inMakeup;
                float bl = 0.0f, br = 0.0f;
                if (! plate)
                {
                    excLine.push (e);
                    const float ec = (e - excLine.linear (excTap)) * inNorm; // excitation point (comb)
                    excTap += dExcTap;
                    float y0 = guide[0].loop(), y1 = guide[1].loop();
                    if (satAmt > 0.0f) // |sat (y)| <= |y|: passive
                    {
                        y0 += satAmt * (guide[0].sat.process (y0, 1.6f) - y0);
                        y1 += satAmt * (guide[1].sat.process (y1, 1.6f) - y1);
                    }
                    const float a = rotC * y0 + rotS * y1, b = rotC * y1 - rotS * y0; // lossless coupling at the bridge
                    guide[0].line.push (a + ec);
                    guide[1].line.push (b + ec);
                    guide[0].last = a;
                    guide[1].last = b;
                    float ol = a * gl0 + b * gr0, orr = a * gr0 + b * gl0;
                    if (radiate > 0.0f) // the open end radiates the highs: a gentle first difference
                    {
                        const float xl = ol, xr = orr;
                        ol -= radiate * radX1[0];
                        orr -= radiate * radX1[1];
                        radX1[0] = xl;
                        radX1[1] = xr;
                    }
                    bl = ol * outNorm;
                    br = orr * outNorm;
                }
                else
                {
                    float sl = 0.0f, sr2 = 0.0f;
                    for (int k = 0; k < numModesActive; ++k)
                    {
                        const float re = mRe[(size_t) k], im = mIm[(size_t) k];
                        const float nr = mCr[(size_t) k] * re - mCi[(size_t) k] * im + mIn[(size_t) k] * e;
                        const float ni = mCi[(size_t) k] * re + mCr[(size_t) k] * im;
                        mRe[(size_t) k] = nr;
                        mIm[(size_t) k] = ni;
                        sl += mOutL[(size_t) k] * ni;
                        sr2 += mOutR[(size_t) k] * ni;
                    }
                    const float m = 0.5f * (sl + sr2), sd = 0.5f * (sl - sr2) * panW;
                    bl = (m + sd) * outNorm;
                    br = (m - sd) * outNorm;
                }
                if (numSymActive > 0) // sympathetic strings, driven by the body
                {
                    const float bridge = 0.5f * (bl + br);
                    float yl = 0.0f, yr = 0.0f;
                    for (int k = 0; k < numSymActive; ++k)
                    {
                        auto& s = sym[(size_t) k];
                        const float y = s.line.hermite (s.delay);
                        s.delay += s.dDelay;
                        s.z = dsp::flush (s.g * (1.0f - s.a) * y + s.a * s.z);
                        s.line.push (s.z + bridge * s.inGain);
                        yl += s.z * s.gl;
                        yr += s.z * s.gr;
                    }
                    bl += yl * symLevel * 2.5f;
                    br += yr * symLevel * 2.5f;
                }
                wetL[(size_t) i] = bl;
                wetR[(size_t) i] = br;
            }
            // flush the plate's decayed modes, guard every state
            float worst = 0.0f;
            if (plate)
            {
                for (int k = 0; k < numModesActive; ++k)
                {
                    mRe[(size_t) k] = dsp::flush (mRe[(size_t) k]);
                    mIm[(size_t) k] = dsp::flush (mIm[(size_t) k]);
                    worst = std::max (worst, std::abs (mRe[(size_t) k]) + std::abs (mIm[(size_t) k]));
                }
            }
            else
                worst = std::max ({ std::abs (guide[0].lossZ), std::abs (guide[1].lossZ), std::abs (guide[0].last), std::abs (guide[1].last) });
            for (int k = 0; k < numSymActive; ++k) worst = std::max (worst, std::abs (sym[(size_t) k].z));
            if (! (worst < stateLimit)) // NaN included
            {
                resetState();
                ++guardCount;
                for (int i = s0; i < s0 + n; ++i) wetL[(size_t) i] = wetR[(size_t) i] = 0.0f;
            }
        }

        // output: DC blocker, peak limiter (instant attack: never above the threshold), dry / wet
        float acc = 0.0f;
        for (int i = 0; i < len; ++i)
        {
            float wl = wetL[(size_t) i], wr = wetR[(size_t) i];
            dcY[0] = wl - dcX[0] + dcCoeff * dcY[0]; dcX[0] = wl; wl = dcY[0];
            dcY[1] = wr - dcX[1] + dcCoeff * dcY[1]; dcX[1] = wr; wr = dcY[1];
            float pk = std::max (std::abs (wl), std::abs (wr));
            compEnv = dsp::flush (compEnv + (pk - compEnv) * (pk > compEnv ? compAttack : compRelease));
            if (compEnv > compThreshold) // soft loudness control: a strongly tuned resonance rings full, not clipped
            {
                const float cg = std::pow (compThreshold / compEnv, 1.0f - 1.0f / compRatio);
                wl *= cg;
                wr *= cg;
                pk *= cg;
            }
            limEnv = pk > limEnv ? pk : limEnv * limRelease;
            const float gain = limEnv > limitThreshold ? limitThreshold / limEnv : 1.0f;
            wl = std::clamp (wl * gain, -1.0f, 1.0f);
            wr = std::clamp (wr * gain, -1.0f, 1.0f);
            acc += std::abs (wl) + std::abs (wr);
            mixSm += (mix - mixSm) * mixCoeff;
            if (std::abs (mix - mixSm) < 1.0e-5f) mixSm = mix;
            l[i] += (wl - l[i]) * mixSm;
            r[i] += (wr - r[i]) * mixSm;
        }
        dcY[0] = dsp::flush (dcY[0]);
        dcY[1] = dsp::flush (dcY[1]);
        dcX[0] = dsp::flush (dcX[0]);
        dcX[1] = dsp::flush (dcX[1]);
        limEnv = dsp::flush (limEnv);
        plateEnv += (acc / (float) std::max (1, 2 * len) - plateEnv) * 0.2f;
        levelAcc += acc;
        levelCount += 2 * len;
        if (levelCount >= 1024)
        {
            const float lv = levelAcc / (float) levelCount;
            const float old = uiLevelValue.load (std::memory_order_relaxed);
            uiLevelValue.store (lv > old ? lv : old * 0.85f + lv * 0.15f, std::memory_order_relaxed);
            levelAcc = 0.0f;
            levelCount = 0;
        }
    }
}
} // namespace ohpe
