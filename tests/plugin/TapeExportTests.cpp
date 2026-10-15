// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// 0.2 plug-in layer: audio input bus -> tape, tape audio in the plug-in state, offline bounce
// (master + stems, 24-bit WAV).

#include <doctest/doctest.h>

#include "plugin/PluginProcessor.h"
#include "plugin/TapeIO.h"

using namespace ohpe;

namespace
{
void setParam (OhpeProcessor& p, const char* id, float plain)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

float sineAt (int i) { return 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 375.0f * (float) i / 48000.0f); }

/** Records `blocks` blocks of a sine from the input bus onto tape track 1. */
void recordFromInput (OhpeProcessor& p, int blocks)
{
    REQUIRE (p.enableAllBuses());
    REQUIRE (p.getBus (true, 0)->isEnabled());
    p.prepareToPlay (48000.0, 512);
    setParam (p, "tape.tr1.arm", 1.0f);
    setParam (p, "tape.tr1.src", (float) tapesource::input);
    setParam (p, "tape.play", 1.0f);
    setParam (p, "tape.rec", 1.0f);
    juce::AudioBuffer<float> buffer (juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), 512);
    juce::MidiBuffer midi;
    int t = 0;
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        auto in = p.getBusBuffer (buffer, true, 0);
        for (int i = 0; i < 512; ++i, ++t)
        {
            in.setSample (0, i, sineAt (t));
            in.setSample (1, i, -sineAt (t));
        }
        p.processBlock (buffer, midi);
    }
    setParam (p, "tape.rec", 0.0f);
    setParam (p, "tape.play", 0.0f);
    setParam (p, "tape.tr1.arm", 0.0f);
    buffer.clear();
    p.processBlock (buffer, midi);
    p.core().tape().housekeeping(); // the timer does this at 20 Hz
}

std::unique_ptr<juce::AudioFormatReader> openWav (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    return std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f));
}
} // namespace

TEST_CASE ("processor 0.2: input bus layouts")
{
    OhpeProcessor p;
    auto layout = p.getBusesLayout();
    REQUIRE (layout.inputBuses.size() == 1);
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    CHECK (p.checkBusesLayoutSupported (layout));
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK (p.checkBusesLayoutSupported (layout));
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::create5point1();
    CHECK_FALSE (p.checkBusesLayoutSupported (layout));
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::disabled();
    CHECK (p.checkBusesLayoutSupported (layout));
}

TEST_CASE ("processor 0.2: the input bus records onto tape sample-exact")
{
    OhpeProcessor p;
    recordFromInput (p, 10);
    auto& tape = p.core().tape();
    CHECK (tape.undoSteps() == 1);
    CHECK (tape.contentEndFrames (0) == 10 * 512);
    std::vector<float> l (5120), r (5120);
    tape.readCommitted (0, 0, 5120, l.data(), r.data());
    for (int i = 0; i < 5120; ++i)
    {
        REQUIRE (l[(size_t) i] == sineAt (i));
        REQUIRE (r[(size_t) i] == -sineAt (i));
    }
}

TEST_CASE ("processor 0.2: tape audio survives get/setStateInformation; JSON-only states clear the tape")
{
    OhpeProcessor a;
    recordFromInput (a, 12);
    juce::MemoryBlock block;
    a.getStateInformation (block);
    CHECK (block.getSize() > 12 * 512 * 8);

    OhpeProcessor b;
    b.setStateInformation (block.getData(), (int) block.getSize());
    REQUIRE (b.core().tape().contentEndFrames (0) == 12 * 512);
    std::vector<float> la (6144), ra (6144), lb (6144), rb (6144);
    a.core().tape().readCommitted (0, 0, 6144, la.data(), ra.data());
    b.core().tape().readCommitted (0, 0, 6144, lb.data(), rb.data());
    CHECK (la == lb);
    CHECK (ra == rb);
    CHECK (b.parameter ("tape.tr1.src")->getValue() == doctest::Approx (a.parameter ("tape.tr1.src")->getValue()));

    // a state saved again is identical (deterministic)
    juce::MemoryBlock again;
    b.getStateInformation (again);
    CHECK (again == block);

    // a 0.1-style state (JSON only) leaves an empty tape
    const auto json = b.core().saveState();
    b.setStateInformation (json.data(), (int) json.size());
    CHECK (b.core().tape().contentEndFrames() == 0);
}

TEST_CASE ("bounce: master + 8 stems as 24-bit WAV with the right content")
{
    OhpeProcessor p;
    recordFromInput (p, 40); // ~0.43 s; mixer channel 6 plays tape track 1 by default
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ohpe-bounce-test");
    dir.deleteRecursively();

    tapeio::BounceOptions opt;
    opt.folder = dir;
    opt.tailSeconds = 0.5;
    const auto res = tapeio::bounce (p.core(), opt);
    REQUIRE_MESSAGE (res.ok, res.error);
    CHECK (res.files.size() == 1 + TapeMachine::numTracks);
    const juce::int64 expectFrames = 40 * 512 + 24000;
    CHECK (res.frames == expectFrames);

    auto master = openWav (dir.getChildFile ("Ohpe Master.wav"));
    auto stem1 = openWav (dir.getChildFile ("Ohpe Track 1.wav"));
    auto stem2 = openWav (dir.getChildFile ("Ohpe Track 2.wav"));
    REQUIRE (master != nullptr);
    REQUIRE (stem1 != nullptr);
    REQUIRE (stem2 != nullptr);
    for (auto* r : { master.get(), stem1.get(), stem2.get() })
    {
        CHECK (r->sampleRate == 48000.0);
        CHECK (r->bitsPerSample == 24);
        CHECK (r->numChannels == 2);
        CHECK (r->lengthInSamples == expectFrames);
    }
    juce::AudioBuffer<float> s1 (2, (int) expectFrames), s2 (2, (int) expectFrames), m (2, (int) expectFrames);
    stem1->read (&s1, 0, (int) expectFrames, 0, true, true);
    stem2->read (&s2, 0, (int) expectFrames, 0, true, true);
    master->read (&m, 0, (int) expectFrames, 0, true, true);

    // stem 1 = the recording (24-bit quantisation), silence after it; stem 2 silent
    float maxErr = 0.0f;
    for (int i = 0; i < 40 * 512; ++i)
    {
        maxErr = juce::jmax (maxErr, std::abs (s1.getSample (0, i) - sineAt (i)));
        maxErr = juce::jmax (maxErr, std::abs (s1.getSample (1, i) + sineAt (i)));
    }
    CHECK (maxErr < 1.0e-6f);
    CHECK (s1.getMagnitude (40 * 512, 24000) == 0.0f);
    CHECK (s2.getMagnitude (0, (int) expectFrames) == 0.0f);

    // master: channel 6 at unity (after the fader smoothing has settled)
    float maxMasterErr = 0.0f;
    for (int i = 9600; i < 40 * 512; ++i)
        maxMasterErr = juce::jmax (maxMasterErr, std::abs (m.getSample (0, i) - sineAt (i)));
    CHECK (maxMasterErr < 1.0e-3f);
    CHECK (m.getMagnitude (0, 0, 40 * 512) > 0.45f);

    // the live instrument was not touched
    CHECK (p.core().tape().positionFrames() > 0);
    CHECK (p.core().params().get ("tape.play") == 0.0f);
    dir.deleteRecursively();

    // empty tape: a clear error
    OhpeProcessor empty;
    opt.folder = dir;
    const auto none = tapeio::bounce (empty.core(), opt);
    CHECK_FALSE (none.ok);
    CHECK (none.error.isNotEmpty());
    CHECK_FALSE (dir.getChildFile ("Ohpe Master.wav").exists());
}
