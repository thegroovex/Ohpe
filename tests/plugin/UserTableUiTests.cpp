// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p20: audio to wavetable in the plug-in - importing a sound and a single-cycle set (worker thread), the library
// file, the "usertables" state (path, embedded copy, missing file falls back), the soft keys on the Wavetable page.

#include <doctest/doctest.h>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "wavetable/TableFile.h"

using namespace ohpe;

namespace
{
constexpr double pi = 3.14159265358979323846;

bool writeWav (const juce::File& f, const std::vector<float>& data, double rate)
{
    f.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out (f.createOutputStream());
    if (out == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (out.get(), rate, 1, 24, {}, 0));
    if (w == nullptr) return false;
    out.release();
    juce::AudioBuffer<float> b (1, (int) data.size());
    b.copyFrom (0, 0, data.data(), (int) data.size());
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

std::vector<float> sawSound (double hz, double seconds, double sr)
{
    std::vector<float> v ((size_t) (seconds * sr), 0.0f);
    for (int h = 1; h < (int) (0.5 * sr / hz); ++h)
        for (size_t i = 0; i < v.size(); ++i) v[i] += (float) (0.3 / h * std::sin (2.0 * pi * hz * h * (double) i / sr));
    return v;
}

/** Waits for the worker, then publishes (the timer's work). */
bool waitForImport (OhpeProcessor& p)
{
    for (int i = 0; i < 1500 && p.userTables().status().busy; ++i) juce::Thread::sleep (20);
    p.userTables().timerTick();
    return ! p.userTables().status().busy;
}

float hostPlain (OhpeProcessor& p, const juce::String& id)
{
    auto* prm = p.parameter (id);
    REQUIRE (prm != nullptr);
    return prm->convertFrom0to1 (prm->getValue());
}

bool renderFinite (OhpeProcessor& p, int blocks, float& peak)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    bool ok = true;
    peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
            {
                const float v = buffer.getSample (c, i);
                ok &= std::isfinite (v);
                peak = juce::jmax (peak, std::abs (v));
            }
    }
    return ok;
}
} // namespace

TEST_CASE ("plug-in p20: import a sound into the Wavetable engine, save, restore, missing file falls back")
{
    const auto dir = juce::File::createTempFile ("ohpe-wt");
    dir.createDirectory();
    const auto lib = dir.getChildFile ("Library");
    const auto wavFile = dir.getChildFile ("Saw Pad.wav");
    REQUIRE (writeWav (wavFile, sawSound (110.0, 1.0, 44100.0), 44100.0));

    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.userTables().setLibraryFolder (lib);
    p.userTables().frameCount = 32;
    p.userTables().smoothing = 0.25f;
    REQUIRE (p.userTables().importFile (wavFile, 0, usertable::Target::Wavetable));
    CHECK_FALSE (p.userTables().importFile (wavFile, 0, usertable::Target::Wavetable)); // one import at a time
    REQUIRE (waitForImport (p));
    const auto t = usertable::tableOf (p.core(), 0, usertable::Target::Wavetable);
    REQUIRE (t != nullptr);
    CHECK (t->numFrames() == 32);
    CHECK (t->name() == "Saw Pad");
    CHECK (hostPlain (p, "p1.usertable.wt") > 0.5f); // switched on by the import
    CHECK (p.userTables().lastMessage() == "Wavetable ready");
    // saved into the library
    const auto files = p.userTables().libraryFiles();
    REQUIRE (files.size() == 1);
    CHECK (juce::String (t->path) == files[0].getFullPathName());

    // plays (engine Wavetable)
    p.parameter ("p1.engine")->setValueNotifyingHost (p.parameter ("p1.engine")->convertTo0to1 ((float) EngineType::Wavetable));
    float pk = 0.0f;
    CHECK (renderFinite (p, 80, pk));
    CHECK (pk > 0.001f);

    juce::MemoryBlock state;
    p.getStateInformation (state);

    // 1. the file is there: found by its path
    {
        OhpeProcessor q;
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        const auto back = usertable::tableOf (q.core(), 0, usertable::Target::Wavetable);
        REQUIRE (back != nullptr);
        CHECK (back->numFrames() == 32);
        CHECK (juce::String (back->path) == files[0].getFullPathName());
    }
    // 2. the file is gone: the embedded copy (32 frames <= 64) plays
    files[0].deleteFile();
    {
        OhpeProcessor q;
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        const auto back = usertable::tableOf (q.core(), 0, usertable::Target::Wavetable);
        REQUIRE (back != nullptr);
        CHECK (back->numFrames() == 32);
    }
    // 3. a big table (not embedded) whose file is gone: the built-in table plays, nothing breaks
    {
        p.userTables().frameCount = 128;
        REQUIRE (p.userTables().importFile (wavFile, 0, usertable::Target::Wavetable));
        REQUIRE (waitForImport (p));
        REQUIRE (usertable::tableOf (p.core(), 0, usertable::Target::Wavetable)->numFrames() == 128);
        juce::MemoryBlock big;
        p.getStateInformation (big);
        for (const auto& f : p.userTables().libraryFiles()) f.deleteFile();
        OhpeProcessor q;
        q.prepareToPlay (48000.0, 256);
        const int before = q.userTables().messageCount();
        q.setStateInformation (big.getData(), (int) big.getSize());
        CHECK (usertable::tableOf (q.core(), 0, usertable::Target::Wavetable) == nullptr);
        CHECK (q.userTables().messageCount() > before);
        CHECK (q.userTables().lastMessage() == "A user wavetable was not found - the built-in table plays");
        CHECK (hostPlain (q, "p1.usertable.wt") > 0.5f); // still asked for: plays the built-in table
        CHECK (renderFinite (q, 80, pk));
        CHECK (pk > 0.001f);
    }
    dir.deleteRecursively();
}

TEST_CASE ("plug-in p20: single-cycle sets, Ohpe tables, formants for the Vocal engine, the soft keys")
{
    const auto dir = juce::File::createTempFile ("ohpe-wt");
    dir.createDirectory();
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.userTables().setLibraryFolder (dir.getChildFile ("Library"));

    // a set of 6 frames x 2048 samples
    std::vector<float> set (6 * 2048);
    for (int f = 0; f < 6; ++f)
        for (int i = 0; i < 2048; ++i)
            set[(size_t) (f * 2048 + i)] = (float) (0.8 * std::sin (2.0 * pi * i / 2048.0) + 0.1 * f * std::sin (6.0 * pi * i / 2048.0));
    const auto setFile = dir.getChildFile ("Six.wav");
    REQUIRE (writeWav (setFile, set, 48000.0));
    REQUIRE (p.userTables().importFile (setFile, 1, usertable::Target::Wavetable));
    REQUIRE (waitForImport (p));
    const auto t = usertable::tableOf (p.core(), 1, usertable::Target::Wavetable);
    REQUIRE (t != nullptr);
    CHECK (t->numFrames() == 6);

    // the saved .ohpewt loads as it is
    const auto saved = juce::File (juce::String (t->path));
    REQUIRE (saved.existsAsFile());
    REQUIRE (p.userTables().importFile (saved, 2, usertable::Target::Wavetable));
    REQUIRE (waitForImport (p));
    REQUIRE (usertable::tableOf (p.core(), 2, usertable::Target::Wavetable) != nullptr);
    CHECK (usertable::tableOf (p.core(), 2, usertable::Target::Wavetable)->numFrames() == 6);

    // a sound into the Vocal engine: formant tracks
    const auto voice = dir.getChildFile ("Voice.wav");
    REQUIRE (writeWav (voice, sawSound (140.0, 0.8, 48000.0), 48000.0));
    REQUIRE (p.userTables().importFile (voice, 0, usertable::Target::Vocal));
    REQUIRE (waitForImport (p));
    const auto v = usertable::tableOf (p.core(), 0, usertable::Target::Vocal);
    REQUIRE (v != nullptr);
    CHECK (v->hasFormants());
    CHECK (hostPlain (p, "p1.usertable.vocal") > 0.5f);
    CHECK (p.userTables().lastMessage() == "Formants ready");

    // a broken file: an error message, nothing published
    const auto junk = dir.getChildFile ("Junk.ohpewt");
    junk.replaceWithText ("not a table");
    REQUIRE (p.userTables().importFile (junk, 3, usertable::Target::Wavetable));
    REQUIRE (waitForImport (p));
    CHECK (usertable::tableOf (p.core(), 3, usertable::Target::Wavetable) == nullptr);
    CHECK (p.userTables().lastMessage() == "The file is not an Ohpe wavetable.");

    // part presets carry the table
    p.presets().setUserFolder (dir.getChildFile ("Presets"));
    preset::Meta meta;
    meta.name = "Six Frames";
    const auto file = p.presets().save (meta, false, 1);
    REQUIRE (file.existsAsFile());
    CHECK (file.loadFileAsString().contains ("\"usertables\""));
    p.userTables().clear (1, usertable::Target::Wavetable);
    preset::Preset pr;
    REQUIRE (preset::fromJson (file.loadFileAsString().toStdString(), pr));
    REQUIRE (p.presets().apply (pr, 3));
    REQUIRE (usertable::tableOf (p.core(), 3, usertable::Target::Wavetable) != nullptr);
    CHECK (usertable::tableOf (p.core(), 3, usertable::Target::Wavetable)->numFrames() == 6);

    // the Wavetable page shows IMPORT / USER / CLEAR, the frame stack, in both languages
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    e.selectPart (1);
    e.selectMode (ui::Mode::Synth);
    e.selectEngine (EngineType::Wavetable);
    auto& d = e.displayComponent();
    for (int k = 0; k < 3; ++k) CHECK_FALSE (d.softKeyArea (k).isEmpty());
    CHECK (d.softKeyArea (3).isEmpty());
    p.parameter ("p2.usertable.wt")->setValueNotifyingHost (0.0f);
    e.softKey (81); // USER: on
    CHECK (hostPlain (p, "p2.usertable.wt") > 0.5f);
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        e.setLanguage (lang);
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    e.setLanguage (i18n::Language::English);
    e.softKey (82); // CLEAR
    CHECK (usertable::tableOf (p.core(), 1, usertable::Target::Wavetable) == nullptr);
    CHECK (hostPlain (p, "p2.usertable.wt") < 0.5f);
    ed.reset();
    dir.deleteRecursively();
}
