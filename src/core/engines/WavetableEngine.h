// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/Filters.h"
#include "../dsp/Oscillators.h"
#include "../sample/SampleData.h"
#include "../wavetable/UserTable.h"
#include "PolyEngine.h"

#include <vector>

namespace ohpe
{
/**
    Wavetable engine: procedurally generated tables of eight frames each, smooth morphing between
    frames, a smooth phase-distortion warp and band-limited playback from per-table mipmaps (one
    level per octave, crossfaded, chosen from the instantaneous phase speed so warping never
    aliases). Three unison oscillators, a sub sine and a resonant low-pass per voice.
*/
class WavetableEngine;

namespace wavetable
{
/** Local parameter indices (append-only, same order as getParamDescriptors()). */
enum Param
{
    Position = 0, Warp, Tone, Unison, // macros 1-4
    Table, WarpMode, EnvPosition, Resonance, Sub, KeyTrack,
    UniVoices, UniWidth, Fm, FmRatio, LfoPosition, EnvFm, // 0.8 (late parameters, registered at the end of the layout)
    numParams
};

enum WarpMode { Bend = 0, Twist };

/** The built-in tables (append-only, same order as the "table" choice). */
enum TableId { Classic = 0, Harmonic, Formant, Digital, Glass, Grit, numTables };

/** Immutable, procedurally generated table data shared by every engine instance. */
class Bank
{
public:
    static constexpr int numFrames = 8;
    static constexpr int size = 2048;          ///< samples per frame
    static constexpr int numLevels = 10;       ///< level k holds harmonics 1 .. 512 >> k
    static constexpr int stride = size + 1;    ///< one guard sample for interpolation
    static constexpr int maxHarmonics = 512;   ///< harmonics in level 0 (every level >= 4x oversampled)

    static const Bank& get();

    const float* frame (int table, int frameIndex, int level) const noexcept
    {
        return data.data() + ((size_t) ((table * numFrames + frameIndex) * numLevels + level)) * (size_t) stride;
    }

private:
    Bank();
    std::vector<float> data;
};

/** Linear interpolated read of one frame at phase 0..1. */
inline float readLinear (const float* t, float phase) noexcept
{
    const float x = phase * (float) Bank::size;
    const int i = std::min ((int) x, Bank::size - 1);
    const float f = x - (float) i;
    return t[i] + f * (t[i + 1] - t[i]);
}

/** Mip level pair + crossfade so the highest harmonic stays below Nyquist at `speed` (cycles per sample). */
inline void pickLevels (float speed, int& a, int& b, float& t) noexcept
{
    const float kf = std::log2 (std::max (1.0e-9f, (float) Bank::maxHarmonics * speed / 0.48f));
    a = std::clamp ((int) std::ceil (kf), 0, Bank::numLevels - 1);
    b = std::min (a + 1, Bank::numLevels - 1);
    t = std::clamp (kf - (float) (a - 1), 0.0f, 1.0f);
}

struct Shared
{
    float position = 0.25f, warp = 0.0f, tone = 0.75f, unison = 0.2f;
    int table = Classic, warpMode = Bend;
    float envPosition = 0.0f, resonance = 0.1f, sub = 0.0f, keyTrack = 0.5f;
    int uniVoices = 3;          ///< 0.8: 3 = the classic centre + two sides; 1, 2, 4 .. 8 = spread stack
    float uniWidth = 0.7f;      ///< 0.8: stereo spread of the stack (not used by the classic three)
    float fm = 0.0f, fmRatio = 1.0f; ///< 0.8: phase modulation of the table by a sine at f0 * ratio
    float lfoPosition = 0.0f, envFm = 0.0f; ///< 0.8: LFO -> position, mod envelope -> FM amount
    /** p20: the user table the voices play instead of the built-in one (nullptr = built-in), set every block. */
    const usertable::UserTable* user = nullptr;
};
constexpr int maxUnison = 8;

class Voice : public VoiceBase
{
public:
    const Shared* shared = nullptr;
    const Bank* bank = nullptr;

    void prepare (float sampleRate) noexcept;
    void start (int note, float vel, bool legato, const VoiceCommon& c) noexcept;
    void release() noexcept { releaseBase(); }
    void kill() noexcept;
    void render (float* l, float* r, int n, const VoiceCommon& c) noexcept;

private:
    float phase[maxUnison] {};
    float fmPhase = 0.0f;
    float lastPos = -1.0f, lastWarp = -1.0f;
    float dcX[2] {}, dcY[2] {};
    dsp::SineOsc subOsc;
    dsp::Svf filterL, filterR;
    float cutoffOct = -1.0f;
};
} // namespace wavetable

class WavetableEngine final : public PolyEngine<wavetable::Voice>
{
public:
    WavetableEngine();
    EngineInfo info() const noexcept override { return { "wavetable", "Wavetable", "Wavetable", "wavetable" }; }
    std::vector<ParamDescriptor> getParamDescriptors() const override;
    void setParam (int localIndex, float value) noexcept override;
    int firstLate08Param() const noexcept override { return wavetable::UniVoices; }

    const wavetable::Shared& getShared() const noexcept { return shared; }

    /** p20: a user wavetable (message thread; nullptr = none). Played while "pN.usertable.wt" is on. */
    void setUserTable (usertable::UserTablePtr table) { user.set (std::move (table)); }
    const usertable::UserTablePtr& userTable() const noexcept { return user.owner(); }
    void setUserSource (bool on) noexcept override { useUser = on; }
    /** Audio thread: the table the voices play now (nullptr = built-in). */
    const usertable::UserTable* playingUserTable() const noexcept { return shared.user; }

    void render (float* l, float* r, int numSamples) noexcept override
    {
        const auto* t = useUser ? user.get() : nullptr;
        shared.user = t != nullptr && t->hasWave() ? t : nullptr;
        PolyEngine<wavetable::Voice>::render (l, r, numSamples);
    }

private:
    wavetable::Shared shared;
    RtShared<usertable::UserTable> user;
    bool useUser = false;
};
} // namespace ohpe
