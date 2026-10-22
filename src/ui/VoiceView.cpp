// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "VoiceView.h"

#include "Palette.h"
#include "plugin/Localisation.h"

#include <cmath>

namespace ohpe::ui::voiceview
{
using i18n::tr;
using voice::VoiceAnalyser;

std::vector<std::string> voiceParams()
{
    return { voice::Ids::on, voice::Ids::gain, voice::Ids::gate, voice::Ids::smooth, voice::Ids::pitch, voice::Ids::monitor };
}

std::vector<std::string> voiceTitles() { return { "Voice", "Voice Options" }; }

namespace
{
bool isOn (const Core& core) { return core.params().get (voice::Ids::on) >= 0.5f; }
float gateDb (const Core& core) { return core.params().get (voice::Ids::gate); }
float dbToMeter (float db) { return juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f); }
} // namespace

void paint (juce::Graphics& g, juce::Rectangle<float> v, const Core& core)
{
    const auto& a = core.voiceControl().analysis();
    const bool on = isOn (core), open = on && a.uiGateOpen();
    const float alpha = on ? 1.0f : 0.45f;
    auto area = v.reduced (v.getHeight() * 0.04f);

    // ---- the vowel map: a square, the five vowels at their places, the live vowel as a dot ----
    const float side = juce::jmin (area.getHeight(), area.getWidth() * 0.55f);
    auto map = area.removeFromLeft (side).withSizeKeepingCentre (side, side);
    const float rad = juce::jmin (5.0f, side * 0.04f);
    g.setColour (colour::displayGrid.withAlpha (0.9f));
    g.fillRoundedRectangle (map, rad);
    g.setColour (colour::displayDim.withAlpha (0.25f * alpha));
    g.drawLine (map.getCentreX(), map.getY() + 4.0f, map.getCentreX(), map.getBottom() - 4.0f, 1.0f);
    g.drawLine (map.getX() + 4.0f, map.getCentreY(), map.getRight() - 4.0f, map.getCentreY(), 1.0f);
    auto toScreen = [&] (float x01, float y01) { // x: back (left) .. front (right), y: open at the top
        return juce::Point<float> (map.getX() + map.getWidth() * (0.08f + 0.84f * x01), map.getBottom() - map.getHeight() * (0.08f + 0.84f * y01));
    };
    const float vx = 0.5f * (a.uiValue (voice::VowelX) + 1.0f), vy = 0.5f * (a.uiValue (voice::VowelY) + 1.0f);
    const int heard = voice::classifyVowel (vx, vy);
    g.setFont (brandFont (side * 0.13f));
    for (int k = 0; k < voice::numVowels; ++k)
    {
        float cx, cy;
        voice::vowelCentre (k, cx, cy);
        const auto p = toScreen (cx, cy);
        const bool lit = open && k == heard;
        g.setColour ((lit ? colour::displayText : colour::displayDim).withAlpha (alpha));
        g.drawText (voice::vowelName (k), juce::Rectangle<float> (side * 0.16f, side * 0.16f).withCentre (p), juce::Justification::centred, false);
    }
    if (on)
    {
        const auto p = toScreen (vx, vy);
        const float d = side * (open ? 0.075f : 0.055f);
        g.setColour ((open ? colour::sunYellow : colour::displayDim).withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (d * 2.0f, d * 2.0f).withCentre (p));
        g.setColour (open ? colour::sunYellow : colour::displayDim);
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
    }

    // ---- the input meter (with the gate line) and the five bars ----
    area.removeFromLeft (area.getWidth() * 0.06f);
    const float barW = juce::jmin (area.getWidth() / 11.5f, side * 0.12f);
    auto bar = [&] (juce::Rectangle<float> r, float value, juce::Colour c, const juce::String& label) {
        auto lab = r.removeFromBottom (side * 0.12f);
        g.setColour (colour::displayGrid.withAlpha (0.9f));
        g.fillRoundedRectangle (r, rad * 0.6f);
        const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, value);
        g.setColour (c.withAlpha (alpha));
        if (h > 0.5f) g.fillRoundedRectangle (r.withTop (r.getBottom() - h), rad * 0.6f);
        g.setColour (colour::displayDim.withAlpha (alpha));
        g.setFont (labelFont (lab.getHeight() * 0.62f));
        g.drawFittedText (label, lab.expanded (barW * 0.48f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.55f);
        return r;
    };
    auto col = [&] { auto r = area.removeFromLeft (barW); area.removeFromLeft (barW * 0.95f); return r.withSizeKeepingCentre (barW, side); };

    const auto meter = bar (col(), on ? dbToMeter (a.uiInputDb()) : 0.0f, open ? colour::playGreen : colour::displayDim, tr ("Input"));
    const float gy = meter.getBottom() - meter.getHeight() * dbToMeter (gateDb (core));
    g.setColour (colour::displayText.withAlpha (0.8f * alpha));
    g.drawLine (meter.getX() - 2.0f, gy, meter.getRight() + 2.0f, gy, 1.5f);
    area.removeFromLeft (barW * 0.3f);
    bar (col(), a.uiValue (voice::Level), colour::encoder (0), tr ("Level"));
    bar (col(), a.uiValue (voice::Brightness), colour::encoder (1), tr ("Bright"));
    bar (col(), a.uiValue (voice::Noisiness), colour::encoder (2), tr ("Noise"));
    const bool pitchOn = core.params().get (voice::Ids::pitch) >= 0.5f;
    bar (col(), pitchOn ? a.uiValue (voice::Pitch) : 0.0f, pitchOn ? colour::encoder (3) : colour::displayDim, tr ("Pitch"));
    bar (col(), a.uiValue (voice::Onset), colour::displayText, tr ("Onset"));
}

juce::String statusLine (const Core& core)
{
    const auto& a = core.voiceControl().analysis();
    if (! isOn (core)) return tr ("Off");
    if (a.uiInputDb() <= -119.0f) return tr ("No input - choose one in the audio settings or the host");
    if (! a.uiGateOpen()) return tr ("Listening") + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (juce::roundToInt (a.latencyMs())) + " ms";
    const float vx = 0.5f * (a.uiValue (voice::VowelX) + 1.0f), vy = 0.5f * (a.uiValue (voice::VowelY) + 1.0f);
    return tr ("Vowel") + " " + voice::vowelName (voice::classifyVowel (vx, vy)) + juce::String::fromUTF8 (" \xc2\xb7 ")
           + juce::String (juce::roundToInt (a.uiInputDb())) + " dB";
}

int signature (const Core& core)
{
    const auto& a = core.voiceControl().analysis();
    return (isOn (core) ? 1 : 0) + (a.uiGateOpen() ? 2 : 0) + (a.uiInputDb() <= -119.0f ? 4 : 0);
}
} // namespace ohpe::ui::voiceview
