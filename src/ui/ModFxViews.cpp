// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ModFxViews.h"

#include "Palette.h"
#include "fx/RackEffects.h"
#include "fx/Space3D.h"
#include "ResoView.h"
#include "fx/Resonator.h"
#include "SpaceView.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <cmath>

namespace ohpe::ui::modfx
{
using i18n::tr;

namespace
{
std::string partPrefix (int part) { return "p" + std::to_string (juce::jlimit (0, Core::numParts - 1, part) + 1) + "."; }
std::string routePrefix (int part, int routing) { return partPrefix (part) + "mod.s" + std::to_string (juce::jlimit (0, mod::numSlots - 1, routing) + 1) + "."; }

int getInt (const Core& core, const std::string& id)
{
    const int i = core.params().indexOf (id);
    return i >= 0 ? (int) std::lround (core.params().get (i)) : 0;
}

juce::String choiceText (const Core& core, const std::string& id)
{
    const int i = core.params().indexOf (id);
    if (i < 0) return {};
    const auto& d = core.params().descriptor (i);
    const int v = (int) std::lround (core.params().get (i));
    if (v < 0 || v >= (int) d.choices.size()) return {};
    return i18n::pick (d.choices[(size_t) v], v < (int) d.choicesDe.size() ? d.choicesDe[(size_t) v] : d.choices[(size_t) v]);
}

/** Display name of a target: the real parameter name for engine / FX targets ("Position", "Delay Time"). */
juce::String targetText (Core& core, int part, int dst)
{
    if (dst <= mod::DstOff || dst >= mod::numDests) return tr ("Off");
    const auto& info = mod::destTable()[(size_t) dst];
    if (info.kind == mod::DestKind::EngineParam || info.kind == mod::DestKind::FxParam)
    {
        const int idx = core.part (part).destParamIndex (dst);
        if (idx >= 0)
        {
            const auto& d = core.params().descriptor (idx);
            auto name = i18n::pick (d.nameEn, d.nameDe);
            // drop the host prefix of rack slots ("P2 FX3 ") - the display shows the slot itself
            if (name.startsWith ("P") && name.indexOf (" FX") > 0 && name.indexOf (" FX") < 4)
                name = name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
            if (info.kind == mod::DestKind::FxParam) name = "FX" + juce::String (info.arg / mod::numFxDestParams + 1) + " " + name;
            return name;
        }
        return i18n::pick (info.nameEn, info.nameDe) + " -";
    }
    return i18n::pick (info.nameEn, info.nameDe);
}

juce::String sourceText (int src)
{
    if (src < 0 || src >= mod::numSources) return {};
    const auto& s = mod::sourceTable()[(size_t) src];
    return i18n::pick (s.nameEn, s.nameDe);
}

void bipolarBar (juce::Graphics& g, juce::Rectangle<float> bar, float v, juce::Colour c)
{
    g.setColour (colour::displayGrid.brighter (0.2f));
    g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
    const float mid = bar.getCentreX();
    const float x = mid + juce::jlimit (-1.0f, 1.0f, v) * bar.getWidth() * 0.5f;
    g.setColour (c);
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (std::min (mid, x), bar.getY(), std::max (mid, x), bar.getBottom()),
                            bar.getHeight() * 0.5f);
    g.setColour (colour::displayDim);
    g.fillRect (mid - 0.5f, bar.getY() - 1.0f, 1.0f, bar.getHeight() + 2.0f);
}

juce::String typeName (const Core& core, const std::string& prefix) { return choiceText (core, prefix + "type"); }
} // namespace

const char* lfoViewName (int v) noexcept { return v == LfoViewMod ? "Mod" : v == LfoViewMorph ? "Morph" : v == LfoViewMotion ? "Motion" : v == LfoViewHarmony ? "Harmony" : "LFO"; }
const char* fxViewName (int v) noexcept
{
    return v == FxViewRack ? "Rack" : v == FxViewMaster ? "Master" : v == FxViewSpace ? "3D Space" : v == FxViewBody ? "Body" : v == FxViewPads ? "Pads" : "Sends";
}

// ------------------------------------------------------------------------------------------------- pages
std::vector<std::string> modParams (const Core&, int part, int routing)
{
    const auto r = routePrefix (part, routing);
    const auto p = partPrefix (part);
    return { r + "src", r + "via", r + "dst", r + "amt",                                   // the routing
             r + "curve", p + "seqlane", "", "",                                          // its curve, the lane
             p + "macro1", p + "macro2", p + "macro3", p + "macro4",                       // macros
             p + "lfo2.rate", p + "lfo2.sync", p + "lfo2.shape", p + "lfo2.depth",         // LFO 2
             p + "lfo2.mode", p + "lfo2.phase", p + "lfo2.fade", "",
             p + "lfo3.rate", p + "lfo3.sync", p + "lfo3.shape", p + "lfo3.depth",         // LFO 3
             p + "lfo3.mode", p + "lfo3.phase", p + "lfo3.fade", "",
             p + "env3.delay", p + "env3.a", p + "env3.hold", p + "env3.d",                // envelope 3
             p + "env3.s", p + "env3.r", "", "" };
}

std::vector<std::string> modTitles (const Core&, int, int)
{
    return { "Routing", "Curve & Lane", "Macros", "LFO 2", "LFO 2 Mode", "LFO 3", "LFO 3 Mode", "Env 3", "Env 3 Release" };
}

std::vector<std::string> rackPrefixes (int fxView, int part)
{
    return fxView == FxViewMaster ? FxRack::masterPrefixes() : FxRack::partPrefixes (partPrefix (part));
}

std::vector<std::string> slotEffectParams (const Core& core, const std::string& pre)
{
    std::vector<std::string> ids;
    const int t = getInt (core, pre + "type");
    if (t <= 0 || t >= InsertSlot::numTypes) return ids;
    const auto info = InsertSlot::typeInfo (t);
    if (info.id == nullptr) return ids;
    const std::string block = pre + info.id + ".";
    for (const auto& d : core.params().all())
        if (d.id.compare (0, block.size(), block) == 0) ids.push_back (d.id);
    return ids;
}

std::vector<std::string> rackParams (const Core& core, const std::vector<std::string>& pre, int slot)
{
    slot = juce::jlimit (0, (int) pre.size() - 1, slot);
    const auto& p = pre[(size_t) slot];
    std::vector<std::string> ids { p + "type", p + "mix", p + "bypass", "" };
    for (const auto& id : slotEffectParams (core, p)) ids.push_back (id);
    while (ids.size() % 4 != 0) ids.emplace_back();
    return ids;
}

std::vector<std::string> rackTitles (const Core& core, const std::vector<std::string>& pre, int slot)
{
    slot = juce::jlimit (0, (int) pre.size() - 1, slot);
    const auto ids = rackParams (core, pre, slot);
    std::vector<std::string> t { "Slot" };
    const auto info = InsertSlot::typeInfo (getInt (core, pre[(size_t) slot] + "type"));
    for (size_t i = 4; i < ids.size(); i += 4) t.emplace_back (info.nameEn);
    return t;
}

// ------------------------------------------------------------------------------------------------- mod list
namespace
{
juce::Rectangle<float> modLine (juce::Rectangle<float> area, int i)
{
    const float colW = area.getWidth() * 0.5f, h = area.getHeight() / 8.0f;
    return { area.getX() + colW * (float) (i / 8), area.getY() + h * (float) (i % 8), colW, h };
}
} // namespace

int modListHit (juce::Rectangle<float> area, juce::Point<float> p)
{
    for (int i = 0; i < mod::numSlots; ++i)
        if (modLine (area, i).contains (p)) return i;
    return -1;
}

void paintModList (juce::Graphics& g, juce::Rectangle<float> area, Core& core, int part, int selected)
{
    const auto& m = core.part (part).modMatrix();
    for (int i = 0; i < mod::numSlots; ++i)
    {
        auto line = modLine (area, i).reduced (area.getWidth() * 0.006f, area.getHeight() * 0.008f);
        const auto r = routePrefix (part, i);
        const int src = getInt (core, r + "src"), dst = getInt (core, r + "dst"), via = getInt (core, r + "via");
        const float amt = core.params().get (r + "amt");
        const bool on = src != mod::SrcOff && dst != mod::DstOff;
        if (i == selected)
        {
            g.setColour (colour::displayText.withAlpha (0.12f));
            g.fillRoundedRectangle (line, line.getHeight() * 0.18f);
            g.setColour (colour::displayText);
            g.drawRoundedRectangle (line, line.getHeight() * 0.18f, 1.0f);
        }
        auto inner = line.reduced (line.getHeight() * 0.14f, line.getHeight() * 0.06f);
        auto bar = inner.removeFromBottom (juce::jmax (2.0f, inner.getHeight() * 0.16f));
        inner.removeFromBottom (inner.getHeight() * 0.06f);
        auto num = inner.removeFromLeft (inner.getHeight() * 1.25f);
        g.setFont (monoFont (inner.getHeight() * 0.78f));
        g.setColour (on ? colour::displayText : colour::displayDim);
        g.drawText (juce::String (i + 1), num, juce::Justification::centredLeft);
        g.setFont (labelFont (inner.getHeight() * 0.86f));
        if (! on)
        {
            g.setColour (colour::displayDim.withAlpha (0.6f));
            g.drawText (juce::String::fromUTF8 ("\xe2\x80\x94"), inner, juce::Justification::centredLeft);
            continue;
        }
        juce::String label = sourceText (src);
        if (via != mod::SrcOff) label << juce::String::fromUTF8 (" \xc3\x97 ") << sourceText (via);
        label << juce::String::fromUTF8 (" \xe2\x86\x92 ") << targetText (core, part, dst);
        g.setColour (colour::displayText);
        g.drawFittedText (label, inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        // amount (static, from the centre) and the live value of the routing, under the text
        bar = bar.withTrimmedLeft (num.getWidth());
        bipolarBar (g, bar, amt, colour::rose);
        const float live = juce::jlimit (-1.0f, 1.0f, m.uiRouteValue (i));
        const float x = bar.getCentreX() + live * bar.getWidth() * 0.5f;
        g.setColour (colour::displayText);
        g.fillEllipse (juce::Rectangle<float> (bar.getHeight() * 2.0f, bar.getHeight() * 2.0f).withCentre ({ x, bar.getCentreY() }));
    }
}

void paintModTrace (juce::Graphics& g, juce::Rectangle<float> area, Core& core, int part, int routing, const std::vector<float>& history)
{
    const auto r = routePrefix (part, routing);
    const int src = getInt (core, r + "src"), dst = getInt (core, r + "dst");
    auto top = area.removeFromTop (area.getHeight() * 0.3f);
    g.setFont (labelFont (top.getHeight() * 0.62f));
    g.setColour (colour::displayDim);
    const juce::String caption = i18n::upper (tr ("Mod") + " " + juce::String (routing + 1));
    g.drawText (caption, top, juce::Justification::centredLeft);
    g.setColour (colour::displayText);
    g.drawText (i18n::upper (sourceText (src)) + juce::String::fromUTF8 ("  \xe2\x86\x92  ") + i18n::upper (targetText (core, part, dst)),
                top.withTrimmedLeft (top.getHeight() * 4.0f), juce::Justification::centredLeft, true);
    // zero line, trace
    auto plot = area.reduced (0, area.getHeight() * 0.08f);
    g.setColour (colour::displayGrid.brighter (0.25f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    if (history.size() < 2) return;
    juce::Path p;
    for (size_t i = 0; i < history.size(); ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (history.size() - 1);
        const float y = plot.getCentreY() - juce::jlimit (-1.0f, 1.0f, history[i]) * plot.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y);
        else p.lineTo (x, y);
    }
    g.setColour (colour::rose);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ------------------------------------------------------------------------------------------------- rack chain
namespace
{
juce::Rectangle<float> chainBlock (juce::Rectangle<float> area, int n, int i)
{
    const int cols = n > 4 ? 3 : 2;
    const int rows = (n + cols - 1) / cols;
    const float w = area.getWidth() / (float) cols, h = area.getHeight() / (float) rows;
    return { area.getX() + w * (float) (i % cols), area.getY() + h * (float) (i / cols), w, h };
}
} // namespace

int rackChainHit (juce::Rectangle<float> area, int n, juce::Point<float> p)
{
    for (int i = 0; i < n; ++i)
        if (chainBlock (area, n, i).contains (p)) return i;
    return -1;
}

void paintRackChain (juce::Graphics& g, juce::Rectangle<float> area, Core& core, const std::vector<std::string>& pre, FxRack& rack, int selected)
{
    const int n = (int) pre.size();
    for (int i = 0; i < n; ++i)
    {
        auto cell = chainBlock (area, n, i);
        auto block = cell.reduced (cell.getWidth() * 0.1f, cell.getHeight() * 0.1f);
        const int type = getInt (core, pre[(size_t) i] + "type");
        const bool bypassed = core.params().get (pre[(size_t) i] + "bypass") >= 0.5f;
        const bool on = type != InsertSlot::Off;
        // arrow to the next block (signal flow)
        if (i + 1 < n)
        {
            const auto next = chainBlock (area, n, i + 1).reduced (cell.getWidth() * 0.1f, cell.getHeight() * 0.1f);
            g.setColour (colour::displayGrid.brighter (0.35f));
            if (std::abs (next.getCentreY() - block.getCentreY()) < 1.0f)
                g.drawArrow ({ block.getRight(), block.getCentreY(), next.getX(), next.getCentreY() }, 1.0f, 5.0f, 5.0f);
        }
        g.setColour (on && ! bypassed ? colour::displayGrid.brighter (0.12f) : colour::display);
        g.fillRoundedRectangle (block, block.getHeight() * 0.12f);
        g.setColour (i == selected ? colour::displayText : colour::displayGrid.brighter (0.35f));
        g.drawRoundedRectangle (block, block.getHeight() * 0.12f, i == selected ? 1.6f : 1.0f);
        auto inner = block.reduced (block.getHeight() * 0.1f);
        auto head = inner.removeFromTop (inner.getHeight() * 0.32f);
        g.setFont (monoFont (head.getHeight() * 0.7f));
        g.setColour (colour::displayDim);
        g.drawText (juce::String (i + 1), head, juce::Justification::centredLeft);
        if (on && bypassed)
        {
            g.setColour (colour::signalRed);
            g.drawText (i18n::upper (tr ("Bypass")), head, juce::Justification::centredRight);
        }
        g.setFont (labelFont (inner.getHeight() * 0.36f));
        g.setColour (on ? (bypassed ? colour::displayDim : colour::displayText) : colour::displayDim.withAlpha (0.6f));
        g.drawFittedText (on ? i18n::upper (typeName (core, pre[(size_t) i])) : juce::String::fromUTF8 ("\xe2\x80\x94"),
                          inner.withHeight (inner.getHeight() * 0.55f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        if (! on) continue;
        // mix bar and output level
        auto bars = inner.withTop (inner.getY() + inner.getHeight() * 0.62f);
        auto mixBar = bars.removeFromTop (bars.getHeight() * 0.4f);
        g.setColour (colour::displayGrid.brighter (0.2f));
        g.fillRoundedRectangle (mixBar, mixBar.getHeight() * 0.5f);
        g.setColour (colour::sunYellow.withAlpha (bypassed ? 0.35f : 1.0f));
        g.fillRoundedRectangle (mixBar.withWidth (mixBar.getWidth() * juce::jlimit (0.0f, 1.0f, core.params().get (pre[(size_t) i] + "mix"))),
                                mixBar.getHeight() * 0.5f);
        auto lvl = bars.withTrimmedTop (bars.getHeight() * 0.35f);
        const float pk = juce::jlimit (0.0f, 1.0f, std::sqrt (rack.slotPeak (i)));
        g.setColour (colour::ultraviolet.withAlpha (0.8f));
        g.fillRoundedRectangle (lvl.withWidth (lvl.getWidth() * pk), lvl.getHeight() * 0.5f);
    }
}

// ------------------------------------------------------------------------------------------------- slot visual
void paintSlotVisual (juce::Graphics& g, juce::Rectangle<float> area, Core& core, const std::string& pre, InsertSlot& slot)
{
    const int type = getInt (core, pre + "type");
    const auto info = InsertSlot::typeInfo (type);
    auto caption = area.removeFromLeft (area.getWidth() * 0.22f);
    g.setFont (labelFont (caption.getHeight() * 0.2f));
    g.setColour (colour::displayDim);
    g.drawFittedText (i18n::upper (type == 0 ? tr ("Empty slot") : juce::String (i18n::pick (info.nameEn, info.nameDe))), caption.toNearestInt(),
                      juce::Justification::centredLeft, 2, 0.8f);
    auto plot = area.reduced (area.getWidth() * 0.01f, area.getHeight() * 0.1f);
    g.setColour (colour::displayGrid.brighter (0.2f));
    g.drawRoundedRectangle (plot, 3.0f, 1.0f);
    if (type == 0 || info.id == nullptr) return;
    if (type == InsertSlot::Space3D) // 1.0: where the slot's source is, seen from behind the listener
    {
        if (auto* e = static_cast<SpaceEffect*> (slot.effect (type)))
            space3d::Scene::paintSingle (g, plot, e->uiPosition(), juce::jmin (1.0f, e->uiLevel() * 2.0f), colour::ultraviolet,
                                         space3d::speakerMonitoring (core), (float) juce::Time::getMillisecondCounter() * 0.001f);
        return;
    }
    if (type == InsertSlot::Resonator) // p19: the slot's body
    {
        if (auto* e = static_cast<ResonatorEffect*> (slot.effect (type)))
            resoview::paintBody (g, plot, resoview::stateOf (*e, core, pre + "resonator.", true), (float) juce::Time::getMillisecondCounter() * 0.001f,
                                 colour::ultraviolet);
        return;
    }
    const std::string block = pre + info.id + ".";
    auto val = [&] (const char* k) { return core.params().get (block + k); };
    const std::string id = info.id;
    juce::Path p;
    auto curve = [&] (int n, auto&& fn) { // fn: 0..1 -> y in plot (0 = bottom, 1 = top)
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / (float) n;
            const float x = plot.getX() + plot.getWidth() * t;
            const float y = plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, fn (t));
            if (i == 0) p.startNewSubPath (x, y);
            else p.lineTo (x, y);
        }
    };
    juce::Colour c = colour::ultraviolet;
    if (id == "distortion")
    {
        // transfer curve, input -1..1 across, output -1..1 up
        const int mode = (int) std::lround (val ("mode"));
        const float drive = val ("drive"), bias = val ("bias");
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.drawLine (plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 0.5f);
        g.drawLine (plot.getCentreX(), plot.getY(), plot.getCentreX(), plot.getBottom(), 0.5f);
        curve (120, [&] (float t) { return 0.5f + 0.48f * DistortionEffect::transfer (mode, drive, bias, t * 2.0f - 1.0f); });
        c = colour::signalRed;
    }
    else if (id == "eq")
    {
        std::array<float, EqEffect::numParams> v {};
        const char* keys[EqEffect::numParams] = { "lowfreq", "lowgain", "p1freq", "p1gain", "p1q", "p2freq", "p2gain", "p2q",
                                                  "p3freq", "p3gain", "p3q", "highfreq", "highgain", "output" };
        for (int i = 0; i < EqEffect::numParams; ++i) v[(size_t) i] = val (keys[i]);
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.drawLine (plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 0.5f);
        curve (160, [&] (float t) {
            const float hz = 20.0f * std::pow (1000.0f, t);
            return 0.5f + EqEffect::responseDb (v.data(), hz, 48000.0f) / 36.0f; // +-18 dB, the gain range
        });
        c = colour::sunYellow;
    }
    else if (id == "compressor")
    {
        // static curve (in -60..0 dB -> out) and the live gain reduction bar
        const float th = val ("thresh"), ratio = val ("ratio"), knee = val ("knee");
        curve (100, [&] (float t) {
            const float in = -60.0f + 60.0f * t;
            return (in + CompressorEffect::staticGainDb (in, th, ratio, knee) + 60.0f) / 60.0f;
        });
        if (auto* e = dynamic_cast<CompressorEffect*> (slot.effect (type)))
        {
            const float gr = juce::jlimit (0.0f, 24.0f, e->gainReductionDb());
            auto bar = plot.withLeft (plot.getRight() - plot.getWidth() * 0.06f).reduced (2.0f);
            g.setColour (colour::signalRed);
            g.fillRect (bar.withHeight (bar.getHeight() * gr / 24.0f));
        }
        c = colour::rose;
    }
    else if (id == "multiband")
    {
        if (auto* e = dynamic_cast<MultibandEffect*> (slot.effect (type)))
        {
            const char* names[3] = { "Low", "Mid", "High" };
            for (int b = 0; b < 3; ++b)
            {
                auto col = plot.withWidth (plot.getWidth() / 3.0f).withX (plot.getX() + plot.getWidth() / 3.0f * (float) b).reduced (plot.getWidth() * 0.04f, 4.0f);
                const float gdb = juce::jlimit (-24.0f, 24.0f, e->bandGainDb (b));
                const float mid = col.getCentreY();
                const float y = mid - gdb / 24.0f * col.getHeight() * 0.5f;
                g.setColour (gdb >= 0.0f ? colour::sunYellow : colour::signalRed);
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (col.getX(), std::min (mid, y), col.getRight(), std::max (mid, y)));
                g.setColour (colour::displayDim);
                g.setFont (labelFont (col.getHeight() * 0.18f));
                g.drawText (i18n::upper (tr (names[b])), col, juce::Justification::centredBottom);
            }
        }
        return;
    }
    else if (id == "filter" || id == "filterdrive")
    {
        // stylised response around the cutoff
        const float cut = val ("cutoff"), reso = val ("reso");
        const int t = id == "filter" ? (int) std::lround (val ("type")) : (int) std::lround (val ("mode")) * 2;
        const float x0 = std::log (cut / 20.0f) / std::log (1000.0f);
        curve (140, [&] (float x) {
            const float d = (x - x0) * 10.0f; // octaves
            const float peak = reso * 0.3f * std::exp (-d * d * 4.0f);
            float y;
            if (id == "filter" && (t == FilterRackEffect::HP12 || t == FilterRackEffect::HP24)) y = d > 0 ? 0.7f : 0.7f + d * 0.2f;
            else if (id == "filter" && t == FilterRackEffect::BP) y = 0.7f - std::abs (d) * 0.15f;
            else if (id == "filter" && t == FilterRackEffect::Notch) y = 0.7f - 0.6f * std::exp (-d * d * 6.0f);
            else if (id == "filter" && t >= FilterRackEffect::CombPlus) y = 0.45f + 0.25f * std::cos (x * 40.0f + x0 * 20.0f);
            else y = d < 0 ? 0.7f : 0.7f - d * (id == "filter" && t == FilterRackEffect::LP12 ? 0.1f : 0.2f);
            return y + peak;
        });
    }
    else
    {
        // generic: a time line of the slot's output level (sine-ish motion for the modulated effects)
        const float pk = slot.isReady (type) ? 1.0f : 0.0f;
        curve (100, [&] (float t) { return 0.5f + 0.3f * pk * std::sin (t * 12.0f) * std::exp (-t * 1.5f); });
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ------------------------------------------------------------------------------------------------- rings
Range modulationRange (Core& core, const std::string& paramId)
{
    Range r;
    if (paramId.size() < 3 || paramId[0] != 'p' || paramId[2] != '.') return r;
    const int part = paramId[1] - '1';
    if (part < 0 || part >= Core::numParts) return r;
    const int idx = core.params().indexOf (paramId);
    if (idx < 0) return r;
    const auto& d = core.params().descriptor (idx);
    const float base = d.toNormalised (core.params().get (idx));
    float lo = 0.0f, hi = 0.0f;
    auto& p = core.part (part);
    for (int k = 0; k < mod::numSlots; ++k)
    {
        const auto pre = routePrefix (part, k);
        const int src = getInt (core, pre + "src"), dst = getInt (core, pre + "dst");
        const float amt = core.params().get (pre + "amt");
        if (src == mod::SrcOff || dst == mod::DstOff || amt == 0.0f || mod::isVoiceDest (dst)) continue;
        if (p.destParamIndex (dst) != idx) continue;
        r.any = true;
        const bool bip = mod::sourceTable()[(size_t) src].bipolar;
        if (bip) { lo -= std::abs (amt); hi += std::abs (amt); }
        else if (amt > 0.0f) hi += amt;
        else lo += amt;
    }
    if (! r.any) return r;
    r.lo = juce::jlimit (0.0f, 1.0f, base + lo);
    r.hi = juce::jlimit (0.0f, 1.0f, base + hi);
    r.live = juce::jlimit (0.0f, 1.0f, base + p.modulationOf (idx));
    r.liveValid = true;
    return r;
}

// ------------------------------------------------------------------------------------------------- names
juce::String compactName (const std::string& id, const std::string& nameEn, const std::string& nameDe)
{
    juce::String name = i18n::pick (nameEn, nameDe);
    const bool part = id.size() > 3 && id[0] == 'p' && id[1] >= '1' && id[1] <= '9' && id[2] == '.';
    const std::string rest = part ? id.substr (3) : id;
    const bool matrix = part && (rest.rfind ("mod.s", 0) == 0 || rest.rfind ("lfo2.", 0) == 0 || rest.rfind ("lfo3.", 0) == 0
                                 || rest.rfind ("env3.", 0) == 0 || rest.rfind ("macro", 0) == 0 || rest == "seqlane");
    const bool rack = (part && rest.rfind ("fx", 0) == 0) || id.rfind ("mfx", 0) == 0;
    if (part && rest.rfind ("space.", 0) == 0) // 1.0: "Part 2 3D Azimuth" -> "Azimuth", "Part 2 3D Space" -> "3D Space"
    {
        const int marker = name.indexOf (" 3D ");
        if (marker < 0) return name;
        if (rest == "space.mode") return tr ("3D Space"); // p41: the 3D switch reads "3D Space" / "3D-Raum"
        return rest == "space.on" ? name.substring (marker + 1) : name.substring (marker + 4);
    }
    if (part && (rest.rfind ("morph.", 0) == 0 || rest.rfind ("reso.", 0) == 0 || rest.rfind ("motion.", 0) == 0) && name.startsWith ("Part ") && name.length() > 7) // p16: "Part 1 Morph X" -> "Morph X" (p19: "Part 1 Body Mix" -> "Body Mix")
        return name.substring (7).trimStart();
    if (id.rfind ("harmony.p", 0) == 0 && name.startsWith ("Part ") && name.length() > 7) // p24: "Part 1 Harmony Follow" -> "Harmony Follow"
        return name.substring (7).trimStart();
    if (! matrix && ! rack) return name;

    auto dropWord = [&name] (const juce::String& word) { // "word" + optional number + space
        if (! name.startsWith (word)) return false;
        auto r = name.substring (word.length());
        int k = 0;
        while (k < r.length() && juce::CharacterFunctions::isDigit (r[k])) ++k;
        if (k == 0 || k >= r.length() || r[k] != ' ') return false;
        name = r.substring (k + 1);
        return true;
    };
    if (! dropWord ("Part ")) dropWord ("P");
    if (matrix)
    {
        dropWord ("Mod ");
        return name;
    }
    // rack: slot prefix ("FX3 ", "FX 3 ", "Master FX 1 ", "Master-FX 1 "), then the slot's own controls / the effect name
    if (! dropWord ("Master FX ") && ! dropWord ("Master-FX ") && ! dropWord ("FX ")) dropWord ("FX");
    std::vector<std::string> seg;
    for (size_t a = 0, b; a <= id.size(); a = b + 1)
    {
        b = id.find ('.', a);
        if (b == std::string::npos) b = id.size();
        seg.push_back (id.substr (a, b - a));
    }
    const size_t slotEnd = part ? 2 : 1; // index of the first segment after the slot ("p1.fx3." / "mfx1.")
    if (seg.size() == slotEnd + 1 && seg[slotEnd] == "type") return tr ("Type");
    if (seg.size() <= slotEnd + 1) return name; // mix, bypass
    for (int t = InsertSlot::Off + 1; t < InsertSlot::numTypes; ++t)
    {
        const auto info = InsertSlot::typeInfo (t);
        if (info.id == nullptr || seg[slotEnd] != info.id) continue;
        for (const juce::String eff : { juce::String (info.nameEn), juce::String::fromUTF8 (info.nameDe) })
        {
            if (! name.startsWith (eff) || name.length() <= eff.length() + 1) continue;
            const auto sep = name[eff.length()];
            if (sep != ' ' && sep != '-') continue;
            auto shortName = name.substring (eff.length() + 1);
            if (shortName == "Mix" || shortName == "Mischung") return name; // keep "Echo Mix" apart from the slot's mix
            return shortName.substring (0, 1).toUpperCase() + shortName.substring (1);
        }
    }
    return name;
}

juce::String specialValueText (Core& core, const std::string& id, float plain)
{
    if (id.size() < 12 || id[0] != 'p' || id.compare (id.size() - 4, 4, ".dst") != 0 || id.find (".mod.s") != 2) return {};
    const int part = id[1] - '1';
    if (part < 0 || part >= Core::numParts) return {};
    return targetText (core, part, (int) std::lround (plain));
}
} // namespace ohpe::ui::modfx
