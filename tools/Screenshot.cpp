// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Renders the Ohpe editor into PNG files for the documentation (headless, e.g. under xvfb).
//   usage: ohpe_screenshot <output directory>
// Writes ohpe-<view>.png (English) and ohpe-<view>-de.png (German).
// 0.3: also the library, the pro view, the tape page, step locks and the drum kit page.
// p11: the tape page while recording (ohpe-tape-recording). p14: the transport block, the keyboard row and the
// octave block while recording (ohpe-transport, 2x crop; REC and PLAY at the top of their breathing).
// 0.7: the Mod view (modulation matrix), the part's FX rack and the master rack; the Mid/Side scope,
// the master scope view and the spectrum terrain. 1.0: the 3D Space view and the 3D badge. The
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
            release (p, { 60, 64, 67 });
        }

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
            release (p, { 48, 55 });
            for (int part = 1; part <= 3; ++part) setPlain (p, "p" + juce::String (part) + ".space.on", 0.0f);
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
