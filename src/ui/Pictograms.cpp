// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pictograms.h"

#include "Palette.h"

#include <cmath>

namespace ohpe::ui
{
namespace
{
constexpr float pi = juce::MathConstants<float>::pi;

juce::Colour accent (int i, const PictoState& s, juce::Colour ink) { return s.large ? colour::encoder (i) : ink; }

void eyes (juce::Graphics& g, juce::Point<float> c, float size, float blinkTime, juce::Colour ink)
{
    // a blink every ~3 s gives every character a bit of life
    const float blink = std::fmod (blinkTime, 3.1f) < 0.12f ? 0.15f : 1.0f;
    g.setColour (ink);
    for (int k = -1; k <= 1; k += 2)
        g.fillEllipse (c.x + k * size * 1.4f - size * 0.5f, c.y - size * 0.5f * blink, size, size * blink);
}

void analog (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a wave creature: the shape morphs saw -> square, the filter is a lowering curtain
    const float shape = s.macro[0], cutoff = s.macro[2], reso = s.macro[3];
    juce::Path p;
    const int n = 64;
    const float amp = r.getHeight() * (0.22f + 0.1f * s.energy);
    const float cy = r.getCentreY() + r.getHeight() * 0.08f;
    const float cycles = s.large ? 2.0f : 1.0f;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / n;
        float ph = std::fmod (x * cycles + (s.large ? s.time * 0.4f : 0.0f), 1.0f);
        const float saw = 1.0f - 2.0f * ph;
        const float sq = ph < 0.5f ? 1.0f : -1.0f;
        float y = saw + (sq - saw) * juce::jlimit (0.0f, 1.0f, shape * 2.0f);
        y += reso * 0.25f * std::sin (ph * 2.0f * pi * 6.0f) * (1.0f - ph);
        const auto px = r.getX() + x * r.getWidth(), py = cy - y * amp;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (accent (0, s, ink));
    g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (s.large)
    {
        g.setColour (accent (2, s, ink).withAlpha (0.25f));
        const float curtain = r.getWidth() * (0.25f + 0.75f * cutoff);
        g.fillRect (r.withLeft (r.getX() + curtain));
        g.setColour (accent (2, s, ink));
        g.drawLine (r.getX() + curtain, r.getY(), r.getX() + curtain, r.getBottom(), lw * 0.6f);
    }
    eyes (g, { r.getX() + r.getWidth() * 0.25f, cy - amp - r.getHeight() * 0.12f }, lw * 1.6f, s.time, ink);
}

void fm (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // operators as planets orbiting each other; algorithm = how many orbit levels, index = wobble
    const auto c = r.getCentre();
    // key icons sit in a wide, flat area: size them by the height but let them use the key's
    // vertical margin, so the icon is as present as its neighbours
    const float rad = s.large ? juce::jmin (r.getWidth(), r.getHeight()) * 0.42f
                              : juce::jmin (r.getWidth() * 0.3f, r.getHeight() * 1.3f);
    const int levels = 1 + (int) std::round (s.macro[0] * 3.0f);
    const float speed = 0.4f + s.macro[1] * 1.5f;
    juce::Point<float> centre = c;
    float orbit = rad * (s.large ? 0.62f : 0.78f);
    g.setColour (accent (0, s, ink));
    g.fillEllipse (juce::Rectangle<float> (lw * (s.large ? 4.0f : 2.6f), lw * (s.large ? 4.0f : 2.6f)).withCentre (c));
    for (int l = 0; l < levels; ++l)
    {
        const float wobble = 1.0f + s.macro[2] * 0.25f * std::sin (s.time * 5.0f + (float) l);
        g.setColour (accent (l % 4, s, ink).withAlpha (s.large ? 0.45f : 0.8f));
        g.drawEllipse (juce::Rectangle<float> (orbit * 2.0f * wobble, orbit * 2.0f * wobble).withCentre (centre), lw * (s.large ? 0.5f : 0.7f));
        const float a = (s.large ? s.time * speed * (1.0f + (float) l * 0.7f) : 0.6f + (float) l) + (float) l;
        centre = centre + juce::Point<float> (std::cos (a), std::sin (a)) * orbit * wobble;
        g.setColour (accent ((l + 1) % 4, s, ink));
        g.fillEllipse (juce::Rectangle<float> (lw * (s.large ? 3.0f : 2.2f), lw * (s.large ? 3.0f : 2.2f)).withCentre (centre));
        orbit *= s.large ? 0.45f : 0.5f;
    }
    if (s.large && s.macro[3] > 0.05f)
    {
        g.setColour (accent (3, s, ink).withAlpha (0.6f));
        g.drawEllipse (juce::Rectangle<float> (lw * 7.0f, lw * 7.0f).withCentre (c), lw * s.macro[3] * 1.2f);
    }
}

void wavetable (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // stacked frames: position morphs the front wave sine -> pulse, warp bends it, tone softens the
    // back frames, unison fans out extra outlines
    const float pos = s.macro[0], warp = s.macro[1], tone = s.macro[2], unison = s.macro[3];
    const int layers = s.large ? 4 : 3;
    for (int k = layers - 1; k >= 0; --k)
    {
        juce::Path p;
        const float off = (float) k * r.getHeight() * 0.11f;
        const float morph = juce::jlimit (0.0f, 1.0f, pos + 0.12f * (float) k);
        for (int i = 0; i <= 48; ++i)
        {
            const float x = (float) i / 48.0f;
            const float ph = x - warp * 0.14f * std::sin (2.0f * pi * x); // bent phase
            const float sine = std::sin (ph * 2.0f * pi);
            const float pulse = std::tanh (sine * (1.0f + 9.0f * morph)) / std::tanh (1.0f + 9.0f * morph);
            float y = sine + (pulse - sine) * morph;
            if (s.large) y += 0.06f * std::sin (x * 8.0f * pi + s.time * 2.0f) * (float) k;
            const auto px = r.getX() + r.getWidth() * (0.08f + 0.72f * x) + off;
            const auto py = r.getCentreY() + off - y * r.getHeight() * (0.16f + 0.04f * s.energy) - r.getHeight() * 0.1f;
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        const float alpha = k == 0 ? 1.0f : (0.25f + 0.5f * tone) * (1.0f - 0.18f * (float) k);
        g.setColour ((k == 0 ? accent (0, s, ink) : ink).withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (lw * (k == 0 ? 0.9f : 0.6f)));
        if (k == 0 && unison > 0.05f)
            for (int u = -1; u <= 1; u += 2)
            {
                g.setColour (accent (3, s, ink).withAlpha (0.35f * unison));
                g.strokePath (p, juce::PathStrokeType (lw * 0.4f),
                              juce::AffineTransform::translation ((float) u * unison * lw * 2.5f, (float) -u * unison * lw));
            }
    }
}

void pluck (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a string between two pegs; material turns it into a struck bar, damping shortens the ringing,
    // brightness adds wiggles, body grows the sound box underneath
    const float material = s.macro[0], damping = s.macro[1], bright = s.macro[2], body = s.macro[3];
    const float period = 0.6f + 2.2f * (1.0f - damping);
    const float decay = std::exp (-std::fmod (s.time, period) * (1.5f + 6.0f * damping));
    const float cy = r.getCentreY() - r.getHeight() * 0.08f;
    if (body > 0.05f)
    {
        const float bw = r.getWidth() * (0.35f + 0.4f * body), bh = r.getHeight() * (0.12f + 0.2f * body);
        g.setColour (accent (3, s, ink).withAlpha (0.35f + 0.3f * body));
        g.drawRoundedRectangle (juce::Rectangle<float> (bw, bh).withCentre ({ r.getCentreX(), cy + r.getHeight() * 0.22f + bh * 0.5f }), bh * 0.4f, lw * 0.6f);
    }
    juce::Path p;
    for (int i = 0; i <= 48; ++i)
    {
        const float x = (float) i / 48.0f;
        const float wobble = std::sin (x * pi) + bright * 0.35f * std::sin (x * 3.0f * pi) * std::sin (s.time * 47.0f);
        const float y = wobble * std::sin (s.time * 30.0f) * decay * r.getHeight() * 0.22f * (1.0f - 0.6f * material);
        const auto px = r.getX() + x * r.getWidth(), py = cy + y;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (ink);
    g.strokePath (p, juce::PathStrokeType (lw * (1.0f + 1.8f * material)));
    g.fillEllipse (r.getX() - lw, cy - lw, lw * 2, lw * 2);
    g.fillEllipse (r.getRight() - lw, cy - lw, lw * 2, lw * 2);
    // pick (string) or mallet head (bar)
    g.setColour (accent (0, s, ink));
    const float hx = r.getX() + r.getWidth() * 0.62f;
    if (material < 0.5f)
        g.fillRoundedRectangle (hx, r.getY() + r.getHeight() * 0.06f, lw * 2.0f, r.getHeight() * 0.22f, lw);
    else
    {
        g.drawLine (hx + lw, r.getY() + r.getHeight() * 0.02f, hx + lw, cy - lw * 3.0f, lw * 0.8f);
        g.fillEllipse (hx - lw * 0.8f, cy - lw * 4.6f, lw * 3.6f, lw * 3.6f);
    }
}

void grain (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a cloud of grains: position moves the cloud through the sample, size = grain size,
    // density = number of grains, spray scatters them
    const float pos = s.macro[0], size = s.macro[1], density = s.macro[2], spray = s.macro[3];
    const int count = s.large ? 8 + (int) (density * 40.0f) : 12;
    const float cx = r.getX() + r.getWidth() * (0.15f + 0.7f * pos);
    if (s.large)
    {
        // the "sample" as a faint baseline with the read window
        g.setColour (ink.withAlpha (0.25f));
        g.drawLine (r.getX(), r.getBottom() - lw, r.getRight(), r.getBottom() - lw, lw * 0.5f);
        g.setColour (accent (0, s, ink));
        g.fillRect (cx - lw, r.getBottom() - lw * 3.0f, lw * 2.0f, lw * 4.0f);
    }
    juce::Random rnd (7);
    for (int i = 0; i < count; ++i)
    {
        const float bx = rnd.nextFloat() - 0.5f, by = rnd.nextFloat() - 0.5f, sp = 0.2f + rnd.nextFloat();
        const float scatter = 0.12f + 0.75f * spray;
        float x = cx + bx * r.getWidth() * scatter + std::sin (s.time * sp * 1.7f + (float) i) * lw * 2.0f;
        float y = r.getCentreY() - r.getHeight() * 0.08f + by * r.getHeight() * (0.2f + 0.6f * spray)
                  + 0.04f * r.getHeight() * std::sin (s.time * sp * 3.0f + (float) i);
        if (! s.large) { x = r.getX() + (bx + 0.5f) * r.getWidth(); y = r.getY() + (by + 0.5f) * r.getHeight(); }
        const float sz = lw * (s.large ? (0.8f + 3.2f * size) * (0.6f + 0.6f * rnd.nextFloat()) : 1.0f + 2.0f * rnd.nextFloat());
        const float life = 0.5f + 0.5f * std::sin (s.time * (1.0f + 2.0f * density) * sp + (float) i * 1.3f);
        g.setColour ((i % 5 == 0 ? accent (2, s, ink) : ink).withAlpha (s.large ? 0.25f + 0.75f * life : 0.4f + 0.6f * rnd.nextFloat()));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void vocal (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a singing face: the vowel shapes the mouth (a e i o u), formant moves it up / down,
    // breath blows little puffs, tension raises the brows
    const float vowel = s.macro[0], formant = s.macro[1], breath = s.macro[2], tension = s.macro[3];
    const auto c = r.getCentre().translated (0, (0.5f - formant) * r.getHeight() * 0.12f);
    const float sing = s.large ? 0.75f + 0.25f * std::sin (s.time * 4.0f) * (0.3f + s.energy) : 1.0f;
    // vowel 0..1 over a, e, i, o, u: width and opening of the mouth
    const float widths[] = { 0.34f, 0.40f, 0.44f, 0.24f, 0.16f };
    const float opens[] = { 0.36f, 0.22f, 0.10f, 0.30f, 0.20f };
    const float vp = juce::jlimit (0.0f, 3.999f, vowel * 4.0f);
    const int vi = (int) vp;
    const float vf = vp - (float) vi;
    const float w = r.getWidth() * (widths[vi] + (widths[vi + 1] - widths[vi]) * vf);
    const float h = r.getHeight() * (opens[vi] + (opens[vi + 1] - opens[vi]) * vf) * sing;
    juce::Path mouth;
    mouth.startNewSubPath (c.x - w, c.y);
    mouth.quadraticTo (c.x, c.y - h, c.x + w, c.y);
    mouth.quadraticTo (c.x, c.y + h * 1.4f, c.x - w, c.y);
    g.setColour (ink);
    g.strokePath (mouth, juce::PathStrokeType (lw));
    eyes (g, { c.x, c.y - r.getHeight() * 0.33f }, lw * 1.5f, s.time, ink);
    if (s.large)
    {
        g.setColour (accent (3, s, ink));
        const float brow = r.getHeight() * (0.42f + 0.08f * tension);
        for (int k = -1; k <= 1; k += 2)
            g.drawLine (c.x + (float) k * lw * 3.2f, c.y - brow + (float) k * tension * lw, c.x + (float) k * lw * 0.9f,
                        c.y - brow - (float) k * tension * lw, lw * 0.6f);
        g.setColour (accent (2, s, ink).withAlpha (0.6f));
        const int puffs = (int) (breath * 6.0f);
        for (int i = 0; i < puffs; ++i)
        {
            const float ph = std::fmod (s.time * 0.8f + (float) i / (float) juce::jmax (1, puffs), 1.0f);
            const float px = c.x + w + ph * r.getWidth() * 0.35f, py = c.y + std::sin (ph * 6.0f + (float) i) * lw * 2.0f;
            g.fillEllipse (px, py, lw * (1.0f - ph) * 2.0f, lw * (1.0f - ph) * 2.0f);
        }
    }
}

void cluster (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a stack of saw teeth: chord sets how many rows and how far apart, detune jitters them,
    // spread fans them out sideways, tone fades the upper rows
    const float detune = s.macro[0], spread = s.macro[1], tone = s.macro[2], chord = s.macro[3];
    const int rows = 3 + (int) std::round (chord * 4.0f);
    const float gap = r.getHeight() * (0.07f + 0.06f * chord);
    for (int k = 0; k < rows; ++k)
    {
        juce::Path p;
        const float centred = (float) k - 0.5f * (float) (rows - 1);
        const float y0 = r.getCentreY() + centred * gap;
        const float fan = 1.0f + 0.25f * spread * std::abs (centred) / (float) rows;
        const float jitter = std::sin (s.time * (1.0f + 0.7f * (float) k) + (float) k) * lw * (0.3f + 2.5f * detune);
        const float w = r.getWidth() * 0.8f * fan;
        const float x0 = r.getCentreX() - w * 0.5f + centred * spread * lw * 1.5f;
        for (int i = 0; i < 5; ++i)
        {
            const float x = x0 + w * (float) i / 5.0f + jitter;
            p.startNewSubPath (x, y0 + gap * 0.7f);
            p.lineTo (x + w / 5.0f * 0.92f, y0);
        }
        const float alpha = 1.0f - (1.0f - tone) * 0.7f * (float) k / (float) rows;
        g.setColour ((k == rows / 2 ? accent (3, s, ink) : ink).withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (lw * 0.6f));
    }
}

void sampler (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a waveform: start marker, tone darkens the tail, loop start / end bracket the loop
    const float start = s.macro[0], tone = s.macro[1], loopStart = s.macro[2], loopEnd = s.macro[3];
    juce::Random rnd (3);
    const int bars = s.large ? 40 : 24;
    for (int i = 0; i < bars; ++i)
    {
        const float x01 = (float) i / (float) bars;
        const float env = s.large ? std::exp (-x01 * 2.2f) * 0.8f + 0.2f : 1.0f;
        const float h = r.getHeight() * (0.1f + 0.5f * rnd.nextFloat()) * env * (i % 6 == 0 ? 1.2f : 1.0f);
        const float x = r.getX() + r.getWidth() * x01;
        const bool played = x01 >= start;
        g.setColour (ink.withAlpha (s.large ? (played ? 1.0f - 0.6f * (1.0f - tone) * x01 : 0.25f) : 1.0f));
        g.fillRect (x, r.getCentreY() - h * 0.5f, r.getWidth() / (float) bars * 0.55f, h);
    }
    if (s.large)
    {
        auto marker = [&] (float pos01, juce::Colour c, float hFrac) {
            const float x = r.getX() + r.getWidth() * pos01;
            g.setColour (c);
            g.drawLine (x, r.getCentreY() - r.getHeight() * hFrac, x, r.getCentreY() + r.getHeight() * hFrac, lw * 0.7f);
        };
        marker (start, accent (0, s, ink), 0.42f);
        const float ls = juce::jmin (loopStart, loopEnd), le = juce::jmax (loopStart, loopEnd);
        if (le > ls)
        {
            g.setColour (accent (2, s, ink).withAlpha (0.18f));
            g.fillRect (r.getX() + r.getWidth() * ls, r.getY() + r.getHeight() * 0.12f, r.getWidth() * (le - ls), r.getHeight() * 0.76f);
            marker (ls, accent (2, s, ink), 0.38f);
            marker (le, accent (3, s, ink), 0.38f);
        }
        const float span = juce::jmax (0.05f, 1.0f - start);
        const float head = start + std::fmod (s.time * 0.25f, 1.0f) * span;
        g.setColour (ink);
        g.drawLine (r.getX() + head * r.getWidth(), r.getY(), r.getX() + head * r.getWidth(), r.getBottom(), lw * 0.5f);
    }
}

void envelope (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    const float a = 0.05f + 0.25f * s.macro[0], d = 0.05f + 0.25f * s.macro[1], sus = s.macro[2], rel = 0.05f + 0.25f * s.macro[3];
    const float total = a + d + 0.25f + rel;
    auto pt = [&] (float t, float v) { return juce::Point<float> (r.getX() + r.getWidth() * t / total, r.getBottom() - v * r.getHeight() * 0.85f); };
    juce::Path p;
    p.startNewSubPath (pt (0, 0));
    p.lineTo (pt (a, 1.0f));
    p.quadraticTo (pt (a + d * 0.3f, sus + (1.0f - sus) * 0.2f), pt (a + d, sus));
    p.lineTo (pt (a + d + 0.25f, sus));
    p.quadraticTo (pt (a + d + 0.25f + rel * 0.3f, sus * 0.2f), pt (total, 0));
    g.setColour (ink);
    g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (s.large)
    {
        const float t = std::fmod (s.time * 0.5f, 1.0f) * total;
        juce::Point<float> dot;
        if (t < a) dot = pt (t, t / a);
        else if (t < a + d) dot = pt (t, 1.0f + (sus - 1.0f) * (t - a) / d);
        else if (t < a + d + 0.25f) dot = pt (t, sus);
        else dot = pt (t, sus * (1.0f - (t - a - d - 0.25f) / rel));
        g.setColour (colour::signalRed);
        g.fillEllipse (juce::Rectangle<float> (lw * 4, lw * 4).withCentre (dot));
    }
}

void lfo (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    juce::Path p;
    const float cycles = s.large ? 1.0f + s.macro[0] * 3.0f : 1.0f;
    for (int i = 0; i <= 48; ++i)
    {
        const float x = (float) i / 48.0f;
        const auto px = r.getX() + x * r.getWidth(), py = r.getCentreY() - std::sin (x * 2.0f * pi * cycles) * r.getHeight() * 0.3f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (ink);
    g.strokePath (p, juce::PathStrokeType (lw));
    const float x = std::fmod (s.time * 0.3f, 1.0f);
    const float y = r.getCentreY() - std::sin (x * 2.0f * pi * cycles) * r.getHeight() * 0.3f;
    g.setColour (s.large ? colour::sunYellow : ink);
    g.fillEllipse (juce::Rectangle<float> (lw * 3.5f, lw * 3.5f).withCentre ({ r.getX() + x * r.getWidth(), y - lw * 2.5f }));
}

void mixer (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    for (int i = 0; i < 4; ++i)
    {
        const float x = r.getX() + r.getWidth() * (0.125f + 0.25f * (float) i);
        g.setColour (ink.withAlpha (0.5f));
        g.drawLine (x, r.getY() + lw, x, r.getBottom() - lw, lw * 0.5f);
        const float pos = 0.5f + 0.35f * std::sin ((float) i * 1.7f + (s.large ? s.time : 0.0f));
        g.setColour (ink);
        g.fillRoundedRectangle (juce::Rectangle<float> (lw * 4.0f, lw * 2.0f).withCentre ({ x, r.getY() + pos * r.getHeight() }), lw * 0.5f);
    }
}

void effects (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    const auto c = r.getCentre().translated (-r.getWidth() * 0.25f, 0);
    for (int i = 0; i < 4; ++i)
    {
        const float ph = std::fmod ((s.large ? s.time * 0.5f : 0.0f) + (float) i * 0.25f, 1.0f);
        const float rad = r.getHeight() * (0.15f + 0.6f * ph);
        g.setColour (ink.withAlpha (1.0f - ph));
        g.drawEllipse (juce::Rectangle<float> (rad, rad).withCentre (c.translated (ph * r.getWidth() * 0.5f, 0)), lw * 0.7f);
    }
}

void sequencer (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    const int cols = 4, rows = 2;
    const int playing = (int) (s.time * 4.0f) % (cols * rows);
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
        {
            const auto cell = juce::Rectangle<float> (r.getX() + r.getWidth() * (float) x / cols, r.getY() + r.getHeight() * (0.15f + 0.38f * (float) y),
                                                      r.getWidth() / cols, r.getHeight() * 0.32f).reduced (lw);
            const bool on = ((x + y * 3) % 3) == 0;
            g.setColour (ink.withAlpha (on ? 1.0f : 0.35f));
            if (on) g.fillRoundedRectangle (cell, lw); else g.drawRoundedRectangle (cell, lw, lw * 0.5f);
            if (s.large && x + y * cols == playing)
            {
                g.setColour (colour::signalRed);
                g.drawRoundedRectangle (cell.expanded (lw), lw, lw);
            }
        }
}

void tape (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    const float rad = juce::jmin (r.getWidth() * 0.22f, r.getHeight() * 0.38f);
    for (int k = 0; k < 2; ++k)
    {
        const auto c = juce::Point<float> (r.getX() + r.getWidth() * (k == 0 ? 0.27f : 0.73f), r.getCentreY());
        g.setColour (ink);
        g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c), lw * 0.7f);
        for (int sp = 0; sp < 3; ++sp)
        {
            const float a = s.time * 2.0f + (float) sp * 2.0f * pi / 3.0f;
            g.drawLine (c.x, c.y, c.x + std::cos (a) * rad * 0.7f, c.y + std::sin (a) * rad * 0.7f, lw * 0.6f);
        }
    }
    g.drawLine (r.getX() + r.getWidth() * 0.27f, r.getCentreY() + rad, r.getX() + r.getWidth() * 0.73f, r.getCentreY() + rad, lw * 0.7f);
}

void drum (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    // a drum: model (kick, snare, hat ...) changes its shape, tune its size, decay how long the
    // skin rings, tone the brightness of the strike flash
    const float model = s.macro[0], tune = s.macro[1], decay = s.macro[2], tone = s.macro[3];
    const float period = s.large ? 0.5f + decay : 1.0f;
    const float hit = std::exp (-std::fmod (s.time, period) * (s.large ? 3.0f + 8.0f * (1.0f - decay) : 6.0f));
    const auto c = r.getCentre().translated (0, r.getHeight() * 0.08f);
    const float scale = s.large ? 0.75f + 0.5f * (1.0f - tune) : 1.0f;
    const bool cymbal = s.large && std::lround (model * 5.0f) == 2; // synth model "Hat"
    const float w = r.getWidth() * 0.32f * scale * (1.0f + 0.06f * hit), h = r.getHeight() * 0.18f * scale;
    g.setColour (ink);
    if (cymbal)
    {
        g.drawEllipse (c.x - w, c.y - h * 0.5f - hit * lw * 2.0f, w * 2.0f, h * 0.7f, lw * 0.7f);
        g.drawLine (c.x, c.y, c.x, c.y + h * 2.0f, lw * 0.6f);
    }
    else
    {
        const float depth = s.large ? 0.6f + 1.4f * (1.0f - model) : 1.4f; // kick deep, perc shallow
        g.drawEllipse (c.x - w, c.y - h * 1.6f, w * 2, h * 2, lw * 0.7f);
        g.drawLine (c.x - w, c.y - h * 0.6f, c.x - w, c.y - h * 0.6f + h * depth * 1.4f, lw * 0.7f);
        g.drawLine (c.x + w, c.y - h * 0.6f, c.x + w, c.y - h * 0.6f + h * depth * 1.4f, lw * 0.7f);
        g.drawLine (c.x - w * 0.6f, c.y - h * 2.8f - hit * h, c.x - w * 0.1f, c.y - h * 1.0f, lw * 0.8f);
    }
    if (s.large)
    {
        // strike flash: rays, brighter and longer with tone
        g.setColour (accent (3, s, ink).withAlpha (hit));
        for (int k = 0; k < 5; ++k)
        {
            const float a = -pi * 0.5f + ((float) k - 2.0f) * 0.35f;
            const float r0 = w * 0.3f, r1 = w * (0.45f + 0.5f * tone);
            const auto o = c.translated (0, -h * 1.0f);
            g.drawLine (o.x + std::cos (a) * r0, o.y + std::sin (a) * r0, o.x + std::cos (a) * r1, o.y + std::sin (a) * r1, lw * 0.6f);
        }
    }
}

void glyph (juce::Graphics& g, juce::Rectangle<float> r, Picto id, juce::Colour ink, float lw)
{
    g.setColour (ink);
    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.3f;
    juce::Path p;
    switch (id)
    {
        case Picto::Play: p.addTriangle (c.x - s * 0.7f, c.y - s, c.x - s * 0.7f, c.y + s, c.x + s, c.y); g.fillPath (p); break;
        case Picto::OctaveDown: p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y); g.fillPath (p); break;
        case Picto::OctaveUp: p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y); g.fillPath (p); break;
        case Picto::Midi:
            g.drawEllipse (juce::Rectangle<float> (s * 2, s * 2).withCentre (c), lw * 0.7f);
            for (int i = 0; i < 5; ++i)
            {
                const float a = pi + (float) i * pi / 4.0f;
                g.fillEllipse (juce::Rectangle<float> (lw, lw).withCentre (c + juce::Point<float> (std::cos (a), -std::sin (a)) * s * 0.6f));
            }
            break;
        case Picto::Library:
            // a shelf of three records, one pulled out
            for (int i = 0; i < 3; ++i)
            {
                const float x = c.x - s * 1.1f + (float) i * s * 0.75f;
                const float up = i == 2 ? s * 0.35f : 0.0f;
                g.drawRoundedRectangle (x, c.y - s - up, s * 0.55f, s * 2.0f, lw * 0.5f, lw * 0.7f);
            }
            g.drawLine (c.x - s * 1.3f, c.y + s * 1.05f, c.x + s * 1.3f, c.y + s * 1.05f, lw * 0.8f);
            break;
        case Picto::Pro:
            // a dense grid of sliders
            for (int i = 0; i < 4; ++i)
            {
                const float x = c.x - s + (float) i * s * 0.66f;
                g.drawLine (x, c.y - s, x, c.y + s, lw * 0.5f);
                const float y = c.y - s + s * 2.0f * (0.25f + 0.17f * (float) ((i * 7) % 4));
                g.fillRoundedRectangle (x - lw * 1.2f, y - lw * 0.6f, lw * 2.4f, lw * 1.2f, lw * 0.3f);
            }
            break;
        case Picto::Load:
            p.startNewSubPath (c.x, c.y + s * 0.4f);
            p.lineTo (c.x, c.y - s);
            p.startNewSubPath (c.x - s * 0.5f, c.y - s * 0.5f);
            p.lineTo (c.x, c.y - s);
            p.lineTo (c.x + s * 0.5f, c.y - s * 0.5f);
            p.startNewSubPath (c.x - s, c.y + s * 0.3f);
            p.lineTo (c.x - s, c.y + s);
            p.lineTo (c.x + s, c.y + s);
            p.lineTo (c.x + s, c.y + s * 0.3f);
            g.strokePath (p, juce::PathStrokeType (lw * 0.8f));
            break;
        case Picto::Export:
            p.startNewSubPath (c.x, c.y - s);
            p.lineTo (c.x, c.y + s * 0.4f);
            p.startNewSubPath (c.x - s * 0.5f, c.y - s * 0.1f);
            p.lineTo (c.x, c.y + s * 0.4f);
            p.lineTo (c.x + s * 0.5f, c.y - s * 0.1f);
            p.startNewSubPath (c.x - s, c.y + s * 0.3f);
            p.lineTo (c.x - s, c.y + s);
            p.lineTo (c.x + s, c.y + s);
            p.lineTo (c.x + s, c.y + s * 0.3f);
            g.strokePath (p, juce::PathStrokeType (lw * 0.8f));
            break;
        // p11: tape transport - plain geometric signs, the same weight as Play
        case Picto::Stop:
            g.fillRoundedRectangle (juce::Rectangle<float> (s * 1.6f, s * 1.6f).withCentre (c), lw * 0.6f);
            break;
        case Picto::Record:
            g.fillEllipse (juce::Rectangle<float> (s * 1.8f, s * 1.8f).withCentre (c));
            break;
        case Picto::ToStart:
            g.fillRoundedRectangle (c.x - s * 0.95f, c.y - s, lw * 1.2f, s * 2.0f, lw * 0.3f);
            p.addTriangle (c.x + s * 0.85f, c.y - s, c.x + s * 0.85f, c.y + s, c.x - s * 0.55f, c.y);
            g.fillPath (p);
            break;
        case Picto::Loop:
        {
            // an open rounded loop with an arrow head: the tape runs round
            const auto box = juce::Rectangle<float> (s * 2.2f, s * 1.4f).withCentre (c);
            const float rr = box.getHeight() * 0.45f;
            p.startNewSubPath (box.getX() + box.getWidth() * 0.62f, box.getBottom());
            p.lineTo (box.getX() + rr, box.getBottom());
            p.quadraticTo (box.getX(), box.getBottom(), box.getX(), box.getBottom() - rr);
            p.lineTo (box.getX(), box.getY() + rr);
            p.quadraticTo (box.getX(), box.getY(), box.getX() + rr, box.getY());
            p.lineTo (box.getRight() - rr, box.getY());
            p.quadraticTo (box.getRight(), box.getY(), box.getRight(), box.getY() + rr);
            p.lineTo (box.getRight(), box.getBottom() - rr);
            g.strokePath (p, juce::PathStrokeType (lw * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path head;
            // the head at the open end, pointing the way the tape runs (clockwise)
            const float hx = box.getRight(), hy = box.getBottom() - rr, hs = s * 0.38f;
            head.addTriangle (hx - hs, hy - hs * 0.2f, hx + hs, hy - hs * 0.2f, hx, hy + hs * 1.2f);
            g.fillPath (head);
            break;
        }
        // p14: RANDOM - a die showing three, drawn as an outline with round pips
        case Picto::Random:
        {
            const auto box = juce::Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (c);
            g.drawRoundedRectangle (box, s * 0.45f, lw * 0.8f);
            const float pr = s * 0.36f;
            for (int i = 0; i < 3; ++i)
            {
                const float t = (float) i / 2.0f;
                const auto pc = juce::Point<float> (box.getX() + box.getWidth() * (0.28f + 0.44f * t), box.getY() + box.getHeight() * (0.28f + 0.44f * t));
                g.fillEllipse (juce::Rectangle<float> (pr, pr).withCentre (pc));
            }
            break;
        }
        // p14: SAVE - a preset card with a plus: "keep this sound"
        case Picto::Save:
        {
            const auto box = juce::Rectangle<float> (s * 1.7f, s * 2.1f).withCentre (c);
            const float fold = s * 0.55f;
            p.startNewSubPath (box.getX(), box.getY());
            p.lineTo (box.getRight() - fold, box.getY());
            p.lineTo (box.getRight(), box.getY() + fold);
            p.lineTo (box.getRight(), box.getBottom());
            p.lineTo (box.getX(), box.getBottom());
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (lw * 0.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
            const auto pc = box.getCentre().translated (0, s * 0.12f);
            const float a = s * 0.45f;
            g.drawLine (pc.x - a, pc.y, pc.x + a, pc.y, lw * 0.8f);
            g.drawLine (pc.x, pc.y - a, pc.x, pc.y + a, lw * 0.8f);
            break;
        }
        default: break;
    }
}
} // namespace

void drawPictogram (juce::Graphics& g, juce::Rectangle<float> area, Picto id, const PictoState& s, juce::Colour ink)
{
    const float lw = juce::jmax (1.0f, juce::jmin (area.getWidth(), area.getHeight()) * (s.large ? 0.022f : 0.062f)); // p14: finer key icons
    const auto r = area.reduced (lw * 2.0f);
    switch (id)
    {
        case Picto::Analog: analog (g, r, s, ink, lw); break;
        case Picto::FM: fm (g, r, s, ink, lw); break;
        case Picto::Wavetable: wavetable (g, r, s, ink, lw); break;
        case Picto::Pluck: pluck (g, r, s, ink, lw); break;
        case Picto::Grain: grain (g, r, s, ink, lw); break;
        case Picto::Vocal: vocal (g, r, s, ink, lw); break;
        case Picto::Cluster: cluster (g, r, s, ink, lw); break;
        case Picto::Sampler: sampler (g, r, s, ink, lw); break;
        case Picto::Synth: analog (g, r, s, ink, lw); break;
        case Picto::Envelope: envelope (g, r, s, ink, lw); break;
        case Picto::Lfo: lfo (g, r, s, ink, lw); break;
        case Picto::Mixer: mixer (g, r, s, ink, lw); break;
        case Picto::Effects: effects (g, r, s, ink, lw); break;
        case Picto::Sequencer: sequencer (g, r, s, ink, lw); break;
        case Picto::Tape: tape (g, r, s, ink, lw); break;
        case Picto::Drum: drum (g, r, s, ink, lw); break;
        default: glyph (g, r, id, ink, lw); break;
    }
}
} // namespace ohpe::ui
