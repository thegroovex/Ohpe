// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HarmonyView.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "seq/ScaleChord.h"

#include <cmath>

namespace ohpe::ui::harmonyview
{
using i18n::tr;

namespace
{
bool german() { return i18n::current() == i18n::Language::German; }
} // namespace

std::vector<std::string> harmonyParams (int part)
{
    return { "harmony.p" + std::to_string (part + 1) + ".follow", "harmony.memory", "harmony.hold", "harmony.kb.layout" };
}

std::vector<std::string> harmonyTitles() { return { "Harmony" }; }

juce::String noteName (int pc)
{
    static const char* en[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* de[] = { "C", "Cis", "D", "Dis", "E", "F", "Fis", "G", "Gis", "A", "Ais", "H" };
    pc = ((pc % 12) + 12) % 12;
    return german() ? de[pc] : en[pc];
}

juce::String chordName (const harmony::Chord& c)
{
    if (c.root < 0) return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93"));
    static const char* en[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String root = en[c.root % 12];
    if (german() && c.root % 12 == 11) root = "H";
    return root + harmony::chordSuffix (c.type);
}

juce::String keyName (int root, int scale)
{
    const auto en = scales::namesEn(), de = scales::namesDe();
    const int s = juce::jlimit (0, (int) en.size() - 1, scale);
    return noteName (root) + " " + i18n::pick (en[(size_t) s], de[(size_t) s]);
}

void paint (juce::Graphics& g, juce::Rectangle<float> v, Core& core, int part, float time)
{
    const auto& h = core.harmony();
    const int kr = h.keyRoot(), ks = h.keyScale();
    const auto chord = h.chord();
    const uint16_t held = h.heldPitchClasses();
    const int follow = h.followMode (part);
    uint16_t chordPcs = 0;
    if (chord.root >= 0)
    {
        const uint16_t shape = harmony::chordShape (chord.type);
        for (int i = 0; i < 12; ++i)
            if (((shape >> i) & 1u) != 0) chordPcs = (uint16_t) (chordPcs | (1u << ((chord.root + i) % 12)));
    }

    auto area = v.reduced (v.getWidth() * 0.01f, v.getHeight() * 0.02f);
    auto top = area.removeFromTop (area.getHeight() * 0.3f);
    auto chips = area.removeFromBottom (area.getHeight() * 0.3f);
    auto bars = area.reduced (0.0f, area.getHeight() * 0.06f);

    // key | chord: small legend over a big value
    auto keyBox = top.removeFromLeft (top.getWidth() * 0.62f);
    auto chordBox = top;
    auto legend = [&] (juce::Rectangle<float> b, const juce::String& text, const juce::String& value, juce::Colour c) {
        auto l = b.removeFromTop (b.getHeight() * 0.36f);
        g.setFont (labelFont (l.getHeight() * 0.82f));
        g.setColour (colour::displayDim);
        g.drawText (i18n::upper (text), l, juce::Justification::bottomLeft, true);
        g.setFont (brandFont (b.getHeight() * 0.78f));
        g.setColour (c);
        g.drawText (value, b, juce::Justification::centredLeft, true);
    };
    legend (keyBox, tr ("Key"), keyName (kr, ks), h.heard() ? colour::displayText : colour::displayText.withAlpha (0.6f));
    const float pulse = core.part (part).modMatrix().uiSourceValue (mod::SrcChordChange);
    legend (chordBox, tr ("Chord"), chordName (chord), colour::ultraviolet.brighter (0.25f + 0.5f * pulse));

    // twelve pitch classes from the key's root: the memory as bars
    const float w = bars.getWidth() / 12.0f;
    const auto labels = bars.removeFromBottom (bars.getHeight() * 0.28f);
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (kr + i) % 12;
        const bool inKey = scales::inScale (pc, kr, ks);
        const bool isChord = ((chordPcs >> pc) & 1u) != 0, isHeld = ((held >> pc) & 1u) != 0;
        auto col = juce::Rectangle<float> (bars.getX() + w * (float) i, bars.getY(), w, bars.getHeight()).reduced (w * 0.14f, 0.0f);
        const float rad = juce::jmin (3.0f, col.getWidth() * 0.25f);
        g.setColour (colour::displayGrid.withAlpha (0.9f));
        g.fillRoundedRectangle (col, rad);
        const float level = juce::jlimit (0.0f, 1.0f, h.weight (pc));
        auto fill = col.withTop (col.getBottom() - juce::jmax (2.0f, col.getHeight() * level));
        juce::Colour c = isHeld ? colour::playGreenLit : isChord ? colour::ultraviolet.brighter (0.2f) : inKey ? colour::displayText.withAlpha (0.75f)
                                                                                                          : colour::displayDim.withAlpha (0.7f);
        g.setColour (c);
        g.fillRoundedRectangle (fill, rad);
        if (isChord && ! isHeld) { g.setColour (colour::ultraviolet.brighter (0.4f)); g.drawRoundedRectangle (col, rad, 1.2f); }
        auto lab = juce::Rectangle<float> (col.getX() - w * 0.14f, labels.getY(), w, labels.getHeight());
        if (i == 0) // the key's root: yellow mark under its name
        {
            g.setColour (colour::sunYellow);
            g.fillRoundedRectangle (lab.withTop (lab.getBottom() - juce::jmax (2.0f, lab.getHeight() * 0.12f)).reduced (w * 0.18f, 0.0f), 1.0f);
        }
        g.setFont (monoFont (juce::jmin (labels.getHeight() * 0.62f, w * 0.42f)));
        g.setColour (isHeld ? colour::playGreenLit : inKey ? colour::displayText : colour::displayDim);
        g.drawText (noteName (pc), lab.withTrimmedBottom (lab.getHeight() * 0.14f), juce::Justification::centred, false);
    }

    // chord history: newest left, violet rim; fading to the right
    const int n = h.historyCount();
    const float cw = chips.getWidth() / (float) HarmonyBrain::historySize;
    for (int i = 0; i < HarmonyBrain::historySize; ++i)
    {
        auto chip = juce::Rectangle<float> (chips.getX() + cw * (float) i, chips.getY(), cw, chips.getHeight()).reduced (cw * 0.06f, chips.getHeight() * 0.1f);
        const float rad = juce::jmin (5.0f, chip.getHeight() * 0.3f);
        if (i >= n)
        {
            g.setColour (colour::displayGrid.withAlpha (0.6f));
            g.drawRoundedRectangle (chip, rad, 1.0f);
            continue;
        }
        const float a = 1.0f - 0.08f * (float) i;
        g.setColour (colour::displayGrid.withAlpha (0.95f));
        g.fillRoundedRectangle (chip, rad);
        g.setColour ((i == 0 ? colour::ultraviolet.brighter (0.2f + 0.4f * pulse) : colour::displayDim).withAlpha (a));
        g.drawRoundedRectangle (chip, rad, i == 0 ? 1.6f : 1.0f);
        g.setFont (brandFont (chip.getHeight() * 0.56f));
        g.setColour (colour::displayText.withAlpha (i == 0 ? 1.0f : 0.75f * a));
        g.drawFittedText (chordName (h.history (i)), chip.reduced (2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    juce::ignoreUnused (time, follow);
}

juce::String statusLine (Core& core, int part)
{
    const auto& h = core.harmony();
    const int follow = h.followMode (part);
    const bool hold = core.params().get ("harmony.hold") >= 0.5f;
    juce::String s;
    if (! h.heard()) s = tr ("Listening - play chords on a part that does not follow");
    else if (hold) s = tr ("Held - the memory does not change");
    else s = juce::String (h.historyCount()) + " " + tr ("chords in memory");
    const auto names = harmony::followNamesEn();
    if (follow > 0)
        s << "  ·  P" << (part + 1) << " " << tr ("follows") << " " << tr (names[(size_t) juce::jlimit (0, (int) names.size() - 1, follow)].c_str());
    return s;
}
} // namespace ohpe::ui::harmonyview
