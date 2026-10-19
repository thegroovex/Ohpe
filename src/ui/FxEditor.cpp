// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FxEditor.h"

#include "ModFxViews.h"
#include "Palette.h"
#include "ResoView.h"
#include "SpaceView.h"
#include "Widgets.h"
#include "fx/RackEffects.h"
#include "fx/Resonator.h"
#include "fx/Space3D.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

juce::Colour edit() { return colour::sunYellow; }
juce::Colour modc() { return colour::ultraviolet; }

const ParamDescriptor* descOf (const Core& core, const std::string& id)
{
    const int i = core.params().indexOf (id);
    return i >= 0 ? &core.params().descriptor (i) : nullptr;
}

/** The value the user sees now (the host parameter; the registry follows with the next audio block). */
float plainOf (OhpeProcessor& proc, const std::string& id)
{
    if (id.empty()) return 0.0f;
    if (auto* q = proc.parameter (juce::String (id))) return q->convertFrom0to1 (q->getValue());
    return proc.core().params().get (id);
}

int typeOf (OhpeProcessor& proc, const std::string& slotPrefix) { return (int) std::lround (plainOf (proc, slotPrefix + "type")); }

juce::String upperPick (const std::string& en, const std::string& de) { return i18n::upper (i18n::pick (en, de)); }
} // namespace

// ================================================================= layouts
namespace fxgraph
{
std::string effectBlock (const Core& core, const std::string& pre)
{
    const int i = core.params().indexOf (pre + "type");
    if (i < 0) return {};
    const int t = (int) std::lround (core.params().get (i));
    if (t <= InsertSlot::Off || t >= InsertSlot::numTypes) return {};
    const auto info = InsertSlot::typeInfo (t);
    return info.id != nullptr ? pre + info.id + "." : std::string();
}

namespace
{
Axis norm (float f0 = 0.0f, float f1 = 1.0f) { Axis a; a.kind = Axis::Norm; a.f0 = f0; a.f1 = f1; return a; }
Axis lin (float lo, float hi, float f0 = 0.0f, float f1 = 1.0f) { Axis a; a.kind = Axis::Lin; a.lo = lo; a.hi = hi; a.f0 = f0; a.f1 = f1; return a; }
Axis lg (float lo, float hi, float f0 = 0.0f, float f1 = 1.0f) { Axis a; a.kind = Axis::Log; a.lo = lo; a.hi = hi; a.f0 = f0; a.f1 = f1; return a; }
Axis fix (float f) { Axis a; a.fixed = f; return a; }

/** The value range of an axis (its own or the parameter's). */
juce::Range<float> rangeOf (const Core& core, const Axis& a, const std::string& id)
{
    if (a.lo < a.hi) return { a.lo, a.hi };
    if (const auto* d = descOf (core, id)) return { d->minValue, juce::jmax (d->minValue + 1.0e-6f, d->maxValue) };
    return { 0.0f, 1.0f };
}
} // namespace

float fractionOf (OhpeProcessor& proc, const Axis& a, const std::string& id)
{
    if (a.kind == Axis::None || id.empty()) return a.fixed;
    float t = 0.0f;
    if (a.kind == Axis::Norm)
    {
        auto* q = proc.parameter (juce::String (id));
        t = q != nullptr ? q->getValue() : 0.0f;
    }
    else
    {
        const auto r = rangeOf (proc.core(), a, id);
        const float v = plainOf (proc, id);
        if (a.kind == Axis::Log && r.getStart() > 0.0f)
            t = std::log (juce::jmax (r.getStart(), v) / r.getStart()) / std::log (r.getEnd() / r.getStart());
        else
            t = (v - r.getStart()) / (r.getEnd() - r.getStart());
    }
    return a.f0 + (a.f1 - a.f0) * juce::jlimit (0.0f, 1.0f, t);
}

float normalisedFor (OhpeProcessor& proc, const Axis& a, const std::string& id, float f)
{
    const auto* d = descOf (proc.core(), id);
    if (d == nullptr) return 0.0f;
    const float t = std::abs (a.f1 - a.f0) < 1.0e-6f ? 0.0f : juce::jlimit (0.0f, 1.0f, (f - a.f0) / (a.f1 - a.f0));
    float v = 0.0f;
    if (a.kind == Axis::Norm) v = d->fromNormalised (t);
    else
    {
        const auto r = rangeOf (proc.core(), a, id);
        if (a.kind == Axis::Log && r.getStart() > 0.0f) v = r.getStart() * std::pow (r.getEnd() / r.getStart(), t);
        else v = r.getStart() + t * (r.getEnd() - r.getStart());
    }
    return juce::jlimit (0.0f, 1.0f, d->toNormalised (d->clamp (v)));
}

Layout layoutFor (OhpeProcessor& proc, const std::string& pre)
{
    Layout L;
    L.regions = { { 0.0f, 0.0f, 1.0f, 1.0f } };
    const auto& core = proc.core();
    const int t = typeOf (proc, pre);
    if (t <= InsertSlot::Off || t >= InsertSlot::numTypes) return L;
    const auto info = InsertSlot::typeInfo (t);
    if (info.id == nullptr) return L;
    const std::string b = pre + info.id + ".";
    const std::string id = info.id;
    auto has = [&] (const char* k) { return core.params().indexOf (b + k) >= 0; };
    auto P = [&] (const char* k) { return has (k) ? b + k : std::string(); };
    auto add = [&] (const char* x, Axis ax, const char* y, Axis ay, const char* en, const char* de, const char* alt = "", int region = 0) -> Handle& {
        Handle h;
        h.x = x != nullptr && *x != 0 ? P (x) : std::string();
        h.y = y != nullptr && *y != 0 ? P (y) : std::string();
        h.alt = alt != nullptr && *alt != 0 ? P (alt) : std::string();
        h.ax = h.x.empty() && ax.kind != Axis::None ? fix (0.5f) : ax;
        h.ay = h.y.empty() && ay.kind != Axis::None ? fix (0.5f) : ay;
        h.nameEn = en;
        h.nameDe = de;
        h.region = region;
        L.handles.push_back (h);
        return L.handles.back();
    };
    // rate or (when the effect follows the tempo) its sync division
    auto rateKey = [&] () -> const char* { return has ("sync") && plainOf (proc, b + "sync") >= 0.5f ? "sync" : "rate"; };
    auto rateAxis = [&] () { return std::string (rateKey()) == "sync" ? norm (0.02f, 0.98f) : lg (0.0f, 0.0f, 0.02f, 0.98f); };
    // two slider rows under a wave
    const juce::Rectangle<float> wave { 0.0f, 0.0f, 1.0f, 0.68f }, row1 { 0.0f, 0.74f, 1.0f, 0.12f }, row2 { 0.0f, 0.88f, 1.0f, 0.12f };

    if (id == "eq")
    {
        L.regions = { { 0.0f, 0.0f, 0.92f, 1.0f }, { 0.945f, 0.0f, 0.055f, 1.0f } };
        const auto X = lg (20.0f, 20000.0f), Y = lin (-18.0f, 18.0f, 0.05f, 0.95f);
        add ("lowfreq", X, "lowgain", Y, "Low Shelf", "Low Shelf");
        add ("p1freq", X, "p1gain", Y, "Band 1", "Band 1", "p1q");
        add ("p2freq", X, "p2gain", Y, "Band 2", "Band 2", "p2q");
        add ("p3freq", X, "p3gain", Y, "Band 3", "Band 3", "p3q");
        add ("highfreq", X, "highgain", Y, "High Shelf", "High Shelf");
        add ("", fix (0.5f), "output", Y, "Output", "Ausgang", "", 1);
    }
    else if (id == "compressor")
    {
        L.regions = { { 0.0f, 0.0f, 0.44f, 1.0f }, { 0.475f, 0.0f, 0.045f, 1.0f }, { 0.545f, 0.0f, 0.03f, 1.0f }, { 0.62f, 0.0f, 0.38f, 1.0f } };
        auto& th = add ("thresh", lin (-60.0f, 0.0f), "", fix (0.5f), "Threshold", "Schwelle", "knee");
        th.special = Special::CompThreshold;
        auto& ra = add ("", fix (1.0f), "ratio", norm(), "Ratio", "Ratio", "knee");
        ra.special = Special::CompRatio;
        ra.aux = P ("thresh");
        add ("", fix (0.5f), "makeup", lin (0.0f, 24.0f), "Makeup", "Aufholen", "", 1);
        add ("attack", lg (0.1f, 200.0f, 0.02f, 0.45f), "", fix (0.9f), "Attack", "Attack", "", 3);
        add ("release", lg (5.0f, 2000.0f, 0.55f, 0.98f), "", fix (0.1f), "Release", "Release", "", 3);
    }
    else if (id == "multiband")
    {
        L.regions = { { 0.0f, 0.0f, 0.92f, 1.0f }, { 0.945f, 0.0f, 0.055f, 1.0f } };
        const auto X = lg (20.0f, 20000.0f), Y = lin (-12.0f, 12.0f, 0.1f, 0.9f);
        const char* bands[3][3] = { { "low", "Low", "Low" }, { "mid", "Mid", "Mid" }, { "high", "High", "High" } };
        for (int k = 0; k < 3; ++k)
        {
            auto& h = add ("", fix (0.5f), bands[k][0], Y, bands[k][1], bands[k][2], "depth");
            h.special = Special::MultibandBand;
            h.band = k;
            h.aux = P (k == 0 ? "lowsplit" : "highsplit");
        }
        add ("lowsplit", X, "", fix (0.04f), "Low split", "Trennung tief");
        add ("highsplit", X, "", fix (0.04f), "High split", "Trennung hoch");
        add ("", fix (0.5f), "output", Y, "Output", "Ausgang", "", 1);
    }
    else if (id == "filter" || id == "filterdrive")
    {
        add ("cutoff", lg (20.0f, 20000.0f), "reso", norm (0.55f, 0.95f), "Cutoff", "Cutoff", "drive");
    }
    else if (id == "distortion")
    {
        L.regions = { { 0.0f, 0.0f, 0.6f, 1.0f }, { 0.635f, 0.0f, 0.045f, 1.0f }, { 0.72f, 0.0f, 0.28f, 1.0f } };
        add ("bias", lin (-1.0f, 1.0f, 0.05f, 0.95f), "drive", norm (0.05f, 0.95f), "Drive / Bias", "Drive / Bias", "tone");
        add ("", fix (0.5f), "output", lin (-24.0f, 12.0f), "Output", "Ausgang", "", 1);
        add ("prefreq", lg (40.0f, 18000.0f, 0.04f, 0.96f), "", fix (0.62f), "Pre-filter", "Vorfilter", "prefilter", 2);
    }
    else if (id == "bitcrush")
    {
        add ("downsample", lg (1.0f, 64.0f, 0.04f, 0.96f), "bits", lin (1.0f, 16.0f, 0.1f, 0.92f), "Bits / Rate", "Bits / Rate");
    }
    else if (id == "chorus" || id == "phaser" || id == "flange" || id == "hyper" || id == "tremolo")
    {
        L.regions = { wave, row1, row2 };
        const char* depth = id == "hyper" ? "detune" : "depth";
        const char* alt = id == "chorus" ? "drift" : id == "phaser" ? "feedback" : id == "flange" ? "feedback" : id == "hyper" ? "spread" : "shape";
        add (rateKey(), rateAxis(), depth, norm (0.1f, 0.95f), "Rate / Depth", "Tempo / Tiefe", alt);
        if (id == "chorus") { add ("spread", norm (0.02f, 0.98f), "", fix (0.5f), "Spread", "Breite", "", 1); add ("lowcut", lg (0.0f, 0.0f, 0.02f, 0.98f), "", fix (0.5f), "Low Cut", "Low Cut", "", 2); }
        if (id == "phaser") add ("feedback", lin (0.0f, 0.0f, 0.02f, 0.98f), "", fix (0.5f), "Feedback", "Feedback", "", 1);
        if (id == "flange") { add ("delay", lg (0.0f, 0.0f, 0.02f, 0.98f), "", fix (0.5f), "Delay", "Verz\xc3\xb6gerung", "", 1); add ("feedback", lin (0.0f, 0.0f, 0.02f, 0.98f), "", fix (0.5f), "Feedback", "Feedback", "", 2); }
        if (id == "hyper") { add ("voices", norm (0.02f, 0.98f), "", fix (0.5f), "Voices", "Stimmen", "", 1); add ("dimension", norm (0.02f, 0.98f), "", fix (0.5f), "Dimension", "Dimension", "", 2); }
        if (id == "tremolo") L.regions = { { 0.0f, 0.0f, 1.0f, 0.86f }, { 0.0f, 0.9f, 1.0f, 0.1f } };
    }
    else if (id == "wobble")
    {
        L.regions = { wave, row1, row2 };
        const auto F = lg (30.0f, 18000.0f, 0.05f, 0.95f);
        add ("", fix (0.03f), "low", F, "Low", "Tief", "reso");
        add ("", fix (0.08f), "high", F, "High", "Hoch", "reso");
        add (rateKey(), rateAxis(), "", fix (0.5f), "Rate", "Tempo", "shape", 1);
        add ("drive", norm (0.02f, 0.98f), "", fix (0.5f), "Drive", "Drive", "", 2);
    }
    else if (id == "formant")
    {
        L.regions = { wave, row1, row2 };
        add ("vowel", norm (0.06f, 0.94f), "shift", lin (-12.0f, 12.0f, 0.1f, 0.9f), "Vowel / Shift", "Vokal / Verschiebung", "reso");
        add (rateKey(), rateAxis(), "", fix (0.5f), "Rate", "Tempo", "", 1);
        add ("depth", norm (0.02f, 0.98f), "", fix (0.5f), "Depth", "Tiefe", "", 2);
    }
    else if (id == "comb")
    {
        L.regions = { wave, row1, row2 };
        add ("freq", lg (20.0f, 5000.0f, 0.02f, 0.98f), "feedback", lin (-0.95f, 0.95f, 0.08f, 0.92f), "Frequency / Feedback", "Frequenz / Feedback", "damp");
        add ("rate", lg (0.0f, 0.0f, 0.02f, 0.98f), "", fix (0.5f), "Rate", "Tempo", "spread", 1);
        add ("depth", norm (0.02f, 0.98f), "", fix (0.5f), "Depth", "Tiefe", "", 2);
    }
    else if (id == "delay" || id == "echo")
    {
        const bool echo = id == "echo";
        L.regions = echo ? std::vector<juce::Rectangle<float>> { wave, row1, row2 } : std::vector<juce::Rectangle<float>> { { 0.0f, 0.0f, 1.0f, 1.0f } };
        const bool synced = echo && plainOf (proc, b + "sync") >= 0.5f;
        const auto T = synced ? norm (0.0f, 1.0f) : (echo ? lin (0.0f, 2000.0f) : lin (0.0f, 2.0f));
        add (synced ? "sync" : "time", T, "feedback", echo ? lin (0.0f, 1.05f, 0.0f, 0.92f) : lin (0.0f, 0.95f, 0.0f, 0.92f),
             "Time / Feedback", "Zeit / Feedback", echo ? "width" : "tone");
        if (echo)
        {
            add ("lowcut", lg (20.0f, 20000.0f, 0.02f, 0.98f), "", fix (0.5f), "Low Cut", "Low Cut", "", 1);
            add ("highcut", lg (20.0f, 20000.0f, 0.02f, 0.98f), "", fix (0.5f), "High Cut", "High Cut", "", 1);
            add ("ratio", lin (0.0f, 0.0f, 0.02f, 0.98f), "", fix (0.5f), "Right ratio", "Verh\xc3\xa4ltnis rechts", "", 2);
        }
    }
    else if (id == "reverb")
    {
        add ("predelay", lin (0.0f, 0.0f, 0.02f, 0.3f), "", fix (0.92f), "Pre-delay", "Pre-Delay");
        add ("size", norm (0.36f, 0.98f), "damping", norm (0.85f, 0.15f), "Size / Damping", "Gr\xc3\xb6\xc3\x9f" "e / D\xc3\xa4mpfung");
    }
    else if (id == "hall")
    {
        L.regions = { wave, row1, row2 };
        add ("predelay", lin (0.0f, 0.0f, 0.02f, 0.3f), "", fix (0.92f), "Pre-delay", "Pre-Delay");
        add ("decay", lg (0.0f, 0.0f, 0.36f, 0.98f), "damping", norm (0.85f, 0.15f), "Decay / Damping", "Nachhall / D\xc3\xa4mpfung", "size");
        add ("size", norm (0.02f, 0.98f), "", fix (0.5f), "Size", "Gr\xc3\xb6\xc3\x9f" "e", "", 1);
        add ("width", norm (0.02f, 0.98f), "", fix (0.5f), "Width", "Breite", "", 2);
    }
    else if (id == "midside")
    {
        L.regions = { { 0.0f, 0.0f, 0.62f, 1.0f }, { 0.68f, 0.0f, 0.13f, 1.0f }, { 0.86f, 0.0f, 0.13f, 1.0f } };
        add ("width", lin (0.0f, 2.0f, 0.06f, 0.94f), "", fix (0.62f), "Width", "Breite", "haas");
        add ("balance", lin (-1.0f, 1.0f, 0.06f, 0.94f), "", fix (0.1f), "Balance", "Balance");
        add ("", fix (0.5f), "mid", lin (-24.0f, 12.0f, 0.04f, 0.96f), "Mid", "Mitte", "midtilt", 1);
        add ("", fix (0.5f), "side", lin (-24.0f, 12.0f, 0.04f, 0.96f), "Side", "Seite", "sidetilt", 2);
    }
    else if (id == "width")
    {
        L.regions = { { 0.0f, 0.0f, 0.86f, 1.0f }, { 0.92f, 0.0f, 0.07f, 1.0f } };
        add ("width", lin (0.0f, 2.0f, 0.06f, 0.94f), "spread", norm (0.2f, 0.9f), "Width / Spread", "Breite / Spreizung", "haas");
        add ("mono", lg (0.0f, 0.0f, 0.06f, 0.94f), "", fix (0.06f), "Mono below", "Mono unter");
        add ("", fix (0.5f), "output", lin (0.0f, 0.0f, 0.04f, 0.96f), "Output", "Ausgang", "", 1);
    }
    // Space 3D and Resonator: their own pictures (no points; the bars edit them)
    return L;
}
} // namespace fxgraph

// ================================================================= the picture
using fxgraph::Axis;
using fxgraph::Handle;
using fxgraph::Special;

FxGraphTile::FxGraphTile (OhpeProcessor& p, int prt, int view, int slot, std::string slotPrefix)
    : Tile (p, prt, {}), fxView (view), slotIndex (slot), prefix (std::move (slotPrefix))
{
    // the effect as the host parameter has it now (the registry follows only with the next audio block)
    const int t = typeOf (p, prefix);
    const auto info = t > InsertSlot::Off && t < InsertSlot::numTypes ? InsertSlot::typeInfo (t) : InsertSlot::typeInfo (InsertSlot::Off);
    type = t > InsertSlot::Off && t < InsertSlot::numTypes && info.id != nullptr ? info.id : "";
    block = type.empty() ? std::string() : prefix + type + ".";
    lay = fxgraph::layoutFor (p, prefix);
    // the tile's parameters: the slot's mix and bypass, then every parameter of its effect (hash, modulation ranges)
    ids = { prefix + "mix", prefix + "bypass" };
    for (const auto& id : modfx::slotEffectParams (p.core(), prefix)) ids.push_back (id);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

float FxGraphTile::u() const noexcept { return juce::jlimit (5.0f, 14.0f, juce::jmin ((float) getHeight() / 26.0f, (float) getWidth() / 46.0f)); }

float FxGraphTile::plain (const std::string& id) const { return plainOf (proc, id); }

InsertSlot* FxGraphTile::slotObject() const
{
    auto& core = proc.core();
    auto& rack = fxView == modfx::FxViewMaster ? core.masterFxRack() : core.fxRack (juce::jlimit (0, Core::numParts - 1, part));
    return slotIndex >= 0 && slotIndex < rack.numSlots() ? &rack.slot (slotIndex) : nullptr;
}

juce::Rectangle<float> FxGraphTile::plotArea() const
{
    const float k = u();
    const auto b = getLocalBounds().toFloat().reduced (k * 0.6f);
    return b.withTrimmedTop (k * 2.2f).reduced (k * 1.2f, k * 0.6f);
}

juce::Rectangle<float> FxGraphTile::regionArea (int r) const
{
    const auto plot = plotArea();
    if (r < 0 || r >= (int) lay.regions.size()) return plot;
    const auto f = lay.regions[(size_t) r];
    return { plot.getX() + plot.getWidth() * f.getX(), plot.getY() + plot.getHeight() * f.getY(), plot.getWidth() * f.getWidth(),
             plot.getHeight() * f.getHeight() };
}

juce::Point<float> FxGraphTile::fractionsOf (const Handle& h) const
{
    switch (h.special)
    {
        case Special::CompThreshold:
        {
            const float f = fxgraph::fractionOf (proc, h.ax, h.x);
            return { f, f }; // on the diagonal: input = output at the threshold
        }
        case Special::CompRatio:
        {
            const float th = plain (h.aux), ratio = juce::jmax (1.0f, plain (h.y));
            const float out = th + (0.0f - th) / ratio; // output at 0 dB input
            return { 1.0f, juce::jlimit (0.0f, 1.0f, (out + 60.0f) / 60.0f) };
        }
        case Special::MultibandBand:
        {
            const float ls = plain (block + "lowsplit"), hs = plain (block + "highsplit");
            const float hz = h.band == 0 ? std::sqrt (20.0f * ls) : h.band == 1 ? std::sqrt (ls * hs) : std::sqrt (hs * 20000.0f);
            return { std::log (juce::jmax (20.0f, hz) / 20.0f) / std::log (1000.0f), fxgraph::fractionOf (proc, h.ay, h.y) };
        }
        case Special::None: break;
    }
    return { fxgraph::fractionOf (proc, h.ax, h.x), fxgraph::fractionOf (proc, h.ay, h.y) };
}

void FxGraphTile::apply (const Handle& h, juce::Point<float> f)
{
    f = { juce::jlimit (0.0f, 1.0f, f.x), juce::jlimit (0.0f, 1.0f, f.y) };
    switch (h.special)
    {
        case Special::CompThreshold:
            set (h.x, fxgraph::normalisedFor (proc, h.ax, h.x, f.x));
            return;
        case Special::CompRatio:
        {
            // the curve's end at 0 dB input: higher = gentler ratio
            const float th = plain (h.aux), out = -60.0f + 60.0f * f.y;
            const float ratio = out > th + 0.05f && th < -0.05f ? juce::jlimit (1.0f, 20.0f, -th / (out - th)) : 20.0f;
            if (const auto* d = descOf (proc.core(), h.y)) set (h.y, d->toNormalised (d->clamp (ratio)));
            return;
        }
        case Special::MultibandBand:
            set (h.y, fxgraph::normalisedFor (proc, h.ay, h.y, f.y));
            return;
        case Special::None: break;
    }
    if (h.ax.kind != Axis::None && ! h.x.empty()) set (h.x, fxgraph::normalisedFor (proc, h.ax, h.x, f.x));
    if (h.ay.kind != Axis::None && ! h.y.empty()) set (h.y, fxgraph::normalisedFor (proc, h.ay, h.y, f.y));
}

juce::Point<float> FxGraphTile::handlePosition (int i) const
{
    if (i < 0 || i >= numHandles()) return {};
    const auto& h = lay.handles[(size_t) i];
    const auto r = regionArea (h.region);
    const auto f = fractionsOf (h);
    return { r.getX() + r.getWidth() * f.x, r.getBottom() - r.getHeight() * f.y };
}

int FxGraphTile::hit (juce::Point<float> p) const
{
    int best = -1;
    float bestD = u() * 1.9f;
    for (int i = 0; i < numHandles(); ++i)
    {
        const float d = handlePosition (i).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

std::vector<std::string> FxGraphTile::handleParams (int i) const
{
    std::vector<std::string> v;
    if (i < 0 || i >= numHandles()) return v;
    const auto& h = lay.handles[(size_t) i];
    for (const auto* id : { &h.x, &h.y, &h.alt, &h.aux })
        if (! id->empty() && std::find (v.begin(), v.end(), *id) == v.end()) v.push_back (*id);
    return v;
}

juce::String FxGraphTile::handleName (int i) const
{
    if (i < 0 || i >= numHandles()) return {};
    return i18n::pick (lay.handles[(size_t) i].nameEn, lay.handles[(size_t) i].nameDe);
}

void FxGraphTile::refreshLayout()
{
    if (dragging >= 0) return;
    auto fresh = fxgraph::layoutFor (proc, prefix);
    bool same = fresh.handles.size() == lay.handles.size();
    for (size_t i = 0; same && i < fresh.handles.size(); ++i)
        same = fresh.handles[i].x == lay.handles[i].x && fresh.handles[i].y == lay.handles[i].y;
    if (same) return;
    lay = std::move (fresh);
    if (selected >= numHandles()) selected = -1;
    repaint();
}

void FxGraphTile::selectHandle (int i, bool notify)
{
    selected = i >= 0 && i < numHandles() ? i : -1;
    repaint();
    if (notify && onHandleSelected) onHandleSelected (selected);
}

void FxGraphTile::dragHandleTo (int i, juce::Point<float> p)
{
    if (i < 0 || i >= numHandles()) return;
    const auto& h = lay.handles[(size_t) i];
    const auto r = regionArea (h.region);
    apply (h, { (p.x - r.getX()) / juce::jmax (1.0f, r.getWidth()), (r.getBottom() - p.y) / juce::jmax (1.0f, r.getHeight()) });
    endAll();
    repaint();
}

std::string FxGraphTile::paramAt (juce::Point<float> p) const
{
    const int i = hit (p);
    if (i < 0) return ids.empty() ? std::string() : ids.front();
    const auto& h = lay.handles[(size_t) i];
    return ! h.x.empty() ? h.x : (! h.y.empty() ? h.y : h.alt);
}

bool FxGraphTile::animating() const
{
    return type == "compressor" || type == "multiband" || type == "chorus" || type == "phaser" || type == "flange" || type == "hyper"
           || type == "tremolo" || type == "wobble" || type == "space3d" || type == "resonator";
}

uint64_t FxGraphTile::keyOf (const float* v, size_t n, juce::Rectangle<float> r, float extra) noexcept
{
    uint64_t h = 1469598103934665603ull;
    auto fold = [&h] (float f) {
        uint32_t bits = 0;
        std::memcpy (&bits, &f, sizeof bits);
        h = (h ^ bits) * 1099511628211ull;
    };
    for (size_t i = 0; i < n; ++i) fold (v[i]);
    for (float f : { r.getX(), r.getY(), r.getWidth(), r.getHeight(), extra }) fold (f);
    return h;
}

const juce::Path& FxGraphTile::cachedCurve (int slot, uint64_t key, const std::function<juce::Path()>& build)
{
    auto& c = curves[(size_t) juce::jlimit (0, (int) curves.size() - 1, slot)];
    if (! c.valid || c.key != key)
    {
        c.path = build();
        c.key = key;
        c.valid = true;
        ++builds;
    }
    return c.path;
}

uint64_t FxGraphTile::stateHash() const
{
    auto h = Tile::stateHash();
    h = mix (h, (float) selected);
    h = mix (h, (float) hover);
    return h;
}

// ---------------------------------------------------------------- mouse
void FxGraphTile::mouseMove (const juce::MouseEvent& e)
{
    Tile::mouseMove (e);
    const int h = hit (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void FxGraphTile::mouseExit (const juce::MouseEvent&)
{
    if (hover >= 0) { hover = -1; repaint(); }
}

void FxGraphTile::mouseDown (const juce::MouseEvent& e)
{
    lastMouse = e.position;
    const int i = hit (e.position);
    if (e.mods.isPopupMenu())
    {
        if (i >= 0)
        {
            const auto& h = lay.handles[(size_t) i];
            const auto& id = e.mods.isAltDown() && ! h.alt.empty() ? h.alt : paramAt (e.position);
            if (! id.empty()) showControlMenu (proc, juce::String (id), this);
        }
        return;
    }
    dragging = i;
    if (i < 0) return;
    if (i != selected) selectHandle (i);
    const auto& h = lay.handles[(size_t) i];
    dragStart = e.position;
    fracStart = fractionsOf (h);
    if (auto* q = h.alt.empty() ? nullptr : proc.parameter (juce::String (h.alt))) altStart = q->getValue();
}

void FxGraphTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0 || dragging >= numHandles()) return;
    const auto& h = lay.handles[(size_t) dragging];
    const auto r = regionArea (h.region);
    const float k = isFineDrag (e.mods) ? 0.15f : 1.0f;
    const auto d = e.position - dragStart;
    if (e.mods.isAltDown() && ! h.alt.empty()) // Alt-drag: the point's second dimension (Q, knee, shape ...)
    {
        if (const auto* desc = descOf (proc.core(), h.alt))
        {
            float n = altStart - d.y / juce::jmax (40.0f, r.getHeight()) * k;
            n = juce::jlimit (0.0f, 1.0f, n);
            set (h.alt, desc->toNormalised (desc->clamp (desc->fromNormalised (n))));
        }
    }
    else
        apply (h, { fracStart.x + d.x / juce::jmax (20.0f, r.getWidth()) * k, fracStart.y - d.y / juce::jmax (20.0f, r.getHeight()) * k });
    repaint();
}

void FxGraphTile::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
    endAll();
}

void FxGraphTile::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = hit (e.position);
    if (i < 0 || e.mods.isPopupMenu()) return;
    const auto& h = lay.handles[(size_t) i];
    if (e.mods.isAltDown() && ! h.alt.empty()) { reset (h.alt); return; }
    if (! h.x.empty() && h.ax.kind != Axis::None) reset (h.x);
    if (! h.y.empty() && (h.ay.kind != Axis::None || h.special != Special::None)) reset (h.y);
    repaint();
}

void FxGraphTile::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int i = hit (e.position);
    if (i < 0) i = selected;
    if (i < 0 || i >= numHandles() || w.deltaY == 0.0f) return;
    const auto& h = lay.handles[(size_t) i];
    const auto& id = ! h.alt.empty() ? h.alt : (! h.y.empty() ? h.y : h.x);
    if (id.empty()) return;
    const auto* d = descOf (proc.core(), id);
    if (d != nullptr && d->kind == ParamKind::Choice) stepChoice (id, w.deltaY > 0.0f ? 1 : -1);
    else nudge (id, w.deltaY * (isFineDrag (e.mods) ? 0.02f : 0.1f));
    repaint();
}

// ---------------------------------------------------------------- painting
juce::String FxGraphTile::handleValues (int i) const
{
    juce::String s;
    for (const auto& id : handleParams (i))
    {
        if (id == lay.handles[(size_t) i].aux) continue;
        if (const auto* d = descOf (proc.core(), id))
        {
            if (s.isNotEmpty()) s << "   ";
            s << modfx::compactName (id, d->nameEn, d->nameDe) << " " << formatValue (*d, plain (id));
        }
    }
    return s;
}

namespace
{
void grid (juce::Graphics& g, juce::Rectangle<float> r, int nx, int ny, float alpha = 1.0f)
{
    g.setColour (colour::displayGrid.brighter (0.18f).withMultipliedAlpha (alpha));
    for (int i = 1; i < nx; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / (float) nx), r.getY(), r.getBottom());
    for (int i = 1; i < ny; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / (float) ny), r.getX(), r.getRight());
}

void frame (juce::Graphics& g, juce::Rectangle<float> r, float k)
{
    g.setColour (colour::displayGrid.brighter (0.32f));
    g.drawRoundedRectangle (r, k * 0.4f, 1.0f);
}

/** log-frequency grid (100 Hz, 1 kHz, 10 kHz) with labels at the bottom. */
void freqGrid (juce::Graphics& g, juce::Rectangle<float> r, float lo, float hi, float k)
{
    g.setFont (monoFont (k * 1.05f));
    for (float hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        if (hz <= lo || hz >= hi) continue;
        const float x = r.getX() + r.getWidth() * std::log (hz / lo) / std::log (hi / lo);
        const bool major = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;
        g.setColour (colour::displayGrid.brighter (major ? 0.3f : 0.15f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        if (major || hz == 50.0f || hz == 500.0f || hz == 5000.0f)
        {
            g.setColour (colour::displayDim);
            const juce::String t = hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz);
            g.drawText (t, juce::Rectangle<float> (x + 2.0f, r.getBottom() - k * 1.4f, k * 4.0f, k * 1.3f), juce::Justification::centredLeft, false);
        }
    }
}

void strokeCurve (juce::Graphics& g, const juce::Path& p, juce::Colour c, float k, juce::Rectangle<float> fillTo = {}, float baseY = 0.0f)
{
    if (! fillTo.isEmpty())
    {
        juce::Path f (p);
        f.lineTo (fillTo.getRight(), baseY);
        f.lineTo (fillTo.getX(), baseY);
        f.closeSubPath();
        g.setColour (c.withAlpha (0.14f));
        g.fillPath (f);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (k * 0.24f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

template <typename Fn>
juce::Path plotCurve (juce::Rectangle<float> r, int n, Fn&& fn) // fn: 0..1 across -> 0..1 up
{
    juce::Path p;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / (float) n;
        const float y = r.getBottom() - r.getHeight() * juce::jlimit (-0.02f, 1.02f, fn (t));
        if (i == 0) p.startNewSubPath (r.getX(), y);
        else p.lineTo (r.getX() + r.getWidth() * t, y);
    }
    return p;
}

/** A vertical value track (output, makeup, mid / side levels). */
void track (juce::Graphics& g, juce::Rectangle<float> r, float k, const juce::String& label)
{
    const auto line = juce::Rectangle<float> (k * 0.3f, r.getHeight()).withCentre (r.getCentre());
    g.setColour (colour::displayGrid.brighter (0.3f));
    g.fillRoundedRectangle (line, k * 0.15f);
    g.setColour (colour::displayDim);
    g.setFont (monoFont (k * 0.95f));
    g.drawText (label, r.withHeight (k * 1.2f).translated (0.0f, -k * 1.4f).expanded (k * 2.0f, 0.0f), juce::Justification::centred, false);
}

/** A horizontal slider row under a picture. */
void row (juce::Graphics& g, juce::Rectangle<float> r, float k)
{
    const auto line = juce::Rectangle<float> (r.getWidth(), k * 0.26f).withCentre (r.getCentre());
    g.setColour (colour::displayGrid.brighter (0.3f));
    g.fillRoundedRectangle (line, k * 0.13f);
}

float lfoWave (int shape, float ph) // -1..1; shapes: 0 sine 1 triangle 2 saw down 3 saw up 4 square 5 s&h
{
    ph -= std::floor (ph);
    switch (shape)
    {
        case 1: return 1.0f - 4.0f * std::abs (ph - 0.5f);
        case 2: return 1.0f - 2.0f * ph;
        case 3: return 2.0f * ph - 1.0f;
        case 4: return ph < 0.5f ? 1.0f : -1.0f;
        case 5: return std::sin (std::floor (ph * 4.0f) * 12.9898f) * 0.9f;
        default: return std::sin (ph * twoPi);
    }
}
} // namespace

void FxGraphTile::paintBackground (juce::Graphics& g, juce::Rectangle<float> plot)
{
    const float k = u();
    auto v = [&] (const char* key) { return plain (block + key); };
    auto R = [&] (int r) { return regionArea (r); };
    const auto& t = type;
    const bool bypassed = plain (prefix + "bypass") >= 0.5f;
    const auto ink = bypassed ? colour::displayDim : edit();

    if (t.empty())
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (k * 1.5f));
        g.drawFittedText (i18n::pick ("Empty slot - choose a type on the right (Type).", "Leerer Slot - rechts unter Typ einen Effekt w\xc3\xa4hlen."),
                          plot.toNearestInt(), juce::Justification::centred, 2);
        return;
    }
    if (t == "space3d" || t == "resonator")
    {
        auto* s = slotObject();
        const int ty = typeOf (proc, prefix);
        if (s != nullptr && t == "space3d")
            if (auto* e = static_cast<SpaceEffect*> (s->effect (ty)))
                space3d::Scene::paintSingle (g, plot, e->uiPosition(), juce::jmin (1.0f, e->uiLevel() * 2.0f), modc(), space3d::speakerMonitoring (proc.core()), time);
        if (s != nullptr && t == "resonator")
            if (auto* e = static_cast<ResonatorEffect*> (s->effect (ty)))
                resoview::paintBody (g, plot, resoview::stateOf (*e, proc.core(), prefix + "resonator.", true), time, modc());
        return;
    }
    if (t == "eq")
    {
        const auto r = R (0);
        frame (g, r, k);
        freqGrid (g, r, 20.0f, 20000.0f, k);
        g.setColour (colour::displayGrid.brighter (0.35f));
        const float zero = r.getBottom() - r.getHeight() * 0.5f;
        g.drawHorizontalLine ((int) zero, r.getX(), r.getRight());
        g.setFont (monoFont (k * 1.0f));
        g.setColour (colour::displayDim);
        for (int db : { -12, 12 })
        {
            const float y = r.getBottom() - r.getHeight() * (0.05f + 0.9f * ((float) db + 18.0f) / 36.0f);
            g.setColour (colour::displayGrid.brighter (0.15f));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            g.setColour (colour::displayDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (r.getX() + 2.0f, y - k * 1.2f, k * 3.0f, k * 1.1f), juce::Justification::centredLeft, false);
        }
        std::array<float, EqEffect::numParams> vals {};
        const char* keys[EqEffect::numParams] = { "lowfreq", "lowgain", "p1freq", "p1gain", "p1q", "p2freq", "p2gain", "p2q",
                                                  "p3freq", "p3gain", "p3q", "highfreq", "highgain", "output" };
        for (int i = 0; i < EqEffect::numParams; ++i) vals[(size_t) i] = v (keys[i]);
        // the selected band alone (faint), then the whole curve
        if (selected >= 0 && selected < 5)
        {
            auto solo = vals;
            for (int b = 0; b < 5; ++b)
                if (b != selected) solo[(size_t) (b == 0 ? 1 : b == 4 ? 12 : 3 * b)] = 0.0f;
            solo[13] = 0.0f;
            const auto& p = cachedCurve (1, keyOf (solo.data(), solo.size(), r, (float) selected), [&] {
                return plotCurve (r, 140, [&] (float x) { return 0.05f + 0.9f * (EqEffect::responseDb (solo.data(), 20.0f * std::pow (1000.0f, x), 48000.0f) + 18.0f) / 36.0f; });
            });
            g.setColour (ink.withAlpha (0.35f));
            g.strokePath (p, juce::PathStrokeType (k * 0.14f));
        }
        // p57: the response (180 points x 5 biquad designs) is rebuilt only when a value of this slot or the size changed
        const auto& p = cachedCurve (0, keyOf (vals.data(), vals.size(), r, 0.0f), [&] {
            return plotCurve (r, 180, [&] (float x) { return 0.05f + 0.9f * (EqEffect::responseDb (vals.data(), 20.0f * std::pow (1000.0f, x), 48000.0f) + 18.0f) / 36.0f; });
        });
        strokeCurve (g, p, ink, k, r, zero);
        track (g, R (1), k, "OUT");
        return;
    }
    if (t == "compressor")
    {
        const auto r = R (0);
        frame (g, r, k);
        grid (g, r, 6, 6);
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.drawLine ({ r.getBottomLeft(), r.getTopRight() }, 1.0f); // 1:1
        const float th = v ("thresh"), ratio = v ("ratio"), knee = v ("knee"), makeup = v ("makeup");
        const float cv[] = { th, ratio, knee, makeup };
        const auto& p = cachedCurve (0, keyOf (cv, 4, r, 0.0f), [&] {
            return plotCurve (r, 120, [&] (float x) {
                const float in = -60.0f + 60.0f * x;
                return (in + CompressorEffect::staticGainDb (in, th, ratio, knee) + 60.0f) / 60.0f;
            });
        });
        strokeCurve (g, p, ink, k);
        if (makeup > 0.05f) // the curve after makeup (faint)
        {
            const auto& pm = cachedCurve (1, keyOf (cv, 4, r, 1.0f), [&] {
                return plotCurve (r, 120, [&] (float x) {
                    const float in = -60.0f + 60.0f * x;
                    return (in + CompressorEffect::staticGainDb (in, th, ratio, knee) + makeup + 60.0f) / 60.0f;
                });
            });
            g.setColour (ink.withAlpha (0.3f));
            g.strokePath (pm, juce::PathStrokeType (k * 0.12f));
        }
        if (knee > 0.1f) // the knee as a light band around the threshold
        {
            const float x0 = r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, (th - knee * 0.5f + 60.0f) / 60.0f);
            const float x1 = r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, (th + knee * 0.5f + 60.0f) / 60.0f);
            g.setColour (ink.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, r.getY(), x1, r.getBottom()));
        }
        g.setFont (monoFont (k * 1.0f));
        g.setColour (colour::displayDim);
        g.drawText (i18n::pick ("IN", "EIN"), r.withTop (r.getBottom() - k * 1.3f).withTrimmedRight (k * 0.4f), juce::Justification::centredRight, false);
        track (g, R (1), k, "MAKE");
        // gain reduction now
        auto gr = R (2);
        g.setColour (colour::displayGrid.brighter (0.2f));
        g.fillRoundedRectangle (gr, k * 0.2f);
        if (auto* s = slotObject())
            if (auto* e = dynamic_cast<CompressorEffect*> (s->effect (typeOf (proc, prefix))))
            {
                const float dB = juce::jlimit (0.0f, 24.0f, e->gainReductionDb());
                g.setColour (colour::signalRed);
                g.fillRoundedRectangle (gr.withHeight (gr.getHeight() * dB / 24.0f), k * 0.2f);
            }
        g.setColour (colour::displayDim);
        g.drawText ("GR", gr.withHeight (k * 1.2f).translated (0.0f, -k * 1.4f).expanded (k * 1.5f, 0.0f), juce::Justification::centred, false);
        // attack / release as an envelope
        const auto er = R (3);
        frame (g, er, k);
        const auto a = handlePosition (3), rl = handlePosition (4);
        juce::Path env;
        env.startNewSubPath (er.getX(), er.getBottom() - er.getHeight() * 0.1f);
        env.quadraticTo ({ er.getX() + (a.x - er.getX()) * 0.2f, a.y }, a);
        env.lineTo (er.getX() + er.getWidth() * 0.5f, a.y);
        env.quadraticTo ({ er.getX() + er.getWidth() * 0.52f, rl.y }, rl);
        env.lineTo (er.getRight(), rl.y);
        strokeCurve (g, env, ink.withAlpha (0.85f), k);
        return;
    }
    if (t == "multiband")
    {
        const auto r = R (0);
        frame (g, r, k);
        freqGrid (g, r, 20.0f, 20000.0f, k);
        const float zero = r.getBottom() - r.getHeight() * 0.5f;
        g.setColour (colour::displayGrid.brighter (0.35f));
        g.drawHorizontalLine ((int) zero, r.getX(), r.getRight());
        const float ls = v ("lowsplit"), hs = v ("highsplit");
        auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * std::log (juce::jmax (20.0f, hz) / 20.0f) / std::log (1000.0f); };
        const float edges[4] = { r.getX(), xOf (ls), xOf (hs), r.getRight() };
        auto* s = slotObject();
        auto* e = s != nullptr ? dynamic_cast<MultibandEffect*> (s->effect (typeOf (proc, prefix))) : nullptr;
        const char* keys[3] = { "low", "mid", "high" };
        for (int b = 0; b < 3; ++b)
        {
            const auto col = juce::Rectangle<float>::leftTopRightBottom (edges[b], r.getY(), edges[b + 1], r.getBottom()).reduced (k * 0.5f, 0.0f);
            const float gain = v (keys[b]);
            const float y = r.getBottom() - r.getHeight() * (0.1f + 0.8f * (gain + 12.0f) / 24.0f);
            g.setColour (ink.withAlpha (0.2f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (col.getX(), juce::jmin (y, zero), col.getRight(), juce::jmax (y, zero)));
            g.setColour (ink);
            g.drawHorizontalLine ((int) y, col.getX(), col.getRight());
            if (e != nullptr) // live gain of the band (violet)
            {
                const float live = juce::jlimit (-24.0f, 24.0f, e->bandGainDb (b));
                const float ly = zero - live / 24.0f * r.getHeight() * 0.45f;
                const auto bar = juce::Rectangle<float> (k * 0.8f, std::abs (ly - zero)).withPosition (col.getRight() - k * 1.4f, juce::jmin (ly, zero));
                g.setColour (modc().withAlpha (0.85f));
                g.fillRect (bar);
            }
        }
        g.setColour (colour::displayText.withAlpha (0.5f));
        for (int i = 1; i <= 2; ++i)
        {
            const float dash[2] = { k * 0.5f, k * 0.4f };
            g.drawDashedLine ({ edges[i], r.getY(), edges[i], r.getBottom() }, dash, 2, 1.0f);
        }
        track (g, R (1), k, "OUT");
        return;
    }
    if (t == "filter" || t == "filterdrive")
    {
        const auto r = R (0);
        frame (g, r, k);
        freqGrid (g, r, 20.0f, 20000.0f, k);
        const float cut = std::log (juce::jmax (20.0f, v ("cutoff")) / 20.0f) / std::log (1000.0f);
        const float reso = v ("reso"), drive = v ("drive");
        int kind = 0; // 0 low-pass, 1 band-pass, 2 high-pass, 3 notch, 4 comb, 5 formant
        bool steep = false;
        if (t == "filter")
        {
            const int ft = (int) std::lround (v ("type"));
            steep = ft == FilterRackEffect::LP24 || ft == FilterRackEffect::HP24;
            kind = ft == FilterRackEffect::HP12 || ft == FilterRackEffect::HP24 ? 2 : ft == FilterRackEffect::BP ? 1 : ft == FilterRackEffect::Notch ? 3
                 : ft == FilterRackEffect::CombPlus || ft == FilterRackEffect::CombMinus ? 4 : ft == FilterRackEffect::Formant ? 5 : 0;
        }
        else
            kind = juce::jlimit (0, 2, (int) std::lround (v ("mode")));
        const float peak = 0.4f * reso;
        const auto p = plotCurve (r, 160, [&] (float x) {
            const float d = (x - cut) * 10.0f; // octaves from the cutoff
            const float bump = peak * std::exp (-d * d * 3.0f);
            const float slope = steep ? 0.24f : 0.12f;
            float y = 0.55f;
            switch (kind)
            {
                case 1: y = 0.55f - std::abs (d) * 0.12f; break;
                case 2: y = d >= 0.0f ? 0.55f : 0.55f + d * slope; break;
                case 3: y = 0.55f - 0.5f * std::exp (-d * d * 5.0f); break;
                case 4: y = 0.42f + 0.18f * std::cos ((x - cut) * 60.0f); break;
                case 5: y = 0.3f + 0.25f * std::exp (-d * d * 2.0f) + 0.2f * std::exp (-(d - 2.2f) * (d - 2.2f) * 3.0f); break;
                default: y = d <= 0.0f ? 0.55f : 0.55f - d * slope; break;
            }
            return (y + bump) * (1.0f + 0.12f * drive);
        });
        strokeCurve (g, p, ink, k, r, r.getBottom());
        return;
    }
    if (t == "distortion" || t == "bitcrush")
    {
        const auto r = R (0);
        frame (g, r, k);
        grid (g, r, 4, 4);
        g.setColour (colour::displayGrid.brighter (0.35f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());
        juce::Path p;
        if (t == "distortion")
        {
            const int mode = (int) std::lround (v ("mode"));
            const float drive = v ("drive"), bias = v ("bias");
            const float dv[] = { (float) mode, drive, bias };
            p = cachedCurve (0, keyOf (dv, 3, r, 0.0f), [&] {
                return plotCurve (r, 160, [&] (float x) { return 0.5f + 0.45f * DistortionEffect::transfer (mode, drive, bias, x * 2.0f - 1.0f); });
            });
            // the pre-filter
            const auto pr = R (2);
            frame (g, pr, k);
            const int pf = (int) std::lround (v ("prefilter"));
            const float fx = std::log (juce::jmax (40.0f, v ("prefreq")) / 40.0f) / std::log (18000.0f / 40.0f);
            const auto pp = plotCurve (pr, 60, [&] (float x) {
                const float d = (x - fx) * 8.0f;
                if (pf == 1) return d <= 0.0f ? 0.62f : 0.62f - d * 0.12f;
                if (pf == 2) return 0.62f - std::abs (d) * 0.12f;
                if (pf == 3) return d >= 0.0f ? 0.62f : 0.62f + d * 0.12f;
                return 0.62f;
            });
            strokeCurve (g, pp, pf == 0 ? colour::displayDim : ink.withAlpha (0.8f), k);
            g.setFont (monoFont (k * 0.95f));
            g.setColour (colour::displayDim);
            g.drawText (upperPick ("Pre-filter", "Vorfilter"), pr.withHeight (k * 1.3f).translated (0.0f, k * 0.2f), juce::Justification::centred, false);
            track (g, R (1), k, "OUT");
            strokeCurve (g, p, bypassed ? ink : colour::signalRed, k);
        }
        else
        {
            // a sine through the crusher: fewer bits = coarser steps, more downsampling = longer holds
            const float bits = v ("bits"), ds = juce::jmax (1.0f, v ("downsample"));
            const float levels = std::pow (2.0f, juce::jlimit (1.0f, 16.0f, bits)) * 0.5f;
            const int n = 256;
            const float hold = ds / 64.0f * 0.12f;
            p = plotCurve (r, n, [&] (float x) {
                const float xs = hold > 0.002f ? std::floor (x / hold) * hold : x;
                const float s = std::sin (xs * twoPi * 1.5f) * 0.9f;
                return 0.5f + 0.45f * std::round (s * levels) / levels;
            });
            g.setColour (colour::displayDim.withAlpha (0.6f));
            g.strokePath (plotCurve (r, 120, [&] (float x) { return 0.5f + 0.405f * std::sin (x * twoPi * 1.5f); }), juce::PathStrokeType (k * 0.1f));
            strokeCurve (g, p, bypassed ? ink : colour::signalRed, k);
        }
        return;
    }
    if (t == "chorus" || t == "phaser" || t == "flange" || t == "hyper" || t == "tremolo" || t == "wobble" || t == "formant" || t == "comb")
    {
        const auto r = R (0);
        frame (g, r, k);
        for (int i = 1; i < (int) lay.regions.size(); ++i) row (g, R (i), k);
        const bool synced = block.size() > 0 && plain (block + "sync") >= 0.5f && descOf (proc.core(), block + "sync") != nullptr;
        const float rate = juce::jmax (0.01f, v ("rate"));
        if (t == "wobble")
        {
            // the filter sweep between low and high over time
            const float lo = v ("low"), hi = v ("high");
            const int shape = (int) std::lround (v ("shape"));
            auto yOf = [&] (float hz) { return 0.05f + 0.9f * std::log (juce::jlimit (30.0f, 18000.0f, hz) / 30.0f) / std::log (600.0f); };
            const float a = yOf (lo), b = yOf (hi);
            g.setColour (ink.withAlpha (0.1f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX(), r.getBottom() - r.getHeight() * juce::jmax (a, b), r.getRight(), r.getBottom() - r.getHeight() * juce::jmin (a, b)));
            const float cycles = synced ? 2.0f + 6.0f * fxgraph::fractionOf (proc, fxgraph::norm(), block + "sync") : 1.0f + 7.0f * std::log (rate / 0.05f) / std::log (400.0f);
            const auto p = plotCurve (r.withTrimmedLeft (r.getWidth() * 0.12f), 200, [&] (float x) { return a + (b - a) * (0.5f + 0.5f * lfoWave (shape, x * cycles - time * rate * 0.25f)); });
            strokeCurve (g, p, ink, k);
            return;
        }
        if (t == "formant")
        {
            // the vowel as two formant peaks over a log axis (100 Hz .. 5 kHz), shifted
            const float vow = v ("vowel") * 4.0f, shift = v ("shift"), reso = v ("reso");
            const float f1s[5] = { 800.0f, 400.0f, 270.0f, 450.0f, 325.0f }, f2s[5] = { 1150.0f, 1900.0f, 2300.0f, 800.0f, 700.0f };
            const int i0 = juce::jlimit (0, 3, (int) vow);
            const float fr = vow - (float) i0;
            const float sc = std::pow (2.0f, shift / 12.0f);
            const float f1 = (f1s[i0] + (f1s[i0 + 1] - f1s[i0]) * fr) * sc, f2 = (f2s[i0] + (f2s[i0 + 1] - f2s[i0]) * fr) * sc;
            auto xOf = [] (float hz) { return std::log (juce::jlimit (100.0f, 5000.0f, hz) / 100.0f) / std::log (50.0f); };
            const float w = 18.0f + 40.0f * reso;
            const auto p = plotCurve (r, 160, [&] (float x) {
                const float d1 = x - xOf (f1), d2 = x - xOf (f2);
                return 0.12f + 0.6f * std::exp (-d1 * d1 * w * w * 0.05f) + 0.5f * std::exp (-d2 * d2 * w * w * 0.05f);
            });
            strokeCurve (g, p, ink.withAlpha (0.55f), k, r, r.getBottom());
            g.setFont (labelFont (k * 1.2f));
            g.setColour (colour::displayDim);
            const char* vowels[5] = { "A", "E", "I", "O", "U" };
            for (int i = 0; i < 5; ++i)
            {
                const float x = r.getX() + r.getWidth() * (0.06f + 0.88f * (float) i / 4.0f);
                g.drawText (vowels[i], juce::Rectangle<float> (k * 2.0f, k * 1.4f).withCentre ({ x, r.getBottom() - k * 0.9f }), juce::Justification::centred, false);
            }
            return;
        }
        if (t == "comb")
        {
            const float hz = juce::jmax (20.0f, v ("freq")), fb = v ("feedback"), damp = v ("damp");
            const auto p = plotCurve (r, 400, [&] (float x) {
                const float f = 20.0f * std::pow (250.0f, x); // 20 Hz .. 5 kHz
                const float c = std::cos (twoPi * f / hz + (fb < 0.0f ? juce::MathConstants<float>::pi : 0.0f));
                const float mag = 1.0f / juce::jmax (0.08f, 1.0f + fb * fb - 2.0f * std::abs (fb) * c);
                return 0.1f + 0.75f * juce::jlimit (0.0f, 1.0f, std::sqrt (mag) / 6.0f * 2.0f) * (1.0f - damp * x * 0.6f);
            });
            strokeCurve (g, p, ink, k, r, r.getBottom());
            return;
        }
        // rate / depth as a wave: more cycles = faster, taller = deeper
        const auto& h = lay.handles.front();
        const float fx = fxgraph::fractionOf (proc, h.ax, h.x);
        const float depth = fxgraph::fractionOf (proc, h.ay, h.y);
        const float cycles = 0.8f + 7.0f * fx;
        const int shape = t == "tremolo" ? std::array<int, 3> { 0, 1, 4 }[(size_t) juce::jlimit (0, 2, (int) std::lround (v ("shape")))] : 0;
        const int lines = t == "hyper" ? juce::jlimit (1, 7, (int) std::lround (v ("voices"))) : t == "chorus" ? juce::jlimit (1, 4, (int) std::lround (v ("voices"))) : 1;
        const float spread = t == "hyper" ? v ("spread") : t == "chorus" ? v ("spread") : 0.0f;
        const float speed = synced ? 1.0f : rate;
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.drawHorizontalLine ((int) (r.getCentreY()), r.getX(), r.getRight());
        for (int l = 0; l < lines; ++l)
        {
            const float off = lines > 1 ? ((float) l / (float) (lines - 1) - 0.5f) : 0.0f;
            const auto p = plotCurve (r, 200, [&] (float x) {
                return 0.5f + 0.5f * depth * lfoWave (shape, x * cycles + off * spread * 0.5f - time * speed * 0.2f) * (1.0f - std::abs (off) * 0.25f);
            });
            if (lines > 1)
            {
                g.setColour (ink.withAlpha (0.35f + 0.5f * (1.0f - std::abs (off) * 2.0f)));
                g.strokePath (p, juce::PathStrokeType (k * 0.16f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
                strokeCurve (g, p, ink, k);
        }
        if (t == "tremolo")
        {
            g.setFont (monoFont (k * 1.0f));
            g.setColour (colour::displayDim);
            const auto* d = descOf (proc.core(), block + "mode");
            if (d != nullptr) g.drawText (i18n::upper (formatValue (*d, v ("mode"))), r.reduced (k * 0.5f), juce::Justification::topRight, false);
        }
        return;
    }
    if (t == "delay" || t == "echo")
    {
        const auto r = R (0);
        frame (g, r, k);
        grid (g, r, 8, 4, 0.6f);
        for (int i = 1; i < (int) lay.regions.size(); ++i) row (g, R (i), k);
        const auto& h = lay.handles.front();
        const float tx = juce::jmax (0.01f, fxgraph::fractionOf (proc, h.ax, h.x));
        const float top = fxgraph::fractionOf (proc, h.ay, h.y);
        const float fb = juce::jlimit (0.0f, 0.99f, t == "echo" ? v ("feedback") / 1.05f : v ("feedback") / 0.95f);
        // the dry hit, then the taps
        g.setColour (colour::displayText.withAlpha (0.5f));
        g.fillRoundedRectangle (juce::Rectangle<float> (k * 0.5f, r.getHeight() * 0.92f).withPosition (r.getX(), r.getBottom() - r.getHeight() * 0.92f), k * 0.2f);
        const float ratio = t == "echo" ? v ("ratio") : 1.0f;
        const bool second = t == "echo" && (int) std::lround (v ("mode")) != EchoEffect::Stereo;
        float hgt = top;
        for (int i = 1; i <= 24 && hgt > 0.01f; ++i)
        {
            const float x = r.getX() + r.getWidth() * tx * (float) i;
            if (x > r.getRight()) break;
            g.setColour (ink.withAlpha (0.4f + 0.6f * hgt));
            g.fillRoundedRectangle (juce::Rectangle<float> (k * 0.55f, r.getHeight() * hgt).withPosition (x - k * 0.27f, r.getBottom() - r.getHeight() * hgt), k * 0.25f);
            if (second && ratio < 0.999f)
            {
                const float x2 = r.getX() + r.getWidth() * tx * ((float) i - 1.0f + ratio);
                g.setColour (colour::rose.withAlpha (0.3f + 0.5f * hgt));
                g.fillRoundedRectangle (juce::Rectangle<float> (k * 0.4f, r.getHeight() * hgt * 0.8f).withPosition (x2 - k * 0.2f, r.getBottom() - r.getHeight() * hgt * 0.8f), k * 0.2f);
            }
            hgt *= fb;
        }
        if (t == "echo") // the band the echoes keep (between low cut and high cut)
        {
            const auto rr = R (1);
            const float a = fxgraph::fractionOf (proc, lay.handles[1].ax, lay.handles[1].x), b = fxgraph::fractionOf (proc, lay.handles[2].ax, lay.handles[2].x);
            g.setColour (ink.withAlpha (0.5f));
            g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (rr.getX() + rr.getWidth() * a, rr.getCentreY() - k * 0.2f, rr.getX() + rr.getWidth() * b, rr.getCentreY() + k * 0.2f), k * 0.2f);
        }
        return;
    }
    if (t == "reverb" || t == "hall")
    {
        const auto r = R (0);
        frame (g, r, k);
        for (int i = 1; i < (int) lay.regions.size(); ++i) row (g, R (i), k);
        const auto pd = handlePosition (0), tail = handlePosition (1);
        const float bright = juce::jlimit (0.15f, 1.0f, (r.getBottom() - tail.y) / juce::jmax (1.0f, r.getHeight()));
        g.setColour (colour::displayText.withAlpha (0.5f));
        g.fillRoundedRectangle (juce::Rectangle<float> (k * 0.5f, r.getHeight() * 0.92f).withPosition (r.getX(), r.getBottom() - r.getHeight() * 0.92f), k * 0.2f);
        g.setColour (colour::displayGrid.brighter (0.4f));
        const float dash[2] = { k * 0.4f, k * 0.4f };
        g.drawDashedLine ({ r.getX(), pd.y, pd.x, pd.y }, dash, 2, 1.0f);
        // the diffuse tail: a decaying cloud from the pre-delay to the tail point
        juce::Path env;
        const float x0 = pd.x, x1 = juce::jmax (pd.x + k, tail.x);
        env.startNewSubPath (x0, r.getBottom());
        for (int i = 0; i <= 80; ++i)
        {
            const float f = (float) i / 80.0f;
            const float y = r.getBottom() - r.getHeight() * 0.88f * std::pow (1.0f - f, 1.6f) * (0.82f + 0.18f * std::sin ((float) i * 2.1f) * std::sin ((float) i * 0.7f));
            env.lineTo (x0 + (x1 - x0) * f, y);
        }
        env.lineTo (x1, r.getBottom());
        env.closeSubPath();
        g.setGradientFill (juce::ColourGradient (ink.withAlpha (0.25f + 0.45f * bright), x0, r.getY(), ink.withAlpha (0.05f), x1, r.getBottom(), false));
        g.fillPath (env);
        g.setColour (ink.withAlpha (0.8f));
        g.strokePath (env, juce::PathStrokeType (k * 0.14f));
        return;
    }
    if (t == "midside" || t == "width")
    {
        const auto r = R (0);
        frame (g, r, k);
        for (int i = 1; i < (int) lay.regions.size(); ++i)
            track (g, R (i), k, t == "width" ? juce::String ("OUT") : (i == 1 ? upperPick ("Mid", "Mitte") : upperPick ("Side", "Seite")));
        // the stereo picture: L / R axes, the image as an ellipse as wide as the width setting
        const auto c = juce::Point<float> (r.getCentreX(), r.getY() + r.getHeight() * 0.55f);
        const float rad = juce::jmin (r.getWidth() * 0.45f, r.getHeight() * 0.42f);
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.drawLine ({ c.translated (-rad, -rad), c.translated (rad, rad) }, 1.0f);
        g.drawLine ({ c.translated (rad, -rad), c.translated (-rad, rad) }, 1.0f);
        g.setFont (monoFont (k * 1.0f));
        g.setColour (colour::displayDim);
        g.drawText ("L", juce::Rectangle<float> (k * 2.0f, k * 1.4f).withCentre (c.translated (-rad - k, -rad)), juce::Justification::centred, false);
        g.drawText ("R", juce::Rectangle<float> (k * 2.0f, k * 1.4f).withCentre (c.translated (rad + k, -rad)), juce::Justification::centred, false);
        const float width = juce::jlimit (0.0f, 2.0f, v ("width"));
        const float bal = t == "midside" ? v ("balance") : 0.0f;
        const float tall = rad * 0.95f;
        const float wide = juce::jmax (k * 0.4f, rad * 0.5f * width);
        juce::Path img;
        img.addEllipse (juce::Rectangle<float> (wide * 2.0f, tall * 1.6f).withCentre (c.translated (bal * rad * 0.5f, -tall * 0.1f)));
        g.setColour (ink.withAlpha (0.16f));
        g.fillPath (img);
        g.setColour (ink);
        g.strokePath (img, juce::PathStrokeType (k * 0.2f));
        return;
    }
}

void FxGraphTile::paintTile (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float k = u();
    const int t = typeOf (proc, prefix);
    const auto info = InsertSlot::typeInfo (t);
    // caption: number and type; right: the selected (or hovered) point and its values
    auto cap = area.withHeight (k * 2.0f);
    g.setFont (labelFont (k * 1.35f));
    g.setColour (colour::displayText);
    juce::String head = juce::String (slotIndex + 1) + "  " + (t > 0 && info.id != nullptr ? i18n::upper (i18n::pick (info.nameEn, info.nameDe)) : i18n::upper (tr ("Empty slot")));
    if (plain (prefix + "bypass") >= 0.5f && t > 0) head << "   " << i18n::upper (tr ("Bypass"));
    const float headW = juce::GlyphArrangement::getStringWidth (labelFont (k * 1.35f), head) + k * 2.0f;
    g.drawText (head, cap.withWidth (headW), juce::Justification::centredLeft, false);
    const int shown = hover >= 0 ? hover : selected;
    if (shown >= 0 && shown < numHandles())
    {
        g.setFont (monoFont (k * 1.15f));
        g.setColour (edit());
        g.drawFittedText (i18n::upper (handleName (shown)) + "   " + handleValues (shown), cap.withTrimmedLeft (headW).toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
    }
    const auto plot = plotArea();
    paintBackground (g, plot);
    // the points: yellow = what you edit (filled when selected), violet bar = what the matrix moves
    for (int i = 0; i < numHandles(); ++i)
    {
        const auto& h = lay.handles[(size_t) i];
        const auto p = handlePosition (i);
        const auto r = regionArea (h.region);
        if (! h.x.empty() && h.ax.kind != Axis::None && h.special == Special::None)
        {
            const auto rg = range (h.x);
            if (! rg.isEmpty())
                drawModBar (g, { r.getX() + r.getWidth() * h.ax.f0, p.y }, { r.getX() + r.getWidth() * h.ax.f1, p.y }, h.x);
        }
        const bool sel = i == selected, hov = i == hover;
        const float rad = k * (sel ? 0.95f : 0.8f);
        if (sel || hov)
        {
            g.setColour (edit().withAlpha (sel ? 0.28f : 0.16f));
            g.fillEllipse (juce::Rectangle<float> (rad * 3.6f, rad * 3.6f).withCentre (p));
        }
        g.setColour (colour::display);
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p));
        g.setColour (edit());
        if (sel) g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p));
        else g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p), k * 0.22f);
        // short label next to the point
        g.setFont (monoFont (k * 0.95f));
        g.setColour (sel ? colour::displayText : colour::displayDim);
        auto label = handleName (i);
        if (label.length() > 14) label = label.upToFirstOccurrenceOf (" ", false, false);
        const bool left = p.x > r.getRight() - k * 8.0f;
        const auto lr = juce::Rectangle<float> (k * 9.0f, k * 1.2f).withPosition (left ? p.x - k * 10.2f : p.x + k * 1.3f, p.y - k * 1.9f);
        if (h.region == 0 || lay.regions.size() == 1 || r.getWidth() > k * 12.0f)
            g.drawText (i18n::upper (label), lr, left ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
    }
}

// ================================================================= the editor panel
FxEditor::FxEditor (OhpeProcessor& p) : proc (p) {}

float FxEditor::unit() const { return (float) getWidth() / 115.2f; } // 1152 design px wide -> 10 px

juce::Rectangle<float> FxEditor::closeArea() const
{
    const float u = unit();
    return { (float) getWidth() - u * 12.5f, u * 1.2f, u * 11.0f, u * 2.8f };
}

juce::Rectangle<float> FxEditor::slotTabArea (int i) const
{
    const float u = unit();
    const float w = u * 11.4f, gap = u * 0.6f;
    const float x0 = closeArea().getX() - u * 2.0f - (w + gap) * (float) numSlots() + gap;
    return { x0 + (w + gap) * (float) i, u * 1.2f, w, u * 2.8f };
}

juce::Rectangle<float> FxEditor::bodyArea() const
{
    const float u = unit();
    return getLocalBounds().toFloat().withTrimmedTop (u * 5.2f).reduced (u * 1.5f, 0.0f).withTrimmedBottom (u * 1.2f);
}

juce::Rectangle<float> FxEditor::hintArea() const
{
    const float u = unit();
    auto b = bodyArea();
    return b.withWidth (b.getWidth() * 0.6f).removeFromBottom (u * 2.0f);
}

void FxEditor::show (int view, int prt, int s)
{
    const auto pre = modfx::rackPrefixes (view, prt);
    s = juce::jlimit (0, (int) pre.size() - 1, s);
    const int t = typeOf (proc, pre[(size_t) s]);
    if (view == fxView && prt == part && s == slot && t == type && tile != nullptr) return;
    const bool sameSlot = view == fxView && prt == part && s == slot && t == type && tile != nullptr;
    const int keep = sameSlot ? tile->selectedHandle() : -1;
    fxView = view;
    part = prt;
    slot = s;
    type = t;
    prefixes = pre;
    rebuild();
    if (keep >= 0 && tile != nullptr && keep < tile->numHandles()) tile->selectHandle (keep, false);
}

void FxEditor::rebuild()
{
    tile.reset();
    bars.clear();
    if (prefixes.empty()) return;
    const auto& pre = prefixes[(size_t) slot];
    tile = std::make_unique<FxGraphTile> (proc, juce::jlimit (0, Core::numParts - 1, part), fxView, slot, pre);
    tile->onHandleSelected = [this] (int h) {
        if (onFocus == nullptr || tile == nullptr) return;
        if (h < 0) onFocus ({}, {});
        else onFocus (tile->handleParams (h), tile->handleName (h));
        repaint();
    };
    addAndMakeVisible (*tile);
    std::vector<std::string> ids { pre + "type", pre + "mix", pre + "bypass" };
    for (const auto& id : modfx::slotEffectParams (proc.core(), pre)) ids.push_back (id);
    for (const auto& id : ids)
    {
        if (proc.parameter (juce::String (id)) == nullptr) continue;
        auto b = std::make_unique<ParamBar> (proc, juce::String (id), id == pre + "type" ? colour::displayText : edit());
        if (const auto* d = descOf (proc.core(), id)) b->setLabel (modfx::compactName (id, d->nameEn, d->nameDe)); // "Band 2 Freq"
        addAndMakeVisible (*b);
        bars.push_back (std::move (b));
    }
    resized();
    repaint();
}

void FxEditor::setEncoderIds (const std::vector<juce::String>& ids)
{
    if (ids == encoderIds) return;
    encoderIds = ids;
    repaint();
}

void FxEditor::refresh()
{
    if (! isVisible() || prefixes.empty()) return;
    // p57: the picture follows every parameter change at the editor's 30 Hz, as the bars do (encoders, MIDI, host,
    // automation): its hash of the slot's values decides whether it repaints. Before, only the headless renders ticked it,
    // so on screen the picture redrew only when something else happened to repaint it.
    auto tickGraph = [this] { if (tile != nullptr) { tile->tick (1.0f / 30.0f); ++graphTickCount; } };
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) { for (auto& b : bars) b->refresh(); tickGraph(); return; } // never rebuild under a drag
    // the slot's type changed (a bar, the host, a preset): new picture and bars
    if (typeOf (proc, prefixes[(size_t) slot]) != type) show (fxView, part, slot);
    else if (tile != nullptr) tile->refreshLayout(); // rate <-> tempo sync swaps a point's parameter
    for (auto& b : bars) b->refresh();
    tickGraph();
    repaint (closeArea().getUnion (slotTabArea (0)).toNearestInt().withLeft (0)); // the tabs (bypass, types)
}

void FxEditor::tick (float seconds)
{
    if (tile != nullptr) tile->tick (seconds);
}

void FxEditor::resized()
{
    const float u = unit();
    auto body = bodyArea();
    auto left = body.removeFromLeft (body.getWidth() * 0.6f);
    left.removeFromBottom (u * 2.0f); // the hint line
    if (tile != nullptr) tile->setBounds (left.toNearestInt());
    body.removeFromLeft (u * 2.0f);
    const int n = (int) bars.size();
    if (n == 0) return;
    const int cols = 2;
    const int rows = (n + cols - 1) / cols;
    const float colGap = u * 2.0f;
    const float colW = (body.getWidth() - colGap) / (float) cols;
    const float rowH = juce::jmin (u * 3.6f, body.getHeight() / (float) juce::jmax (1, rows));
    for (int i = 0; i < n; ++i)
    {
        const int c = i / rows, r = i % rows;
        const juce::Rectangle<float> cell (body.getX() + (colW + colGap) * (float) c + u * 1.2f, body.getY() + rowH * (float) r, colW - u * 1.2f, rowH * 0.9f);
        bars[(size_t) i]->setBounds (cell.toNearestInt());
    }
}

void FxEditor::paint (juce::Graphics& g)
{
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    const float du = (float) getWidth() / 1152.0f;
    g.setGradientFill (juce::ColourGradient (colour::bezel, 0, b.getY(), colour::bezelLow, 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, radius::panel * du);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), radius::inset * du);
    // title
    const auto titleArea = juce::Rectangle<float> (u * 2.0f, u * 1.2f, slotTabArea (0).getX() - u * 3.0f, u * 2.8f);
    g.setFont (labelFont (u * 1.6f));
    g.setColour (colour::displayText);
    const juce::String title = i18n::upper (i18n::pick ("FX editor", "FX-Editor"));
    const float tw = juce::GlyphArrangement::getStringWidth (labelFont (u * 1.6f), title) + u * 1.5f;
    g.drawText (title, titleArea.withWidth (tw), juce::Justification::centredLeft, false);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.25f));
    const juce::String sub = fxView == modfx::FxViewMaster ? i18n::upper (i18n::pick ("Master rack", "Master-Rack"))
                                                           : "P" + juce::String (part + 1) + "  " + i18n::upper (i18n::pick ("Rack", "Rack"));
    g.drawText ("/  " + sub, titleArea.withTrimmedLeft (tw), juce::Justification::centredLeft, true);
    // slot tabs: number + type, the shown one filled
    for (int i = 0; i < numSlots(); ++i)
    {
        const auto r = slotTabArea (i);
        const auto& pre = prefixes[(size_t) i];
        const int t = typeOf (proc, pre);
        const bool sel = i == slot, byp = plainOf (proc, pre + "bypass") >= 0.5f;
        g.setColour (sel ? colour::displayText : colour::displayGrid.brighter (0.3f));
        if (sel) g.fillRoundedRectangle (r, u * 0.6f);
        else g.drawRoundedRectangle (r, u * 0.6f, 1.0f);
        const auto info = InsertSlot::typeInfo (t);
        juce::String name = t > 0 && info.id != nullptr ? i18n::upper (i18n::pick (info.nameEn, info.nameDe)) : juce::String::fromUTF8 ("\xe2\x80\x94");
        g.setFont (monoFont (u * 1.1f));
        g.setColour (sel ? colour::display : colour::displayDim);
        g.drawText (juce::String (i + 1), r.withTrimmedLeft (u * 0.7f), juce::Justification::centredLeft, false);
        g.setFont (labelFont (u * 1.2f));
        g.setColour (sel ? colour::display : (t > 0 && ! byp ? colour::displayText : colour::displayDim));
        g.drawFittedText (name, r.withTrimmedLeft (u * 2.2f).withTrimmedRight (u * 0.6f).toNearestInt(), juce::Justification::centredLeft, 1, 0.65f);
        if (byp && t > 0)
        {
            g.setColour (colour::signalRed);
            g.fillEllipse (juce::Rectangle<float> (u * 0.6f, u * 0.6f).withCentre ({ r.getRight() - u * 0.8f, r.getY() + u * 0.7f }));
        }
    }
    const auto c = closeArea();
    g.setColour (colour::signalRed);
    g.fillRoundedRectangle (c, u * 0.6f);
    g.setColour (colour::displayText);
    g.setFont (labelFont (u * 1.4f));
    g.drawText (i18n::upper (tr ("Close")), c, juce::Justification::centred, true);
    // the hint under the picture
    g.setFont (monoFont (u * 1.05f));
    g.setColour (colour::displayDim);
    g.drawFittedText (i18n::pick ("Drag a point  -  Alt-drag or wheel: Q / second value  -  Shift: fine  -  double-click: reset  -  click: on the encoders",
                                  "Punkt ziehen  -  Alt-Ziehen oder Rad: Q / zweiter Wert  -  Shift: fein  -  Doppelklick: zur\xc3\xbc" "cksetzen  -  Klick: auf die Encoder"),
                      hintArea().toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
}

void FxEditor::paintOverChildren (juce::Graphics& g)
{
    // the encoder colours beside the parameters on the encoders now
    const float u = unit();
    for (auto& b : bars)
        for (size_t k = 0; k < encoderIds.size() && k < 4; ++k)
            if (encoderIds[k] == b->paramId())
            {
                const auto r = b->getBounds().toFloat();
                g.setColour (colour::encoder ((int) k));
                g.fillEllipse (juce::Rectangle<float> (u * 0.8f, u * 0.8f).withCentre ({ r.getX() - u * 0.7f, r.getY() + r.getHeight() * 0.32f }));
            }
}

void FxEditor::mouseDown (const juce::MouseEvent& e)
{
    if (closeArea().contains (e.position)) { if (onClose) onClose(); return; }
    for (int i = 0; i < numSlots(); ++i)
        if (slotTabArea (i).contains (e.position))
        {
            if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (prefixes[(size_t) i] + "type"), this); return; }
            if (onSlotSelected) onSlotSelected (i);
            return;
        }
}
} // namespace ohpe::ui
