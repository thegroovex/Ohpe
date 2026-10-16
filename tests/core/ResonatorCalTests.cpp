// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p19 TEMPORARY: level calibration printout of the resonator models and presets (removed before merge).
#include "OhpeCore.h"
#include "TestHelpers.h"
#include "fx/Resonator.h"
#include "preset/Preset.h"
#include "render/PresetRender.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ohpe;
using namespace ohpe::test;

TEST_CASE ("p19 CAL: resonator levels")
{
    std::ostringstream o;
    for (int model = 0; model < 4; ++model)
        for (int in = 0; in < 3; ++in)
        {
            ResonatorEffect e;
            e.prepare (48000.0, 512);
            const auto d = e.getParamDescriptors();
            for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
            KeyState k;
            k.lastNote = 57; k.numHeld = 1; k.held[0] = 57;
            e.setKeys (&k);
            e.setParam (ResonatorEffect::Model, (float) model);
            e.setParam (ResonatorEffect::Mix, 1.0f);
            e.reset();
            Stereo s (48000);
            dsp::Random r (3);
            for (size_t i = 0; i < s.size(); ++i)
            {
                float x = 0.0f;
                if (in == 0) x = 0.3f * r.nextBipolar();
                else if (in == 1) { const double ph = std::fmod (i * 220.0 / 48000.0, 1.0); x = 0.5f * (float) (2.0 * ph - 1.0); }
                else x = i % 9600 == 10 ? 1.0f : 0.0f;
                s.l[i] = s.r[i] = x;
            }
            const float inRms = rms (s.l);
            for (size_t p = 0; p < s.size(); p += 256) e.process (s.l.data() + p, s.r.data() + p, 256);
            o << "model " << model << " in " << in << " inRms " << inRms << " outRms " << rms (s.l) << " peak " << peak (s.l) << "\n";
        }
    namespace fs = std::filesystem;
    for (const auto& f : fs::directory_iterator (std::string (OHPE_TEST_DATA_DIR) + "/../../resources/presets/factory"))
    {
        std::ifstream in (f.path());
        std::stringstream ss;
        ss << in.rdbuf();
        if (ss.str().find ("\"reso.on\"") == std::string::npos) continue;
        preset::Preset p;
        REQUIRE (preset::fromJson (ss.str(), p));
        for (int w = 0; w < 2; ++w)
        {
            Core core;
            core.prepare (48000.0, 256);
            const auto a = w == 0 ? render::renderPart (core, p, 0, 100.0, 2.0, 0.4) : render::renderPart (core, p, 0, 100.0, 4.0, 1.0);
            const auto m = render::measure (a);
            o << "preset " << p.meta.name << " win " << w << " active " << m.activeRmsDb << " peak " << m.peakDb << " hf " << m.hfDb << "\n";
        }
    }
    MESSAGE (o.str());
    FAIL_CHECK ("calibration printout");
}
