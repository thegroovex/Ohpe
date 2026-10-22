// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// p68: Voice Control - the analyser (VoiceAnalyser) behind its host parameters and the seven modulation-matrix
// sources Voice Level, Vowel X, Vowel Y, Brightness, Noisiness, Voice Pitch, Onset (mod::SrcVoiceLevel ...).
//
// Parameters (global, appended after the p59 grid block; none of them in part presets, see isExcluded):
//   voice.on       Voice Control on / off (default on - silent input costs next to nothing; off: every source 0)
//   voice.gain     input gain, -24 .. +24 dB
//   voice.gate     threshold, -80 .. -20 dBFS (below: silent, level 0, the rest held, then released)
//   voice.smooth   smoothing of the sources, 1 .. 250 ms
//   voice.pitch    pitch tracking (the Voice Pitch source), off by default
//   voice.monitor  hear the input on the main output (off by default: no feedback through speakers)
//   pN.voice.map   trigger: Quick map "Vowel -> Filter + Morph" for part N (message thread, see quickMap)

#include "../model/ParamRegistry.h"
#include "../model/ParamSnapshot.h"
#include "VoiceAnalyser.h"

#include <string>
#include <utility>
#include <vector>

namespace ohpe::voice
{
struct Ids
{
    static constexpr const char* on = "voice.on";
    static constexpr const char* gain = "voice.gain";
    static constexpr const char* gate = "voice.gate";
    static constexpr const char* smooth = "voice.smooth";
    static constexpr const char* pitch = "voice.pitch";
    static constexpr const char* monitor = "voice.monitor";
    static std::string map (int part) { return "p" + std::to_string (part + 1) + ".voice.map"; } ///< part 0..3
};

class VoiceControl
{
public:
    static void registerParams (ParamRegistry& reg, int numParts);
    /** True for the voice parameters (kept out of part / project presets: they belong to the player's setup). */
    static bool isExcluded (const std::string& id) noexcept;

    /**
        The quick mapping "Vowel -> Filter + Morph" for part `part` (0-based) as parameter changes (id, plain value):
        Vowel Y -> Timbre (+40 %), Vowel X -> Morph X (+50 %), Vowel Y -> Morph Y (+50 %), and voice.on. A routing that
        already exists (same source and target) only gets its amount back; new ones take free slots (source and target
        Off). Routings that find no free slot are left out (`placed` says how many of the three made it).
    */
    static std::vector<std::pair<std::string, float>> quickMap (const ParamRegistry& reg, int part, int* placed = nullptr);

    void bind (const ParamRegistry& reg);
    void prepare (double sampleRate) noexcept { analyser.prepare (sampleRate); }
    void reset() noexcept { analyser.reset(); }

    /** Audio thread: reads the settings, analyses the input (both nullptr = no input). */
    void process (const ParamSnapshot& s, const float* left, const float* right, int n) noexcept;
    /** Value of source k (0 .. numValues-1, the order of Value) for the matrix; 0 while off. */
    float sourceValue (int k) const noexcept { return enabled ? analyser.value (k) : 0.0f; }
    bool isOn() const noexcept { return enabled; }
    /** Audio thread: adds the monitored input (when voice.monitor is on) to the main pair. */
    void monitor (float* left, float* right, const float* inL, const float* inR, int n) const noexcept;

    const VoiceAnalyser& analysis() const noexcept { return analyser; }
    void publishUi() noexcept { analyser.publishUi(); }

private:
    VoiceAnalyser analyser;
    int idxOn = -1, idxGain = -1, idxGate = -1, idxSmooth = -1, idxPitch = -1, idxMonitor = -1;
    bool enabled = false, monitorOn = false;
    float gainLin = 1.0f;
};
} // namespace ohpe::voice
