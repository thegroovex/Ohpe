// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Renders the Ohpe editor into PNG files for the documentation (headless, e.g. under xvfb).
//   usage: ohpe_screenshot <output directory>
// Writes ohpe-<view>.png (English) and ohpe-<view>-de.png (German).
// 0.3: also the library, the pro view, the tape page, step locks and the drum kit page.
// p20: the user wavetable frame stack (ohpe-user-wavetable).
// p23: bounce to engine - a loop bounced into Grain (ohpe-bounce).
// p11: the tape page while recording (ohpe-tape-recording). p14: the transport block, the keyboard row and the
// octave block while recording (ohpe-transport, 2x crop; REC and PLAY at the top of their breathing).
// 0.7: the Mod view (modulation matrix), the part's FX rack and the master rack; the Mid/Side scope,
// the master scope view and the spectrum terrain. 1.0: the 3D Space view and the 3D badge. The
// preset library reads the user folder from OHPE_PRESET_DIR when set (otherwise the default).

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "ui/ProTiles.h"
#include "mod/ModVoice.h"

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
            release (p, { 57, 60, 64, 67 });
            e.showGridKeyboard (false);
            setPlain (p, "harmony.p2.follow", 0.0f);
            p.core().harmony().requestClear();
            run (p, 8);
            e.selectView (0);
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
        e.showLibrary (false);
    }
    return 0;
}
