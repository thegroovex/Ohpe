// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineTestKit.h"
#include "engines/WavetableEngine.h"

using namespace ohpe;
using namespace ohpe::test;

namespace
{
constexpr auto type = EngineType::Wavetable;

void cleanSine (ISynthEngine& e)
{
    e.setParam (wavetable::Table, (float) wavetable::Classic);
    e.setParam (wavetable::Position, 0.0f); // frame 0: pure sine
    e.setParam (wavetable::Warp, 0.0f);
    e.setParam (wavetable::Unison, 0.0f);
    e.setParam (wavetable::Tone, 1.0f);
    e.setParam (wavetable::Resonance, 0.0f);
    e.setParam (wavetable::KeyTrack, 0.0f);
}
} // namespace

TEST_CASE ("wavetable: silent when idle") { kit::checkSilentWhenIdle (type); }
TEST_CASE ("wavetable: finite, no denormals, bounded at extreme settings") { kit::checkExtremes (type); }
TEST_CASE ("wavetable: release ends in true silence and frees the voice") { kit::checkReleaseToSilence (type); }
TEST_CASE ("wavetable: 16-voice polyphony") { kit::checkPolyphony (type); }
TEST_CASE ("wavetable: parameters survive a state round trip") { kit::checkStateRoundTrip (type); }

TEST_CASE ("wavetable: pitch accuracy within 2 cents")
{
    for (int note : { 28, 45, 57, 69, 81, 96, 108 })
    {
        WavetableEngine e;
        kit::prepare (e);
        cleanSine (e);
        e.noteOn (note, 1.0f);
        auto out = renderEngine (e, 48000);
        const double ref = dsp::midiToHz ((float) note);
        const double f = measureFrequency (out.l, sampleRate, 4800, 48000);
        CAPTURE (note);
        CAPTURE (f);
        CHECK (std::abs (centsBetween (f, ref)) < 2.0);
    }
}

TEST_CASE ("wavetable: macro ranges, every macro changes the sound")
{
    kit::checkMacros (type, [] (ISynthEngine& e) {
        e.setParam (wavetable::Position, 0.5f);
        e.setParam (wavetable::Warp, 0.3f);
    });
}

TEST_CASE ("wavetable: every table sounds and frames differ")
{
    for (int t = 0; t < wavetable::numTables; ++t)
    {
        auto render = [t] (float pos) {
            WavetableEngine e;
            kit::prepare (e);
            e.setParam (wavetable::Table, (float) t);
            e.setParam (wavetable::Position, pos);
            e.setParam (wavetable::Tone, 1.0f);
            e.setParam (wavetable::Unison, 0.0f);
            e.noteOn (48, 1.0f);
            return renderEngine (e, 12000);
        };
        const auto a = render (0.0f), b = render (1.0f);
        CAPTURE (t);
        CHECK (rms (a.l, 2400) > 0.01f);
        CHECK (rms (b.l, 2400) > 0.01f);
        CHECK (kit::difference (a, b) > 0.05);
    }
}

TEST_CASE ("wavetable: band-limited - no aliasing at high notes, even fully warped")
{
    // bright pulse frame, high notes, warp on: everything outside the harmonics must stay far down
    for (int note : { 84, 96, 103 })
        for (float warp : { 0.0f, 1.0f })
            for (int mode : { 0, 1 })
            {
                WavetableEngine e;
                kit::prepare (e);
                e.setParam (wavetable::Table, (float) wavetable::Classic);
                e.setParam (wavetable::Position, 0.9f);
                e.setParam (wavetable::Warp, warp);
                e.setParam (wavetable::WarpMode, (float) mode);
                e.setParam (wavetable::Unison, 0.0f);
                e.setParam (wavetable::Tone, 1.0f);
                e.setParam (wavetable::Resonance, 0.0f);
                e.noteOn (note, 1.0f);
                auto out = renderEngine (e, 8192 + 16384);
                const double db = kit::inharmonicDb (out.l, sampleRate, dsp::midiToHz ((float) note));
                CAPTURE (note);
                CAPTURE (warp);
                CAPTURE (mode);
                CAPTURE (db);
                CHECK (db < (warp > 0.0f ? -60.0 : -80.0));
            }
}

TEST_CASE ("wavetable: warp brightens a sine")
{
    auto bright = [] (float warp) {
        WavetableEngine e;
        kit::prepare (e);
        cleanSine (e);
        e.setParam (wavetable::Warp, warp);
        e.noteOn (45, 1.0f);
        auto out = renderEngine (e, 24000);
        double s = 0.0;
        for (size_t i = 4801; i < out.l.size(); ++i) s += std::pow (out.l[i] - out.l[i - 1], 2.0);
        return s;
    };
    CHECK (bright (1.0f) > 2.0 * bright (0.0f));
}

TEST_CASE ("wavetable: CPU sanity") { kit::checkCpu (type, [] (ISynthEngine& e) { e.setParam (wavetable::Unison, 1.0f); e.setParam (wavetable::Warp, 0.5f); }); }
