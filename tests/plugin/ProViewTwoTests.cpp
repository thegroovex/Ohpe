// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p21: pro view 2.0 - pictures on the group cards (edited in the picture), the Discover strip (character macros,
// VARY, cross, dice, locks, timeline), Simple / All, hover texts, modulation drag onto a picture, 1 - 4 columns.

#include <doctest/doctest.h>

#include "mod/ModVoice.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"
#include "ui/ProView.h"

using namespace ohpe;

namespace
{
void play (OhpeProcessor& p, int blocks, int note = -1, int channel = 1)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && note >= 0) midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
    }
}

juce::MouseEvent mouseEvent (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos, juce::ModifierKeys mods = {})
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, pos != downPos);
}

float value (OhpeProcessor& p, const char* id) { return p.parameter (id)->getValue(); }
float plain (OhpeProcessor& p, const char* id) { auto* q = p.parameter (id); return q->convertFrom0to1 (q->getValue()); }

struct Fixture
{
    Fixture()
    {
        p.prepareToPlay (48000.0, 256);
        ed.reset (p.createEditor());
        e = dynamic_cast<OhpeEditor*> (ed.get());
        e->setSize (1440, 960);
    }
    OhpeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    OhpeEditor* e = nullptr;
};
} // namespace

TEST_CASE ("pro view 2.0: the Synth cards carry pictures for every engine; Discover is shown in part modes")
{
    Fixture f;
    auto& e = *f.e;
    e.selectPart (1);
    e.selectMode (ui::Mode::Synth);
    e.showProView (true);
    auto& pv = e.proViewComponent();
    for (auto t : { EngineType::FM, EngineType::Analog, EngineType::Wavetable, EngineType::Pluck, EngineType::Cluster, EngineType::Grain,
                    EngineType::Vocal, EngineType::Sampler })
    {
        CAPTURE (engineTypeInfo (t).id);
        e.selectEngine (t);
        const int engine = pv.cardIndex ("Engine");
        REQUIRE (engine >= 0);
        CHECK (pv.tile (engine) != nullptr);
        CHECK (pv.tile (pv.cardIndex ("Amp Envelope")) != nullptr);
        CHECK (pv.tile (pv.cardIndex ("Mod Envelope")) != nullptr);
        CHECK (pv.tile (pv.cardIndex ("LFO")) != nullptr);
        CHECK (pv.tile (pv.cardIndex ("Pitch & Level")) != nullptr);
        CHECK (pv.showsDiscover());
        e.advanceAnimation (1.0f / 30.0f);
        CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    }
    // the analog engine's filter is a card of its own with the response picture
    e.selectEngine (EngineType::Analog);
    const int filter = pv.cardIndex ("Filter");
    REQUIRE (filter >= 0);
    CHECK (pv.tile (filter) != nullptr);
    const auto& ids = pv.cardGroup (filter).ids;
    CHECK (std::find (ids.begin(), ids.end(), "p2.analog.cutoff") != ids.end());
    // no parameter twice in the Synth groups
    std::vector<std::string> all;
    for (int c = 0; c < pv.numCards(); ++c)
        for (const auto& id : pv.cardGroup (c).ids)
        {
            CAPTURE (id);
            CHECK (std::find (all.begin(), all.end(), id) == all.end());
            all.push_back (id);
        }
    // effects: FX slots, sends and the 3D stage have pictures too; the drum kit has none (no Discover)
    e.selectMode (ui::Mode::Effects);
    CHECK (pv.numTiles() >= 3);
    e.selectMode (ui::Mode::Drum);
    CHECK (pv.numTiles() == 0);
    CHECK_FALSE (pv.showsDiscover());
}

TEST_CASE ("pro view 2.0: Simple shows the pictures only; All adds the bars; a mode without pictures keeps its bars")
{
    Fixture f;
    auto& e = *f.e;
    e.selectEngine (EngineType::FM);
    e.selectMode (ui::Mode::Synth);
    e.showProView (true);
    auto& pv = e.proViewComponent();
    auto visibleBars = [&] {
        int n = 0;
        for (int i = 0; i < pv.numBars(); ++i)
            if (pv.bar (i).isVisible()) ++n;
        return n;
    };
    CHECK_FALSE (pv.isSimple());
    CHECK (visibleBars() == pv.numBars());
    pv.setSimple (true);
    CHECK (visibleBars() == 0);
    for (int c = 0; c < pv.numCards(); ++c)
        if (pv.tile (c) == nullptr) CHECK (pv.cardBounds (c).isEmpty());
    // the setting is part of the editor state
    f.e->showProView (false);
    f.e->showProView (true);
    CHECK (pv.isSimple());
    e.selectMode (ui::Mode::Drum);
    CHECK (pv.numBars() == 2 + 24 * 8);
    CHECK (visibleBars() == pv.numBars());
    e.selectMode (ui::Mode::Synth);
    pv.setSimple (false);
    // collapse one card: its bars go, the others stay
    const int amp = pv.cardIndex ("Amp Envelope");
    pv.setCollapsed (amp, true);
    CHECK (visibleBars() == pv.numBars() - 4);
    pv.setCollapsed (amp, false);
    CHECK (visibleBars() == pv.numBars());
}

TEST_CASE ("pro view 2.0: one to four columns by width")
{
    Fixture f;
    auto& e = *f.e;
    e.showProView (true);
    auto& pv = e.proViewComponent();
    e.setSize (600, 400);
    CHECK (pv.numColumns() >= 1);
    CHECK (pv.numColumns() <= 2);
    e.setSize (2400, 1600);
    CHECK (pv.numColumns() == 4);
}

TEST_CASE ("pro view 2.0: the envelope picture is edited directly; the filter takes X = cutoff, Y = resonance")
{
    Fixture f;
    auto& e = *f.e;
    e.selectEngine (EngineType::Analog);
    e.selectMode (ui::Mode::Synth);
    e.showProView (true);
    auto& pv = e.proViewComponent();
    auto* env = pv.tile (pv.cardIndex ("Amp Envelope"));
    REQUIRE (env != nullptr);
    const float a0 = value (f.p, "p1.amp.a");
    // the attack handle sits at the peak, left in the picture: drag it right
    const auto b = env->getLocalBounds().toFloat();
    const juce::Point<float> start (b.getX() + b.getWidth() * 0.06f, b.getY() + b.getHeight() * 0.25f);
    const auto to = start.translated (b.getWidth() * 0.2f, 0.0f);
    env->mouseDown (mouseEvent (*env, start, start));
    env->mouseDrag (mouseEvent (*env, to, start));
    env->mouseUp (mouseEvent (*env, to, start));
    CHECK (value (f.p, "p1.amp.a") > a0 + 0.05f);

    auto* filter = pv.tile (pv.cardIndex ("Filter"));
    REQUIRE (filter != nullptr);
    const float c0 = value (f.p, "p1.analog.cutoff"), r0 = value (f.p, "p1.analog.reso");
    const auto fb = filter->getLocalBounds().toFloat();
    const auto s = fb.getCentre().translated (0.0f, fb.getHeight() * 0.2f);
    const auto t = s.translated (-fb.getWidth() * 0.2f, -fb.getHeight() * 0.2f);
    filter->mouseDown (mouseEvent (*filter, s, s));
    filter->mouseDrag (mouseEvent (*filter, t, s));
    filter->mouseUp (mouseEvent (*filter, t, s));
    CHECK (value (f.p, "p1.analog.cutoff") < c0 - 0.1f);
    CHECK (value (f.p, "p1.analog.reso") > r0 + 0.1f);
    // hover text: name, value and one sentence (EN / DE)
    i18n::setCurrent (i18n::Language::German);
    CHECK (ui::pro::hoverText (f.p, "p1.amp.a").contains ("Pegel"));
    CHECK (ui::pro::explanation ("p1.analog.cutoff").isNotEmpty());
    i18n::setCurrent (i18n::Language::English);
    CHECK (ui::pro::hoverText (f.p, "p1.amp.a").contains ("full level"));
    for (int i = 0; i < pv.numBars(); ++i) CHECK (pv.bar (i).getTooltip().isNotEmpty());
}

TEST_CASE ("pro view 2.0: a modulation source dropped onto a picture creates a matrix routing; the range shows violet")
{
    Fixture f;
    auto& e = *f.e;
    e.selectEngine (EngineType::Analog);
    e.selectMode (ui::Mode::Synth);
    e.showProView (true);
    auto& pv = e.proViewComponent();
    auto* filter = pv.tile (pv.cardIndex ("Filter"));
    REQUIRE (filter != nullptr);
    CHECK (ui::pro::modRange (f.p, 0, "p1.analog.cutoff").isEmpty());
    juce::DragAndDropTarget::SourceDetails d (ui::pro::sourceDragDescription (mod::SrcLfo2), nullptr,
                                              filter->getLocalBounds().getCentre().translated (0, filter->getHeight() / 4));
    CHECK (filter->isInterestedInDragSource (d));
    filter->itemDropped (d);
    CHECK (std::lround (plain (f.p, "p1.mod.s1.src")) == mod::SrcLfo2);
    CHECK (std::lround (plain (f.p, "p1.mod.s1.dst")) == ui::pro::destForParam (f.p, 0, "p1.analog.cutoff"));
    CHECK (plain (f.p, "p1.mod.s1.amt") > 0.0f);
    const auto r = ui::pro::modRange (f.p, 0, "p1.analog.cutoff");
    CHECK (r.getStart() < 0.0f); // LFO 2 is bipolar
    CHECK (r.getEnd() > 0.0f);
    // a second drop takes the next free slot
    CHECK (ui::pro::addRouting (f.p, 0, mod::SrcModEnv, "p1.analog.shape") == 1);
    CHECK (ui::pro::addRouting (f.p, 0, mod::SrcModEnv, "p1.amp.d") == -1); // not a matrix target
    CHECK (ui::pro::sourceFromDragDescription ("something") == -1);
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
}

TEST_CASE ("pro view 2.0: Discover - VARY, audition, keep, cross, dice, locks, the timeline, the character macros")
{
    Fixture f;
    auto& e = *f.e;
    e.selectPart (1);
    e.selectEngine (EngineType::Wavetable);
    e.selectMode (ui::Mode::Synth);
    e.showProView (true);
    auto& pv = e.proViewComponent();
    auto& strip = pv.discoverStrip();
    REQUIRE (strip.isVisible());
    const auto before = strip.currentSound();
    REQUIRE (before.size() > 10);
    for (const auto& g : before.genes) CHECK (g.id.rfind ("p2.", 0) == 0);

    // lock the amp envelope: no variation touches it
    const int amp = pv.cardIndex ("Amp Envelope");
    pv.setLocked (amp, true);
    strip.setStrength (1.0f);
    strip.vary();
    REQUIRE (strip.numVariants() == 6);
    for (int i = 0; i < 6; ++i)
    {
        const auto* v = strip.variant (i);
        CHECK (v->distance (before) > 0.0f);
        for (const char* k : { "p2.amp.a", "p2.amp.d", "p2.amp.s", "p2.amp.r" })
        {
            const int j = v->indexOf (k);
            REQUIRE (j >= 0);
            CHECK (v->genes[(size_t) j].value == before.genes[(size_t) before.indexOf (k)].value);
        }
    }
    // audition applies the sibling; VARY again returns to the sound before (not kept)
    strip.audition (1);
    CHECK (strip.currentSound().distance (*strip.variant (1)) < 1.0e-3f);
    play (f.p, 4);
    strip.vary();
    CHECK (strip.currentSound().distance (before) < 1.0e-3f);
    // keep: the sound stays and is a new state on the timeline
    const auto states = strip.timeline().size();
    strip.keep (3);
    const auto kept = strip.currentSound();
    CHECK (kept.distance (before) > 0.0f);
    CHECK (strip.timeline().size() == states + 1);
    // cross: tile 0 dropped on tile 5 - the child replaces tile 5 and is auditioned
    const auto t5 = *strip.variant (5);
    strip.cross (0, 5);
    CHECK (strip.variant (5)->distance (t5) > 0.0f);
    CHECK (strip.auditioned() == 5);
    strip.keep (5);
    // the dice of one card change only that card
    pv.setLocked (amp, false);
    const auto pre = strip.currentSound();
    pv.rollCard (pv.cardIndex ("LFO"));
    const auto post = strip.currentSound();
    for (size_t i = 0; i < pre.size(); ++i)
        if (pre.genes[i].id.find (".lfo.") == std::string::npos) CHECK (post.genes[i].value == pre.genes[i].value);
    // the timeline goes back to the first state
    strip.restore (0);
    CHECK (strip.currentSound().distance (before) < 1.0e-3f);
    CHECK (strip.timeline().current() == 0);

    // the character macro knobs are host parameters (drag up = more)
    const auto k = strip.knobArea (0);
    const auto c = k.getCentre();
    strip.mouseDown (mouseEvent (strip, c, c));
    strip.mouseDrag (mouseEvent (strip, c.translated (0.0f, -40.0f), c));
    strip.mouseUp (mouseEvent (strip, c.translated (0.0f, -40.0f), c));
    CHECK (plain (f.p, "p2.char.bright") > 0.05f);
    strip.mouseDoubleClick (mouseEvent (strip, c, c));
    CHECK (std::abs (plain (f.p, "p2.char.bright")) < 0.01f);

    // another engine = other genes: the tiles of the old sound are cleared, the timeline of the part stays
    strip.vary();
    REQUIRE (strip.numVariants() == 6);
    const auto statesBefore = strip.timeline().size();
    e.selectEngine (EngineType::FM);
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (strip.numVariants() == 0);
    CHECK (strip.auditioned() == -1);
    CHECK (strip.timeline().size() == statesBefore);
    strip.vary();
    CHECK (strip.numVariants() == 6);
    for (const auto& g : strip.variant (0)->genes) CHECK (g.id.find (".wavetable.") == std::string::npos);
    e.advanceAnimation (1.0f / 30.0f);
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
}
