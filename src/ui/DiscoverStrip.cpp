// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DiscoverStrip.h"

#include "Palette.h"
#include "ProTiles.h"
#include "Widgets.h"
#include "mod/Character.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace ohpe::ui
{
using i18n::tr;

DiscoverStrip::DiscoverStrip (OhpeProcessor& p) : proc (p) {}

std::string DiscoverStrip::macroId (int m) const
{
    return "p" + std::to_string (partIndex + 1) + ".char." + character::key (m);
}

void DiscoverStrip::setPart (int part, const std::vector<std::string>& soundIds)
{
    const int pt = juce::jlimit (0, Core::numParts - 1, part);
    if (pt != partIndex || soundIds != ids)
    {
        // another part or another set of genes (engine change, other page): the tiles were siblings of a sound
        // that no longer exists - drop them (the timeline only goes with the part)
        if (pt != partIndex) history.clear();
        variants.clear();
        sketches.clear();
        base = {};
        varyFrom = {};
        auditionIndex = keptTile = -1;
        dropTile = pressTile = -1;
        partIndex = pt;
        ids = soundIds;
        repaint();
    }
}

discover::Genome DiscoverStrip::soundOf (const std::vector<std::string>& over) const
{
    // read from the host parameters (what the user sees right now, also before the core caught up)
    discover::Genome g;
    const auto& reg = proc.core().params();
    const std::string prefix = "p" + std::to_string (partIndex + 1) + ".";
    for (const auto& id : over)
    {
        if (id.rfind (prefix, 0) != 0) continue;
        const int idx = reg.indexOf (id);
        auto* q = proc.parameter (juce::String (id));
        if (idx < 0 || q == nullptr) continue;
        const auto dot = id.find ('.');
        if (dot == std::string::npos || ! discover::isBreedable (id.substr (dot + 1))) continue;
        const auto& d = reg.descriptor (idx);
        discover::Gene gene;
        gene.id = id;
        gene.value = q->getValue();
        gene.steps = d.isDiscrete() ? juce::jmax (2, (int) std::lround (d.maxValue - d.minValue) + 1) : 0;
        g.genes.push_back (std::move (gene));
    }
    return g;
}

void DiscoverStrip::apply (const discover::Genome& g)
{
    for (const auto& gene : g.genes)
        if (auto* q = proc.parameter (juce::String (gene.id)))
        {
            float v = juce::jlimit (0.0f, 1.0f, gene.value);
            if (gene.steps > 1) v = std::round (v * (float) (gene.steps - 1)) / (float) (gene.steps - 1);
            if (std::abs (q->getValue() - v) < 1.0e-6f) continue;
            q->beginChangeGesture();
            q->setValueNotifyingHost (v);
            q->endChangeGesture();
        }
    if (onSoundChanged) onSoundChanged();
    repaint();
}

std::vector<bool> DiscoverStrip::maskFor (const discover::Genome& g, const std::vector<std::string>& onlyIds) const
{
    std::vector<bool> mask (g.size(), true);
    for (size_t i = 0; i < g.size(); ++i)
    {
        const auto& id = g.genes[i].id;
        if (isLocked && isLocked (id)) mask[i] = false;
        if (! onlyIds.empty() && std::find (onlyIds.begin(), onlyIds.end(), id) == onlyIds.end()) mask[i] = false;
    }
    return mask;
}

void DiscoverStrip::recordState()
{
    auto g = currentSound();
    if (! g.empty()) history.push (std::move (g));
    repaint();
}

void DiscoverStrip::revertAudition()
{
    if (auditionIndex >= 0 && keptTile != auditionIndex && ! base.empty()) apply (base);
    auditionIndex = -1;
    repaint();
}

void DiscoverStrip::vary()
{
    revertAudition();
    base = currentSound();
    if (base.empty()) return;
    if (history.size() == 0 || history.at (history.current()) == nullptr || history.at (history.current())->distance (base) > 0.0f) history.push (base);
    varyFrom = base;
    variants = discover::siblings (base, 6, strength, seed, maskFor (base));
    seed += 101u;
    sketches.clear();
    for (const auto& v : variants) sketches.push_back (sketch (v));
    auditionIndex = keptTile = -1;
    repaint();
}

void DiscoverStrip::previewNote()
{
    const int ch = juce::jlimit (1, 16, (int) std::lround (proc.core().params().get ("p" + std::to_string (partIndex + 1) + ".midich")));
    if (noteOnAt >= 0) proc.keyboardState.noteOff (ch, noteOnAt, 0.0f);
    noteOnAt = 60;
    noteTimer = 0.45f;
    proc.keyboardState.noteOn (ch, noteOnAt, 0.8f);
}

void DiscoverStrip::audition (int tile)
{
    if (tile < 0 || tile >= (int) variants.size()) return;
    auditionIndex = tile;
    apply (variants[(size_t) tile]);
    previewNote();
}

void DiscoverStrip::keep (int tile)
{
    if (tile < 0 || tile >= (int) variants.size()) return;
    if (auditionIndex != tile) apply (variants[(size_t) tile]);
    auditionIndex = keptTile = tile;
    base = variants[(size_t) tile];
    recordState();
}

void DiscoverStrip::cross (int fromTile, int toTile)
{
    if (fromTile == toTile || fromTile < 0 || toTile < 0 || fromTile >= (int) variants.size() || toTile >= (int) variants.size()) return;
    auto child = discover::breeder().cross (variants[(size_t) toTile], variants[(size_t) fromTile], seed);
    seed += 13u;
    // locked genes stay as in the sound the strip started from
    const auto mask = maskFor (child);
    for (size_t i = 0; i < child.size(); ++i)
        if (! mask[i])
            if (const int j = base.indexOf (child.genes[i].id); j >= 0) child.genes[i].value = base.genes[(size_t) j].value;
    variants[(size_t) toTile] = std::move (child);
    sketches[(size_t) toTile] = sketch (variants[(size_t) toTile]);
    keptTile = -1;
    audition (toTile);
}

void DiscoverStrip::roll (const std::vector<std::string>& onlyIds)
{
    revertAudition();
    auto cur = onlyIds.empty() ? currentSound() : soundOf (onlyIds);
    if (cur.empty()) return;
    if (history.size() == 0) history.push (currentSound());
    auto next = discover::breeder().vary (cur, juce::jmax (0.15f, strength), seed, maskFor (cur, onlyIds));
    seed += 37u;
    apply (next);
    recordState();
}

void DiscoverStrip::restore (int state)
{
    if (const auto* g = history.select (state))
    {
        auditionIndex = keptTile = -1;
        base = *g;
        apply (*g);
    }
    repaint();
}

std::vector<float> DiscoverStrip::sketch (const discover::Genome& g) const
{
    // a picture of the sibling, not a render: one waveform whose harmonics come from how its genes moved away from
    // the sound VARY started from (each gene feeds one of six partials, by a hash of its ID), shaped by the amp
    // envelope. Siblings that differ in other genes get visibly different waves.
    auto val = [&] (const char* tail, float def) {
        for (const auto& gene : g.genes)
            if (gene.id.size() > std::strlen (tail) && gene.id.compare (gene.id.size() - std::strlen (tail), std::strlen (tail), tail) == 0) return gene.value;
        return def;
    };
    const float a = val (".amp.a", 0.1f), d = val (".amp.d", 0.4f), s = val (".amp.s", 0.7f), r = val (".amp.r", 0.4f);
    constexpr int numPartials = 6;
    std::array<float, numPartials> amp {}, phase {};
    float bright = 0.0f, nb = 0.0f;
    for (const auto& gene : g.genes)
    {
        uint32_t h = 2166136261u;
        for (char c : gene.id) h = (h ^ (uint8_t) c) * 16777619u;
        const int j = varyFrom.indexOf (gene.id);
        const float delta = j >= 0 ? gene.value - varyFrom.genes[(size_t) j].value : 0.0f;
        const auto k = (size_t) (h % (uint32_t) numPartials);
        amp[k] += delta;
        phase[(size_t) ((h >> 8) % (uint32_t) numPartials)] += delta * 25.0f;
        const auto dot = gene.id.rfind ('.');
        if (character::engineWeights (dot == std::string::npos ? gene.id : gene.id.substr (dot + 1)).w[character::Bright] > 0.0f)
        {
            bright += gene.value;
            nb += 1.0f;
        }
    }
    bright = nb > 0.0f ? bright / nb : 0.5f;
    float peakDelta = 1.0e-4f;
    for (auto v : amp) peakDelta = juce::jmax (peakDelta, std::abs (v));
    // partial weights: a fundamental always, the others from the (normalised) moves; brightness tilts towards the top
    std::array<float, numPartials> w {};
    for (int k = 0; k < numPartials; ++k)
    {
        const float move = amp[(size_t) k] / peakDelta; // -1 .. 1
        const float tilt = 1.0f / (1.0f + (float) k * (1.6f - 1.3f * bright));
        w[(size_t) k] = k == 0 ? 0.45f : juce::jmax (0.0f, tilt * (0.1f + 1.3f * std::abs (move)));
    }
    // the pattern of which partials went up / down picks the number of cycles shown (2 .. 4): siblings that moved
    // differently read differently at a glance
    uint32_t pattern = 0;
    for (int k = 0; k < numPartials; ++k) pattern = pattern * 3u + (amp[(size_t) k] > 0.15f * peakDelta ? 2u : (amp[(size_t) k] < -0.15f * peakDelta ? 1u : 0u));
    std::vector<float> out (64);
    const float aEnd = 0.02f + 0.3f * a, dEnd = aEnd + 0.05f + 0.35f * d, relStart = 0.72f, relLen = 0.04f + 0.24f * r;
    const float cycles = 2.0f + (float) ((pattern * 2654435761u) >> 30) * 0.67f;
    float norm = 0.0f;
    std::vector<float> wave (out.size());
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float t = (float) i / (float) (out.size() - 1);
        float y = 0.0f;
        for (int k = 0; k < numPartials; ++k)
            y += w[(size_t) k] * std::sin (juce::MathConstants<float>::twoPi * cycles * (float) (k + 1) * t + phase[(size_t) k]);
        wave[i] = y;
        norm = juce::jmax (norm, std::abs (y));
    }
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float t = (float) i / (float) (out.size() - 1);
        float env;
        if (t < aEnd) env = t / aEnd;
        else if (t < dEnd) env = s + (1.0f - s) * std::exp (-4.0f * (t - aEnd) / (dEnd - aEnd));
        else if (t < relStart) env = s;
        else env = s * juce::jmax (0.0f, 1.0f - (t - relStart) / relLen);
        out[i] = juce::jlimit (-1.0f, 1.0f, (0.15f + 0.85f * env) * wave[i] / juce::jmax (1.0e-4f, norm));
    }
    return out;
}

void DiscoverStrip::tick (float seconds)
{
    time += seconds;
    if (noteOnAt >= 0)
    {
        noteTimer -= seconds;
        if (noteTimer <= 0.0f)
        {
            const int ch = juce::jlimit (1, 16, (int) std::lround (proc.core().params().get ("p" + std::to_string (partIndex + 1) + ".midich")));
            proc.keyboardState.noteOff (ch, noteOnAt, 0.0f);
            noteOnAt = -1;
        }
    }
    // hold = keep
    if (pressTile >= 0 && ! holdKept && juce::Time::getMillisecondCounterHiRes() - pressTime > 550.0 && dropTile < 0)
    {
        holdKept = true;
        keep (pressTile);
    }
    bool dirty = false;
    for (int m = 0; m < character::numMacros; ++m)
    {
        const auto id = macroId (m);
        float v = 0.0f;
        if (auto* q = proc.parameter (juce::String (id))) v = q->getValue();
        const int idx = proc.core().params().indexOf (id);
        const float md = idx >= 0 ? proc.core().part (partIndex).modulationOf (idx) : 0.0f;
        if (std::abs (v - shownMacro[(size_t) m]) > 1.0e-5f || std::abs (md - shownMod[(size_t) m]) > 1.0e-4f) dirty = true;
        shownMacro[(size_t) m] = v;
        shownMod[(size_t) m] = md;
    }
    if (dirty) repaint (knobArea (0).getUnion (knobArea (3)).expanded (4.0f).toNearestInt());
}

// ---------------------------------------------------------------- layout
juce::Rectangle<float> DiscoverStrip::knobArea (int m) const
{
    const float u = unit();
    return { u * 0.8f + (float) m * u * 7.4f, u * 0.6f, u * 6.6f, u * 8.8f };
}

juce::Rectangle<float> DiscoverStrip::varyArea() const
{
    const float u = unit();
    return { u * 31.6f, u * 3.0f, u * 10.0f, u * 4.0f };
}

juce::Rectangle<float> DiscoverStrip::tileArea (int t) const
{
    const float u = unit();
    const float x0 = u * 43.0f, x1 = (float) getWidth() - u * 24.0f - u * 13.0f;
    const float w = juce::jmax (u * 3.0f, (x1 - x0) / 6.0f);
    return { x0 + w * (float) t, u * 1.2f, w - u * 0.8f, u * 7.6f };
}

juce::Rectangle<float> DiscoverStrip::diceArea() const
{
    const float u = unit();
    return { (float) getWidth() - u * 24.0f - u * 12.4f, u * 1.0f, u * 5.4f, u * 5.4f };
}

juce::Rectangle<float> DiscoverStrip::lockArea() const
{
    const float u = unit();
    const auto d = diceArea();
    return { d.getRight() + u * 0.8f, d.getY() + u * 1.4f, u * 3.0f, u * 3.0f };
}

juce::Rectangle<float> DiscoverStrip::strengthArea() const
{
    const float u = unit();
    const auto d = diceArea();
    return { d.getX(), u * 7.0f, u * 11.0f, u * 2.4f };
}

juce::Rectangle<float> DiscoverStrip::dotArea (int i) const
{
    const float u = unit();
    const float x0 = (float) getWidth() - u * 23.0f;
    const float step = u * 22.0f / 20.0f;
    return { x0 + step * (float) i, u * 4.2f, step, u * 1.6f };
}

int DiscoverStrip::tileAt (juce::Point<float> p) const
{
    for (int t = 0; t < 6; ++t)
        if (tileArea (t).contains (p)) return t;
    return -1;
}

// ---------------------------------------------------------------- paint
void DiscoverStrip::paint (juce::Graphics& g)
{
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    g.setColour (colour::displayGrid);
    g.fillRoundedRectangle (b, radius::inset);

    // character macros: bipolar knobs (0 at the top), violet = what the matrix adds
    for (int m = 0; m < character::numMacros; ++m)
    {
        const auto a = knobArea (m);
        const float d = juce::jmin (a.getWidth(), u * 5.6f);
        const auto k = juce::Rectangle<float> (d, d).withCentre ({ a.getCentreX(), a.getY() + d * 0.5f + u * 0.2f });
        const float r = d * 0.5f - u * 0.4f;
        const auto c = k.getCentre();
        const float a0 = -juce::MathConstants<float>::pi * 0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
        auto* q = proc.parameter (juce::String (macroId (m)));
        const float v = q != nullptr ? q->getValue() : 0.5f;
        const float at = a0 + (a1 - a0) * v;
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
        g.setColour (colour::displayGrid.brighter (0.45f));
        g.strokePath (track, juce::PathStrokeType (u * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (0.0f, at), juce::jmax (0.0f, at), true);
        g.setColour (colour::sunYellow);
        g.strokePath (val, juce::PathStrokeType (u * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (const float md = shownMod[(size_t) m]; std::abs (md) > 1.0e-4f)
        {
            juce::Path mp;
            const float am = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v + md * 0.5f);
            mp.addCentredArc (c.x, c.y, r + u * 0.75f, r + u * 0.75f, 0.0f, juce::jmin (at, am), juce::jmax (at, am), true);
            g.setColour (colour::ultraviolet);
            g.strokePath (mp, juce::PathStrokeType (u * 0.35f));
        }
        g.setColour (colour::display);
        g.fillEllipse (juce::Rectangle<float> (r * 1.45f, r * 1.45f).withCentre (c));
        g.setColour (colour::displayText);
        g.drawLine ({ c.getPointOnCircumference (r * 0.2f, at), c.getPointOnCircumference (r * 0.7f, at) }, u * 0.35f);
        if (proc.core().midiMap().isMapped (macroId (m)))
        {
            g.setColour (colour::sunYellow);
            g.fillEllipse (a.getX(), a.getY(), u * 0.6f, u * 0.6f);
        }
        g.setColour (colour::displayText);
        g.setFont (labelFont (u * 1.25f));
        g.drawText (i18n::upper (i18n::pick (character::nameEn (m), character::nameDe (m))), a.withTop (a.getBottom() - u * 2.0f), juce::Justification::centred, false);
    }
    g.setColour (colour::displayGrid.brighter (0.35f));
    g.drawVerticalLine ((int) (u * 30.6f), u * 1.2f, b.getBottom() - u * 1.2f);

    // VARY
    {
        const auto v = varyArea();
        g.setColour (colour::displayText.withAlpha (0.9f));
        g.drawRoundedRectangle (v, u * 0.8f, u * 0.2f);
        g.setFont (labelFont (u * 1.5f));
        g.drawText (i18n::upper (tr ("Vary")), v, juce::Justification::centred, false);
    }
    // the six variation tiles
    for (int t = 0; t < 6; ++t)
    {
        const auto a = tileArea (t);
        const bool hasV = t < (int) variants.size();
        const bool aud = t == auditionIndex, kept = t == keptTile;
        g.setColour (aud ? colour::sunYellow : colour::display);
        g.fillRoundedRectangle (a, u * 0.7f);
        g.setColour (t == dropTile ? colour::ultraviolet : (hasV ? colour::displayDim : colour::displayGrid.brighter (0.4f)));
        g.drawRoundedRectangle (a, u * 0.7f, t == dropTile ? u * 0.3f : 1.0f);
        if (! hasV) continue;
        const auto& sk = sketches[(size_t) t];
        const auto ink = aud ? colour::display : colour::displayText;
        // number (top left) and how far the sibling is from the start sound (dots, top right)
        g.setColour (ink.withAlpha (0.75f));
        g.setFont (labelFont (u * 1.05f));
        g.drawText (juce::String (t + 1), a.reduced (u * 0.6f, u * 0.35f).withHeight (u * 1.2f), juce::Justification::centredLeft, false);
        {
            // relative to the farthest of the six: 1 = closest .. 3 = farthest
            float nearD = 1.0e9f, farD = 0.0f;
            for (const auto& v : variants)
            {
                const float dv = varyFrom.empty() ? 0.0f : v.distance (varyFrom);
                nearD = juce::jmin (nearD, dv);
                farD = juce::jmax (farD, dv);
            }
            const float dist = varyFrom.empty() ? 0.0f : variants[(size_t) t].distance (varyFrom);
            const int dots = farD - nearD > 1.0e-6f ? juce::jlimit (1, 3, 1 + (int) std::lround (2.0f * (dist - nearD) / (farD - nearD))) : 2;
            for (int k = 0; k < 3; ++k)
            {
                const auto dc = juce::Rectangle<float> (u * 0.45f, u * 0.45f).withCentre ({ a.getRight() - u * (0.9f + 0.65f * (float) (2 - k)), a.getY() + u * 0.95f }); // fill from the left
                if (k < dots) g.fillEllipse (dc);
                else g.drawEllipse (dc, 0.8f);
            }
        }
        const auto inner = a.reduced (u * 0.7f, u * 0.6f).withTrimmedTop (u * 1.2f).withTrimmedBottom (kept ? u * 1.1f : 0.0f);
        juce::Path wave;
        for (size_t i = 0; i < sk.size(); ++i)
        {
            const float x = inner.getX() + inner.getWidth() * (float) i / (float) (sk.size() - 1);
            const float y = inner.getCentreY() - inner.getHeight() * 0.48f * sk[i];
            if (i == 0) wave.startNewSubPath (x, y);
            else wave.lineTo (x, y);
        }
        juce::Path fill (wave);
        fill.lineTo (inner.getRight(), inner.getCentreY());
        fill.lineTo (inner.getX(), inner.getCentreY());
        fill.closeSubPath();
        g.setColour (ink.withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (ink.withAlpha (0.95f));
        g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.0f, u * 0.2f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (kept)
        {
            g.setColour (aud ? colour::display : colour::sunYellow);
            g.setFont (labelFont (u * 1.05f));
            g.drawText (i18n::upper (tr ("Kept")), a.withHeight (u * 1.5f).translated (0.0f, a.getHeight() - u * 1.6f), juce::Justification::centred, false);
        }
    }
    // dice (pips) + lock + strength
    {
        const auto d = diceArea();
        g.setColour (colour::displayText);
        g.drawRoundedRectangle (d.reduced (u * 0.3f), u * 0.9f, u * 0.25f);
        const auto c = d.getCentre();
        const float s = d.getWidth() * 0.22f, pr = u * 0.42f;
        for (auto off : { juce::Point<float> (-s, -s), juce::Point<float> (s, -s), juce::Point<float> (0, 0), juce::Point<float> (-s, s), juce::Point<float> (s, s) })
            g.fillEllipse (juce::Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (c + off));
        // lock: shows that cards are locked (click: unlock all)
        const auto l = lockArea();
        int locked = 0;
        if (isLocked)
            for (const auto& id : ids)
                if (isLocked (id)) ++locked;
        const auto lc = locked > 0 ? colour::sunYellow : colour::displayDim;
        g.setColour (lc);
        const auto body = l.withTrimmedTop (l.getHeight() * 0.45f);
        g.fillRoundedRectangle (body, u * 0.3f);
        juce::Path shackle;
        shackle.addCentredArc (body.getCentreX(), body.getY(), body.getWidth() * 0.3f, l.getHeight() * 0.3f, 0.0f, -juce::MathConstants<float>::halfPi,
                               juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (u * 0.3f));
        // strength: gentle .. wild
        const auto st = strengthArea();
        const auto bar = st.withHeight (u * 0.5f).withY (st.getY() + u * 0.2f);
        g.setColour (colour::displayGrid.brighter (0.4f));
        g.fillRoundedRectangle (bar, u * 0.25f);
        g.setColour (colour::sunYellow);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (u * 0.5f, bar.getWidth() * strength)), u * 0.25f);
        g.fillEllipse (juce::Rectangle<float> (u * 1.1f, u * 1.1f).withCentre ({ bar.getX() + bar.getWidth() * strength, bar.getCentreY() }));
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.0f));
        const auto lbl = st.withTop (bar.getBottom() + u * 0.2f);
        g.drawText (i18n::upper (tr ("Gentle")), lbl, juce::Justification::centredLeft, false);
        g.drawText (i18n::upper (tr ("Wild")), lbl, juce::Justification::centredRight, false);
    }
    g.setColour (colour::displayGrid.brighter (0.35f));
    g.drawVerticalLine ((int) ((float) getWidth() - u * 24.0f), u * 1.2f, b.getBottom() - u * 1.2f);
    // the timeline: the last 20 states
    for (int i = 0; i < (int) history.capacity(); ++i)
    {
        const auto a = dotArea (i);
        const bool filled = i < (int) history.size();
        const bool cur = i == history.current();
        const float r = cur ? u * 0.65f : u * 0.42f;
        g.setColour (cur ? colour::sunYellow : (filled ? colour::displayText.withAlpha (0.75f) : colour::displayGrid.brighter (0.45f)));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (a.getCentre()));
    }
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.05f));
    g.drawText (i18n::upper (tr ("History")), juce::Rectangle<float> ((float) getWidth() - u * 23.0f, u * 6.4f, u * 22.0f, u * 1.6f), juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------- mouse
void DiscoverStrip::mouseDown (const juce::MouseEvent& e)
{
    anchor = e.position;
    dragMacro = -1;
    pressTile = -1;
    dropTile = -1;
    strengthDrag = false;
    holdKept = false;
    for (int m = 0; m < character::numMacros; ++m)
        if (knobArea (m).contains (e.position))
        {
            if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (macroId (m)), this); return; }
            if (auto* q = proc.parameter (juce::String (macroId (m))))
            {
                dragMacro = m;
                dragStart = q->getValue();
                q->beginChangeGesture();
            }
            return;
        }
    if (varyArea().contains (e.position)) { vary(); return; }
    if (diceArea().contains (e.position)) { roll(); return; }
    if (lockArea().contains (e.position)) { if (onUnlockAll) onUnlockAll(); repaint(); return; }
    if (strengthArea().expanded (0.0f, unit()).contains (e.position))
    {
        strengthDrag = true;
        const auto st = strengthArea();
        setStrength ((e.position.x - st.getX()) / st.getWidth());
        return;
    }
    for (int i = 0; i < (int) history.size(); ++i)
        if (dotArea (i).contains (e.position)) { restore (i); return; }
    if (const int t = tileAt (e.position); t >= 0 && t < (int) variants.size())
    {
        pressTile = t;
        pressTime = juce::Time::getMillisecondCounterHiRes();
        audition (t);
    }
}

void DiscoverStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMacro >= 0)
    {
        if (auto* q = proc.parameter (juce::String (macroId (dragMacro))))
            q->setValueNotifyingHost (dragToNormalised (dragStart, anchor.y - e.position.y, unit() * 18.0f, isFineDrag (e.mods)));
        repaint();
        return;
    }
    if (strengthDrag)
    {
        const auto st = strengthArea();
        setStrength ((e.position.x - st.getX()) / st.getWidth());
        return;
    }
    if (pressTile >= 0 && e.getDistanceFromDragStart() > 6)
    {
        const int t = tileAt (e.position);
        const int d = t >= 0 && t != pressTile && t < (int) variants.size() ? t : -1;
        if (d != dropTile) { dropTile = d; repaint(); }
    }
}

void DiscoverStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragMacro >= 0)
        if (auto* q = proc.parameter (juce::String (macroId (dragMacro)))) q->endChangeGesture();
    if (pressTile >= 0 && dropTile >= 0) cross (pressTile, dropTile);
    dragMacro = -1;
    pressTile = -1;
    dropTile = -1;
    strengthDrag = false;
    repaint();
}

void DiscoverStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int m = 0; m < character::numMacros; ++m)
        if (knobArea (m).contains (e.position))
        {
            setParameterNormalised (proc, juce::String (macroId (m)), 0.5f); // 0 = neutral
            return;
        }
    if (const int t = tileAt (e.position); t >= 0) keep (t);
}

juce::String DiscoverStrip::getTooltip()
{
    for (int m = 0; m < character::numMacros; ++m)
        if (knobArea (m).contains (lastMouse)) return pro::hoverText (proc, macroId (m));
    if (varyArea().contains (lastMouse))
        return i18n::pick ("Six relatives of the current sound - click to listen, hold or double-click to keep, drag one onto another to cross them.",
                           "Sechs Verwandte des aktuellen Klangs - klicken zum Anhören, halten oder doppelklicken zum Behalten, einen auf einen anderen ziehen zum Kreuzen.");
    if (tileAt (lastMouse) >= 0)
        return i18n::pick ("Click: listen. Hold or double-click: keep. Drag onto another tile: cross.",
                           "Klick: anhören. Halten oder Doppelklick: behalten. Auf eine andere Kachel ziehen: kreuzen.");
    if (diceArea().contains (lastMouse))
        return i18n::pick ("Roll the dice: changes every unlocked group by the strength below.",
                           "Würfeln: ändert jede nicht gesperrte Gruppe um die Stärke darunter.");
    if (lockArea().contains (lastMouse))
        return i18n::pick ("Locked groups stay as they are. Click: unlock all.", "Gesperrte Gruppen bleiben, wie sie sind. Klick: alle entsperren.");
    if (strengthArea().contains (lastMouse))
        return i18n::pick ("How far VARY and the dice move the sound: gentle .. wild.", "Wie weit VARIIEREN und Würfel den Klang bewegen: sanft .. wild.");
    for (int i = 0; i < (int) history.size(); ++i)
        if (dotArea (i).contains (lastMouse))
            return i18n::pick ("An earlier state - click to go back to it.", "Ein früherer Zustand - klicken, um dorthin zurückzukehren.");
    return {};
}
} // namespace ohpe::ui
