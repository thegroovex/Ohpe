// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleTestHelpers.h"
#include "engines/VocalEngine.h"

#include <doctest/doctest.h>

using namespace ohpe;
using namespace ohpe::test;

namespace
{
struct VocalRig
{
    VocalEngine e;
    VocalRig()
    {
        e.prepare (sampleRate, 512);
        e.setCommon (plainCommon());
        const auto d = e.getParamDescriptors();
        for (size_t i = 0; i < d.size(); ++i) e.setParam ((int) i, d[i].defaultValue);
        e.setParam (vocal::VibratoDepth, 0.0f);
        e.setParam (vocal::Jitter, 0.0f);
        e.setParam (vocal::Breath, 0.0f);
    }
    Stereo play (int note, size_t n = 24000)
    {
        e.allNotesOff (true);
        e.noteOn (note, 1.0f);
        return renderEngine (e, n);
    }
};
} // namespace

TEST_CASE ("vocal: silence when idle")
{
    VocalRig rig;
    CHECK (peak (renderEngine (rig.e, 4800).l) == 0.0f);
}

TEST_CASE ("vocal: pitch accuracy of the glottal source")
{
    VocalRig rig;
    for (int note : { 45, 57, 64, 69 })
        for (float vowel : { 0.0f, 2.0f, 4.0f })
        {
            CAPTURE (note);
            CAPTURE (vowel);
            rig.e.setParam (vocal::Vowel, vowel);
            auto out = rig.play (note);
            const double f = measurePitchAutocorr (out.l, sampleRate, 4800, 24000);
            CHECK (std::abs (centsBetween (f, dsp::midiToHz ((float) note))) < 10.0);
        }
}

TEST_CASE ("vocal: vowel table and continuous morph")
{
    const auto i = vocal::morphVowel (2.0f);
    for (int k = 0; k < vocal::numFormants; ++k) CHECK (i.freq[k] == doctest::Approx (vocal::vowelFormants (2).freq[k]));
    const auto ae = vocal::morphVowel (0.5f);
    CHECK (ae.freq[1] > vocal::vowelFormants (0).freq[1]);
    CHECK (ae.freq[1] < vocal::vowelFormants (1).freq[1]);
    CHECK (vocal::morphVowel (-3.0f).freq[0] == doctest::Approx (vocal::vowelFormants (0).freq[0]));
    CHECK (vocal::morphVowel (9.0f).freq[0] == doctest::Approx (vocal::vowelFormants (4).freq[0]));
}

TEST_CASE ("vocal: the vowel moves the formants in the output")
{
    VocalRig rig;
    // note 43 = 98 Hz: harmonics close to F1 of A (600 Hz) and of I (250 Hz)
    const double f0 = dsp::midiToHz (43.0f);
    auto bandRatio = [&] (float vowel) {
        rig.e.setParam (vocal::Vowel, vowel);
        auto out = rig.play (43);
        const double nearA = toneEnergy (out.l, sampleRate, 4800, 24000, f0 * std::round (600.0 / f0));
        const double nearI = toneEnergy (out.l, sampleRate, 4800, 24000, f0 * std::round (250.0 / f0));
        return nearA / nearI;
    };
    const double a = bandRatio (0.0f), iVowel = bandRatio (2.0f);
    CHECK (a > 4.0 * iVowel);
}

TEST_CASE ("vocal: breath adds noise, tension changes the brightness")
{
    VocalRig rig;
    const int lag = (int) std::round (sampleRate / dsp::midiToHz (57.0f));
    auto clean = rig.play (57);
    rig.e.setParam (vocal::Breath, 1.0f);
    auto breathy = rig.play (57);
    CHECK (autocorrPeak (breathy.l, 4800, 24000, lag) < autocorrPeak (clean.l, 4800, 24000, lag) - 0.05);

    rig.e.setParam (vocal::Breath, 0.0f);
    auto hf = [&] (float tension) {
        rig.e.setParam (vocal::Tension, tension);
        auto out = rig.play (57);
        double d = 0.0, e = 0.0;
        for (size_t i = 4801; i < 24000; ++i) { d += std::pow (out.l[i] - out.l[i - 1], 2.0); e += std::pow (out.l[i], 2.0); }
        return d / e; // high-frequency share
    };
    CHECK (hf (1.0f) > hf (0.0f));
}

TEST_CASE ("vocal: finite, no denormals at extremes, release to silence")
{
    VocalRig rig;
    for (float shift : { -12.0f, 12.0f })
    {
        rig.e.setParam (vocal::Formant, shift);
        rig.e.setParam (vocal::Breath, 1.0f);
        rig.e.setParam (vocal::Tension, 1.0f);
        rig.e.setParam (vocal::VibratoDepth, 1.0f);
        rig.e.setParam (vocal::VibratoRate, 10.0f);
        rig.e.setParam (vocal::Jitter, 1.0f);
        rig.e.setParam (vocal::Vowel, 4.0f);
        for (int n : { 0, 30, 60, 100, 127 }) rig.e.noteOn (n, 1.0f);
        auto out = renderEngine (rig.e, 24000, 77);
        CHECK (allFinite (out.l));
        CHECK (noDenormals (out.l));
        CHECK (peak (out.l) < 4.0f);
        CHECK (rms (out.l) > 0.01f);
        rig.e.allNotesOff (false);
        renderEngine (rig.e, 48000);
        CHECK (rig.e.activeVoiceCount() == 0);
        CHECK (peak (renderEngine (rig.e, 1024).l) == 0.0f);
    }
}
