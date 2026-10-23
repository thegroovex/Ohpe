// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Renders the Ohpe editor into PNG files for the documentation (headless, e.g. under xvfb).
//   usage: ohpe_screenshot <output directory>
// Writes ohpe-<view>.png (English) and ohpe-<view>-de.png (German).
// 0.3: also the library, the pro view, the tape page, step locks and the drum kit page.
// p20: the user wavetable frame stack (ohpe-user-wavetable). p27: the update view (ohpe-update).
// p32: the settings menu (ohpe-settings*, ohpe-about*, ohpe-help-overlay*), the night skin (ohpe-dark, *-dark).
// p23: bounce to engine - a loop bounced into Grain (ohpe-bounce). p67: tape memory slices (ohpe-tapemem). p66: Key Gravity pages (ohpe-key-gravity). p70: the gravity ring on the header (ohpe-gravity-header). p33: the Expression page (ohpe-expression).
// p41: the 3D switch (ohpe-space-switch-{off,headphones,speakers}[-dark]). p77: the 3D page Amount & Retrigger (ohpe-space-retrigger).
// p38: the tape page's clear-all confirmation (ohpe-tape-clear, ohpe-tape-clear-dark) and the tape page at night (ohpe-tape-dark).
// p82: tooltips and the help search (ohpe-tooltip-encoder, ohpe-tooltip-key, ohpe-help-search, ohpe-help-search-jump).
// p42: the tape page's loop keys and loop readout (ohpe-tape-loop, ohpe-tape-loop-dark).
// p11: the tape page while recording (ohpe-tape-recording). p14: the transport block, the keyboard row and the
// octave block while recording (ohpe-transport, 2x crop; REC and PLAY at the top of their breathing).
// p47: the wordmark / header close-ups (ohpe-wordmark*, ohpe-header-right*, ohpe-about-wordmark*, 3x crops).
// p39: tempo, metronome, count-in on the tape page during a count-in (ohpe-metronome*, ohpe-tempo-panel* 2x crop).
// p45: the chord under the voice dots, the key and Scale Lock on the header (ohpe-chords*, ohpe-chords-display* 2x crop).
// 0.7: the Mod view (modulation matrix), the part's FX rack and the master rack; the Mid/Side scope,
// the master scope view and the spectrum terrain. 1.0: the 3D Space view and the 3D badge. The
// preset library reads the user folder from OHPE_PRESET_DIR when set (otherwise the default).

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "engines/SamplerEngine.h"
#include "seq/ScaleChord.h"
#include "ui/ProTiles.h"
#include "mod/ModVoice.h"
#include "plugin/Updater.h"
#include "plugin/UserSettingsStore.h"
#include "ui/Tooltips.h"
#include "ui/Palette.h"
#include "preset/Preset.h"
#include "update/Sha256.h"

#include <cmath>
#include <algorithm>

using namespace ohpe;

namespace
{
void run (OhpeProcessor& p, int blocks, int note = -1)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && note >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        p.processBlock (buffer, midi);
    }
}

/** Releases held notes so the on-screen keyboard shows no stuck keys in the next view. */
void release (OhpeProcessor& p, std::initializer_list<int> notes, int channel = 1)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    for (int n : notes) midi.addEvent (juce::MidiMessage::noteOff (channel, n), 0);
    p.processBlock (buffer, midi);
    run (p, 8);
}

void setPlain (OhpeProcessor& p, const juce::String& id, float v)
{
    if (auto* q = p.parameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v));
}

/** Sets a choice parameter by its English label. */
void setChoice (OhpeProcessor& p, const juce::String& id, const std::string& labelEn)
{
    const auto& reg = p.core().params();
    const int i = reg.indexOf (id.toStdString());
    if (i < 0) return;
    const auto& d = reg.descriptor (i);
    for (size_t c = 0; c < d.choices.size(); ++c)
        if (d.choices[c] == labelEn) setPlain (p, id, (float) c);
}

/** Lets the processor see new rack types, prepares them (the plug-in's timer does this) and lets them settle. */
void prepareRack (OhpeProcessor& p)
{
    run (p, 2);
    while (p.core().prepareEffects()) {}
    run (p, 4);
}

/** Renders audio and animation frames together (traces, meters). */
void animate (OhpeProcessor& p, OhpeEditor& e, int frames)
{
    for (int f = 0; f < frames; ++f)
    {
        run (p, 2);
        e.advanceAnimation (1.0f / 30.0f);
    }
}

/** 0.7: plays and animates together, like the editor timer does (30 frames per second). */
void playAnimated (OhpeProcessor& p, OhpeEditor& e, int frames, std::initializer_list<int> notes = {}, int channel = 1)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
    for (int f = 0; f < frames; ++f)
    {
        juce::MidiBuffer midi;
        if (f == 0)
            for (int n : notes) midi.addEvent (juce::MidiMessage::noteOn (channel, n, (juce::uint8) 100), 0);
        p.processBlock (buffer, midi);
        juce::MidiBuffer none;
        p.processBlock (buffer, none);
        p.processBlock (buffer, none);
        e.advanceAnimation (1.0f / 30.0f);
    }
}

void setEnvVar (const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s (name, value.toRawUTF8());
#else
    if (value.isEmpty()) unsetenv (name);
    else setenv (name, value.toRawUTF8(), 1);
#endif
}

/** Content packs: an .ohpepack (pack.json with checksums) of part presets for the screenshots. */
juce::File makeShotPack (const juce::File& f, const char* id, const char* en, const char* de, const char* descEn, const char* descDe, bool paid,
                         const std::vector<std::pair<std::string, std::string>>& presets)
{
    std::vector<std::pair<juce::String, juce::String>> files;
    for (const auto& [name, tag] : presets)
    {
        ohpe::Core core;
        ohpe::preset::Meta m;
        m.name = name;
        m.author = "groovex";
        m.tags = { tag };
        juce::String file = juce::String (name).toLowerCase().replaceCharacter (' ', '-');
        files.push_back ({ "presets/" + file + ".ohpepreset", juce::String::fromUTF8 (ohpe::preset::toJson (ohpe::preset::capturePart (core.params(), 0, m)).c_str()) });
    }
    juce::String sums;
    for (const auto& [path, text] : files)
        sums << (sums.isEmpty() ? "" : ",") << "\"" << path << "\":\"" << juce::String (ohpe::update::Sha256::of (text.toStdString())) << "\"";
    juce::String meta;
    meta << "{\"format\":1,\"id\":\"" << id << "\",\"version\":\"1.0.0\",\"kind\":\"presets\",\"minApp\":\"0.0.1\","
         << "\"name\":{\"en\":\"" << en << "\",\"de\":\"" << juce::String::fromUTF8 (de) << "\"},"
         << "\"description\":{\"en\":\"" << descEn << "\",\"de\":\"" << juce::String::fromUTF8 (descDe) << "\"},"
         << (paid ? juce::String ("\"license\":{\"type\":\"paid\",\"productId\":\"ohpe-") + id + "\"},\"signature\":{\"alg\":\"ed25519\",\"keyId\":\"groovex-1\",\"value\":\"c2ln\"}," : juce::String())
         << "\"files\":{" << sums << "}}";
    juce::ZipFile::Builder b;
    b.addEntry (new juce::MemoryInputStream (meta.toRawUTF8(), meta.getNumBytesAsUTF8(), true), 6, "pack.json", juce::Time());
    for (const auto& [path, text] : files)
        b.addEntry (new juce::MemoryInputStream (text.toRawUTF8(), text.getNumBytesAsUTF8(), true), 6, path, juce::Time());
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream out (f);
    b.writeToStream (out, nullptr);
    return f;
}

void save (juce::Component& c, const juce::File& f, float scale = 1.0f, juce::Rectangle<int> area = {})
{
    auto img = c.createComponentSnapshot (area.isEmpty() ? c.getLocalBounds() : area, true, scale);
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "wrote " << f.getFullPathName() << std::endl;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();

    // p40: the views below start from the plain parameter defaults; only the first-start shot uses the start sound
    OhpeProcessor::startWithFactorySound.store (false);
    OhpeEditor::showTourOnFirstOpen.store (false); // p64: the tour only in its own shots below
    TasteStore::filesEnabled.store (false);        // p69: the taste profile in memory only
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        const juce::String suffix = lang == i18n::Language::German ? "-de" : "";
        OhpeProcessor::startWithFactorySound.store (true);
        OhpeProcessor first; // p40: what a new instance shows - the first factory preset, empty tape and sequencers
        OhpeProcessor::startWithFactorySound.store (false);
        first.enableAllBuses();
        first.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> firstEd (first.createEditor());
        auto& fe = *dynamic_cast<OhpeEditor*> (firstEd.get());
        fe.setLanguage (lang);
        animate (first, fe, 4);
        save (fe, dir.getChildFile ("ohpe-first-start" + suffix + ".png"));
        // p63: the crash-safe start - the last session did not end cleanly, Ohpe offers to restore it
        first.offerRecovery (1, [] {});
        fe.showPendingRecoveryOffer();
        save (fe, dir.getChildFile ("ohpe-recovery-notice" + suffix + ".png"));
        fe.recoveryNoticeComponent().dismiss();
        // p64: the first-start tour, every step
        for (int step = 0; step < ui::FirstStartTour::numSteps; ++step)
        {
            fe.showTour (true, step);
            animate (first, fe, 2);
            save (fe, dir.getChildFile ("ohpe-tour-" + juce::String (step + 1) + suffix + ".png"));
        }
        fe.showTour (false);
        firstEd = nullptr;
    }
    for (auto lang : { i18n::Language::English, i18n::Language::German })
    {
        const juce::String suffix = lang == i18n::Language::German ? "-de" : "";
        OhpeProcessor p;
        p.enableAllBuses();
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
        e.setLanguage (lang);

        // a chord on the analog engine
        p.parameter ("p1.analog.shape")->setValueNotifyingHost (0.35f);
        p.parameter ("p1.analog.reso")->setValueNotifyingHost (0.45f);
        e.selectEngine (EngineType::Analog);
        e.displayComponent().setAnimatedVisuals (true);
        setPlain (p, "p1.life", 0.35f); // p15: the LIFE bar lit (and shimmering: the chord plays)
        playAnimated (p, e, 2, { 60 });
        playAnimated (p, e, 2, { 64 });
        playAnimated (p, e, 30, { 67 });
        save (e, dir.getChildFile ("ohpe-analog" + suffix + ".png"));
        // p14: the same view at 2x (pixel check) and a close-up of the keyboard row at 2x: wheels, keys, glide
        if (auto* q = p.parameter ("perf.mod")) q->setValueNotifyingHost (0.35f);
        setChoice (p, "p1.glide.mode", "Legato");
        setPlain (p, "p1.glide", 0.18f);
        run (p, 2);
        e.advanceAnimation (1.0f / 30.0f);
        save (e, dir.getChildFile ("ohpe-analog-2x" + suffix + ".png"), 2.0f);
        {
            const auto bed = OhpeEditor::designArea ("keyboardBed").expanded (8.0f).toNearestInt();
            save (e, dir.getChildFile ("ohpe-keyboard-row" + suffix + ".png"), 2.0f, bed);
        }
        {   // p15: the engine column with the LIFE bar at its foot, 2x
            const auto col = OhpeEditor::designArea ("engineColumn").expanded (8.0f).toNearestInt();
            save (e, dir.getChildFile ("ohpe-life" + suffix + ".png"), 2.0f, col);
        }
        {   // p35: the CPU meter on the mode column's legend line (the chord above is playing), 2x
            e.advanceAnimation (1.0f / 30.0f);
            e.cpuMeterComponent().setLoad (0.12f, 0.18f); // no audio thread runs here: a typical reading for the picture
            save (e, dir.getChildFile ("ohpe-cpu-meter" + suffix + ".png"), 2.0f, juce::Rectangle<int> (952, 80, 224, 40));
        }
        setPlain (p, "p1.life", 0.0f);
        if (auto* q = p.parameter ("perf.mod")) q->setValueNotifyingHost (0.0f);

        release (p, { 60, 64, 67 });
        e.selectEngine (EngineType::FM);
        p.parameter ("p1.fm.algo")->setValueNotifyingHost (0.4f);
        p.parameter ("p1.fm.index")->setValueNotifyingHost (0.6f);
        run (p, 4, 48);
        e.advanceAnimation (0.7f);
        save (e, dir.getChildFile ("ohpe-fm" + suffix + ".png"));

        release (p, { 48 });

        // a pattern on the step sequencer
        for (int s : { 0, 3, 6, 8, 10, 12, 14 })
        {
            StepSequencer::Step st;
            st.on = true;
            st.note = 48 + s;
            p.core().stepSequencer().setStep (s, st);
        }
        e.selectMode (ui::Mode::Sequencer);
        p.parameter ("seq.run")->setValueNotifyingHost (1.0f);
        run (p, 30);
        e.advanceAnimation (0.4f);
        save (e, dir.getChildFile ("ohpe-sequencer" + suffix + ".png"));

        // 0.3: parameter locks on a step (cutoff + resonance on step 7)
        {
            auto& seq = p.core().stepSequencer();
            auto st = seq.getStep (6);
            st.locks[0] = { p.core().params().indexOf ("p1.analog.cutoff"), 0.25f };
            st.locks[1] = { p.core().params().indexOf ("p1.analog.reso"), 0.7f };
            seq.setStep (6, st);
            auto st2 = seq.getStep (12);
            st2.locks[0] = { p.core().params().indexOf ("p1.analog.cutoff"), 0.8f };
            seq.setStep (12, st2);
            e.selectMode (ui::Mode::Sequencer);
            e.selectLockStep (6);
            e.selectEngine (EngineType::Analog);
            run (p, 8);
            e.advanceAnimation (0.3f);
            save (e, dir.getChildFile ("ohpe-step-locks" + suffix + ".png"));
            e.selectLockStep (-1);
        }

        e.selectMode (ui::Mode::Mixer);
        e.advanceAnimation (0.2f);
        save (e, dir.getChildFile ("ohpe-mixer" + suffix + ".png"));
        p.parameter ("seq.run")->setValueNotifyingHost (0.0f);
        run (p, 4);

        // p75: the display's header on every page at 2x - every title whole next to the full key (C# Phrygian)
        {
            const auto header = juce::Rectangle<int> (256, 112, 688, 52);
            const auto engineBefore = (EngineType) (int) std::lround (p.core().params().get ("p1.engine"));
            p.setKeyFromUi (1, scales::Phrygian);
            const std::pair<ui::Mode, const char*> pages[] = { { ui::Mode::Synth, "synth" }, { ui::Mode::Envelope, "envelope" }, { ui::Mode::Lfo, "lfo" },
                                                               { ui::Mode::Mixer, "mixer" }, { ui::Mode::Effects, "effects" }, { ui::Mode::Sequencer, "sequencer" },
                                                               { ui::Mode::Tape, "tape" }, { ui::Mode::Drum, "drum" } };
            for (const auto& [m, name] : pages)
            {
                e.selectMode (m);
                e.advanceAnimation (0.1f);
                save (e, dir.getChildFile ("ohpe-header-" + juce::String (name) + suffix + ".png"), 2.0f, header);
            }
            e.selectMode (ui::Mode::Synth);
            e.selectEngine (EngineType::Wavetable);
            e.advanceAnimation (0.1f);
            save (e, dir.getChildFile ("ohpe-header-wavetable" + suffix + ".png"), 2.0f, header);
            e.selectEngine (engineBefore);
            p.setKeyFromUi (0, scales::Major);
            e.selectMode (ui::Mode::Mixer);
            e.advanceAnimation (0.1f);
        }

        // p11: tape page - recorded with the transport keys: REC on tracks 1 and 2 (part 1), loop set;
        //      one shot while recording (REC lit), one after STOP with track 2 selected
        {
            p.parameter ("tape.tr1.arm")->setValueNotifyingHost (1.0f);
            p.parameter ("tape.tr2.arm")->setValueNotifyingHost (1.0f);
            p.parameter ("tape.tr2.src")->setValueNotifyingHost (p.parameter ("tape.tr2.src")->convertTo0to1 (1.0f)); // part 1
            p.parameter ("tape.loopin")->setValueNotifyingHost (p.parameter ("tape.loopin")->convertTo0to1 (1.0f));
            p.parameter ("tape.loopout")->setValueNotifyingHost (p.parameter ("tape.loopout")->convertTo0to1 (5.0f));
            run (p, 2);
            e.selectMode (ui::Mode::Tape);
            e.selectTapeTrack (1);
            p.core().tape().housekeeping();
            e.transport (OhpeProcessor::TransportKey::Record);
            for (int k = 0; k < 12; ++k)
            {
                run (p, 12, 48 + (k * 5) % 24);
                release (p, { 48 + (k * 5) % 24 });
                p.core().tape().housekeeping();
                e.advanceAnimation (0.05f);
            }
            // p14: REC and PLAY breathe while recording - shoot near the top of the pulse
            for (int k = 0; k < 80 && e.recordKey().getLedLevel() < 0.97f; ++k) e.advanceAnimation (1.0f / 30.0f);
            save (e, dir.getChildFile ("ohpe-tape-recording" + suffix + ".png"));
            {
                const auto area = OhpeEditor::designArea ("transportPanel").getUnion (OhpeEditor::designArea ("octavePanel"))
                                      .getUnion (OhpeEditor::designArea ("keyboardBed")).expanded (8.0f).toNearestInt();
                save (e, dir.getChildFile ("ohpe-transport" + suffix + ".png"), 2.0f, area);
            }
            e.transport (OhpeProcessor::TransportKey::Record); // punch out
            run (p, 4);
            p.core().tape().housekeeping();
            e.transport (OhpeProcessor::TransportKey::Stop);
            e.transport (OhpeProcessor::TransportKey::Loop);
            run (p, 2);
            p.core().tape().housekeeping();
            p.parameter ("tape.tr1.arm")->setValueNotifyingHost (0.0f);
            run (p, 2);
            e.selectTapeTrack (1);
            e.advanceAnimation (0.7f);
            save (e, dir.getChildFile ("ohpe-tape" + suffix + ".png"));
            // p38: CLEAR ALL asks first - the confirmation over the plate; the tape page and the confirmation in the
            // night skin too (the user settings of the render live in a temporary folder)
            {
                auto& store = e.userSettingsStore();
                const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ohpe-shot-settings-tape");
                tmp.deleteRecursively();
                store.setFolderForTests (tmp);
                auto setSkin = [&] (bool dark) {
                    auto v = store.get();
                    v.skin = dark ? settings::Skin::Dark : settings::Skin::Light;
                    store.set (v);
                    e.applyUserSettings();
                    e.advanceAnimation (0.2f);
                };
                auto confirmShot = [&] (const char* name) {
                    e.softKey (131); // CLEAR ALL
                    e.advanceAnimation (0.2f);
                    save (e, dir.getChildFile (juce::String (name) + suffix + ".png"));
                    e.confirmDialogComponent().cancel();
                };
                confirmShot ("ohpe-tape-clear");
                setSkin (true);
                save (e, dir.getChildFile ("ohpe-tape-dark" + suffix + ".png"));
                confirmShot ("ohpe-tape-clear-dark");
                setSkin (false);
                // p42: the loop keys behind the loop readout (the loop of the recording, on the bar grid)
                e.showTapeLoopKeys (true);
                for (int k = 0; k < 12; ++k) e.advanceAnimation (0.5f); // messages fade: the readout shows the loop
                save (e, dir.getChildFile ("ohpe-tape-loop" + suffix + ".png"));
                setSkin (true);
                for (int k = 0; k < 4; ++k) e.advanceAnimation (0.5f);
                save (e, dir.getChildFile ("ohpe-tape-loop-dark" + suffix + ".png"));
                setSkin (false);
                e.showTapeLoopKeys (false);
                tmp.deleteRecursively();
                e.setLanguage (lang);
            }
            p.parameter ("tape.tr2.arm")->setValueNotifyingHost (0.0f);
            run (p, 2);
        }

        // 0.3: drum kit page
        e.selectMode (ui::Mode::Drum);
        e.selectDrumPad (2);
        e.advanceAnimation (0.4f);
        save (e, dir.getChildFile ("ohpe-drum" + suffix + ".png"));

        // 0.7: Mod view - six routings on part 1, routing 1 selected, a chord sounding
        {
            setChoice (p, "p1.fx2.type", "Hyper");
            setChoice (p, "p1.mod.s1.src", "LFO 2");    setChoice (p, "p1.mod.s1.dst", "Engine 2");     setPlain (p, "p1.mod.s1.amt", 0.4f);
            setChoice (p, "p1.mod.s2.src", "Mod Env");  setChoice (p, "p1.mod.s2.dst", "Engine 3");     setPlain (p, "p1.mod.s2.amt", -0.25f);
            setChoice (p, "p1.mod.s3.src", "Velocity"); setChoice (p, "p1.mod.s3.dst", "Level");        setPlain (p, "p1.mod.s3.amt", 0.5f);
            setChoice (p, "p1.mod.s4.src", "LFO 3");    setChoice (p, "p1.mod.s4.via", "Mod Wheel");
            setChoice (p, "p1.mod.s4.dst", "Pan");      setPlain (p, "p1.mod.s4.amt", 0.35f);
            setChoice (p, "p1.mod.s5.src", "Random");   setChoice (p, "p1.mod.s5.dst", "FX 2 Param 2"); setPlain (p, "p1.mod.s5.amt", 0.3f);
            setChoice (p, "p1.mod.s6.src", "Macro 1");  setChoice (p, "p1.mod.s6.dst", "FX 2 Mix");     setPlain (p, "p1.mod.s6.amt", 0.5f);
            setChoice (p, "p1.mod.s1.curve", "S-Curve");
            setPlain (p, "p1.lfo2.rate", 0.8f);
            setPlain (p, "p1.lfo3.rate", 3.0f);
            setPlain (p, "p1.macro1", 0.6f);
            prepareRack (p);
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            run (p, 1, 60); run (p, 1, 64); run (p, 1, 67);
            e.selectMode (ui::Mode::Lfo);
            e.selectView (1);
            e.selectModRouting (0);
            animate (p, e, 96);
            save (e, dir.getChildFile ("ohpe-mod-view" + suffix + ".png"));
        }

        // 0.7: FX rack of part 1 - five effects and an empty slot, the EQ selected
        {
            setChoice (p, "p1.fx.type", "Distortion");
            setPlain (p, "p1.fx.distortion.drive", 0.35f);
            setChoice (p, "p1.fx3.type", "EQ");
            setPlain (p, "p1.fx3.eq.lowgain", 3.0f);
            setPlain (p, "p1.fx3.eq.p1gain", -4.0f);
            setPlain (p, "p1.fx3.eq.p2freq", 2000.0f);
            setPlain (p, "p1.fx3.eq.p2gain", 5.0f);
            setPlain (p, "p1.fx3.eq.p2q", 1.5f);
            setPlain (p, "p1.fx3.eq.highgain", 2.0f);
            setChoice (p, "p1.fx4.type", "Echo");
            setChoice (p, "p1.fx5.type", "Hall");
            setPlain (p, "p1.fx5.mix", 0.6f);
            prepareRack (p);
            e.selectMode (ui::Mode::Effects);
            e.selectView (1);
            e.selectFxSlot (2);
            animate (p, e, 20);
            save (e, dir.getChildFile ("ohpe-rack-view" + suffix + ".png"));
            // p55: the FX editor - the EQ with band 2 selected (its frequency, gain and Q on the encoders)
            e.showFxEditor (true);
            auto selectPoint = [&e] (const std::string& paramId) {
                if (auto* g = e.fxEditorComponent().graph())
                    for (int h = 0; h < g->numHandles(); ++h)
                        if (g->layout().handles[(size_t) h].x == paramId) { g->selectHandle (h); return; }
            };
            selectPoint ("p1.fx3.eq.p2freq");
            animate (p, e, 10);
            save (e, dir.getChildFile ("ohpe-fx-editor-eq" + suffix + ".png"));
            // a modulation effect: the Hyper slot, rate / depth on its wave
            e.selectFxSlot (1);
            selectPoint ("p1.fx2.hyper.rate");
            animate (p, e, 10);
            save (e, dir.getChildFile ("ohpe-fx-editor-mod" + suffix + ".png"));
            e.showFxEditor (false);
            e.selectFxSlot (2);
        }

        // 0.7: master rack - compressor and multiband working on the chord, the compressor selected
        {
            setChoice (p, "mfx1.type", "Compressor");
            setPlain (p, "mfx1.compressor.thresh", -30.0f);
            setPlain (p, "mfx1.compressor.ratio", 3.0f);
            setPlain (p, "mfx1.compressor.makeup", 4.0f);
            setChoice (p, "mfx2.type", "Multiband");
            setPlain (p, "mfx2.multiband.depth", 0.6f);
            prepareRack (p);
            e.selectView (2);
            e.selectFxSlot (0);
            animate (p, e, 20);
            save (e, dir.getChildFile ("ohpe-master-rack" + suffix + ".png"));
            // p55: the compressor in the FX editor - threshold on its transfer curve, the gain reduction working
            e.showFxEditor (true);
            if (auto* g = e.fxEditorComponent().graph()) g->selectHandle (0);
            animate (p, e, 20);
            save (e, dir.getChildFile ("ohpe-fx-editor-comp" + suffix + ".png"));
            e.showFxEditor (false);
            release (p, { 60, 64, 67 });
        }

        // 0.3: pro view of the FM engine on part 2. p21 (pro view 2.0): the pictures on top of the cards, a chord held
        //      (playheads), LFO 2 -> FM index in the matrix (violet), Discover with six variations
        {
            auto& pv = e.proViewComponent();
            auto route = [&] (int slot, int src, const std::string& target, float amt) {
                const auto r = "p2.mod.s" + juce::String (slot) + ".";
                setPlain (p, r + "dst", (float) ui::pro::destForParam (p, 1, target));
                setPlain (p, r + "amt", amt);
                setPlain (p, r + "src", (float) src);
            };
            auto unroute = [&] (int slot) {
                const auto r = "p2.mod.s" + juce::String (slot) + ".";
                setPlain (p, r + "src", 0.0f);
                setPlain (p, r + "dst", 0.0f);
                setPlain (p, r + "amt", 0.0f);
            };
            e.selectPart (1);
            e.selectEngine (EngineType::FM);
            e.selectMode (ui::Mode::Synth);
            setPlain (p, "p2.lfo2.rate", 0.6f);
            setPlain (p, "p2.lfo.depth", 0.35f);
            setPlain (p, "p2.amp.a", 0.25f);
            setPlain (p, "p2.amp.r", 1.2f);
            e.showProView (true);
            pv.setSimple (false);
            route (1, mod::SrcLfo2, "p2.fm.index", 0.3f);
            playAnimated (p, e, 18, { 60, 67 }, 2);
            save (e, dir.getChildFile ("ohpe-pro-view" + suffix + ".png"));
            // Discover: six variations, the third one auditioned
            pv.discoverStrip().setStrength (0.55f);
            pv.discoverStrip().vary();
            pv.discoverStrip().audition (2);
            playAnimated (p, e, 8);
            save (e, dir.getChildFile ("ohpe-pro-discover" + suffix + ".png"));
            save (e, dir.getChildFile ("ohpe-pro-discover-strip" + suffix + ".png"), 2.0f,
                  pv.discoverStrip().getBounds() + pv.getPosition());
            pv.discoverStrip().revertAudition();
            unroute (1);
            release (p, { 60, 67 }, 2);

            // the wavetable engine: LFO 2 -> position (the violet frames), a chord
            e.selectEngine (EngineType::Wavetable);
            route (1, mod::SrcLfo2, "p2.wavetable.pos", 0.35f);
            playAnimated (p, e, 16, { 57, 64 }, 2);
            save (e, dir.getChildFile ("ohpe-pro-wavetable" + suffix + ".png"));
            unroute (1);
            release (p, { 57, 64 }, 2);

            // the grain engine: the waveform with its grain cloud while a note plays
            e.selectEngine (EngineType::Grain);
            setPlain (p, "p2.grain.spray", 0.35f);
            playAnimated (p, e, 16, { 60 }, 2);
            save (e, dir.getChildFile ("ohpe-pro-grain" + suffix + ".png"));
            release (p, { 60 }, 2);

            // Simple: pictures and Discover only; the analog filter with a modulation shadow (LFO 2 -> cutoff),
            // the mod envelope -> oscillator shape
            e.selectEngine (EngineType::Analog);
            route (1, mod::SrcLfo2, "p2.analog.cutoff", 0.25f);
            route (2, mod::SrcModEnv, "p2.analog.shape", 0.3f);
            pv.setSimple (true);
            pv.discoverStrip().vary(); // six relatives of the analog sound (none auditioned)
            playAnimated (p, e, 16, { 48, 55, 64 }, 2);
            save (e, dir.getChildFile ("ohpe-pro-simple" + suffix + ".png"));
            pv.setSimple (false);
            playAnimated (p, e, 4);
            save (e, dir.getChildFile ("ohpe-pro-modulation" + suffix + ".png"));
            unroute (1);
            unroute (2);
            release (p, { 48, 55, 64 }, 2);

            // p31: the SUB layer of the analog part - round wave one octave down, macro 1 -> sub level (violet)
            setPlain (p, "p2.sub.level", 0.65f);
            setPlain (p, "p2.sub.shape", 1.0f);
            setPlain (p, "p2.macro1", 0.5f);
            route (1, mod::SrcMacro1, "p2.sub.level", 0.2f);
            {
                const int sc = pv.cardIndex ("Sub");
                if (sc >= 0)
                {
                    auto& vp = pv.scroller();
                    vp.setViewPosition (0, juce::jmax (0, pv.cardBounds (sc).getY() - 40));
                    playAnimated (p, e, 12, { 45 }, 2);
                    save (e, dir.getChildFile ("ohpe-pro-sub" + suffix + ".png"));
                    const auto cb = pv.cardBounds (sc) - vp.getViewPosition() + vp.getPosition() + pv.getPosition();
                    save (e, dir.getChildFile ("ohpe-pro-sub-card" + suffix + ".png"), 2.0f, cb.expanded (6));
                    vp.setViewPosition (0, 0);
                    release (p, { 45 }, 2);
                }
            }
            unroute (1);
            setPlain (p, "p2.sub.level", 0.0f);
            setPlain (p, "p2.sub.shape", 0.0f);
            setPlain (p, "p2.macro1", 0.0f);
            setPlain (p, "p2.lfo.depth", 0.0f);
            e.showProView (false);
            e.selectPart (0);
        }

        // 0.7: Mid/Side FX slot on part 1 (a wide cluster chord), the stereo scope on its page
        {
            // a clean start: the rack and modulation scenes above are switched off
            for (int s = 2; s <= 6; ++s) setPlain (p, "p1.fx" + juce::String (s) + ".type", 0.0f);
            for (int s = 1; s <= 4; ++s) setPlain (p, "mfx" + juce::String (s) + ".type", 0.0f);
            for (int r = 1; r <= 6; ++r) setPlain (p, "p1.mod.s" + juce::String (r) + ".amt", 0.0f);
            prepareRack (p);
            e.selectPart (0);
            e.selectEngine (EngineType::Cluster);
            setPlain (p, "p1.cluster.spread", 1.0f);
            setPlain (p, "p1.fx.type", (float) InsertSlot::MidSide);
            setPlain (p, "p1.fx.midside.width", 1.45f);
            setPlain (p, "p1.fx.midside.side", 1.5f);
            setPlain (p, "p1.fx.midside.bassmono", 1.0f);
            setPlain (p, "p1.fx.midside.haas", 0.25f);
            prepareRack (p); // the core sees the new type before the pages bind its block
            e.selectMode (ui::Mode::Effects);
            e.selectView (0); // sends + FX slot pages: the scope in the display's visual area
            auto& d = e.displayComponent();
            d.onPageChange (3);
            playAnimated (p, e, 40, { 50, 57, 62, 65 });
            save (e, dir.getChildFile ("ohpe-midside-scope" + suffix + ".png"));
            release (p, { 50, 57, 62, 65 });

            // the master scope as the display view (a pad with reverb; Mid/Side in the master rack, a little wider)
            setPlain (p, "p1.fx.type", 0.0f);
            setPlain (p, "mix.ch1.sendb", 0.45f);
            setPlain (p, "mfx1.type", (float) InsertSlot::MidSide);
            setPlain (p, "mfx1.midside.width", 1.25f);
            setPlain (p, "mfx1.midside.bassmono", 1.0f);
            p.core().params().set ("mfx1.type", (float) InsertSlot::MidSide);
            prepareRack (p);
            e.selectMode (ui::Mode::Mixer);
            d.setVisual (ui::Display::Visual::Scope);
            playAnimated (p, e, 40, { 45, 52, 60, 64 });
            save (e, dir.getChildFile ("ohpe-master-scope" + suffix + ".png"));
            release (p, { 45, 52, 60, 64 });

            // the spectrum terrain as the display view (a moving line of notes)
            e.selectEngine (EngineType::Wavetable);
            d.setVisual (ui::Display::Visual::Terrain);
            for (int k = 0; k < 8; ++k)
            {
                const int note = 48 + (k * 7) % 24;
                playAnimated (p, e, 4, { note, note + 12 });
                release (p, { note, note + 12 });
            }
            playAnimated (p, e, 6, { 55, 67, 74 });
            save (e, dir.getChildFile ("ohpe-terrain" + suffix + ".png"));
            release (p, { 55, 67, 74 });

            // p26: the living display - part 1 as a 3D object: a wide chord (the master's Mid/Side widens it), LFO 2
            //      routed to the timbre (it bulges), the mod envelope ripples its surface
            setPlain (p, "p1.lfo2.rate", 0.6f);
            setPlain (p, "p1.lfo2.depth", 1.0f);
            setPlain (p, "p1.mod.s1.src", (float) mod::SrcLfo2);
            setPlain (p, "p1.mod.s1.dst", (float) mod::DstTimbre);
            setPlain (p, "p1.mod.s1.amt", 0.7f);
            setPlain (p, "p1.lfo.depth", 0.4f);
            e.selectMode (ui::Mode::Synth);
            d.setVisual (ui::Display::Visual::Living);
            playAnimated (p, e, 2, { 48, 55, 64 });
            playAnimated (p, e, 26, { 71 });
            save (e, dir.getChildFile ("ohpe-living" + suffix + ".png"));
            release (p, { 48, 55, 64, 71 });
            setPlain (p, "p1.mod.s1.amt", 0.0f);
            setPlain (p, "p1.mod.s1.dst", 0.0f);
            setPlain (p, "p1.mod.s1.src", 0.0f);
            setPlain (p, "p1.lfo.depth", 0.0f);
            run (p, 8);
            d.setVisual (ui::Display::Visual::Character);
            setPlain (p, "mfx1.type", 0.0f);
            setPlain (p, "mix.ch1.sendb", 0.0f);
            e.selectEngine (EngineType::Analog);
        }

        // 1.0: the 3D Space view - three parts moving around the listener (circle, figure-8, random orbit), part 1 edited,
        // binaural monitoring: the 3D badge in the header and the part selector's headphones are lit
        {
            setChoice (p, "space.out", "Headphones");
            const char* paths[] = { "Circle", "Figure-8", "Random Orbit" };
            const char* lengths[] = { "1 Bar", "2 Bars", "4 Bars" };
            for (int part = 1; part <= 3; ++part)
            {
                const juce::String pre = "p" + juce::String (part) + ".space.";
                setPlain (p, pre + "on", 1.0f);
                setChoice (p, pre + "path", paths[part - 1]);
                setChoice (p, pre + "pathlen", lengths[part - 1]);
                setPlain (p, pre + "pathsize", 0.4f + 0.2f * (float) part);
                setPlain (p, pre + "distance", 1.2f + 1.1f * (float) part);
                setPlain (p, pre + "room", 0.3f);
            }
            p.syncSpaceModes(); // p41: the 3D switches follow (the plug-in's timer does this)
            e.selectPart (0);
            e.selectEngine (EngineType::Wavetable);
            e.selectMode (ui::Mode::Effects);
            e.selectView (3); // Sends | Rack | Master | 3D Space
            playAnimated (p, e, 2, { 48, 55 }, 1);
            playAnimated (p, e, 2, { 64 }, 2);
            playAnimated (p, e, 70, { 72 }, 3);
            save (e, dir.getChildFile ("ohpe-space-view" + suffix + ".png"));

            // the badge on its own page: Synth mode of the wavetable part (a long title), the headphones lit
            e.selectMode (ui::Mode::Synth);
            playAnimated (p, e, 12);
            save (e, dir.getChildFile ("ohpe-space-badge" + suffix + ".png"));

            // p30: the newer motion paths - Weave (part 1, edited: its path drawn ahead of the dot as a dotted curve),
            // Swarm and Over the Head
            {
                const char* newPaths[] = { "Weave", "Swarm", "Over the Head" };
                for (int part = 1; part <= 3; ++part)
                    setChoice (p, "p" + juce::String (part) + ".space.path", newPaths[part - 1]);
                setPlain (p, "p1.space.pathsize", 1.0f);
                setPlain (p, "p1.space.elevation", 10.0f);
                e.selectMode (ui::Mode::Effects);
                e.selectView (3);
                playAnimated (p, e, 45);
                save (e, dir.getChildFile ("ohpe-space-paths" + suffix + ".png"));
                setPlain (p, "p1.space.elevation", 0.0f);
                // p77: the last 3D page - Amount and Motion Retrigger (ohpe-space-retrigger)
                setPlain (p, "p1.space.amount", 0.7f);
                setChoice (p, "p1.space.retrig", "Note");
                auto& disp = e.displayComponent();
                if (disp.onPageChange) disp.onPageChange (5 - e.shownPage());
                playAnimated (p, e, 30, { 60 }, 1);
                save (e, dir.getChildFile ("ohpe-space-retrigger" + suffix + ".png"));
                release (p, { 60 });
                if (disp.onPageChange) disp.onPageChange (-e.shownPage());
                setPlain (p, "p1.space.amount", 1.0f);
                setChoice (p, "p1.space.retrig", "Free");
            }
            release (p, { 48, 55 });
            for (int part = 1; part <= 3; ++part) setPlain (p, "p" + juce::String (part) + ".space.on", 0.0f);
            p.syncSpaceModes(); // p41
            {
                juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOff (2, 64), 0);
                midi.addEvent (juce::MidiMessage::noteOff (3, 72), 0);
                p.processBlock (buffer, midi);
            }
            run (p, 8);
            e.selectEngine (EngineType::Analog);
        }

        // p19: the resonator body - part 1 (Pluck) through a string body with sympathetic strings on a chord
        {
            e.selectPart (0);
            setChoice (p, "p1.engine", "Pluck");
            e.selectEngine (EngineType::Pluck);
            setPlain (p, "p1.reso.on", 1.0f);
            setChoice (p, "p1.reso.model", "String");
            setChoice (p, "p1.reso.follow", "Chord");
            setPlain (p, "p1.reso.mix", 0.7f);
            setPlain (p, "p1.reso.sympathetic", 0.8f);
            setPlain (p, "p1.reso.size", 0.8f);
            setPlain (p, "p1.reso.decay", 4.0f);
            e.selectMode (ui::Mode::Effects);
            e.selectView (4); // Sends | Rack | Master | 3D Space | Body
            playAnimated (p, e, 2, { 48, 55, 64 }, 1);
            playAnimated (p, e, 40);
            save (e, dir.getChildFile ("ohpe-reso-view" + suffix + ".png"));
            release (p, { 48, 55, 64 });
            setPlain (p, "p1.reso.on", 0.0f);
            setChoice (p, "p1.engine", "Analog");
            run (p, 8);
            e.selectEngine (EngineType::Analog);
            e.selectView (0);
        }

        // p14: key LEDs - the parts on different engines (P1 Analog edited, P2 FM, P3 Wavetable, P4 Analog), a chorus
        //      in part 1's rack and the sequencer running: yellow / violet engine LEDs with their part dots,
        //      status LEDs on the mode keys (Effects, Sequencer violet; Synth yellow = the shown page)
        {
            setChoice (p, "p2.engine", "FM");
            setChoice (p, "p3.engine", "Wavetable");
            setChoice (p, "p4.engine", "Analog");
            setChoice (p, "p1.fx.type", "Chorus");
            prepareRack (p);
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            e.selectMode (ui::Mode::Synth);
            setPlain (p, "seq.run", 1.0f);
            playAnimated (p, e, 20);
            save (e, dir.getChildFile ("ohpe-engine-leds" + suffix + ".png"));
            setPlain (p, "seq.run", 0.0f);
            setChoice (p, "p1.fx.type", "Off");
            for (int part = 2; part <= 4; ++part) setChoice (p, "p" + juce::String (part) + ".engine", "Analog");
            run (p, 8);
            e.advanceAnimation (1.0f / 30.0f);
        }

        // p16: the morph pad - four corners on four engines (two library pads, the current sound, a library bell),
        //      a chord held, the position between the corners
        {
            auto byName = [&] (const char* name) {
                const auto& list = p.presets().entries();
                for (int i = 0; i < (int) list.size(); ++i)
                    if (list[(size_t) i].meta.name == name && list[(size_t) i].meta.kind == preset::Kind::Part) return i;
                return -1;
            };
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            e.selectMode (ui::Mode::Lfo);
            e.selectView (2); // LFO | Mod | Morph
            const char* corners[] = { "Glass Shimmer", "Breath Choir Pad", nullptr, "Crystal FM Pad" };
            for (int c = 0; c < 4; ++c) e.assignMorphCorner (c, corners[c] != nullptr ? byName (corners[c]) : -1);
            setPlain (p, "p1.morph.glide", 0.0f);
            setPlain (p, "p1.morph.x", 0.62f);
            setPlain (p, "p1.morph.y", 0.34f);
            prepareRack (p);
            playAnimated (p, e, 2, { 48, 55 });
            playAnimated (p, e, 40, { 64 });
            save (e, dir.getChildFile ("ohpe-morph-view" + suffix + ".png"));
            release (p, { 48, 55, 64 });
            e.assignMorphCorner (0, -3);
            setPlain (p, "p1.morph.on", 0.0f);
            setPlain (p, "p1.morph.x", 0.5f);
            setPlain (p, "p1.morph.y", 0.5f);
            run (p, 8);
            e.selectView (0);
        }

        // p22: the motion recorder - three recorded lanes on part 1 (cutoff sweep, resonance swells, a stepped detune),
        //      a 12-step loop against the bar, playing with the transport
        {
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            e.selectMode (ui::Mode::Lfo);
            e.selectView (3); // LFO | Mod | Morph | Motion
            auto& m = p.core().motion();
            const int len = 12 * MotionRecorder::pointsPerStep;
            std::vector<float> a ((size_t) len), b ((size_t) len, -1.0f), c ((size_t) len);
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) i / (float) len;
                a[(size_t) i] = 0.5f + 0.32f * std::sin (6.2831853f * t) + 0.1f * std::sin (18.849556f * t);
                if (i >= len / 4 && i < len - 10) b[(size_t) i] = 0.2f + 0.6f * std::pow (std::sin (3.14159265f * (float) (i - len / 4) / (float) (len - 10 - len / 4)), 2.0f);
                c[(size_t) i] = 0.15f + 0.7f * (float) ((i / 16) % 3) / 2.0f;
            }
            const auto& reg = p.core().params();
            m.setLane (0, 0, reg.indexOf ("p1.analog.cutoff"), a);
            m.setLane (0, 1, reg.indexOf ("p1.analog.reso"), b);
            m.setLane (0, 2, reg.indexOf ("p1.analog.detune"), c);
            setPlain (p, "p1.motion.len", 12.0f);
            setPlain (p, "p1.motion.on", 1.0f);
            setPlain (p, "seq.run", 1.0f);
            playAnimated (p, e, 2, { 48, 55 });
            playAnimated (p, e, 30);
            save (e, dir.getChildFile ("ohpe-motion-view" + suffix + ".png"));
            release (p, { 48, 55 });
            setPlain (p, "seq.run", 0.0f);
            setPlain (p, "p1.motion.on", 0.0f);
            setPlain (p, "p1.motion.len", 16.0f);
            m.clearAll();
            run (p, 8);
            e.selectView (0);
        }

        // p68: Voice Control - a sung "A" (synthetic: pulse train through four formants) on the input, the Voice view
        {
            e.selectPart (0);
            e.selectMode (ui::Mode::Lfo);
            e.selectView (5); // LFO | Mod | Morph | Motion | Harmony | Voice
            juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
            double ph = 0.0, glottal = 0.0, peak = 1.0e-9;
            struct Res { double a1, a2, g, y1 = 0, y2 = 0; double run (double x) { const double y = g * x - a1 * y1 - a2 * y2; y2 = y1; y1 = y; return y; } };
            auto res = [] (double f, double bw) { const double r = std::exp (-3.14159265 * bw / 48000.0); return Res { -2.0 * r * std::cos (6.2831853 * f / 48000.0), r * r, 1.0 - r }; };
            Res r1 = res (730, 80), r2 = res (1150, 100), r3 = res (2600, 140), r4 = res (3500, 200);
            for (int f = 0; f < 40; ++f)
            {
                for (int k = 0; k < 3; ++k)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        ph += 140.0 / 48000.0;
                        double ex = 0.0;
                        if (ph >= 1.0) { ph -= 1.0; ex = 1.0; }
                        glottal = 0.97 * glottal + ex;
                        const double y = r4.run (r3.run (r2.run (r1.run (glottal))));
                        peak = std::max (peak, std::abs (y));
                        const float v = (float) (0.3 * y / peak);
                        buffer.setSample (0, i, v);
                        buffer.setSample (1, i, v);
                    }
                    juce::MidiBuffer none;
                    p.processBlock (buffer, none);
                }
                e.advanceAnimation (1.0f / 30.0f);
            }
            save (e, dir.getChildFile ("ohpe-voice-view" + suffix + ".png"));
            buffer.clear();
            for (int k = 0; k < 120; ++k) { juce::MidiBuffer none; p.processBlock (buffer, none); } // the voice goes home
            e.selectView (0);
        }

        // p25: the performance pads - a chord with the sequencer running, the filter sweep and the reverb freeze held,
        //      the stutter waiting for its grid line
        {
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            e.selectMode (ui::Mode::Effects);
            e.selectView (5); // Sends | Rack | Master | 3D Space | Body | Pads
            setPlain (p, "seq.run", 1.0f);
            setChoice (p, "pads.quant", "1/4");
            playAnimated (p, e, 2, { 48, 55, 60 });
            playAnimated (p, e, 10);
            setPlain (p, "pads.freeze", 1.0f);
            setPlain (p, "pads.sweep", 1.0f);
            playAnimated (p, e, 56); // about 1.8 s: the 1-bar sweep is nearly closed, the freeze holds
            setPlain (p, "pads.stutter", 1.0f);
            playAnimated (p, e, 1);
            save (e, dir.getChildFile ("ohpe-pads-view" + suffix + ".png"));
            release (p, { 48, 55, 60 });
            for (const char* id : { "pads.stutter", "pads.sweep", "pads.freeze" }) setPlain (p, id, 0.0f);
            setChoice (p, "pads.quant", "1/16");
            setPlain (p, "seq.run", 0.0f);
            run (p, 400); // the freeze tail rings out
            e.selectView (0);
        }

        // p24: the harmony brain - a progression on part 1 (Am F C G, then Am7 held), part 2 following the chord, the Harmony
        //      view and the isomorphic grid keyboard in place of the piano keys
        {
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            e.selectMode (ui::Mode::Lfo);
            e.selectView (4); // LFO | Mod | Morph | Motion | Harmony
            setPlain (p, "harmony.p2.follow", 2.0f);
            const std::vector<std::vector<int>> prog = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55, 60 }, { 55, 59, 62 }, { 50, 53, 57 },
                                                         { 52, 56, 59 }, { 53, 57, 60 }, { 55, 59, 62, 65 } };
            for (const auto& c : prog)
            {
                juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
                juce::MidiBuffer on;
                for (int n : c) on.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
                p.processBlock (buffer, on);
                for (int k = 0; k < 40; ++k) { juce::MidiBuffer none; p.processBlock (buffer, none); }
                juce::MidiBuffer off;
                for (int n : c) off.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                p.processBlock (buffer, off);
            }
            e.showGridKeyboard (true);
            playAnimated (p, e, 12, { 57, 60, 64, 67 });
            playAnimated (p, e, 20);
            save (e, dir.getChildFile ("ohpe-harmony" + suffix + ".png"));
            // p59: encoder page "Grid" - Grid Layout, In Key / Chromatic, Fixed C (In Key on for the picture)
            setPlain (p, "harmony.kb.inkey", 1.0f);
            e.displayComponent().onPageChange (1);
            playAnimated (p, e, 12);
            save (e, dir.getChildFile ("ohpe-harmony-grid" + suffix + ".png"));
            e.displayComponent().onPageChange (-1);
            setPlain (p, "harmony.kb.inkey", 0.0f);
            release (p, { 57, 60, 64, 67 });
            e.showGridKeyboard (false);
            setPlain (p, "harmony.p2.follow", 0.0f);
            p.core().harmony().requestClear();
            run (p, 8);
            e.selectView (0);
        }

        // p27: the update view - a newer version on offer with its notes in the UI language, a content pack (no network)
        {
            update::Release r;
            r.version = update::Version::of ("0.8.0");
            r.tag = "v0.8.0";
            r.channel = update::Channel::Preview;
            r.date = "2026-11-02";
            r.notesEn = "**Preview release** - for testing and feedback.\n\n### In-app updates\n- Check for new versions from inside Ohpe, "
                        "download in the background, verified before it opens.\n- Stable and preview channels, skip a version or be reminded later.\n\n"
                        "### Content packs\n- New factory presets without reinstalling; the previous pack is kept for a rollback.\n\n"
                        "### Fixes\n- Smoother glide on held chords.\n- The library remembers the last filter.";
            r.notesDe = "**Vorschau-Release** - zum Testen und für Rückmeldungen.\n\n### Updates in der App\n- Neue Versionen direkt in Ohpe finden, "
                        "im Hintergrund laden, vor dem Öffnen geprüft.\n- Kanäle Stabil und Vorschau, eine Version überspringen oder später erinnern.\n\n"
                        "### Inhaltspakete\n- Neue Werks-Presets ohne Neuinstallation; das vorige Paket bleibt für ein Zurücksetzen.\n\n"
                        "### Korrekturen\n- Weicheres Glide bei gehaltenen Akkorden.\n- Die Bibliothek merkt sich den letzten Filter.";
            update::Asset a;
            a.platform = update::currentPlatform();
            a.kind = "installer";
            a.part = "all";
            a.name = "Ohpe-0.8.0-Installer";
            a.url = "https://example.invalid/Ohpe-0.8.0";
            a.size = 48u * 1024u * 1024u;
            a.sha256 = std::string (64, 'a');
            r.assets.push_back (a);
            update::ContentPack pk;
            pk.id = "night-keys";
            pk.version = "1.0.0";
            pk.kind = "presets";
            pk.nameEn = "Night Keys (24 presets)";
            pk.nameDe = juce::String::fromUTF8 ("Nachttasten (24 Presets)").toStdString();
            pk.url = "https://example.invalid/night-keys.zip";
            pk.sha256 = std::string (64, 'b');
            r.content.push_back (pk);
            e.updaterInstance().showReleaseForTests (r);
            e.showUpdates (true);
            e.advanceAnimation (0.2f);
            animate (p, e, 2);
            save (e, dir.getChildFile ("ohpe-update" + suffix + ".png"));
            e.showUpdates (false);
        }

        // p32: the settings menu (light and night skin), the About page, the help overlay; the user settings of the
        // render live in a temporary folder (the real ones are never touched)
        {
            auto& store = e.userSettingsStore();
            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ohpe-shot-settings");
            tmp.deleteRecursively();
            store.setFolderForTests (tmp);
            auto set = [&] (std::function<void (settings::UserSettings&)> fn) {
                auto v = store.get();
                fn (v);
                store.set (v);
                e.applyUserSettings();
            };
            auto shot = [&] (const char* name) {
                e.advanceAnimation (0.2f);
                animate (p, e, 2);
                save (e, dir.getChildFile (juce::String (name) + suffix + ".png"));
            };
            for (int dark = 0; dark < 2; ++dark)
            {
                set ([dark] (settings::UserSettings& v) { v.skin = dark ? settings::Skin::Dark : settings::Skin::Light; });
                e.selectPart (0);
                e.selectMode (ui::Mode::Synth);
                e.selectEngine (EngineType::Analog);
                if (dark)
                {
                    playAnimated (p, e, 2, { 60 });
                    playAnimated (p, e, 20, { 67 });
                    shot ("ohpe-dark"); // the whole plate in the night skin
                    e.showTour (true, 0); // p64: the first-start tour in the night skin
                    shot ("ohpe-tour-dark");
                    e.showTour (false);
                    release (p, { 60, 67 });
                }
                {   // p47: the header's left block (version legend, logo mark, wordmark, engine caption) and its right
                    // block (keys flush on x 1160, CPU meter), 3x close-ups
                    e.advanceAnimation (0.2f);
                    const juce::String sk = dark ? "-dark" : "";
                    save (e, dir.getChildFile ("ohpe-wordmark" + sk + suffix + ".png"), 3.0f, juce::Rectangle<int> (24, 12, 280, 96));
                    save (e, dir.getChildFile ("ohpe-header-right" + sk + suffix + ".png"), 3.0f, juce::Rectangle<int> (744, 12, 432, 96));
                }
                set ([] (settings::UserSettings& v) { v.helpOverlay = true; });
                shot (dark ? "ohpe-help-overlay-dark" : "ohpe-help-overlay");
                if (! dark) // p48: the help overlay with the grid keyboard (its steps over the keyboard)
                {
                    e.showGridKeyboard (true);
                    shot ("ohpe-grid-help");
                    e.showGridKeyboard (false);
                }
                set ([] (settings::UserSettings& v) { v.helpOverlay = false; });
                if (! dark)
                {   // p82: tooltips (an encoder: what its value does and the MIDI learn hint; the REC key) and the help search
                    e.selectPart (0);
                    e.selectMode (ui::Mode::Synth);
                    e.selectEngine (EngineType::Analog);
                    e.advanceAnimation (0.2f);
                    auto tipShot = [&] (juce::Component& c, const juce::String& text, const char* name) {
                        auto img = e.createComponentSnapshot (e.getLocalBounds(), true, 1.0f);
                        {
                            juce::Graphics g (img);
                            auto& lf = e.getLookAndFeel();
                            const auto at = e.getLocalPoint (&c, juce::Point<int> (c.getWidth() / 2, c.getHeight() / 2));
                            const auto tb = lf.getTooltipBounds (text, at, e.getLocalBounds());
                            g.setOrigin (tb.getPosition());
                            lf.drawTooltip (g, text, tb.getWidth(), tb.getHeight());
                        }
                        const auto f = dir.getChildFile (juce::String (name) + suffix + ".png");
                        f.deleteFile();
                        juce::FileOutputStream out (f);
                        juce::PNGImageFormat().writeImageToStream (img, out);
                        std::cout << "wrote " << f.getFullPathName() << std::endl;
                    };
                    tipShot (e.encoder (2), e.encoder (2).getTooltip(), "ohpe-tooltip-encoder");
                    tipShot (e.recordKey(), e.recordKey().getTooltip(), "ohpe-tooltip-key");
                    e.openHelpSearch (lang == i18n::Language::German ? juce::String::fromUTF8 ("H\xc3\xbcllkurve") : juce::String ("envelope"));
                    shot ("ohpe-help-search");
                    auto& sv = e.settingsComponent();
                    sv.setSearchText ("cutoff");
                    for (size_t i = 0; i < sv.searchResults().size(); ++i)
                        if (sv.searchResults()[i].entry.kind == ui::tips::Kind::Parameter) { sv.clickSearchResult ((int) i); break; }
                    shot ("ohpe-help-search-jump"); // the parameter's page, its encoder ringed in yellow
                    e.showSettings (false);
                }
                e.showSettings (true, dark ? ui::SettingsView::Display : ui::SettingsView::General);
                shot (dark ? "ohpe-settings-dark" : "ohpe-settings");
                {   // MIDI and MIDI learn with a few mappings
                    auto map = [&] (const char* id, MidiSourceType t, int ch, int num, MidiMapMode m, float lo = 0.0f, float hi = 1.0f) {
                        MidiMapping mm;
                        mm.paramId = id;
                        mm.type = t;
                        mm.channel = ch;
                        mm.number = num;
                        mm.mode = m;
                        mm.minNorm = lo;
                        mm.maxNorm = hi;
                        p.core().midiMap().add (mm);
                    };
                    map ("p1.analog.cutoff", MidiSourceType::CC, 1, 74, MidiMapMode::Absolute);
                    map ("p1.analog.reso", MidiSourceType::CC, 1, 71, MidiMapMode::Absolute, 0.0f, 0.5f);
                    map ("tempo", MidiSourceType::CC, 0, 20, MidiMapMode::Relative);
                    map ("tape.tx.play", MidiSourceType::Note, 10, 36, MidiMapMode::Toggle);
                    map ("p2.vol", MidiSourceType::CC, 2, 7, MidiMapMode::Absolute, 1.0f, 0.0f);
                    p.core().midiMapChanged();
                    setPlain (p, "p2.mpe", 1.0f);
                    e.showSettings (true, ui::SettingsView::Midi);
                    shot (dark ? "ohpe-settings-midi-dark" : "ohpe-settings-midi");
                    e.showSettings (true, ui::SettingsView::MidiLearn);
                    shot (dark ? "ohpe-settings-learn-dark" : "ohpe-settings-learn");
                    p.clearAllMidiMappings();
                    setPlain (p, "p2.mpe", 0.0f);
                }
                if (! dark)
                {   // p50: part 1 in Fingerboard mode, a controller sending MPE HD data, its smoothing cutoffs and bend range
                    setPlain (p, "p1.mpe", 1.0f);
                    setPlain (p, "p1.mpe.bend", 96.0f);
                    setPlain (p, "p1.mpe.onset", 1.0f);
                    setPlain (p, "p1.mpe.hd", 1.0f);
                    run (p, 2);
                    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
                    juce::MidiBuffer midi;
                    auto cc = [&midi] (int ch, int n, int v) { midi.addEvent (juce::MidiMessage::controllerEvent (ch, n, v), 0); };
                    for (int d = 0; d < 3; ++d) { cc (2, 99, 0); cc (2, 98, 100 + d); cc (2, 6, d == 0 ? 60 : d == 1 ? 30 : 125); }
                    cc (2, 99, 127); cc (2, 98, 127);
                    cc (2, 101, 0); cc (2, 100, 0); cc (2, 6, 96); cc (2, 101, 127); cc (2, 100, 127);
                    cc (2, 87, 64);
                    midi.addEvent (juce::MidiMessage::channelPressureChange (2, 0), 0);
                    p.processBlock (buffer, midi);
                    run (p, 2);
                    e.showSettings (true, ui::SettingsView::Midi);
                    shot ("ohpe-settings-fingerboard");
                    setPlain (p, "p1.mpe.onset", 0.0f);
                    setPlain (p, "p1.mpe.bend", 48.0f);
                    setPlain (p, "p1.mpe", 0.0f);
                    run (p, 2);
                }
                if (! dark)
                {   // p46: the controller section with the virtual controller view (a mock device: no MIDI port needed);
                    // the four display lines of three pages as text next to the shots
                    auto& view = e.settingsComponent();
                    view.setStandalone (true);
                    auto& ctl = e.gridControllerForTests();
                    ctl.attachForTests ([] (const juce::MidiMessage&) {});
                    set ([] (settings::UserSettings& v) { v.controllerProfile = settings::ControllerGrid1; v.controllerIn = "Grid Port 1"; v.controllerOut = "Grid Port 1"; });
                    ctl.attachForTests ([] (const juce::MidiMessage&) {});
                    juce::String text;
                    for (auto m : { ui::Mode::Synth, ui::Mode::Envelope, ui::Mode::Mixer })
                    {
                        e.selectMode (m);
                        ctl.update();
                        const auto lines = ctl.displayLines();
                        text << "+" << juce::String::repeatedString ("-", 68) << "+\n";
                        for (const auto& l : lines) text << "|" << juce::String (l) << "|\n";
                        text << "+" << juce::String::repeatedString ("-", 68) << "+\n\n";
                    }
                    dir.getChildFile ("ohpe-controller-display" + suffix + ".txt").replaceWithText (text);
                    e.selectMode (ui::Mode::Synth);
                    ctl.update();
                    // p53: the Controller page - automatic ports found (user port in and out), connected, two pads held
                    set ([] (settings::UserSettings& v) { v.controllerProfile = settings::ControllerGrid1; v.controllerAuto = true; v.controllerChosen = true; });
                    ctl.attachForTests ([] (const juce::MidiMessage&) {});
                    ctl.setPortNamesForTests ("Pad Grid User Port", "Pad Grid User Port");
                    ctl.receive (juce::MidiMessage::noteOn (1, ohpe::gridctl::padNote (2, 1), (juce::uint8) 100));
                    ctl.receive (juce::MidiMessage::noteOn (1, ohpe::gridctl::padNote (4, 2), (juce::uint8) 100));
                    ctl.receive (juce::MidiMessage::noteOn (1, 3, (juce::uint8) 127)); // encoder 4 touched: its full name on line 3
                    ctl.update();
                    e.showSettings (true, ui::SettingsView::Controller);
                    shot ("ohpe-settings-controller");
                    ctl.receive (juce::MidiMessage::noteOff (1, ohpe::gridctl::padNote (2, 1)));
                    ctl.receive (juce::MidiMessage::noteOff (1, ohpe::gridctl::padNote (4, 2)));
                    ctl.receive (juce::MidiMessage::noteOff (1, 3));
                    ctl.update();
                    e.showSettings (true, ui::SettingsView::Midi);
                    shot ("ohpe-settings-midi-controller-on");
                    ctl.attachForTests (nullptr);
                    set ([] (settings::UserSettings& v) { v.controllerProfile = settings::ControllerOff; v.controllerChosen = true; v.controllerIn.clear(); v.controllerOut.clear(); });
                    view.setStandalone (false);
                }
                if (! dark)
                {   // p49: the controller section in a plug-in - automatic ports (the names found), the instance holding it
                    auto& view = e.settingsComponent();
                    view.setStandalone (false);
                    auto& ctl = e.gridControllerForTests();
                    ctl.attachForTests ([] (const juce::MidiMessage&) {});
                    set ([] (settings::UserSettings& v) { v.controllerProfile = settings::ControllerGrid1; v.controllerAuto = true; });
                    ctl.attachForTests ([] (const juce::MidiMessage&) {});
                    ctl.setPortNamesForTests ("Pad Grid User Port", "Pad Grid User Port");
                    ctl.claim();
                    ctl.update();
                    e.showSettings (true, ui::SettingsView::Controller);
                    shot ("ohpe-settings-controller-plugin");
                    ctl.attachForTests (nullptr);
                    set ([] (settings::UserSettings& v) { v.controllerProfile = settings::ControllerOff; v.controllerChosen = true; });
                }
                e.settingsComponent().setHelpTopic (1);
                e.showSettings (true, ui::SettingsView::Help);
                shot (dark ? "ohpe-settings-help-dark" : "ohpe-settings-help");
                if (! dark) // p48: the help page of the grid keyboard
                {
                    for (int t = 0; t < ui::SettingsView::numHelpTopics(); ++t)
                        if (ui::SettingsView::helpTitle (t) == i18n::tr ("Grid keyboard")) e.settingsComponent().setHelpTopic (t);
                    e.showSettings (true, ui::SettingsView::Help);
                    shot ("ohpe-settings-help-grid");
                }
                e.showSettings (true, ui::SettingsView::About);
                shot (dark ? "ohpe-about-dark" : "ohpe-about");
                save (e, dir.getChildFile (juce::String (dark ? "ohpe-about-wordmark-dark" : "ohpe-about-wordmark") + suffix + ".png"), 3.0f,
                      juce::Rectangle<int> (248, 164, 340, 116)); // p47: the About header, 3x
                e.showSettings (false);
                {   // p39: tempo, metronome and count-in - the tape page one beat into a one-bar count-in (the countdown,
                    // the header tempo with TAP, the transport panel's tempo / click / count-in), a 2x crop of the panel
                    e.selectMode (ui::Mode::Tape);
                    setPlain (p, "metro.on", 1.0f);
                    setPlain (p, "metro.countin", 1.0f);
                    run (p, 2);
                    p.core().tape().housekeeping();
                    e.transport (OhpeProcessor::TransportKey::Record);
                    for (int k = 0; k < 10; ++k) { run (p, 6); e.advanceAnimation (1.0f / 30.0f); } // 1.3 beats at 120 BPM
                    run (p, 1);
                    e.advanceAnimation (1.0f / 30.0f); // the click key lights on the beat's flash
                    save (e, dir.getChildFile (juce::String (dark ? "ohpe-metronome-dark" : "ohpe-metronome") + suffix + ".png"));
                    const auto panel = OhpeEditor::designArea ("transportPanel").expanded (8.0f).toNearestInt();
                    save (e, dir.getChildFile (juce::String (dark ? "ohpe-tempo-panel-dark" : "ohpe-tempo-panel") + suffix + ".png"), 2.0f, panel);
                    e.transport (OhpeProcessor::TransportKey::Stop);
                    run (p, 2);
                    p.core().tape().housekeeping();
                    e.transport (OhpeProcessor::TransportKey::Stop); // to the start
                    setPlain (p, "tape.tr1.arm", 0.0f);
                    setPlain (p, "tape.tr2.arm", 0.0f);
                    setPlain (p, "metro.on", 0.0f);
                    setPlain (p, "metro.countin", 0.0f);
                    run (p, 2);
                    e.selectMode (ui::Mode::Synth);
                }
                {   // p45: the chord under the voice dots and the note readout (no overlap), the key with Scale Lock on
                    // the header - the vocal page in F sharp minor, F#m7 held with F#5 on top; a 2x crop of the display
                    e.selectPart (0);
                    e.selectMode (ui::Mode::Synth);
                    e.selectEngine (EngineType::Vocal);
                    p.setKeyFromUi (6, scales::Minor);
                    p.setScaleLockFromUi (true, keys::LockNearest);
                    playAnimated (p, e, 8, { 54, 57, 61, 64, 78 });
                    save (e, dir.getChildFile (juce::String (dark ? "ohpe-chords-dark" : "ohpe-chords") + suffix + ".png"));
                    const auto disp = OhpeEditor::designArea ("display").expanded (4.0f).toNearestInt();
                    save (e, dir.getChildFile (juce::String (dark ? "ohpe-chords-display-dark" : "ohpe-chords-display") + suffix + ".png"), 2.0f, disp);
                    release (p, { 54, 57, 61, 64, 78 });
                    p.setScaleLockFromUi (false, keys::LockNearest);
                    p.setKeyFromUi (0, scales::Major);
                    e.selectEngine (EngineType::Analog);
                    run (p, 2);
                }
                {   // p41: the 3D switch on the 3D Space view - Off (the hint on its own line), Headphones, Speakers
                    e.selectPart (0);
                    e.selectEngine (EngineType::Wavetable);
                    setChoice (p, "p1.space.path", "Circle");
                    setChoice (p, "p1.space.pathlen", "2 Bars");
                    setPlain (p, "p1.space.distance", 2.0f);
                    e.selectMode (ui::Mode::Effects);
                    e.selectView (3);
                    p.syncSpaceModes();
                    const char* names[] = { "off", "headphones", "speakers" };
                    for (int m = 0; m < 3; ++m)
                    {
                        setPlain (p, "p1.space.mode", (float) m);
                        p.syncSpaceModes();
                        playAnimated (p, e, 2, { 60 });
                        playAnimated (p, e, 30);
                        release (p, { 60 });
                        save (e, dir.getChildFile (juce::String ("ohpe-space-switch-") + names[m] + (dark ? "-dark" : "") + suffix + ".png"));
                    }
                    setPlain (p, "p1.space.mode", 0.0f);
                    setPlain (p, "space.out", 0.0f);
                    p.syncSpaceModes();
                    setChoice (p, "p1.space.path", "Off");
                    setPlain (p, "p1.space.distance", 1.5f);
                    run (p, 2);
                    e.selectEngine (EngineType::Analog);
                    e.selectMode (ui::Mode::Synth);
                }
            }
            set ([] (settings::UserSettings& v) { v = settings::UserSettings {}; });
            ui::applySkin (ui::Skin::Light);
            e.setLanguage (lang);
        }

        // content packs: two installed .ohpepack files (temporary data folder): the pack manager page, the library
        // filtered by a pack
        {
            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ohpe-shot-packs");
            tmp.deleteRecursively();
            const auto prevData = juce::SystemStats::getEnvironmentVariable ("OHPE_DATA_DIR", {});
            setEnvVar ("OHPE_DATA_DIR", tmp.getChildFile ("data").getFullPathName());
            auto& up = e.updaterInstance();
            up.setDataFolderForTests (tmp.getChildFile ("data"));
            up.installPackFile (makeShotPack (tmp.getChildFile ("atmo-1.0.0.ohpepack"), "atmo", "Atmo", "Atmo", "Cinematic, dark and wide",
                                              "Filmisch, dunkel und weit", true,
                                              { { "Night Tide", "pads" }, { "Crypt Organ", "keys" }, { "Distant Horn", "leads" }, { "Abyss Reese", "bass" } }));
            up.waitIdle (20000);
            up.installPackFile (makeShotPack (tmp.getChildFile ("keys-plus-1.0.0.ohpepack"), "keys-plus", "Keys Plus", "Tasten Plus",
                                              "Bright keys and plucks", "Helle Tasten und Zupfer", false, { { "Glass Keys", "keys" }, { "Felt Pluck", "pluck" } }));
            up.waitIdle (20000);
            e.showSettings (true, e.packPage());
            e.advanceAnimation (0.2f);
            animate (p, e, 2);
            save (e, dir.getChildFile ("ohpe-packs" + suffix + ".png"));
            e.showSettings (true, ui::SettingsView::Updates);
            e.advanceAnimation (0.2f);
            save (e, dir.getChildFile ("ohpe-settings-updates" + suffix + ".png"));
            e.showSettings (false);
            e.showLibrary (true);
            auto& plib = e.libraryComponent();
            plib.setPackFilter ("atmo");
            if (plib.numRows() > 0) plib.selectRow (0);
            e.advanceAnimation (0.2f);
            save (e, dir.getChildFile ("ohpe-library-packs" + suffix + ".png"));
            plib.setPackFilter ({});
            e.showLibrary (false);
            for (const char* id : { "atmo", "keys-plus" }) up.removePack (id);
            up.setDataFolderForTests ({});
            setEnvVar ("OHPE_DATA_DIR", prevData);
            p.presets().rescan();
            tmp.deleteRecursively();
        }

        // p20: a user wavetable from a sound whose harmonics evolve (analysed like an imported file), the frame stack
        {
            const double sr = 48000.0, hz = 110.0;
            std::vector<float> sound ((size_t) (1.5 * sr), 0.0f);
            for (int h = 1; h <= 60; ++h)
                for (size_t i = 0; i < sound.size(); ++i)
                {
                    const double t = (double) i / (double) sound.size();
                    const double amp = (h == 1 ? 1.0 : (h % 2 ? 1.0 : t) * std::pow (t, 0.5) / h) * std::exp (-0.002 * h * h * (1.0 - t));
                    sound[i] += (float) (0.25 * amp * std::sin (2.0 * 3.14159265358979323846 * hz * h * (double) i / sr));
                }
            usertable::AnalysisOptions o;
            o.frames = 64;
            o.smoothing = 0.25f;
            const auto r = usertable::analyse (sound.data(), (int64_t) sound.size(), sr, o);
            if (r.ok)
            {
                usertable::setTable (p.core(), 0, usertable::Target::Wavetable, usertable::UserTable::create ("Rising Brass", r.frames, r.formants));
                e.selectPart (0);
                e.selectMode (ui::Mode::Synth);
                e.selectEngine (EngineType::Wavetable);
                setPlain (p, "p1.usertable.wt", 1.0f);
                setPlain (p, "p1.wavetable.pos", 0.62f);
                playAnimated (p, e, 30, { 57 });
                e.softKey (0); // refresh the soft keys (unknown id: no action)
                save (e, dir.getChildFile ("ohpe-user-wavetable" + suffix + ".png"));
                release (p, { 57 });
                usertable::setTable (p.core(), 0, usertable::Target::Wavetable, nullptr);
                setPlain (p, "p1.usertable.wt", 0.0f);
                setPlain (p, "p1.wavetable.pos", 0.25f);
                run (p, 8);
            }
        }

        // p23: bounce to engine - two bars of a tape-like chord loop bounced into part 1's Grain (endless loop)
        {
            p.bounces().setLibraryFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ohpe-shot-bounces"));
            const double sr = 48000.0;
            bounce::Audio a;
            a.rate = sr;
            const auto n = (size_t) (4.0 * sr);
            a.l.assign (n, 0.0f);
            a.r.assign (n, 0.0f);
            const double notes[] = { 45.0, 52.0, 57.0, 60.0, 64.0 };
            for (size_t i = 0; i < n; ++i)
            {
                const double t = (double) i / sr;
                const double beat = std::fmod (t, 0.5);
                const double env = std::exp (-beat * 3.0) * (0.6 + 0.4 * std::sin (t * 1.7));
                for (int k = 0; k < 5; ++k)
                {
                    const double f = 440.0 * std::pow (2.0, (notes[k] - 69.0) / 12.0);
                    a.l[i] += (float) (0.08 * env * std::sin (6.283185307179586 * f * t + k));
                    a.r[i] += (float) (0.08 * env * std::sin (6.283185307179586 * f * 1.002 * t + 2 * k));
                }
            }
            e.selectPart (0);
            e.selectMode (ui::Mode::Synth);
            const float engineBefore = p.core().params().get ("p1.engine");
            if (p.bounces().bounceAudio (std::move (a), 0, bounce::TargetGrain, true, "Tape 1"))
            {
                run (p, 4);
                e.selectEngine (EngineType::Grain);
                setPlain (p, "p1.grain.pos", 0.9f);
                setPlain (p, "p1.grain.scan", 0.6f);
                playAnimated (p, e, 30, { 57 });
                e.softKey (0); // refresh the soft keys (unknown id: no action)
                save (e, dir.getChildFile ("ohpe-bounce" + suffix + ".png"));
                release (p, { 57 });
                p.sampleLoader().clearGrain (0);
                setPlain (p, "p1.grain.pos", 0.3f);
                setPlain (p, "p1.grain.scan", 0.1f);
                setPlain (p, "p1.engine", engineBefore);
                run (p, 8);
                e.selectEngine ((EngineType) (int) engineBefore);
            }
        }

        // p66: Key Gravity - the part's two gravity pages at the end of the Effects pages, a chord held into the stage
        {
            setPlain (p, "p1.grav.amt", 0.8f);
            setPlain (p, "p1.grav.target", 2.0f); // Chord+Scale
            setPlain (p, "p1.grav.glide", 0.3f);
            setPlain (p, "p1.grav.curve", 0.4f);
            run (p, 4);
            e.selectPart (0);
            e.selectMode (ui::Mode::Effects);
            e.selectView (0);
            auto& disp = e.displayComponent();
            if (disp.onPageChange) disp.onPageChange (-e.shownPage() - 2); // the first Key Gravity page (second to last)
            playAnimated (p, e, 30, { 48, 55, 60, 64 });
            save (e, dir.getChildFile ("ohpe-key-gravity" + suffix + ".png"));
            release (p, { 48, 55, 60, 64 });
            // p70: the Key Gravity ring on the header next to the key - from the Synth page one click opens the gravity page
            setPlain (p, "p1.grav.amt", 0.65f);
            e.selectMode (ui::Mode::Synth);
            e.advanceAnimation (0.1f);
            if (disp.headerGravity().onOpen) disp.headerGravity().onOpen();
            playAnimated (p, e, 20, { 50, 57, 62, 65 });
            save (e, dir.getChildFile ("ohpe-gravity-header" + suffix + ".png"));
            release (p, { 50, 57, 62, 65 });
            if (disp.onPageChange) disp.onPageChange (-e.shownPage());
            setPlain (p, "p1.grav.amt", 0.0f);
            setPlain (p, "p1.grav.target", 2.0f);
            setPlain (p, "p1.grav.glide", 0.15f);
            setPlain (p, "p1.grav.curve", 0.0f);
            run (p, 8);
        }

        // p67: tape memory - a phrase of part 1 played into the memory, CAPTURE (Slices) into part 2's Sampler
        {
            p.core().tapeMemory().clear();
            for (int k = 0; k < 8; ++k)
            {
                run (p, 22, 48 + (k * 7) % 19);
                release (p, { 48 + (k * 7) % 19 });
            }
            run (p, 30);
            const float engineBefore = p.core().params().get ("p2.engine");
            if (p.tapeMemory().capture (1, tapemem::ModeSlices))
            {
                p.tapeMemory().finishNow();
                run (p, 4);
                e.selectPart (1);
                e.selectMode (ui::Mode::Synth);
                e.selectEngine (EngineType::Sampler);
                e.softKey (0); // refresh the soft keys (unknown id: no action)
                e.advanceAnimation (0.05f);
                save (e, dir.getChildFile ("ohpe-tapemem" + suffix + ".png"));
                if (auto* smp = dynamic_cast<SamplerEngine*> (p.core().part (1).engine (EngineType::Sampler))) smp->setSample (nullptr);
                setPlain (p, "p2.sampler.mode", 0.0f);
                setPlain (p, "p2.engine", engineBefore);
                run (p, 8);
                e.selectEngine ((EngineType) (int) engineBefore);
                e.selectPart (0);
            }
            // the demo render (once): the phrase as played, then CAPTURE (Chromatic) into part 1 playing a melody
            if (suffix.isEmpty())
            {
                juce::AudioBuffer<float> rec (2, 0);
                auto take = [&p, &rec] (int blocks, int note, bool off) {
                    juce::AudioBuffer<float> b (p.getTotalNumOutputChannels(), 512);
                    for (int k = 0; k < blocks; ++k)
                    {
                        juce::MidiBuffer m;
                        if (k == 0 && note >= 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                        if (k == blocks - 1 && off && note >= 0) m.addEvent (juce::MidiMessage::noteOff (1, note), 511);
                        b.clear();
                        p.processBlock (b, m);
                        const int at = rec.getNumSamples();
                        rec.setSize (2, at + 512, true, true);
                        for (int c = 0; c < 2; ++c) rec.copyFrom (c, at, b, juce::jmin (c, b.getNumChannels() - 1), 0, 512);
                    }
                };
                p.core().tapeMemory().clear();
                const int phrase[] = { 57, 60, 64, 62 };
                for (int n : phrase) take (24, n, true);
                take (60, -1, false);
                const float p1Before = p.core().params().get ("p1.engine");
                p.core().params().set (Core::Ids::tapememLen, (float) tapemem::Len5s);
                if (p.tapeMemory().capture (0, tapemem::ModeChromatic))
                {
                    p.tapeMemory().finishNow();
                    take (8, -1, false);
                    const int melody[] = { 45, 52, 57, 60, 64, 69, 72, 76 };
                    for (int n : melody) take (20, n, true);
                    take (90, -1, false);
                    juce::WavAudioFormat wav;
                    const auto file = dir.getChildFile ("ohpe-tapemem-demo.wav");
                    file.deleteFile();
                    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
                    if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (24)))
                        w->writeFromAudioSampleBuffer (rec, 0, rec.getNumSamples());
                    if (auto* smp = dynamic_cast<SamplerEngine*> (p.core().part (0).engine (EngineType::Sampler))) smp->setSample (nullptr);
                    setPlain (p, "p1.engine", p1Before);
                    run (p, 8);
                }
                p.core().params().set (Core::Ids::tapememLen, (float) tapemem::Len30s);
            }
            p.core().tapeMemory().clear();
        }

        // p33: the Expression page - aftertouch -> vibrato and cutoff on the analog part, a note held with pressure
        {
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            setPlain (p, "p1.expr.vel.dst1", 1.0f); // Cutoff
            setPlain (p, "p1.expr.vel.amt1", 0.45f);
            setPlain (p, "p1.expr.at.dst1", 11.0f); // Vibrato
            setPlain (p, "p1.expr.at.amt1", 0.6f);
            setPlain (p, "p1.expr.at.dst2", 1.0f); // Cutoff
            setPlain (p, "p1.expr.at.amt2", -0.35f);
            setPlain (p, "p1.expr.at.curve2", 2.0f); // Logarithmic
            e.selectMode (ui::Mode::Envelope);
            e.displayComponent().onPageChange (5); // Velocity, Aftertouch: the sixth page
            run (p, 2);
            {
                juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
                midi.addEvent (juce::MidiMessage::channelPressureChange (1, 80), 8);
                p.processBlock (buffer, midi);
            }
            playAnimated (p, e, 20);
            save (e, dir.getChildFile ("ohpe-expression" + suffix + ".png"));
            {
                juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::channelPressureChange (1, 0), 0);
                p.processBlock (buffer, midi);
            }
            release (p, { 57 });
            for (const char* id : { "p1.expr.vel.amt1", "p1.expr.at.amt1", "p1.expr.at.amt2" }) setPlain (p, id, 0.0f);
            e.selectMode (ui::Mode::Synth);
        }

        // p18: the breeding view - two library sounds on two engines, a brood of six with thumbnails, two kept
        {
            auto byName = [&] (const char* name) {
                const auto& list = p.presets().entries();
                for (int i = 0; i < (int) list.size(); ++i)
                    if (list[(size_t) i].meta.name == name && list[(size_t) i].meta.kind == preset::Kind::Part) return i;
                return -1;
            };
            e.selectPart (0);
            e.selectEngine (EngineType::Analog);
            e.showBreeding (true);
            auto& bv = e.breedingComponent();
            bv.session().clear();
            bv.setSeedBase (18);
            bv.setParentFromLibrary (0, byName ("Glass Shimmer"));
            bv.setParentFromLibrary (1, byName ("Crystal FM Pad"));
            bv.setAmount (0.35f);
            bv.breedNew();
            bv.waitForSketches (180000);
            bv.toggleKept (1);
            bv.toggleKept (4);
            bv.audition (1, false);
            e.advanceAnimation (0.2f);
            save (e, dir.getChildFile ("ohpe-breeding" + suffix + ".png"));
            // p69: learning breed - the kept children and a favourite taught the profile; WHY shows what it learned
            p.presets().setFavourite (byName ("Glass Shimmer"), true);
            bv.setCuriosity (0.3f);
            bv.breedNew();
            bv.waitForSketches (180000);
            bv.setShowWhy (true);
            e.advanceAnimation (0.2f);
            save (e, dir.getChildFile ("ohpe-breeding-learn" + suffix + ".png"));
            bv.setShowWhy (false);
            p.presets().setFavourite (byName ("Glass Shimmer"), false);
            bv.close (false);
            run (p, 8);
        }

        // 0.3: preset library with a category filter and a preset selected
        e.showLibrary (true);
        auto& lib = e.libraryComponent();
        lib.toggleTag ("Pads");
        lib.toggleTag ("Keys");
        if (lib.numRows() > 1) lib.selectRow (1);
        e.advanceAnimation (0.2f);
        save (e, dir.getChildFile ("ohpe-library" + suffix + ".png"));
        lib.showSavePanel (true);
        lib.setSaveFields ("Night Keys", "groovex", { "Keys", "Pads" }, false);
        save (e, dir.getChildFile ("ohpe-library-save" + suffix + ".png"));
        lib.showSavePanel (false);
        lib.toggleTag ("Pads");
        lib.toggleTag ("Keys");
        // p54: the search across categories and tags - "MPE" with the Pads category (German: Flächen)
        lib.setSearchText ("MPE");
        lib.toggleTag ("Pads");
        if (lib.numRows() > 0) lib.selectRow (0);
        e.advanceAnimation (0.2f);
        save (e, dir.getChildFile ("ohpe-library-search" + suffix + ".png"));
        lib.toggleTag ("Pads");
        lib.setSearchText ({});
        e.showLibrary (false);
    }
    return 0;
}
