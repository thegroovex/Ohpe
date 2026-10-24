// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p86: the audio input reaches Voice Control in the standalone app - a mono device (one channel), blocks longer than
// prepared, and the Voice view tells "no input chosen", "microphone access denied" and "input open, no signal" apart.

#include <doctest/doctest.h>

#include "plugin/MicAccess.h"
#include "plugin/PluginProcessor.h"
#include "standalone/AudioSettingsPage.h"
#include "ui/VoiceView.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <memory>
#include <vector>

using namespace ohpe;

namespace
{
/** A device as the standalone player sees it: `ins` input and `outs` output channels, already open. */
class MockDevice final : public juce::AudioIODevice
{
public:
    MockDevice (int ins, int outs, int block) : juce::AudioIODevice ("Mock", "Mock"), numIns (ins), numOuts (outs), blockSize (block) {}
    juce::StringArray getOutputChannelNames() override { juce::StringArray s; for (int i = 0; i < numOuts; ++i) s.add ("Out " + juce::String (i + 1)); return s; }
    juce::StringArray getInputChannelNames() override { juce::StringArray s; for (int i = 0; i < numIns; ++i) s.add ("In " + juce::String (i + 1)); return s; }
    juce::Array<double> getAvailableSampleRates() override { return { 48000.0 }; }
    juce::Array<int> getAvailableBufferSizes() override { return { blockSize }; }
    int getDefaultBufferSize() override { return blockSize; }
    juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
    void close() override {}
    bool isOpen() override { return true; }
    void start (juce::AudioIODeviceCallback*) override {}
    void stop() override {}
    bool isPlaying() override { return true; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return blockSize; }
    double getCurrentSampleRate() override { return 48000.0; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { juce::BigInteger b; b.setRange (0, numOuts, true); return b; }
    juce::BigInteger getActiveInputChannels() const override { juce::BigInteger b; b.setRange (0, numIns, true); return b; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }

private:
    int numIns, numOuts, blockSize;
};

void voiceOn (OhpeProcessor& p, bool on)
{
    if (auto* prm = p.parameter (juce::String (voice::Ids::on))) prm->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

/** The app's start: the standalone bus layout, then what the standalone holder does (non-main buses off). */
void standaloneLayout (OhpeProcessor& p)
{
    p.setChannelLayoutOfBus (true, 0, juce::AudioChannelSet::stereo());
    p.disableNonMainBuses();
}

/** Runs `blocks` device callbacks with a 220 Hz sine (amplitude `amp`) on every device input. */
void runDevice (juce::AudioProcessorPlayer& player, int ins, int outs, int block, int blocks, float amp, double& phase)
{
    std::vector<std::vector<float>> in ((size_t) ins, std::vector<float> ((size_t) block)), out ((size_t) outs, std::vector<float> ((size_t) block));
    std::vector<const float*> inPtr;
    std::vector<float*> outPtr;
    for (auto& c : in) inPtr.push_back (c.data());
    for (auto& c : out) outPtr.push_back (c.data());
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float v = amp * (float) std::sin (phase);
            phase += 2.0 * 3.14159265358979 * 220.0 / 48000.0;
            for (auto& c : in) c[(size_t) i] = v;
        }
        player.audioDeviceIOCallbackWithContext (inPtr.data(), ins, outPtr.data(), outs, block, {});
    }
}

/** Runs `blocks` processBlock calls of `n` samples with a sine on input channels 0 / 1 (when the bus has them). */
void runBlocks (OhpeProcessor& p, int n, int blocks, float amp, double& phase)
{
    const int chans = juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buf (chans, n);
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        for (int i = 0; i < n; ++i)
        {
            const float v = amp * (float) std::sin (phase);
            phase += 2.0 * 3.14159265358979 * 220.0 / 48000.0;
            for (int c = 0; c < p.getTotalNumInputChannels(); ++c) buf.setSample (c, i, v);
        }
        juce::MidiBuffer midi;
        p.processBlock (buf, midi);
    }
}

struct MicOverride
{
    explicit MicOverride (mic::Access a) { mic::setAccessForTesting (a); }
    ~MicOverride() { mic::resetAccessForTesting(); }
};
} // namespace

TEST_CASE ("p86: the standalone app starts with the input bus on, hosts with it off")
{
    const auto app = OhpeProcessor::makeBuses (true);
    const auto host = OhpeProcessor::makeBuses (false);
    REQUIRE (app.inputLayouts.size() == 1);
    REQUIRE (host.inputLayouts.size() == 1);
    CHECK (app.inputLayouts.getReference (0).isActivatedByDefault);
    CHECK_FALSE (host.inputLayouts.getReference (0).isActivatedByDefault);
    CHECK (app.inputLayouts.getReference (0).defaultLayout == juce::AudioChannelSet::stereo());
}

TEST_CASE ("p86: a mono input device reaches Voice Control through the standalone player (both sides)")
{
    OhpeProcessor p;
    standaloneLayout (p);
    MockDevice dev (1, 2, 512); // a headset microphone: one channel; the built-in speakers: two
    juce::AudioProcessorPlayer player;
    player.setProcessor (&p);
    player.audioDeviceAboutToStart (&dev);
    CHECK (p.getTotalNumInputChannels() == 2); // the player keeps the stereo input (and copies the one channel)
    voiceOn (p, true);
    double ph = 0.0;
    runDevice (player, 1, 2, 512, 40, 0.3f, ph);
    CHECK (p.core().inputConnected());
    const float db = p.core().voiceControl().analysis().inputDb();
    CHECK (db > -30.0f);
    CHECK (std::isfinite (db));
    {
        MicOverride granted (mic::Access::granted);
        CHECK (ui::voiceview::inputState (p.core()) == ui::voiceview::InputState::signal);
        CHECK (ui::voiceview::statusLine (p.core()).contains ("dB"));
    }
    player.audioDeviceStopped();
    player.setProcessor (nullptr);
}

TEST_CASE ("p86: before the fix - an input bus that starts off hears nothing through the player")
{
    OhpeProcessor p; // tests are no standalone app: the input bus starts off, as it did in the app before p86
    p.disableNonMainBuses();
    MockDevice dev (1, 2, 512);
    juce::AudioProcessorPlayer player;
    player.setProcessor (&p);
    player.audioDeviceAboutToStart (&dev);
    voiceOn (p, true);
    double ph = 0.0;
    runDevice (player, 1, 2, 512, 10, 0.3f, ph);
    CHECK_FALSE (p.core().inputConnected());
    MicOverride granted (mic::Access::granted);
    CHECK (ui::voiceview::inputState (p.core()) == ui::voiceview::InputState::noDevice);
    player.audioDeviceStopped();
    player.setProcessor (nullptr);
}

TEST_CASE ("p86: a stereo device and blocks longer than prepared keep their input")
{
    OhpeProcessor p;
    standaloneLayout (p);
    p.prepareToPlay (48000.0, 512);
    voiceOn (p, true);
    double ph = 0.0;
    runBlocks (p, 2048, 12, 0.3f, ph); // four times the prepared block (separate input / output devices)
    CHECK (p.core().inputConnected());
    CHECK (p.core().voiceControl().analysis().inputDb() > -30.0f);
    runBlocks (p, 512, 4, 0.3f, ph);
    CHECK (p.core().voiceControl().analysis().inputDb() > -30.0f);
}

TEST_CASE ("p86: the Voice view tells the input states apart (and offers the microphone key)")
{
    using ui::voiceview::InputState;
    OhpeProcessor p;
    standaloneLayout (p);
    p.prepareToPlay (48000.0, 512);
    double ph = 0.0;
    MicOverride granted (mic::Access::granted);

    runBlocks (p, 512, 2, 0.0f, ph);
    CHECK (ui::voiceview::inputState (p.core()) == InputState::off);
    CHECK (ui::voiceview::statusLine (p.core()) == "Off");

    voiceOn (p, true);
    runBlocks (p, 512, 8, 0.0f, ph); // open, digital silence
    CHECK (ui::voiceview::inputState (p.core()) == InputState::noSignal);
    CHECK (ui::voiceview::statusLine (p.core()) == "Input open - no signal (check level / cable)");
    CHECK (ui::voiceview::micKey (p.core()) == 0);

    {
        MicOverride denied (mic::Access::denied); // the system refuses: every input is silent
        CHECK (ui::voiceview::inputState (p.core()) == InputState::micBlocked);
        CHECK (ui::voiceview::statusLine (p.core()) == "Microphone access denied - allow it in the system settings");
        CHECK (ui::voiceview::micKey (p.core()) == 1);
    }
    {
        MicOverride restricted (mic::Access::restricted);
        CHECK (ui::voiceview::micKey (p.core()) == 1);
    }
    {
        MicOverride ask (mic::Access::notDetermined);
        CHECK (ui::voiceview::inputState (p.core()) == InputState::micAsk);
        CHECK (ui::voiceview::micKey (p.core()) == 2);
    }

    runBlocks (p, 512, 30, 0.3f, ph);
    CHECK (ui::voiceview::inputState (p.core()) == InputState::signal);
    const auto line = ui::voiceview::statusLine (p.core());
    CHECK ((line.startsWith ("Listening") || line.startsWith ("Vowel")));
    CHECK (line.contains (" dB"));

    // no input device: the bus delivers no channels
    p.setChannelLayoutOfBus (true, 0, juce::AudioChannelSet::disabled());
    runBlocks (p, 512, 2, 0.0f, ph);
    CHECK_FALSE (p.core().inputConnected());
    CHECK (ui::voiceview::inputState (p.core()) == InputState::noDevice);
    CHECK (ui::voiceview::statusLine (p.core()) == "No input device chosen - choose one in the audio settings or the host");
}

TEST_CASE ("p86: the input channels the settings page opens")
{
    auto bits = [] (const juce::BigInteger& b) { return b.toString (2); };
    CHECK (bits (AudioSettingsPage::inputChannelsFor (1)) == "1");  // mono: channel 1 only
    CHECK (bits (AudioSettingsPage::inputChannelsFor (2)) == "11");
    CHECK (bits (AudioSettingsPage::inputChannelsFor (8)) == "11"); // the first two of a larger interface
    CHECK (bits (AudioSettingsPage::inputChannelsFor (0)) == "11"); // unknown: the driver opens what exists
}

TEST_CASE ("p86: the core takes a right-only input on both sides and reports the connection")
{
    auto owned = std::make_unique<Core>();
    auto& core = *owned;
    core.prepare (48000.0, 256);
    std::vector<float> l (4096), r (4096), in (4096);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.25f * (float) std::sin (0.05 * (double) i);
    OutputBuffers out;
    out.left[0] = l.data();
    out.right[0] = r.data();
    Transport tr;
    core.setAudioInput (nullptr, nullptr);
    core.process (out, 256, nullptr, 0, tr);
    CHECK_FALSE (core.inputConnected());
    core.setAudioInput (nullptr, in.data());
    core.process (out, 256, nullptr, 0, tr);
    CHECK (core.inputConnected());
    core.setAudioInput (in.data(), nullptr);
    core.process (out, 4096, nullptr, 0, tr); // longer than prepared (up to Core::maxInputBlock)
    CHECK (core.inputConnected());
    for (size_t i = 0; i < l.size(); ++i) { REQUIRE (std::isfinite (l[i])); REQUIRE (std::isfinite (r[i])); }
}
