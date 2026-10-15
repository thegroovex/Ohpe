// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Host test: loads the built Ohpe.vst3 through JUCE's VST3 hosting code, like a DAW,
// checks it is an instrument with multi-out buses, renders a note and restores its state.
//   usage: ohpe_hosttest <path to Ohpe.vst3>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  [ OK ] " : "  [FAIL] ") << what << std::endl;
    if (! condition) ++failures;
}

float render (juce::AudioPluginInstance& p, int blocks, bool note)
{
    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (note && b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 110), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        peak = juce::jmax (peak, buffer.getMagnitude (0, 0, 512));
    }
    return peak;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::cerr << "usage: ohpe_hosttest <Ohpe.vst3>" << std::endl; return 2; }

    const juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : formats.getFormats()) f->findAllTypesForFile (types, file.getFullPathName());
    check (types.size() == 1, "plug-in found in the bundle");
    if (types.isEmpty()) return 1;
    const auto& desc = *types[0];
    check (desc.name == "Ohpe", "name is Ohpe (" + desc.name + ")");
    check (desc.isInstrument, "registered as an instrument");

    juce::String error;
    auto plugin = formats.createPluginInstance (desc, 48000.0, 512, error);
    check (plugin != nullptr, "instance created " + error);
    if (plugin == nullptr) return 1;
    check (plugin->acceptsMidi(), "accepts MIDI");
    check (plugin->getBusCount (false) == 9, "9 output buses (main + 8 aux): " + juce::String (plugin->getBusCount (false)));

    int params = 0;
    for (auto* p : plugin->getParameters())
        if (! p->getName (64).startsWith ("MIDI CC") && p->getName (64) != "Bypass") ++params;
    check (params > 150, juce::String (params) + " parameters exposed");

    plugin->enableAllBuses();
    plugin->prepareToPlay (48000.0, 512);
    check (render (*plugin, 4, false) == 0.0f, "silent while idle");
    check (render (*plugin, 20, true) > 0.01f, "a note plays");

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    check (state.getSize() > 1000, "state saved (" + juce::String ((int) state.getSize()) + " bytes)");
    auto second = formats.createPluginInstance (desc, 48000.0, 512, error);
    if (second != nullptr)
    {
        second->setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock again;
        second->getStateInformation (again);
        check (again.getSize() == state.getSize(), "state restored in a second instance");
        if (auto* dump = std::getenv ("OHPE_HOSTTEST_DUMP"))
        {
            juce::File (juce::String (dump) + ".1.json").replaceWithData (state.getData(), state.getSize());
            juce::File (juce::String (dump) + ".2.json").replaceWithData (again.getData(), again.getSize());
        }
    }
    plugin->releaseResources();
    std::cout << (failures == 0 ? "ALL HOST CHECKS PASSED" : "HOST CHECKS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
