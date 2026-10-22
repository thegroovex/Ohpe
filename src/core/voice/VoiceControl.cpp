// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "VoiceControl.h"

#include "../mod/ModMatrix.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ohpe::voice
{
static_assert (mod::SrcVoiceOnset - mod::SrcVoiceLevel + 1 == numValues, "one matrix source per analysed value, same order");

void VoiceControl::registerParams (ParamRegistry& reg, int numParts)
{
    using namespace pd;
    auto add = [&] (ParamDescriptor d) { d.group = "global/voice"; reg.add (std::move (d)); };
    add (toggle (Ids::on, "Voice Control", "Stimmsteuerung", true)); // on: routings from voice sources just work
    add (continuous (Ids::gain, "Voice Input Gain", "Stimme Eingangspegel", -24.0f, 24.0f, 0.0f, ParamUnit::Decibel));
    add (continuous (Ids::gate, "Voice Gate", "Stimme Schwelle", -80.0f, -20.0f, -50.0f, ParamUnit::Decibel));
    add (continuous (Ids::smooth, "Voice Smoothing", "Stimme Glättung", 1.0f, 250.0f, 20.0f, ParamUnit::Milliseconds, 0.4f));
    add (toggle (Ids::pitch, "Voice Pitch Tracking", "Stimme Tonhöhe erkennen", false));
    add (toggle (Ids::monitor, "Voice Monitor", "Stimme abhören", false));
    for (int p = 0; p < numParts; ++p)
    {
        const auto n = std::to_string (p + 1);
        auto d = toggle (Ids::map (p), "Part " + n + " Voice Quick Map", "Part " + n + " Stimme Schnellzuweisung", false);
        d.group = "part" + n + "/voice";
        reg.add (std::move (d));
    }
}

bool VoiceControl::isExcluded (const std::string& id) noexcept
{
    return id.rfind ("voice.", 0) == 0 || id.find (".voice.map") != std::string::npos || id == "voice.map";
}

std::vector<std::pair<std::string, float>> VoiceControl::quickMap (const ParamRegistry& reg, int part, int* placed)
{
    struct Want { int src, dst; float amt; };
    const Want want[] = { { mod::SrcVoiceVowelY, mod::destIndexOfKey ("timbre"), 0.4f },
                          { mod::SrcVoiceVowelX, mod::destIndexOfKey ("morphx"), 0.5f },
                          { mod::SrcVoiceVowelY, mod::destIndexOfKey ("morphy"), 0.5f } };
    std::vector<std::pair<std::string, float>> out;
    const auto pp = "p" + std::to_string (part + 1) + ".mod.s";
    auto slotId = [&] (int k, const char* f) { return pp + std::to_string (k + 1) + "." + f; };
    auto get = [&] (int k, const char* f) { return (int) std::lround (reg.get (slotId (k, f))); };
    std::array<bool, mod::numSlots> taken {};
    int done = 0;
    for (const auto& w : want)
    {
        if (w.dst <= 0) continue;
        int slot = -1;
        for (int k = 0; k < mod::numSlots && slot < 0; ++k)
            if (get (k, "src") == w.src && get (k, "dst") == w.dst) slot = k;
        for (int k = 0; k < mod::numSlots && slot < 0; ++k)
            if (! taken[(size_t) k] && get (k, "src") == mod::SrcOff && get (k, "dst") == mod::DstOff) slot = k;
        if (slot < 0) continue;
        taken[(size_t) slot] = true;
        out.emplace_back (slotId (slot, "src"), (float) w.src);
        out.emplace_back (slotId (slot, "via"), (float) mod::SrcOff);
        out.emplace_back (slotId (slot, "dst"), (float) w.dst);
        out.emplace_back (slotId (slot, "amt"), w.amt);
        out.emplace_back (slotId (slot, "curve"), (float) mod::CurveLinear);
        ++done;
    }
    out.emplace_back (Ids::on, 1.0f);
    if (placed != nullptr) *placed = done;
    return out;
}

void VoiceControl::bind (const ParamRegistry& reg)
{
    idxOn = reg.indexOf (Ids::on);
    idxGain = reg.indexOf (Ids::gain);
    idxGate = reg.indexOf (Ids::gate);
    idxSmooth = reg.indexOf (Ids::smooth);
    idxPitch = reg.indexOf (Ids::pitch);
    idxMonitor = reg.indexOf (Ids::monitor);
}

void VoiceControl::process (const ParamSnapshot& s, const float* left, const float* right, int n) noexcept
{
    const bool on = idxOn >= 0 && s.getBool (idxOn);
    monitorOn = idxMonitor >= 0 && s.getBool (idxMonitor);
    Settings st;
    st.gainDb = idxGain >= 0 ? s[idxGain] : 0.0f;
    st.gateDb = idxGate >= 0 ? s[idxGate] : -50.0f;
    st.smoothMs = idxSmooth >= 0 ? s[idxSmooth] : 20.0f;
    st.pitch = idxPitch >= 0 && s.getBool (idxPitch);
    gainLin = std::pow (10.0f, std::clamp (st.gainDb, -60.0f, 40.0f) / 20.0f);
    if (on && ! enabled) analyser.reset(); // a fresh start: no values from long ago
    enabled = on;
    if (! on) return;
    analyser.setSettings (st);
    analyser.process (left, right, n);
}

void VoiceControl::monitor (float* left, float* right, const float* inL, const float* inR, int n) const noexcept
{
    if (! monitorOn || left == nullptr || right == nullptr || (inL == nullptr && inR == nullptr)) return;
    const float* l = inL != nullptr ? inL : inR;
    const float* r = inR != nullptr ? inR : inL;
    for (int i = 0; i < n; ++i)
    {
        left[i] += std::clamp (l[i] * gainLin, -1.0f, 1.0f);
        right[i] += std::clamp (r[i] * gainLin, -1.0f, 1.0f);
    }
}
} // namespace ohpe::voice
