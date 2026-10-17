// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../dsp/DspUtil.h"
#include "../dsp/Oscillators.h"

#include <algorithm>
#include <cmath>

namespace ohpe
{
/**
    p31: the part's SUB layer - one sub oscillator per voice, one or two octaves below the voice's pitch, to dial in
    extra low end on any synth engine ("pN.sub.level", "pN.sub.oct", "pN.sub.shape", "pN.sub.direct").

    - Phase-locked to the voice: a master phase runs at the voice's own pitch (the pitch the engine plays: note, glide,
      pitch bend, matrix pitch, pitch envelope, life drift) and the sub phase is that phase divided by 2 or 4 (the
      master's wraps are counted), reset at every note start. So the sub never beats against the voice.
    - Band-limited: Sine; Round = a triangle bent through a quarter sine (smooth at the peaks, harmonics fall at
      18 dB / octave); Square with polyBLEP edges.
    - Mono-safe: one signal, the same on both channels (never panned, no stereo width).
    - Level-safe: the level ramps sample by sample (no zipper steps), the shapes are normalised to about the same
      loudness, and the sub rides on the voice's amp envelope and velocity.
    - Level 0 = off: nothing runs at all, the voice renders exactly as without the sub (bit-identical).
*/
class PartSub
{
public:
    enum Shape { Sine = 0, Round, Square, numShapes };

    /** Output gain of the sub at level 1 relative to the engines' voice level (per channel, before the amp envelope). */
    static constexpr float outGain = 0.22f;

    /** A new note (retriggered voice): phase back to the start, level at once (the envelope fades it in). */
    void start (float levelTarget) noexcept
    {
        master = 0.0f;
        count = 0;
        level = target = std::clamp (levelTarget, 0.0f, 1.0f);
        step = 0.0f;
        running = level > 0.0f;
    }

    /** Hard stop (voice killed). */
    void kill() noexcept
    {
        level = target = step = 0.0f;
        running = false;
    }

    /**
        Per control block: the level / octave / shape and the voice's pitch (semitones) for the next n samples.
        Returns true while the sub sounds (call next() once per sample then).
    */
    bool tick (float levelTarget, int octave, int shapeIndex, float pitch, float sampleRate, int n) noexcept
    {
        const float t = std::clamp (levelTarget, 0.0f, 1.0f);
        if (! running && t <= 0.0f) return false;
        running = true;
        target = t;
        step = (target - level) / (float) std::max (1, n);
        div = octave >= 1 ? 4 : 2; // choice: 0 = one octave down, 1 = two octaves down
        if (count >= div) count %= div;
        shape = std::clamp (shapeIndex, 0, (int) numShapes - 1);
        inc = std::clamp (dsp::midiToHz (pitch) / sampleRate, 0.0f, 0.49f);
        if (level <= 0.0f && target <= 0.0f)
        {
            running = false;
            level = 0.0f;
            return false;
        }
        return true;
    }

    /** One sample of the sub, level applied (no envelope). */
    float next() noexcept
    {
        level += step;
        if ((step > 0.0f && level > target) || (step < 0.0f && level < target)) level = target;
        master += inc;
        if (master >= 1.0f)
        {
            master -= 1.0f;
            if (++count >= div) count = 0;
        }
        const float fdiv = (float) div;
        const float ph = ((float) count + master) / fdiv; // 0..1
        const float dt = inc / fdiv;
        float y;
        switch (shape)
        {
            case Round:
            {
                const float tri = 1.0f - 4.0f * std::abs (ph - 0.5f); // -1 at 0, +1 at 0.5 ... start at the trough
                y = std::sin (0.5f * dsp::twoPi * 0.5f * tri);       // quarter-sine bend: smooth peaks
                break;
            }
            case Square:
            {
                float sq = ph < 0.5f ? 1.0f : -1.0f;
                sq += dsp::polyBlep (ph, dt);
                float t2 = ph - 0.5f;
                if (t2 < 0.0f) t2 += 1.0f;
                sq -= dsp::polyBlep (t2, dt);
                y = 0.72f * sq; // about the loudness of the sine
                break;
            }
            default: y = std::sin (dsp::twoPi * ph); break;
        }
        return y * level;
    }

    bool isRunning() const noexcept { return running; }
    float currentLevel() const noexcept { return level; }
    int divider() const noexcept { return div; }
    float subPhase() const noexcept { return ((float) count + master) / (float) div; }

private:
    float master = 0.0f, inc = 0.0f;
    int count = 0, div = 2, shape = Sine;
    float level = 0.0f, target = 0.0f, step = 0.0f;
    bool running = false;
};
} // namespace ohpe
