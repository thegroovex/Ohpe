// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pictograms.h"

#include "Palette.h"

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
    float orbit = rad * 0.62f;
    g.setColour (accent (0, s, ink));
    g.fillEllipse (juce::Rectangle<float> (lw * 4.0f, lw * 4.0f).withCentre (c));
    for (int l = 0; l < levels; ++l)
    {
        const float wobble = 1.0f + s.macro[2] * 0.25f * std::sin (s.time * 5.0f + (float) l);
        g.setColour (accent (l % 4, s, ink).withAlpha (s.large ? 0.45f : 0.8f));
        g.drawEllipse (juce::Rectangle<float> (orbit * 2.0f * wobble, orbit * 2.0f * wobble).withCentre (centre), lw * (s.large ? 0.5f : 0.7f));
        const float a = (s.large ? s.time * speed * (1.0f + (float) l * 0.7f) : 0.6f + (float) l) + (float) l;
        centre = centre + juce::Point<float> (std::cos (a), std::sin (a)) * orbit * wobble;
        g.setColour (accent ((l + 1) % 4, s, ink));
        g.fillEllipse (juce::Rectangle<float> (lw * 3.0f, lw * 3.0f).withCentre (centre));
        orbit *= 0.45f;
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
    juce::Random rnd (7);
    for (int i = 0; i < 22; ++i)
    {
        const float bx = rnd.nextFloat(), by = rnd.nextFloat(), sp = 0.2f + rnd.nextFloat();
        const float x = std::fmod (bx + s.time * 0.05f * sp, 1.0f);
        const float y = by + 0.05f * std::sin (s.time * sp * 3.0f + (float) i);
        const float sz = lw * (1.0f + 2.0f * rnd.nextFloat());
        g.setColour (ink.withAlpha (0.4f + 0.6f * rnd.nextFloat()));
        g.fillEllipse (r.getX() + x * r.getWidth(), r.getY() + juce::jlimit (0.05f, 0.9f, y) * r.getHeight(), sz, sz);
    }
}

void vocal (juce::Graphics& g, juce::Rectangle<float> r, const PictoState& s, juce::Colour ink, float lw)
{
    const float open = 0.15f + 0.35f * (0.5f + 0.5f * std::sin (s.time * 4.0f));
    const auto c = r.getCentre();
    const float w = r.getWidth() * 0.38f, h = r.getHeight() * open;
    juce::Path mouth;
    mouth.startNewSubPath (c.x - w, c.y);
    mouth.quadraticTo (c.x, c.y - h, c.x + w, c.y);
    mouth.quadraticTo (c.x, c.y + h * 1.4f, c.x - w, c.y);
    g.setColour (ink);
    g.strokePath (mouth, juce::PathStrokeType (lw));
    eyes (g, { c.x, c.y - r.getHeight() * 0.33f }, lw * 1.5f, s.time, ink);
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
    juce::Random rnd (3);
    g.setColour (ink);
    const int bars = 24;
    for (int i = 0; i < bars; ++i)
    {
        const float h = r.getHeight() * (0.1f + 0.5f * rnd.nextFloat()) * (i % 6 == 0 ? 1.2f : 1.0f);
        const float x = r.getX() + r.getWidth() * (float) i / bars;
        g.fillRect (x, r.getCentreY() - h * 0.5f, r.getWidth() / bars * 0.55f, h);
    }
    const float head = std::fmod (s.time * 0.25f, 1.0f);
    g.drawLine (r.getX() + head * r.getWidth(), r.getY(), r.getX() + head * r.getWidth(), r.getBottom(), lw * 0.7f);
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
    const float hit = std::exp (-std::fmod (s.time * 2.0f, 1.0f) * 6.0f);
    const auto c = r.getCentre();
    const float w = r.getWidth() * 0.32f * (1.0f + 0.06f * hit), h = r.getHeight() * 0.18f;
    g.setColour (ink);
    g.drawEllipse (c.x - w, c.y - h * 1.6f, w * 2, h * 2, lw * 0.7f);
    g.drawLine (c.x - w, c.y - h * 0.6f, c.x - w, c.y + h * 1.4f, lw * 0.7f);
    g.drawLine (c.x + w, c.y - h * 0.6f, c.x + w, c.y + h * 1.4f, lw * 0.7f);
    g.drawLine (c.x - w * 0.6f, c.y - h * 2.8f - hit * h, c.x - w * 0.1f, c.y - h * 1.0f, lw * 0.8f);
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
        default: break;
    }
}
} // namespace

void drawPictogram (juce::Graphics& g, juce::Rectangle<float> area, Picto id, const PictoState& s, juce::Colour ink)
{
    const float lw = juce::jmax (1.0f, juce::jmin (area.getWidth(), area.getHeight()) * (s.large ? 0.022f : 0.075f));
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
