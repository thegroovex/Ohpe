// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UserTableView.h"

#include "Palette.h"
#include "plugin/Localisation.h"

namespace ohpe::ui::usertableview
{
using i18n::tr;

namespace
{
usertable::Target targetOf (EngineType e) { return e == EngineType::Vocal ? usertable::Target::Vocal : usertable::Target::Wavetable; }

bool userOn (OhpeProcessor& proc, int part, EngineType e)
{
    const auto id = "p" + std::to_string (part + 1) + (e == EngineType::Vocal ? ".usertable.vocal" : ".usertable.wt");
    return proc.core().params().get (id) > 0.5f;
}

void paintProgress (juce::Graphics& g, juce::Rectangle<float> area, float progress, const juce::String& label)
{
    const float u = area.getWidth() / 42.0f;
    auto bar = area.withTrimmedTop (area.getHeight() * 0.80f).withHeight (u * 1.2f).reduced (u * 2.0f, 0);
    g.setColour (colour::displayGrid);
    g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
    g.setColour (colour::sunYellow);
    g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * juce::jlimit (0.0f, 1.0f, progress))), bar.getHeight() * 0.5f);
    g.setColour (colour::displayText);
    g.setFont (labelFont (u * 2.2f));
    g.drawText (i18n::upper (label) + "  " + juce::String (juce::roundToInt (progress * 100.0f)) + " %",
                bar.translated (0, -u * 3.4f).withHeight (u * 2.8f), juce::Justification::centredLeft, false);
}
} // namespace

bool wants (OhpeProcessor& proc, int part, EngineType engine)
{
    if (engine != EngineType::Wavetable && engine != EngineType::Vocal) return false;
    const auto st = proc.userTables().status();
    if (st.busy && st.part == part && st.target == targetOf (engine)) return true;
    return userOn (proc, part, engine) && usertable::tableOf (proc.core(), part, targetOf (engine)) != nullptr;
}

void paint (juce::Graphics& g, juce::Rectangle<float> area, OhpeProcessor& proc, int part, EngineType engine, float position, float time)
{
    const auto st = proc.userTables().status();
    const bool busyHere = st.busy && st.part == part && st.target == targetOf (engine);
    if (position < 0.0f) // the engine's own position / vowel (the first macro)
    {
        const auto& reg = proc.core().params();
        const auto pre = "p" + std::to_string (part + 1);
        position = engine == EngineType::Vocal ? reg.get (pre + ".vocal.vowel") / 4.0f : reg.get (pre + ".wavetable.pos");
    }
    const auto table = usertable::tableOf (proc.core(), part, targetOf (engine));
    const float u = area.getWidth() / 42.0f;
    const auto plot = area.reduced (u * 1.5f, u * 2.5f).withTrimmedTop (u * 1.5f).withTrimmedBottom (busyHere ? area.getHeight() * 0.22f : 0.0f);

    if (table != nullptr && engine == EngineType::Wavetable && table->hasWave())
    {
        // a stack of frames, the first in front, later frames further back (up and to the right)
        const int n = table->numFrames();
        const int shown = juce::jmin (n, 24);
        const float depthX = plot.getWidth() * 0.28f, depthY = plot.getHeight() * 0.46f;
        const float w = plot.getWidth() - depthX, h = (plot.getHeight() - depthY) * 0.5f;
        const int lit = juce::jlimit (0, n - 1, juce::roundToInt (juce::jlimit (0.0f, 1.0f, position) * (float) (n - 1)));
        auto drawFrame = [&] (int f, float depth, juce::Colour c, float thickness) {
            const float* d = table->frame (f, 2); // a smooth level: about one harmonic per screen pixel
            const float x0 = plot.getX() + depthX * depth, yMid = plot.getBottom() - h - depthY * depth;
            juce::Path p;
            constexpr int points = 96;
            for (int i = 0; i <= points; ++i)
            {
                const float v = d[(i * usertable::frameSize) / points];
                const float x = x0 + w * (float) i / (float) points, y = yMid - v * h * 0.92f;
                if (i == 0) p.startNewSubPath (x, y);
                else p.lineTo (x, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
        };
        for (int k = shown - 1; k >= 0; --k)
        {
            const int f = shown > 1 ? (k * (n - 1)) / (shown - 1) : 0;
            const float depth = shown > 1 ? (float) k / (float) (shown - 1) : 0.0f;
            drawFrame (f, depth, colour::displayText.withAlpha (0.16f + 0.42f * (1.0f - depth)), u * 0.16f);
        }
        const float litDepth = n > 1 ? (float) lit / (float) (n - 1) : 0.0f;
        const float glow = 0.85f + 0.15f * std::sin (time * 3.0f);
        drawFrame (lit, litDepth, colour::sunYellow.withAlpha (glow), u * 0.32f);
    }
    else if (table != nullptr && engine == EngineType::Vocal && table->hasFormants())
    {
        // the five formant tracks over the frames: height = log frequency (100 Hz .. 5 kHz), thickness = level
        const auto& fm = table->formants();
        const int n = (int) fm.size();
        auto yOf = [&] (float hz) {
            const float t = std::log (juce::jlimit (100.0f, 5000.0f, hz) / 100.0f) / std::log (50.0f);
            return plot.getBottom() - t * plot.getHeight();
        };
        g.setColour (colour::displayGrid);
        for (float hz : { 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f }) g.drawHorizontalLine ((int) yOf (hz), plot.getX(), plot.getRight());
        for (int k = 0; k < usertable::numFormants; ++k)
            for (int f = 0; f + 1 < juce::jmax (2, n); ++f)
            {
                const auto& a = fm[(size_t) juce::jmin (f, n - 1)];
                const auto& b = fm[(size_t) juce::jmin (f + 1, n - 1)];
                const float x0 = plot.getX() + plot.getWidth() * (float) f / (float) juce::jmax (1, n - 1);
                const float x1 = n > 1 ? plot.getX() + plot.getWidth() * (float) (f + 1) / (float) (n - 1) : plot.getRight();
                const float level = juce::jlimit (0.0f, 1.0f, 1.0f + a.gainDb[(size_t) k] / 48.0f);
                g.setColour (colour::encoder (k % 4).withAlpha (0.35f + 0.65f * level));
                g.drawLine (x0, yOf (a.freq[(size_t) k]), x1, yOf (b.freq[(size_t) k]), u * (0.15f + 0.5f * level));
            }
        const float x = plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, position);
        g.setColour (colour::sunYellow);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), u * 0.25f);
    }
    else if (! busyHere)
        return;

    // caption: what is shown
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 2.0f));
    juce::String caption = i18n::upper (tr (engine == EngineType::Vocal ? "User formants" : "User wavetable"));
    if (table != nullptr) caption << "  " << juce::String::fromUTF8 (table->name().c_str());
    g.drawText (caption, area.reduced (u * 1.0f, u * 0.4f).withHeight (u * 2.6f), juce::Justification::topLeft, true);

    if (busyHere) paintProgress (g, area, st.progress, tr (st.recording ? "Recording input" : "Analysing"));
}

juce::String statusLine (OhpeProcessor& proc, int part, EngineType engine)
{
    const auto st = proc.userTables().status();
    if (st.busy && st.part == part && st.target == targetOf (engine))
        return tr (st.recording ? "Recording input" : "Analysing") + " " + juce::String (juce::roundToInt (st.progress * 100.0f)) + " %";
    const auto table = usertable::tableOf (proc.core(), part, targetOf (engine));
    if (table == nullptr) return tr (engine == EngineType::Vocal ? "Built-in vowels - drop audio here to import" : "Built-in tables - drop audio or a wavetable here");
    juce::String s = juce::String::fromUTF8 (table->name().c_str());
    if (engine == EngineType::Wavetable) s << " - " << table->numFrames() << " " << tr ("frames");
    if (! table->contentRef.empty()) // wavetable banks: a read-only pack table, named with its pack
        s << "  /  " << tr ("Pack") << " " << juce::String::fromUTF8 (table->contentRef.c_str()).upToFirstOccurrenceOf ("/", false, false);
    if (! userOn (proc, part, engine)) s << " (" << tr ("off") << ")";
    return s;
}
} // namespace ohpe::ui::usertableview
