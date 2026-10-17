// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Character.h"

namespace ohpe::character
{
const char* key (int m) noexcept
{
    static const char* k[numMacros] = { "bright", "hard", "alive", "wide" };
    return m >= 0 && m < numMacros ? k[m] : "";
}

const char* nameEn (int m) noexcept
{
    static const char* k[numMacros] = { "Bright", "Hard", "Alive", "Wide" };
    return m >= 0 && m < numMacros ? k[m] : "";
}

const char* nameDe (int m) noexcept
{
    static const char* k[numMacros] = { "Hell", "Hart", "Lebendig", "Weit" };
    return m >= 0 && m < numMacros ? k[m] : "";
}

Weights engineWeights (const std::string& id) noexcept
{
    Weights r;
    auto set = [&r] (float bright, float hard, float alive, float wide) { r.w = { bright, hard, alive, wide }; };
    // brightness
    if (id == "cutoff") set (0.30f, 0.05f, 0.0f, 0.0f);
    else if (id == "tone") set (0.28f, 0.04f, 0.0f, 0.0f);
    else if (id == "bright") set (0.30f, 0.06f, 0.0f, 0.0f);
    else if (id == "index") set (0.18f, 0.08f, 0.0f, 0.0f);
    else if (id == "damping") set (-0.25f, -0.05f, 0.0f, 0.0f);
    else if (id == "formant") set (0.15f, 0.0f, 0.0f, 0.0f);
    else if (id == "reso") set (0.06f, 0.06f, 0.0f, 0.0f);
    else if (id == "warp") set (0.10f, 0.04f, 0.0f, 0.0f);
    // hardness
    else if (id == "drive") set (0.0f, 0.35f, 0.0f, 0.0f);
    else if (id == "feedback") set (0.04f, 0.22f, 0.0f, 0.0f);
    else if (id == "excite") set (0.05f, 0.25f, 0.0f, 0.0f);
    else if (id == "crackle") set (0.0f, 0.15f, 0.05f, 0.0f);
    else if (id == "tension") set (0.05f, 0.25f, 0.0f, 0.0f);
    else if (id == "noise") set (0.03f, 0.08f, 0.0f, 0.0f);
    // life
    else if (id == "drift") set (0.0f, 0.0f, 0.35f, 0.0f);
    else if (id == "detune") set (0.0f, 0.0f, 0.12f, 0.05f);
    else if (id == "jitter") set (0.0f, 0.0f, 0.30f, 0.0f);
    else if (id == "vibdepth") set (0.0f, 0.0f, 0.15f, 0.0f);
    else if (id == "spray") set (0.0f, 0.0f, 0.20f, 0.05f);
    else if (id == "scan") set (0.0f, 0.0f, 0.20f, 0.0f);
    // width
    else if (id == "spread") set (0.0f, 0.0f, 0.0f, 0.40f);
    else if (id == "uniwidth") set (0.0f, 0.0f, 0.0f, 0.40f);
    else if (id == "side") set (0.0f, 0.0f, 0.0f, 0.30f);
    else if (id == "unison") set (0.0f, 0.0f, 0.05f, 0.20f);
    return r;
}
} // namespace ohpe::character
