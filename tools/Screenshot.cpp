// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Renders the Ohpe editor into PNG files for the documentation (headless, e.g. under xvfb).
//   usage: ohpe_screenshot <output directory>
// Writes ohpe-<view>.png (English) and ohpe-<view>-de.png (German).
// 0.3: also the library, the pro view, the tape page, step locks and the drum kit page.
// 0.7: the Mid/Side scope, the master scope view and the spectrum terrain. The
// preset library reads the user folder from OHPE_PRESET_DIR when set (otherwise the default).

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

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
void release (OhpeProcessor& p, std::initializer_list<int> notes)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    for (int n : notes) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
    p.processBlock (buffer, midi);
    run (p, 8);
}

/** 0.7: plays and animates together, like the editor timer does (30 frames per second). */
void animate (OhpeProcessor& p, OhpeEditor& e, int frames, std::initializer_list<int> notes = {}, int channel = 1)
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

void setPlain (OhpeProcessor& p, const juce::String& id, float plain)
{
    if (auto* prm = p.parameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

void save (juce::Component& c, const juce::File& f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
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
        animate (p, e, 2, { 60 });
        animate (p, e, 2, { 64 });
        animate (p, e, 30, { 67 });
        save (e, dir.getChildFile ("ohpe-analog" + suffix + ".png"));

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

        // 0.3: tape page - part 1 recorded onto tracks 1 and 2, track 2 selected, loop set
        {
            p.parameter ("tape.tr1.arm")->setValueNotifyingHost (1.0f);
            p.parameter ("tape.tr2.arm")->setValueNotifyingHost (1.0f);
            p.parameter ("tape.tr2.src")->setValueNotifyingHost (p.parameter ("tape.tr2.src")->convertTo0to1 (1.0f)); // part 1
            p.parameter ("tape.loop")->setValueNotifyingHost (1.0f);
            p.parameter ("tape.loopin")->setValueNotifyingHost (p.parameter ("tape.loopin")->convertTo0to1 (1.0f));
            p.parameter ("tape.loopout")->setValueNotifyingHost (p.parameter ("tape.loopout")->convertTo0to1 (5.0f));
            p.parameter ("tape.rec")->setValueNotifyingHost (1.0f);
            p.parameter ("tape.play")->setValueNotifyingHost (1.0f);
            for (int k = 0; k < 12; ++k)
            {
                run (p, 12, 48 + (k * 5) % 24);
                release (p, { 48 + (k * 5) % 24 });
                p.core().tape().housekeeping();
            }
            p.parameter ("tape.rec")->setValueNotifyingHost (0.0f);
            run (p, 4);
            p.core().tape().housekeeping();
            p.parameter ("tape.tr1.arm")->setValueNotifyingHost (0.0f);
            p.parameter ("tape.tr2.arm")->setValueNotifyingHost (0.0f);
            p.parameter ("tape.play")->setValueNotifyingHost (0.0f);
            run (p, 2);
            e.selectMode (ui::Mode::Tape);
            e.selectTapeTrack (1);
            e.advanceAnimation (0.7f);
            save (e, dir.getChildFile ("ohpe-tape" + suffix + ".png"));
        }

        // 0.3: drum kit page
        e.selectMode (ui::Mode::Drum);
        e.selectDrumPad (2);
        e.advanceAnimation (0.4f);
        save (e, dir.getChildFile ("ohpe-drum" + suffix + ".png"));

        // 0.3: pro view of the FM engine on part 2
        e.selectPart (1);
        e.selectEngine (EngineType::FM);
        e.showProView (true);
        e.advanceAnimation (0.2f);
        save (e, dir.getChildFile ("ohpe-pro-view" + suffix + ".png"));
        e.showProView (false);
        e.selectPart (0);

        // 0.7: Mid/Side FX slot on part 1 (a wide cluster chord), the stereo scope on its page
        {
            e.selectPart (0);
            e.selectEngine (EngineType::Cluster);
            setPlain (p, "p1.cluster.spread", 1.0f);
            setPlain (p, "p1.fx.type", (float) InsertSlot::MidSide);
            setPlain (p, "p1.fx.midside.width", 1.45f);
            setPlain (p, "p1.fx.midside.side", 1.5f);
            setPlain (p, "p1.fx.midside.bassmono", 1.0f);
            setPlain (p, "p1.fx.midside.haas", 0.25f);
            e.selectMode (ui::Mode::Effects);
            auto& d = e.displayComponent();
            d.onPageChange (3);
            animate (p, e, 40, { 50, 57, 62, 65 });
            save (e, dir.getChildFile ("ohpe-midside-scope" + suffix + ".png"));
            release (p, { 50, 57, 62, 65 });

            // the master scope as the display view (a pad with reverb; Mid/Side in the master rack, a little wider)
            setPlain (p, "p1.fx.type", 0.0f);
            setPlain (p, "mix.ch1.sendb", 0.45f);
            setPlain (p, "mfx1.type", (float) InsertSlot::MidSide);
            setPlain (p, "mfx1.midside.width", 1.25f);
            setPlain (p, "mfx1.midside.bassmono", 1.0f);
            p.core().params().set ("mfx1.type", (float) InsertSlot::MidSide);
            p.core().prepareEffects();
            e.selectMode (ui::Mode::Mixer);
            d.setVisual (ui::Display::Visual::Scope);
            animate (p, e, 40, { 45, 52, 60, 64 });
            save (e, dir.getChildFile ("ohpe-master-scope" + suffix + ".png"));
            release (p, { 45, 52, 60, 64 });

            // the spectrum terrain as the display view (a moving line of notes)
            e.selectEngine (EngineType::Wavetable);
            d.setVisual (ui::Display::Visual::Terrain);
            for (int k = 0; k < 8; ++k)
            {
                const int note = 48 + (k * 7) % 24;
                animate (p, e, 4, { note, note + 12 });
                release (p, { note, note + 12 });
            }
            animate (p, e, 6, { 55, 67, 74 });
            save (e, dir.getChildFile ("ohpe-terrain" + suffix + ".png"));
            release (p, { 55, 67, 74 });
            d.setVisual (ui::Display::Visual::Character);
            setPlain (p, "mfx1.type", 0.0f);
            setPlain (p, "mix.ch1.sendb", 0.0f);
            e.selectEngine (EngineType::Analog);
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
        e.showLibrary (false);
    }
    return 0;
}
