// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ProTiles.h"

#include "Pages.h"
#include "Palette.h"
#include "Widgets.h"
#include "engines/FmEngine.h"
#include "engines/GrainEngine.h"
#include "engines/WavetableEngine.h"
#include "mod/ModMatrix.h"
#include "plugin/Localisation.h"
#include "plugin/ParameterBridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace ohpe::ui::pro
{
using i18n::tr;

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

/** The part-local suffix of a parameter ID ("p2.fm.index" -> "fm.index"); other IDs are returned as they are. */
std::string localOf (const std::string& id)
{
    if (id.size() > 3 && id[0] == 'p' && id[1] >= '1' && id[1] <= '4' && id[2] == '.') return id.substr (3);
    return id;
}

/** The last component ("fm.index" -> "index"). */
std::string tailOf (const std::string& id)
{
    const auto dot = id.rfind ('.');
    return dot == std::string::npos ? id : id.substr (dot + 1);
}

bool endsWith (const std::string& s, const char* t)
{
    const size_t n = std::strlen (t);
    return s.size() >= n && s.compare (s.size() - n, n, t) == 0;
}

juce::Colour edit() { return colour::sunYellow; }
juce::Colour modc() { return colour::ultraviolet; }
juce::Colour play() { return colour::playGreenLit; }

/** The plain value the user sees now (the host parameter; the registry follows on the next audio block). */
float plainOf (OhpeProcessor& proc, const std::string& id)
{
    if (auto* q = proc.parameter (juce::String (id))) return q->convertFrom0to1 (q->getValue());
    return proc.core().params().get (id);
}
} // namespace

// ---------------------------------------------------------------- explanations
juce::String explanation (const std::string& paramId)
{
    struct Line { const char* key; const char* en; const char* de; };
    // matched on the part-local ID first (exact), then on its last component
    static const Line exact[] = {
        { "amp.a", "How long the sound takes to reach full level.", "Wie lange der Klang bis zum vollen Pegel braucht." },
        { "amp.d", "How fast the level falls from the peak to the sustain level.", "Wie schnell der Pegel vom Gipfel auf den Haltepegel fällt." },
        { "amp.s", "The level held while the key stays down.", "Der Pegel, solange die Taste gehalten wird." },
        { "amp.r", "How long the sound fades after the key is released.", "Wie lange der Klang nach dem Loslassen ausklingt." },
        { "mod.a", "Rise time of the modulation envelope.", "Anstiegszeit der Modulationshüllkurve." },
        { "mod.d", "Fall time of the modulation envelope to its sustain level.", "Abfallzeit der Modulationshüllkurve auf ihren Haltepegel." },
        { "mod.s", "Level the modulation envelope holds while the key is down.", "Pegel, den die Modulationshüllkurve bei gehaltener Taste hält." },
        { "mod.r", "Fade of the modulation envelope after release.", "Ausklingen der Modulationshüllkurve nach dem Loslassen." },
        { "mod.amt", "How strongly the modulation envelope shapes the timbre.", "Wie stark die Modulationshüllkurve die Klangfarbe formt." },
        { "lfo.rate", "Speed of the LFO.", "Tempo des LFO." },
        { "lfo.depth", "How far the LFO moves its target.", "Wie weit der LFO sein Ziel bewegt." },
        { "lfo.shape", "Wave shape of the LFO - swipe the picture to change it.", "Wellenform des LFO - über das Bild wischen, um sie zu ändern." },
        { "lfo.dest", "What the LFO moves.", "Was der LFO bewegt." },
        { "tune", "Coarse tuning of the part in semitones.", "Grobstimmung des Parts in Halbtönen." },
        { "fine", "Fine tuning of the part in cents.", "Feinstimmung des Parts in Cent." },
        { "vol", "Output level of the part.", "Ausgangspegel des Parts." },
        { "glide.press", "Aftertouch changes the glide time: right = pressing harder glides slower, left = faster.",
          "Aftertouch ändert die Glide-Zeit: rechts = fester drücken gleitet langsamer, links = schneller." },
        { "life", "Held notes keep evolving: drift, breathing, motion.", "Gehaltene Töne leben weiter: Drift, Atmen, Bewegung." },
        { "char.bright", "Opens the sound: filter, tone, brightness of the engine.", "Öffnet den Klang: Filter, Ton, Helligkeit der Engine." },
        { "char.hard", "More bite: faster attack, drive, feedback, excitation.", "Mehr Biss: schnellerer Anstieg, Drive, Feedback, Anregung." },
        { "char.alive", "More motion: life, drift, jitter, scatter.", "Mehr Bewegung: Leben, Drift, Jitter, Streuung." },
        { "char.wide", "More stereo width: unison width, spread, layer width.", "Mehr Stereobreite: Unisono-Breite, Spreizung, Layer-Breite." },
        { "fm.algo", "Which operators modulate which - the lines in the graph.", "Welche Operatoren welche modulieren - die Linien im Graphen." },
        { "fm.index", "Overall modulation depth: the thickness of the lines.", "Gesamte Modulationstiefe: die Stärke der Linien." },
        { "fm.feedback", "Operator 6 modulates itself: from bright to noisy.", "Operator 6 moduliert sich selbst: von hell bis rauschig." },
        { "fm.modratio", "Frequency ratio of the modulators.", "Frequenzverhältnis der Modulatoren." },
        { "wavetable.pos", "Position in the wavetable: which frame plays.", "Position in der Wavetable: welcher Frame spielt." },
        { "wavetable.warp", "Bends the phase of the wave.", "Verbiegt die Phase der Welle." },
        { "grain.pos", "Where in the source the grains are taken from.", "Wo in der Quelle die Grains entnommen werden." },
        { "grain.size", "Length of each grain.", "Länge jedes Grains." },
        { "grain.density", "Grains per second.", "Grains pro Sekunde." },
        { "grain.spray", "Random scatter of the grain positions.", "Zufällige Streuung der Grain-Positionen." },
        { "sampler.start", "Where the sample starts playing.", "Wo das Sample zu spielen beginnt." },
        { "vocal.vowel", "The vowel the voice sings.", "Der Vokal, den die Stimme singt." },
        { "vocal.formant", "Shifts the formants: smaller or larger throat.", "Verschiebt die Formanten: kleinerer oder größerer Rachen." },
    };
    static const Line tails[] = {
        { "cutoff", "Where the filter starts to cut the highs.", "Wo das Filter beginnt, die Höhen zu beschneiden." },
        { "reso", "Emphasis around the cutoff - the peak of the curve.", "Betonung um die Grenzfrequenz - die Spitze der Kurve." },
        { "drive", "Saturation before the filter: warmer, louder, harder.", "Sättigung vor dem Filter: wärmer, lauter, härter." },
        { "tone", "Brightness of the sound.", "Helligkeit des Klangs." },
        { "keytrack", "How much the filter follows the played note.", "Wie stark das Filter der gespielten Note folgt." },
        { "ftype", "Filter type.", "Filtertyp." },
        { "shape", "Wave shape of the oscillator.", "Wellenform des Oszillators." },
        { "detune", "Detuning of the oscillators against each other.", "Verstimmung der Oszillatoren gegeneinander." },
        { "spread", "Stereo spread of the voices.", "Stereoverteilung der Stimmen." },
        { "mix", "Balance between the dry and the processed signal.", "Verhältnis von trockenem und bearbeitetem Signal." },
        { "feedback", "How much of the output is fed back.", "Wie viel vom Ausgang zurückgeführt wird." },
        { "time", "Delay time.", "Verzögerungszeit." },
        { "size", "Size of the space.", "Größe des Raums." },
        { "damping", "How fast the highs die away.", "Wie schnell die Höhen abklingen." },
        { "rate", "Speed of the motion.", "Tempo der Bewegung." },
        { "depth", "Amount of the motion.", "Stärke der Bewegung." },
        { "azimuth", "Direction around the head - drag the dot.", "Richtung um den Kopf - den Punkt ziehen." },
        { "distance", "Distance from the head.", "Entfernung vom Kopf." },
        { "elevation", "Height above or below the head.", "Höhe über oder unter dem Kopf." },
        { "level", "Level.", "Pegel." },
        { "glide", "Time the pitch slides between notes.", "Zeit, in der die Tonhöhe zwischen Noten gleitet." },
        { "sub", "Level of the sub oscillator one octave down.", "Pegel des Suboszillators eine Oktave tiefer." },
        { "noise", "Noise mixed into the oscillators.", "Rauschen, das den Oszillatoren beigemischt wird." },
        { "oscmix", "Balance between oscillator 1 and 2.", "Verhältnis von Oszillator 1 und 2." },
        { "bright", "Brightness of the excitation.", "Helligkeit der Anregung." },
        { "excite", "How the string is excited.", "Wie die Saite angeregt wird." },
        { "body", "Resonance of the instrument body.", "Resonanz des Instrumentenkörpers." },
        { "breath", "Air in the voice.", "Luft in der Stimme." },
        { "type", "Which effect sits in this slot.", "Welcher Effekt in diesem Slot sitzt." },
        { "bypass", "Skips the effect without losing its settings.", "Überspringt den Effekt, ohne seine Einstellungen zu verlieren." },
    };
    const auto local = localOf (paramId);
    for (const auto& l : exact)
        if (local == l.key) return i18n::pick (l.en, l.de);
    if (local.rfind ("fm.op", 0) == 0)
        return endsWith (local, "ratio") ? i18n::pick ("Frequency ratio of this operator - drag its circle up or down.",
                                                       "Frequenzverhältnis dieses Operators - seinen Kreis nach oben oder unten ziehen.")
                                         : i18n::pick ("Output level of this operator - its circle's ring.", "Ausgangspegel dieses Operators - der Ring seines Kreises.");
    const auto t = tailOf (local);
    for (const auto& l : tails)
        if (t == l.key) return i18n::pick (l.en, l.de);
    return {};
}

juce::String hoverText (OhpeProcessor& proc, const std::string& id)
{
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id);
    if (idx < 0) return {};
    const auto& d = reg.descriptor (idx);
    juce::String s = i18n::pick (d.nameEn, d.nameDe) + "  " + formatValue (d, plainOf (proc, id));
    if (const auto e = explanation (id); e.isNotEmpty()) s << " - " << e;
    return s;
}

int destForParam (OhpeProcessor& proc, int part, const std::string& paramId)
{
    auto& core = proc.core();
    const int pi = juce::jlimit (0, Core::numParts - 1, part);
    auto& pt = core.part (pi);
    const int idx = core.params().indexOf (paramId);
    if (idx < 0) return -1;
    const std::string prefix = pt.prefix();
    if (paramId.rfind (prefix, 0) != 0) return -1;
    const auto local = paramId.substr (prefix.size());
    // the engine the part plays now (the selected one - the audio thread may not have switched yet)
    const auto type = selectedEngine (core, pi);
    const std::string ep = std::string (engineTypeInfo (type).id) + ".";
    if (local.rfind (ep, 0) == 0)
        if (auto* e = pt.engine (type))
        {
            // engine descriptors never change: cache the local order per engine (message thread only)
            static std::map<std::string, int> cache;
            const auto key = local;
            if (const auto it = cache.find (key); it != cache.end()) return it->second;
            int r = -1;
            const auto ds = e->getParamDescriptors();
            for (size_t i = 0; i < ds.size() && i < (size_t) mod::numEngineDests; ++i)
                if (ds[i].id == local.substr (ep.size())) { r = mod::DstEngine1 + (int) i; break; }
            cache[key] = r;
            return r;
        }
    for (int d = mod::DstOff + 1; d < mod::numDests; ++d)
    {
        const auto& info = mod::destTable()[(size_t) d];
        if (info.kind == mod::DestKind::PartParam && info.paramSuffix != nullptr && local == info.paramSuffix) return d;
    }
    for (int d = mod::DstOff + 1; d < mod::numDests; ++d) // FX rack slots: as the part resolves them now
    {
        const auto& info = mod::destTable()[(size_t) d];
        if ((info.kind == mod::DestKind::FxMix || info.kind == mod::DestKind::FxParam) && pt.destParamIndex (d) == idx) return d;
    }
    return -1;
}

juce::Range<float> modRange (OhpeProcessor& proc, int part, const std::string& paramId)
{
    const int dst = destForParam (proc, part, paramId);
    if (dst < 0) return {};
    const std::string p = "p" + std::to_string (juce::jlimit (0, Core::numParts - 1, part) + 1) + ".mod.s";
    float lo = 0.0f, hi = 0.0f;
    for (int k = 1; k <= mod::numSlots; ++k)
    {
        const auto r = p + std::to_string (k) + ".";
        if (std::lround (plainOf (proc, r + "dst")) != dst) continue;
        const int src = (int) std::lround (plainOf (proc, r + "src"));
        if (src <= mod::SrcOff || src >= mod::numSources) continue;
        const float amt = plainOf (proc, r + "amt");
        if (amt == 0.0f) continue;
        const bool bip = mod::sourceTable()[(size_t) src].bipolar;
        if (bip) { lo -= std::abs (amt); hi += std::abs (amt); }
        else if (amt > 0.0f) hi += amt;
        else lo += amt;
    }
    return { juce::jmax (-1.0f, lo), juce::jmin (1.0f, hi) };
}

int addRouting (OhpeProcessor& proc, int part, int source, const std::string& paramId, float amount)
{
    const int dst = destForParam (proc, part, paramId);
    if (dst < 0 || source <= mod::SrcOff || source >= mod::numSources) return -1;
    const std::string p = "p" + std::to_string (juce::jlimit (0, Core::numParts - 1, part) + 1) + ".mod.s";
    auto setPlain = [&] (const std::string& id, float v) {
        if (auto* q = proc.parameter (juce::String (id)))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost (q->convertTo0to1 (v));
            q->endChangeGesture();
        }
    };
    for (int k = 1; k <= mod::numSlots; ++k)
    {
        const auto r = p + std::to_string (k) + ".";
        if (std::lround (plainOf (proc, r + "src")) != mod::SrcOff || std::lround (plainOf (proc, r + "dst")) != mod::DstOff) continue;
        setPlain (r + "dst", (float) dst);
        setPlain (r + "amt", amount);
        setPlain (r + "src", (float) source);
        return k - 1;
    }
    return -1;
}

juce::String sourceDragDescription (int source) { return "modsrc:" + juce::String (source); }

int sourceFromDragDescription (const juce::var& description)
{
    const auto s = description.toString();
    if (! s.startsWith ("modsrc:")) return -1;
    const int v = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
    return v > mod::SrcOff && v < mod::numSources ? v : -1;
}

// ---------------------------------------------------------------- Tile
Tile::Tile (OhpeProcessor& p, int partIndex, std::vector<std::string> paramIdsIn)
    : proc (p), part (juce::jlimit (0, Core::numParts - 1, partIndex)), ids (std::move (paramIdsIn))
{
    setRepaintsOnMouseActivity (false);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

uint64_t Tile::mix (uint64_t h, float v) noexcept
{
    const auto q = (int64_t) std::lround ((double) v * 2000.0);
    h ^= (uint64_t) q + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}

uint64_t Tile::stateHash() const
{
    uint64_t h = 1469598103934665603ull;
    for (const auto& id : ids)
    {
        h = mix (h, norm (id));
        h = mix (h, mod (id));
    }
    h = mix (h, dropHover ? 1.0f : 0.0f);
    return h ^ rangeHash;
}

bool Tile::tick (float seconds)
{
    time += seconds;
    if (! isShowing() && ! first) return false;
    if (++ticks % 10 == 1) // the modulation ranges change only with the routings: look every third of a second
    {
        uint64_t r = 0x51ED27ull;
        for (const auto& id : ids)
            if (! id.empty())
            {
                const auto rg = range (id);
                r = mix (mix (r, rg.getStart()), rg.getEnd());
            }
        rangeHash = r;
    }
    const auto h = stateHash();
    if (first || h != lastHash || animating())
    {
        first = false;
        lastHash = h;
        repaint();
        return true;
    }
    return false;
}

float Tile::norm (const std::string& id) const
{
    if (auto* q = proc.parameter (juce::String (id))) return q->getValue();
    return 0.0f;
}

float Tile::mod (const std::string& id) const
{
    const int idx = proc.core().params().indexOf (id);
    return idx >= 0 ? proc.core().part (part).modulationOf (idx) : 0.0f;
}

bool Tile::has (const std::string& id) const { return proc.parameter (juce::String (id)) != nullptr; }

juce::String Tile::choiceText (const std::string& id) const
{
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id);
    if (idx < 0) return {};
    const auto& d = reg.descriptor (idx);
    const int c = (int) std::lround (plainOf (proc, id));
    return c >= 0 && c < (int) d.choices.size() ? juce::String (d.choices[(size_t) c]) : juce::String();
}

juce::String Tile::valueText (const std::string& id) const
{
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id);
    return idx >= 0 ? formatValue (reg.descriptor (idx), plainOf (proc, id)) : juce::String();
}

void Tile::begin (const std::string& id)
{
    if (std::find (open.begin(), open.end(), id) != open.end()) return;
    if (auto* q = proc.parameter (juce::String (id))) { q->beginChangeGesture(); open.push_back (id); }
}

void Tile::set (const std::string& id, float v)
{
    begin (id);
    if (auto* q = proc.parameter (juce::String (id))) q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}

void Tile::end (const std::string& id)
{
    const auto it = std::find (open.begin(), open.end(), id);
    if (it == open.end()) return;
    if (auto* q = proc.parameter (juce::String (id))) q->endChangeGesture();
    open.erase (it);
}

void Tile::endAll()
{
    const bool any = ! open.empty();
    while (! open.empty()) end (open.back());
    if (any && onEdited) onEdited();
}

void Tile::nudge (const std::string& id, float delta)
{
    if (auto* q = proc.parameter (juce::String (id)))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q->getValue() + delta));
        q->endChangeGesture();
        if (onEdited) onEdited();
    }
}

void Tile::reset (const std::string& id)
{
    if (auto* q = proc.parameter (juce::String (id)))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (q->getDefaultValue());
        q->endChangeGesture();
        if (onEdited) onEdited();
    }
}

void Tile::stepChoice (const std::string& id, int delta)
{
    const auto& reg = proc.core().params();
    const int idx = reg.indexOf (id);
    if (idx < 0) return;
    const auto& d = reg.descriptor (idx);
    const int n = (int) std::lround (d.maxValue - d.minValue) + 1;
    if (n < 2) return;
    int c = (int) std::lround (plainOf (proc, id) - d.minValue) + delta;
    c = ((c % n) + n) % n;
    if (auto* q = proc.parameter (juce::String (id)))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (q->convertTo0to1 (d.minValue + (float) c));
        q->endChangeGesture();
        if (onEdited) onEdited();
    }
}

void Tile::drawModBar (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, const std::string& id) const
{
    const auto r = range (id);
    const float live = mod (id);
    if (r.isEmpty() && live == 0.0f) return;
    const float v = norm (id);
    auto at = [&] (float n) { return a + (b - a) * juce::jlimit (0.0f, 1.0f, n); };
    g.setColour (modc().withAlpha (0.55f));
    g.drawLine ({ at (v + r.getStart()), at (v + r.getEnd()) }, unit() * 0.55f);
    if (live != 0.0f)
    {
        const auto p = at (v + live);
        g.setColour (modc().brighter (0.4f));
        g.fillEllipse (juce::Rectangle<float> (unit() * 0.7f, unit() * 0.7f).withCentre (p));
    }
}

juce::String Tile::getTooltip() { return hoverText (proc, paramAt (lastMouse)); }

std::string Tile::paramAt (juce::Point<float>) const { return ids.empty() ? std::string() : ids.front(); }

bool Tile::isInterestedInDragSource (const SourceDetails& d) { return sourceFromDragDescription (d.description) >= 0; }

void Tile::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = sourceFromDragDescription (d.description);
    if (src >= 0)
    {
        auto target = paramAt (d.localPosition.toFloat());
        if (destForParam (proc, part, target) < 0)
            for (const auto& id : ids)
                if (destForParam (proc, part, id) >= 0) { target = id; break; }
        if (addRouting (proc, part, src, target) >= 0 && onEdited) onEdited();
    }
    repaint();
}

void Tile::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (colour::display);
    g.fillRoundedRectangle (b, radius::inset);
    g.saveState();
    g.reduceClipRegion (getLocalBounds());
    paintTile (g, b.reduced (unit() * 0.6f));
    g.restoreState();
    if (dropHover)
    {
        g.setColour (modc());
        g.drawRoundedRectangle (b.reduced (1.0f), radius::inset, 2.0f);
    }
}

// ================================================================= tiles
namespace
{
/** Small caption in a tile corner. */
void caption (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Justification j, float u,
              juce::Colour c = colour::displayDim)
{
    g.setColour (c);
    g.setFont (monoFont (u * 1.25f));
    g.drawText (text, area.reduced (u * 0.3f), j, false);
}

// ---------------------------------------------------------------- envelope (ADSR, 4 handles, playhead)
class EnvelopeTile final : public Tile
{
public:
    /** ids: attack, decay, sustain, release; stageSource 0 = amp, 1 = mod, -1 = none (env 3). */
    EnvelopeTile (OhpeProcessor& p, int prt, std::vector<std::string> abcd, int stageSrc)
        : Tile (p, prt, std::move (abcd)), stageSource (stageSrc) {}

    bool animating() const override { return stageSource >= 0 && stage() != 0; }

    std::string paramAt (juce::Point<float> p) const override
    {
        const int h = nearestHandle (p);
        static const int map[4] = { 0, 1, 2, 3 };
        return h >= 0 ? ids[(size_t) map[h]] : ids[0];
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        handle = nearestHandle (e.position);
        anchor = e.position;
        startA = normAt (0); startD = normAt (1); startS = normAt (2); startR = normAt (3);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (handle < 0) return;
        const auto L = layout();
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        const float dx = (e.position.x - anchor.x) / L.seg * k, dy = -(e.position.y - anchor.y) / L.h * k;
        switch (handle)
        {
            case 0: set (ids[0], startA + dx); break;
            case 1: set (ids[1], startD + dx); set (ids[2], startS + dy); break;
            case 2: set (ids[2], startS + dy); break;
            case 3: set (ids[3], startR + dx); break;
            default: break;
        }
    }
    void mouseUp (const juce::MouseEvent&) override { handle = -1; endAll(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = nearestHandle (e.position);
        if (h == 1) { reset (ids[1]); reset (ids[2]); }
        else if (h >= 0) reset (ids[(size_t) h]);
    }

protected:
    uint64_t stateHash() const override
    {
        auto h = Tile::stateHash();
        if (stageSource >= 0)
        {
            h = mix (h, (float) stage());
            h = mix (h, level());
        }
        return h;
    }

    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto L = layout();
        const auto pts = points (L);
        // grid
        g.setColour (colour::displayGrid.brighter (0.15f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (L.top + L.h * (float) i / 4.0f), area.getX(), area.getRight());
        // curve
        juce::Path curve;
        curve.startNewSubPath (pts[0]);
        curve.lineTo (pts[1]);
        const float s = normAt (2);
        for (int i = 1; i <= 24; ++i) // decay: exponential towards the sustain level
        {
            const float t = (float) i / 24.0f;
            const float v = s + (1.0f - s) * std::exp (-5.0f * t);
            curve.lineTo (pts[1].x + (pts[2].x - pts[1].x) * t, yOf (L, v));
        }
        curve.lineTo (pts[3]);
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            curve.lineTo (pts[3].x + (pts[4].x - pts[3].x) * t, yOf (L, s * std::exp (-5.0f * t) * (1.0f - t * 0.1f)));
        }
        juce::Path fill (curve);
        fill.lineTo (pts[4].x, L.top + L.h);
        fill.lineTo (pts[0].x, L.top + L.h);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (edit().withAlpha (0.38f), 0, L.top, edit().withAlpha (0.02f), 0, L.top + L.h, false));
        g.fillPath (fill);
        g.setColour (edit());
        g.strokePath (curve, juce::PathStrokeType (u * 0.32f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // modulation of the attack / decay / sustain / release handles (violet)
        drawModBar (g, { pts[0].x, L.top + L.h + u * 0.6f }, { pts[0].x + L.seg, L.top + L.h + u * 0.6f }, ids[0]);
        drawModBar (g, { pts[1].x, L.top + L.h + u * 0.6f }, { pts[1].x + L.seg, L.top + L.h + u * 0.6f }, ids[1]);
        drawModBar (g, { pts[3].x + u * 0.8f, L.top + L.h }, { pts[3].x + u * 0.8f, L.top }, ids[2]);
        drawModBar (g, { pts[3].x, L.top + L.h + u * 0.6f }, { pts[3].x + L.seg, L.top + L.h + u * 0.6f }, ids[3]);
        // handles
        for (int hdl = 0; hdl < 4; ++hdl)
        {
            const auto c = handlePoint (pts, hdl);
            const float r = u * (hdl == handle ? 0.95f : 0.75f);
            g.setColour (edit().withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (r * 3.2f, r * 3.2f).withCentre (c));
            g.setColour (edit());
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        }
        // playhead of the newest voice (green)
        if (stageSource >= 0 && stage() != 0)
        {
            const float lv = juce::jlimit (0.0f, 1.0f, level());
            juce::Point<float> c;
            switch (stage())
            {
                case 1: c = { pts[0].x + (pts[1].x - pts[0].x) * lv, yOf (L, lv) }; break;
                case 2:
                {
                    const float t = s < 0.999f ? juce::jlimit (0.0f, 1.0f, -std::log (juce::jmax (1.0e-4f, (lv - s) / (1.0f - s))) / 5.0f) : 1.0f;
                    c = { pts[1].x + (pts[2].x - pts[1].x) * t, yOf (L, lv) };
                    break;
                }
                case 3: c = { pts[2].x + (pts[3].x - pts[2].x) * 0.5f, yOf (L, lv) }; break;
                default:
                {
                    const float t = s > 1.0e-3f ? juce::jlimit (0.0f, 1.0f, -std::log (juce::jmax (1.0e-4f, lv / s)) / 5.0f) : 1.0f;
                    c = { pts[3].x + (pts[4].x - pts[3].x) * t, yOf (L, lv) };
                    break;
                }
            }
            g.setColour (play().withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (u * 2.4f, u * 2.4f).withCentre (c));
            g.setColour (play());
            g.fillEllipse (juce::Rectangle<float> (u * 1.2f, u * 1.2f).withCentre (c));
        }
        // values
        caption (g, area.withHeight (u * 2.0f), valueText (ids[0]) + "  " + valueText (ids[1]) + "  " + valueText (ids[2]) + "  " + valueText (ids[3]),
                 juce::Justification::topRight, u);
    }

private:
    struct Layout { float left, top, w, h, seg; };
    Layout layout() const
    {
        const float u = unit();
        const auto a = getLocalBounds().toFloat().reduced (u * 1.2f);
        Layout L;
        L.left = a.getX();
        L.top = a.getY() + u * 1.8f;
        L.w = a.getWidth();
        L.h = a.getHeight() - u * 3.0f;
        L.seg = L.w * 0.27f;
        return L;
    }
    float yOf (const Layout& L, float v) const { return L.top + L.h * (1.0f - v); }
    std::array<juce::Point<float>, 5> points (const Layout& L) const
    {
        const float a = 0.04f + 0.96f * normAt (0), d = 0.04f + 0.96f * normAt (1), r = 0.04f + 0.96f * normAt (3), s = normAt (2);
        std::array<juce::Point<float>, 5> p;
        p[0] = { L.left, yOf (L, 0.0f) };
        p[1] = { L.left + L.seg * a, yOf (L, 1.0f) };
        p[2] = { p[1].x + L.seg * d, yOf (L, s) };
        p[3] = { p[2].x + L.w * 0.15f, yOf (L, s) };
        p[4] = { p[3].x + L.seg * r, yOf (L, 0.0f) };
        return p;
    }
    static juce::Point<float> handlePoint (const std::array<juce::Point<float>, 5>& p, int h)
    {
        switch (h)
        {
            case 0: return p[1];
            case 1: return p[2];
            case 2: return p[3];
            default: return p[4];
        }
    }
    int nearestHandle (juce::Point<float> pos) const
    {
        const auto pts = points (layout());
        int best = -1;
        float bd = unit() * 5.0f;
        for (int h = 0; h < 4; ++h)
            if (const float d = handlePoint (pts, h).getDistanceFrom (pos); d < bd) { bd = d; best = h; }
        if (best < 0) // no handle close: the segment under the mouse
        {
            if (pos.x < pts[1].x) best = 0;
            else if (pos.x < pts[2].x) best = 1;
            else if (pos.x < pts[3].x) best = 2;
            else best = 3;
        }
        return best;
    }
    int stage() const { return proc.core().part (part).modMatrix().uiEnvStage (stageSource); }
    float level() const
    {
        return proc.core().part (part).modMatrix().uiSourceValue (stageSource == 0 ? mod::SrcAmpEnv : mod::SrcModEnv);
    }

    int stageSource;
    int handle = -1;
    juce::Point<float> anchor;
    float startA = 0, startD = 0, startS = 0, startR = 0;
};

// ---------------------------------------------------------------- filter response (X = cutoff, Y = reso, wheel = drive)
class FilterTile final : public Tile
{
public:
    FilterTile (OhpeProcessor& p, int prt, std::string cutoff, std::string reso, std::string drive, std::string type)
        : Tile (p, prt, { cutoff, reso, drive, type }) {}

    std::string paramAt (juce::Point<float> p) const override
    {
        const auto b = getLocalBounds().toFloat();
        return p.y < b.getHeight() * 0.35f && has (ids[1]) ? ids[1] : ids[0];
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        anchor = e.position;
        c0 = normAt (0);
        r0 = normAt (1);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        const auto b = getLocalBounds().toFloat();
        set (ids[0], c0 + (e.position.x - anchor.x) / b.getWidth() * k);
        if (has (ids[1])) set (ids[1], r0 - (e.position.y - anchor.y) / b.getHeight() * k);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { reset (ids[0]); if (has (ids[1])) reset (ids[1]); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (has (ids[2])) nudge (ids[2], w.deltaY * 0.15f);
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto plot = area.withTrimmedTop (u * 1.6f).withTrimmedBottom (u * 0.4f);
        const float cut = normAt (0), res = has (ids[1]) ? normAt (1) : 0.0f, drv = has (ids[2]) ? normAt (2) : 0.0f;
        const auto type = choiceText (ids[3]);
        const int mode = type.contains ("HP") ? 2 : (type.contains ("BP") ? 1 : 0);
        // grid: octaves
        g.setColour (colour::displayGrid.brighter (0.12f));
        for (int i = 1; i < 10; ++i) g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 10.0f), plot.getY(), plot.getBottom());
        // modulation shadow: the curves at the edges of the range, the space between them filled violet
        const auto r = range (ids[0]);
        const float live = mod (ids[0]);
        if (! r.isEmpty())
        {
            auto lo = curve (plot, juce::jlimit (0.0f, 1.0f, cut + r.getStart()), res, drv, mode);
            auto hi = curve (plot, juce::jlimit (0.0f, 1.0f, cut + r.getEnd()), res, drv, mode);
            juce::Path band;
            band.addPath (hi);
            band.lineTo (plot.getRight(), plot.getBottom());
            band.lineTo (plot.getX(), plot.getBottom());
            band.closeSubPath();
            g.setGradientFill (juce::ColourGradient (modc().withAlpha (0.55f), 0, plot.getY(), modc().withAlpha (0.12f), 0, plot.getBottom(), false));
            g.fillPath (band);
            g.setColour (modc().withAlpha (0.7f));
            g.strokePath (lo, juce::PathStrokeType (u * 0.18f));
            g.strokePath (hi, juce::PathStrokeType (u * 0.18f));
        }
        const auto main = curve (plot, cut, res, drv, mode);
        juce::Path fill (main);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (edit().withAlpha (0.22f + 0.2f * drv), 0, plot.getY(), edit().withAlpha (0.0f), 0, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (edit());
        g.strokePath (main, juce::PathStrokeType (u * (0.3f + 0.25f * drv), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (live != 0.0f)
        {
            g.setColour (modc().brighter (0.5f));
            g.strokePath (curve (plot, juce::jlimit (0.0f, 1.0f, cut + live), res, drv, mode), juce::PathStrokeType (u * 0.14f));
        }
        // the handle at the cutoff (peak)
        const juce::Point<float> hp (plot.getX() + plot.getWidth() * cut, yOf (plot, gainAt (cut, cut, res, mode) * (1.0f + 0.25f * drv)));
        g.setColour (edit().withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (u * 2.6f, u * 2.6f).withCentre (hp));
        g.setColour (edit());
        g.fillEllipse (juce::Rectangle<float> (u * 1.5f, u * 1.5f).withCentre (hp));
        caption (g, area.withHeight (u * 1.8f), valueText (ids[0]) + (has (ids[1]) ? "  Q " + valueText (ids[1]) : juce::String()), juce::Justification::topRight, u);
        if (type.isNotEmpty()) caption (g, area.withHeight (u * 1.8f), type.toUpperCase(), juce::Justification::topLeft, u);
    }

private:
    static float gainAt (float x, float cut, float res, int mode)
    {
        // a 2-pole response on a log axis (x and cut 0..1 = 10 octaves)
        const float r = std::pow (2.0f, (x - cut) * 10.0f);
        const float q = 0.6f + res * 7.0f;
        const float den = std::sqrt ((1.0f - r * r) * (1.0f - r * r) + (r / q) * (r / q));
        float m = 1.0f / juce::jmax (1.0e-3f, den);
        if (mode == 2) m *= r * r;
        else if (mode == 1) m *= r / q;
        return std::sqrt (juce::jmin (m, 8.0f)) * 0.55f;
    }
    static float yOf (juce::Rectangle<float> plot, float g) { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, g); }
    static juce::Path curve (juce::Rectangle<float> plot, float cut, float res, float drv, int mode)
    {
        juce::Path p;
        for (int i = 0; i <= 64; ++i)
        {
            const float x = (float) i / 64.0f;
            const float y = yOf (plot, gainAt (x, cut, res, mode) * (1.0f + 0.25f * drv));
            if (i == 0) p.startNewSubPath (plot.getX(), y);
            else p.lineTo (plot.getX() + plot.getWidth() * x, y);
        }
        return p;
    }
    juce::Point<float> anchor;
    float c0 = 0, r0 = 0;
};

// ---------------------------------------------------------------- LFO wave on a tempo grid (height = depth, swipe = shape)
class LfoTile final : public Tile
{
public:
    LfoTile (OhpeProcessor& p, int prt, std::string rate, std::string depth, std::string shape, int source)
        : Tile (p, prt, { rate, depth, shape }), src (source) {}

    bool animating() const override { return normAt (1) > 0.001f || proc.core().part (part).activeVoiceCount() > 0; }
    std::string paramAt (juce::Point<float> p) const override
    {
        return p.y < (float) getHeight() * 0.3f ? ids[1] : ids[0];
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        anchor = e.position;
        rate0 = normAt (0);
        depth0 = normAt (1);
        downTime = juce::Time::getMillisecondCounterHiRes();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        const auto b = getLocalBounds().toFloat();
        set (ids[1], depth0 - (e.position.y - anchor.y) / b.getHeight() * k);
        set (ids[0], rate0 + (e.position.x - anchor.x) / b.getWidth() * 0.6f * k);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const double ms = juce::Time::getMillisecondCounterHiRes() - downTime;
        const float dx = e.position.x - anchor.x, dy = e.position.y - anchor.y;
        if (ms < 260.0 && std::abs (dx) > (float) getWidth() * 0.25f && std::abs (dy) < std::abs (dx) * 0.5f)
        {
            set (ids[0], rate0); // a swipe changes the shape, not the rate
            set (ids[1], depth0);
            endAll();
            stepChoice (ids[2], dx > 0.0f ? 1 : -1);
            return;
        }
        endAll();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { reset (ids[0]); reset (ids[1]); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f) stepChoice (ids[2], w.deltaY > 0.0f ? 1 : -1);
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto plot = area.withTrimmedTop (u * 1.8f).reduced (0.0f, u * 0.3f);
        // tempo grid: quarter notes at 120 BPM over two seconds
        g.setColour (colour::displayGrid.brighter (0.15f));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
        const float hz = juce::jmax (0.01f, plainOf (proc, ids[0]));
        const float depth = normAt (1);
        const auto shape = choiceText (ids[2]);
        phase = std::fmod (time * hz, 1.0f);
        const float cycles = juce::jlimit (0.5f, 16.0f, hz * 2.0f); // two seconds across the tile
        juce::Path wave;
        const float amp = plot.getHeight() * 0.48f * juce::jmax (0.06f, depth);
        for (int i = 0; i <= 96; ++i)
        {
            const float x = (float) i / 96.0f;
            const float ph = std::fmod (x * cycles, 1.0f);
            const float y = plot.getCentreY() - amp * shapeAt (shape, ph, (int) (x * cycles));
            if (i == 0) wave.startNewSubPath (plot.getX(), y);
            else wave.lineTo (plot.getX() + plot.getWidth() * x, y);
        }
        const auto c = depth > 0.001f ? play() : colour::displayDim;
        g.setColour (c);
        g.strokePath (wave, juce::PathStrokeType (u * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // where the LFO is now (the part-wide value of the source; the phase drives the dot's x)
        const float now = src >= 0 ? proc.core().part (part).modMatrix().uiSourceValue (src) : shapeAt (shape, phase, 0);
        const juce::Point<float> dot (plot.getX() + plot.getWidth() * phase / juce::jmax (0.5f, cycles), plot.getCentreY() - amp * juce::jlimit (-1.0f, 1.0f, now));
        g.setColour (c.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (u * 2.2f, u * 2.2f).withCentre (dot));
        g.setColour (c);
        g.fillEllipse (juce::Rectangle<float> (u * 1.1f, u * 1.1f).withCentre (dot));
        drawModBar (g, { plot.getRight() - u * 0.6f, plot.getBottom() }, { plot.getRight() - u * 0.6f, plot.getY() }, ids[1]);
        drawModBar (g, { plot.getX(), plot.getBottom() + u * 0.2f }, { plot.getRight(), plot.getBottom() + u * 0.2f }, ids[0]);
        caption (g, area.withHeight (u * 1.8f), valueText (ids[0]), juce::Justification::topRight, u, play());
        caption (g, area.withHeight (u * 1.8f), shape.toUpperCase() + "  " + valueText (ids[1]), juce::Justification::topLeft, u);
    }

private:
    static float shapeAt (const juce::String& s, float ph, int cycle)
    {
        if (s.startsWithIgnoreCase ("Tri")) return 1.0f - 4.0f * std::abs (ph - 0.5f);
        if (s.startsWithIgnoreCase ("Saw") || s.startsWithIgnoreCase ("Ramp")) return 1.0f - 2.0f * ph;
        if (s.startsWithIgnoreCase ("Squ") || s.startsWithIgnoreCase ("Pulse")) return ph < 0.5f ? 1.0f : -1.0f;
        if (s.startsWithIgnoreCase ("S&H") || s.containsIgnoreCase ("Random") || s.containsIgnoreCase ("Zufall"))
        {
            uint32_t h = (uint32_t) (cycle * 2 + (ph < 0.5f ? 0 : 1) + 1) * 2654435761u;
            h ^= h >> 13;
            return (float) (h % 2001u) / 1000.0f - 1.0f;
        }
        return std::sin (ph * twoPi);
    }
    int src;
    float phase = 0.0f;
    juce::Point<float> anchor;
    float rate0 = 0, depth0 = 0;
    double downTime = 0.0;
};

// ---------------------------------------------------------------- FM operator graph
class FmTile final : public Tile
{
public:
    FmTile (OhpeProcessor& p, int prt, const std::string& pre) : Tile (p, prt, makeIds (pre)) {}

    std::string paramAt (juce::Point<float> p) const override
    {
        const int op = opAt (p);
        if (op >= 0) return ids[(size_t) (4 + op)]; // ratio
        return badge().contains (p) ? ids[0] : ids[2];
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        if (badge().contains (e.position)) { stepChoiceContinuous(); return; }
        dragOp = opAt (e.position);
        anchor = e.position;
        start = norm (paramAt (e.position));
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        const std::string id = dragOp >= 0 ? ids[(size_t) (4 + dragOp)] : ids[2];
        set (id, start - (e.position.y - anchor.y) / (float) getHeight() * 0.8f * k);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { reset (paramAt (e.position)); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int op = opAt (e.position);
        nudge (op >= 0 ? ids[(size_t) (10 + op)] : ids[3], w.deltaY * 0.1f); // a circle: its level; else feedback
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto algo = fm::morphAlgorithm (plainOf (proc, ids[0]));
        const float index = normAt (2);
        const auto pos = positions();
        const float rad = radiusPx();
        // modulation lines: thickness = connection x index x modulator level
        for (int dst = 0; dst < fm::numOps; ++dst)
            for (int src = 0; src < fm::numOps; ++src)
            {
                const float m = algo.mod[dst][src];
                if (m <= 0.001f) continue;
                const float lv = normAt ((size_t) (10 + src));
                const float w = u * (0.15f + 1.4f * m * index * (0.3f + 0.7f * lv));
                g.setColour (edit().withAlpha (0.35f + 0.6f * juce::jmin (1.0f, m * (0.3f + index))));
                if (src == dst) continue;
                g.drawLine ({ pos[(size_t) src], pos[(size_t) dst] }, w);
                drawModBar (g, pos[(size_t) src], pos[(size_t) dst], ids[2]);
            }
        // p28: no extra ring for the feedback (it floated as a stray faint circle over the first operator row);
        // the Feedback bar below the graph shows it
        // operators: carriers filled, modulators as rings; ratio inside, level as an arc
        for (int op = 0; op < fm::numOps; ++op)
        {
            const auto c = pos[(size_t) op];
            const bool carrier = algo.out[op] > 0.01f;
            const auto r = juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c);
            g.setColour (colour::display);
            g.fillEllipse (r);
            if (carrier)
            {
                g.setColour (edit().withAlpha (0.22f));
                g.fillEllipse (r);
            }
            g.setColour (edit());
            g.drawEllipse (r, u * 0.25f);
            juce::Path arc;
            const float lv = normAt ((size_t) (10 + op));
            arc.addCentredArc (c.x, c.y, rad + u * 0.55f, rad + u * 0.55f, 0.0f, -twoPi * 0.4f, -twoPi * 0.4f + twoPi * 0.8f * lv, true);
            g.setColour (edit().withAlpha (0.8f));
            g.strokePath (arc, juce::PathStrokeType (u * 0.22f));
            drawModBar (g, c.translated (-rad, rad + u * 1.0f), c.translated (rad, rad + u * 1.0f), ids[(size_t) (4 + op)]);
            g.setColour (colour::displayText);
            g.setFont (labelFont (rad * 0.8f));
            g.drawText (juce::String (op + 1), r.withTrimmedBottom (rad * 0.8f).translated (0.0f, rad * 0.25f), juce::Justification::centred, false);
            g.setColour (edit());
            g.setFont (monoFont (rad * 0.62f));
            g.drawText (juce::String (plainOf (proc, ids[(size_t) (4 + op)]), 2), r.withTrimmedTop (rad * 0.95f).translated (0.0f, -rad * 0.2f),
                        juce::Justification::centred, false);
        }
        // algorithm badge + index
        const auto bd = badge();
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.drawRoundedRectangle (bd, u * 0.5f, 1.0f);
        caption (g, bd, tr ("Algorithm").toUpperCase() + " " + juce::String ((int) std::floor (plainOf (proc, ids[0])) + 1), juce::Justification::centred, u,
                 colour::displayText);
        caption (g, area.withHeight (u * 1.8f), "INDEX " + valueText (ids[2]), juce::Justification::topRight, u);
    }

private:
    static std::vector<std::string> makeIds (const std::string& pre)
    {
        std::vector<std::string> v { pre + "algo", pre + "modratio", pre + "index", pre + "feedback" };
        for (int i = 1; i <= 6; ++i) v.push_back (pre + "op" + std::to_string (i) + "ratio");
        for (int i = 1; i <= 6; ++i) v.push_back (pre + "op" + std::to_string (i) + "level");
        return v;
    }
    void stepChoiceContinuous()
    {
        float a = std::floor (plainOf (proc, ids[0])) + 1.0f;
        if (a > (float) (fm::numAlgorithms - 1)) a = 0.0f;
        if (auto* q = proc.parameter (juce::String (ids[0])))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost (q->convertTo0to1 (a));
            q->endChangeGesture();
            if (onEdited) onEdited();
        }
    }
    juce::Rectangle<float> badge() const
    {
        const float u = unit();
        return { u * 0.8f, u * 0.5f, u * 11.0f, u * 2.0f };
    }
    float radiusPx() const { return juce::jlimit (8.0f, 30.0f, juce::jmin ((float) getWidth() / 10.0f, (float) getHeight() / 6.2f)); }
    /** Operators on two rows of three slots: carriers first (bottom row, left to right), then their modulators by
        depth, snaking back along the top row - a chain reads as a line, nothing overlaps. */
    std::array<juce::Point<float>, fm::numOps> positions() const
    {
        const auto algo = fm::morphAlgorithm (plainOf (proc, ids[0]));
        std::array<int, fm::numOps> depth {};
        for (int op = 0; op < fm::numOps; ++op) depth[(size_t) op] = algo.out[op] > 0.01f ? 0 : -1;
        for (int pass = 0; pass < fm::numOps; ++pass)
            for (int dst = 0; dst < fm::numOps; ++dst)
                for (int src = 0; src < fm::numOps; ++src)
                    if (src != dst && algo.mod[dst][src] > 0.001f && depth[(size_t) dst] >= 0)
                        depth[(size_t) src] = juce::jmax (depth[(size_t) src], depth[(size_t) dst] + 1);
        std::array<int, fm::numOps> order { 0, 1, 2, 3, 4, 5 };
        std::stable_sort (order.begin(), order.end(), [&] (int x, int y) {
            const int dx = depth[(size_t) x] < 0 ? 99 : depth[(size_t) x], dy = depth[(size_t) y] < 0 ? 99 : depth[(size_t) y];
            return dx < dy;
        });
        const float u = unit();
        const auto area = getLocalBounds().toFloat().reduced (radiusPx() * 1.25f + u * 0.6f).withTrimmedTop (u * 2.4f);
        std::array<juce::Point<float>, fm::numOps> pos {};
        for (int slot = 0; slot < fm::numOps; ++slot)
        {
            const int row = slot / 3, col = row == 0 ? slot % 3 : 2 - slot % 3;
            const float x = area.getX() + area.getWidth() * (float) col / 2.0f;
            const float y = row == 0 ? area.getBottom() : area.getY();
            pos[(size_t) order[(size_t) slot]] = { x, y };
        }
        return pos;
    }
    int opAt (juce::Point<float> p) const
    {
        const auto pos = positions();
        for (int op = 0; op < fm::numOps; ++op)
            if (pos[(size_t) op].getDistanceFrom (p) < radiusPx() * 1.15f) return op;
        return -1;
    }
    int dragOp = -1;
    juce::Point<float> anchor;
    float start = 0.0f;
};

// ---------------------------------------------------------------- wavetable: the frame stack with the glowing position plane
class WavetableTile final : public Tile
{
public:
    WavetableTile (OhpeProcessor& p, int prt, const std::string& pre)
        : Tile (p, prt, { pre + "pos", pre + "warp", pre + "table", pre + "tone" }) {}

    std::string paramAt (juce::Point<float> p) const override { return p.x > (float) getWidth() * 0.75f ? ids[1] : ids[0]; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        anchor = e.position;
        p0 = normAt (0);
        w0 = normAt (1);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        set (ids[0], p0 - (e.position.y - anchor.y) / (float) getHeight() * k);
        set (ids[1], w0 + (e.position.x - anchor.x) / (float) getWidth() * k);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { reset (ids[0]); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f) stepChoice (ids[2], w.deltaY > 0.0f ? 1 : -1);
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto& bank = wavetable::Bank::get();
        const int table = juce::jlimit (0, wavetable::numTables - 1, (int) std::lround (plainOf (proc, ids[2])));
        const float pos = normAt (0) + mod (ids[0]);
        const auto r = range (ids[0]);
        const int frames = wavetable::Bank::numFrames;
        const auto plot = area.withTrimmedTop (u * 1.8f);
        const float dx = plot.getWidth() * 0.26f / (float) frames, dy = plot.getHeight() * 0.62f / (float) frames;
        const float fw = plot.getWidth() * 0.66f, fh = plot.getHeight() * 0.30f;
        auto frameOrigin = [&] (float f) { return juce::Point<float> (plot.getX() + dx * f, plot.getBottom() - fh * 0.5f - dy * f); };
        const float posF = juce::jlimit (0.0f, 1.0f, pos) * (float) (frames - 1);
        // back to front
        for (int f = frames - 1; f >= 0; --f)
        {
            const float* data = bank.frame (table, f, 3);
            const auto o = frameOrigin ((float) f);
            juce::Path p;
            for (int i = 0; i <= 64; ++i)
            {
                const float v = juce::jlimit (-1.0f, 1.0f, data[(size_t) (i * (wavetable::Bank::size / 64)) % wavetable::Bank::size]);
                const float x = o.x + fw * (float) i / 64.0f, y = o.y - v * fh * 0.5f;
                if (i == 0) p.startNewSubPath (x, y);
                else p.lineTo (x, y);
            }
            const float fn = (float) f / (float) (frames - 1);
            const bool inRange = ! r.isEmpty() && fn >= normAt (0) + r.getStart() - 0.01f && fn <= normAt (0) + r.getEnd() + 0.01f;
            const float near = 1.0f - juce::jmin (1.0f, std::abs ((float) f - posF));
            g.setColour (inRange ? modc().withAlpha (0.75f) : colour::displayDim.withAlpha (0.55f));
            g.strokePath (p, juce::PathStrokeType (u * 0.14f));
            if (near > 0.0f)
            {
                g.setColour (edit().withAlpha (near));
                g.strokePath (p, juce::PathStrokeType (u * (0.18f + 0.18f * near)));
            }
        }
        // the glowing position plane
        const auto o = frameOrigin (posF);
        juce::Path plane;
        plane.startNewSubPath (o.x - u * 0.3f, o.y - fh * 0.58f);
        plane.lineTo (o.x + fw + u * 0.3f, o.y - fh * 0.58f);
        plane.lineTo (o.x + fw + u * 0.3f, o.y + fh * 0.58f);
        plane.lineTo (o.x - u * 0.3f, o.y + fh * 0.58f);
        plane.closeSubPath();
        g.setColour (edit().withAlpha (0.10f));
        g.fillPath (plane);
        g.setColour (edit().withAlpha (0.5f));
        g.strokePath (plane, juce::PathStrokeType (u * 0.12f));
        drawModBar (g, frameOrigin (0.0f).translated (fw + u * 1.2f, 0.0f), frameOrigin ((float) (frames - 1)).translated (fw + u * 1.2f, 0.0f), ids[0]);
        caption (g, area.withHeight (u * 1.8f), choiceText (ids[2]).toUpperCase(), juce::Justification::topLeft, u);
        caption (g, area.withHeight (u * 1.8f), "POS " + valueText (ids[0]) + "  WARP " + valueText (ids[1]), juce::Justification::topRight, u);
    }

private:
    juce::Point<float> anchor;
    float p0 = 0, w0 = 0;
};

// ---------------------------------------------------------------- grain / sampler: waveform with the grain cloud
class WaveTile final : public Tile
{
public:
    /** grain: ids pos, size, spray, density; sampler: start, loopstart, loopend, tone */
    WaveTile (OhpeProcessor& p, int prt, std::vector<std::string> v, bool grainEngine) : Tile (p, prt, std::move (v)), grain (grainEngine) {}

    bool animating() const override { return grain && proc.core().part (part).activeVoiceCount() > 0; }
    std::string paramAt (juce::Point<float> p) const override
    {
        if (! grain)
        {
            const float x = p.x / (float) juce::jmax (1, getWidth());
            int best = 0;
            float bd = 2.0f;
            for (int i = 0; i < 3; ++i)
                if (const float d = std::abs (normAt ((size_t) i) - x); d < bd) { bd = d; best = i; }
            return ids[(size_t) best];
        }
        return p.y > (float) getHeight() * 0.7f ? ids[1] : ids[0];
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        anchor = e.position;
        target = paramAt (e.position);
        a0 = norm (grain ? ids[0] : target);
        b0 = normAt (1);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        const float dx = (e.position.x - anchor.x) / (float) getWidth() * k;
        if (grain)
        {
            set (ids[0], a0 + dx);
            set (ids[1], b0 - (e.position.y - anchor.y) / (float) getHeight() * k);
        }
        else set (target, a0 + dx);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { reset (paramAt (e.position)); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        nudge (grain ? ids[2] : ids[3], w.deltaY * 0.1f);
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto plot = area.withTrimmedTop (u * 1.8f);
        updatePeaks();
        const float mid = plot.getCentreY(), h = plot.getHeight() * 0.46f;
        g.setColour (colour::displayDim.withAlpha (0.8f));
        const float bw = plot.getWidth() / (float) peaks.size();
        for (size_t i = 0; i < peaks.size(); ++i)
            g.fillRect (plot.getX() + bw * (float) i, mid - h * peaks[i], juce::jmax (1.0f, bw - 1.0f), 2.0f * h * peaks[i] + 1.0f);
        if (grain)
        {
            const float pos = normAt (0), spray = normAt (2);
            const float sizeN = normAt (1);
            const float x = plot.getX() + plot.getWidth() * pos;
            const float sw = plot.getWidth() * (0.02f + 0.18f * sizeN);
            const float sp = plot.getWidth() * 0.3f * spray;
            g.setColour (edit().withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (x - sp, plot.getY(), 2.0f * sp + sw, plot.getHeight()));
            g.setColour (edit().withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (x, plot.getY(), sw, plot.getHeight()));
            g.setColour (edit());
            g.fillRect (juce::Rectangle<float> (x - 1.0f, plot.getY(), 2.0f, plot.getHeight()));
            drawModBar (g, { plot.getX(), plot.getBottom() - u * 0.4f }, { plot.getRight(), plot.getBottom() - u * 0.4f }, ids[0]);
            // the grain cloud: dots around the position (green while the part plays), deterministic per frame
            const bool sounding = proc.core().part (part).activeVoiceCount() > 0;
            const int n = 8 + (int) (normAt (3) * 30.0f);
            uint32_t s = 0x2545F491u + (uint32_t) (sounding ? (int) (time * 30.0f) : 0);
            for (int i = 0; i < n; ++i)
            {
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                const float rx = (float) (s % 1000u) / 1000.0f;
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                const float ry = (float) (s % 1000u) / 1000.0f;
                const float gx = x - sp + (2.0f * sp + sw) * rx, gy = plot.getY() + plot.getHeight() * (0.15f + 0.7f * ry);
                const float gr = u * (0.35f + 0.5f * sizeN);
                g.setColour ((sounding ? play() : edit()).withAlpha (0.75f));
                g.fillEllipse (juce::Rectangle<float> (gr * 2.0f, gr * 2.0f).withCentre ({ gx, gy }));
            }
            caption (g, area.withHeight (u * 1.8f), "POS " + valueText (ids[0]) + "  " + valueText (ids[1]) + "  SPRAY " + valueText (ids[2]),
                     juce::Justification::topRight, u);
        }
        else
        {
            const float st = normAt (0), ls = normAt (1), le = normAt (2);
            g.setColour (edit().withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (plot.getX() + plot.getWidth() * ls, plot.getY(), plot.getWidth() * juce::jmax (0.0f, le - ls), plot.getHeight()));
            for (int i = 0; i < 3; ++i)
            {
                const float x = plot.getX() + plot.getWidth() * normAt ((size_t) i);
                g.setColour (i == 0 ? edit() : edit().withAlpha (0.6f));
                g.fillRect (juce::Rectangle<float> (x - 1.0f, plot.getY(), 2.0f, plot.getHeight()));
                drawModBar (g, { plot.getX(), plot.getBottom() - u * (0.4f + 0.7f * (float) i) }, { plot.getRight(), plot.getBottom() - u * (0.4f + 0.7f * (float) i) }, ids[(size_t) i]);
            }
            g.setColour (edit());
            juce::Path tri;
            const float x = plot.getX() + plot.getWidth() * st;
            tri.addTriangle (x, plot.getY(), x + u * 1.2f, plot.getY(), x, plot.getY() + u * 1.2f);
            g.fillPath (tri);
            caption (g, area.withHeight (u * 1.8f), "START " + valueText (ids[0]) + "  LOOP " + valueText (ids[1]) + " - " + valueText (ids[2]),
                     juce::Justification::topRight, u);
        }
    }

private:
    void updatePeaks()
    {
        const IAudioRegion* region = nullptr;
        if (grain)
            if (auto* e = dynamic_cast<GrainEngine*> (proc.core().part (part).engine (EngineType::Grain))) region = e->getShared().activeSource();
        if (region == lastRegion && ! peaks.empty()) return;
        lastRegion = region;
        peaks.assign (72, 0.0f);
        if (region != nullptr && region->numFrames() > 0)
        {
            const auto n = region->numFrames();
            const float* d = region->channel (0);
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const auto a = (int64_t) ((double) n * (double) i / (double) peaks.size()), b = (int64_t) ((double) n * (double) (i + 1) / (double) peaks.size());
                float m = 0.0f;
                for (int64_t k = a; k < b; k += juce::jmax ((int64_t) 1, (b - a) / 64)) m = juce::jmax (m, std::abs (d[k]));
                peaks[i] = m;
            }
            float top = 1.0e-4f;
            for (float p : peaks) top = juce::jmax (top, p);
            for (auto& p : peaks) p = juce::jmin (1.0f, p / top);
        }
        else // a neutral texture (the built-in tone): a soft swell with a little grain
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float x = (float) i / (float) peaks.size();
                peaks[i] = (0.35f + 0.55f * std::sin (x * 3.14159f)) * (0.8f + 0.2f * std::sin ((float) i * 2.7f));
            }
    }
    bool grain;
    std::vector<float> peaks;
    const IAudioRegion* lastRegion = nullptr;
    juce::Point<float> anchor;
    std::string target;
    float a0 = 0, b0 = 0;
};

// ---------------------------------------------------------------- oscillator mix preview (analog / pluck / cluster) and the formant curve (vocal)
class OscTile final : public Tile
{
public:
    enum Kind { Analog, Pluck, Cluster, Vocal };
    OscTile (OhpeProcessor& p, int prt, std::vector<std::string> v, Kind k) : Tile (p, prt, std::move (v)), kind (k) {}

    std::string paramAt (juce::Point<float> p) const override { return p.y < (float) getHeight() * 0.35f ? ids[1] : ids[0]; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        anchor = e.position;
        x0 = normAt (0);
        y0 = normAt (1);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        set (ids[0], x0 + (e.position.x - anchor.x) / (float) getWidth() * k);
        set (ids[1], y0 - (e.position.y - anchor.y) / (float) getHeight() * k);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { reset (ids[0]); reset (ids[1]); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (ids.size() > 2) nudge (ids[2], w.deltaY * 0.1f);
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto plot = area.withTrimmedTop (u * 1.8f).reduced (0.0f, u * 0.3f);
        g.setColour (colour::displayGrid.brighter (0.15f));
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
        const float a = normAt (0), b = normAt (1), c = ids.size() > 2 ? normAt (2) : 0.0f, d = ids.size() > 3 ? normAt (3) : 0.0f;
        if (kind == Vocal)
        {
            // formant curve: three peaks moved by the vowel (a) and shifted by the formant (b)
            static const float table[5][3] = { { 0.32f, 0.45f, 0.72f }, { 0.22f, 0.62f, 0.78f }, { 0.12f, 0.70f, 0.82f },
                                               { 0.26f, 0.36f, 0.70f }, { 0.14f, 0.30f, 0.68f } };
            const float vp = a * 4.0f;
            const int i0 = juce::jlimit (0, 3, (int) vp);
            const float t = vp - (float) i0;
            const float shift = (b - 0.5f) * 0.2f;
            juce::Path p;
            for (int i = 0; i <= 96; ++i)
            {
                const float x = (float) i / 96.0f;
                float y = 0.04f + c * 0.08f;
                for (int f = 0; f < 3; ++f)
                {
                    const float fc = table[i0][f] + (table[i0 + 1][f] - table[i0][f]) * t + shift;
                    const float bw = 0.035f + 0.01f * (float) f;
                    y += (0.9f - 0.22f * (float) f) * std::exp (-(x - fc) * (x - fc) / (2.0f * bw * bw));
                }
                const float py = plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, y * 0.9f);
                if (i == 0) p.startNewSubPath (plot.getX(), py);
                else p.lineTo (plot.getX() + plot.getWidth() * x, py);
            }
            juce::Path fill (p);
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (edit().withAlpha (0.35f), 0, plot.getY(), edit().withAlpha (0.0f), 0, plot.getBottom(), false));
            g.fillPath (fill);
            g.setColour (edit());
            g.strokePath (p, juce::PathStrokeType (u * 0.3f));
            static const char* vowels = "AEIOU";
            caption (g, area.withHeight (u * 1.8f), juce::String::charToString ((juce::juce_wchar) vowels[juce::jlimit (0, 4, (int) std::lround (vp))]) + "  "
                         + valueText (ids[1]), juce::Justification::topRight, u);
        }
        else
        {
            const int voices = kind == Cluster ? 5 : 1;
            for (int vce = voices - 1; vce >= 0; --vce)
            {
                juce::Path p;
                const float det = kind == Cluster ? ((float) vce - (float) (voices - 1) * 0.5f) * (0.02f + 0.1f * a) : 0.0f;
                for (int i = 0; i <= 120; ++i)
                {
                    const float x = (float) i / 120.0f;
                    const float ph = std::fmod (x * 2.0f * (1.0f + det) + 10.0f, 1.0f);
                    float y = 0.0f;
                    if (kind == Analog)
                    {
                        const float saw = 1.0f - 2.0f * ph, sq = ph < 0.5f ? 0.8f : -0.8f, tri = 1.0f - 4.0f * std::abs (ph - 0.5f);
                        const float sh = a * 2.0f;
                        const float osc1 = sh < 1.0f ? tri + (saw - tri) * sh : saw + (sq - saw) * (sh - 1.0f);
                        const float osc2 = std::sin (std::fmod (x * 4.0f, 1.0f) * twoPi);
                        y = osc1 * (1.0f - 0.5f * d) + osc2 * 0.5f * d + c * 0.5f * std::sin (std::fmod (x, 1.0f) * twoPi);
                        // the filter (b = cutoff when present) rounds the edges
                        y *= 0.8f;
                    }
                    else if (kind == Pluck)
                    {
                        const float env = std::exp (-x * (1.0f + 6.0f * b));
                        y = env * (std::sin (ph * twoPi) + a * 0.5f * std::sin (ph * twoPi * 3.0f) + a * 0.25f * std::sin (ph * twoPi * 5.0f));
                    }
                    else
                        y = (1.0f - 2.0f * ph) * 0.7f;
                    const float py = plot.getCentreY() - plot.getHeight() * 0.45f * juce::jlimit (-1.0f, 1.0f, y);
                    if (i == 0) p.startNewSubPath (plot.getX(), py);
                    else p.lineTo (plot.getX() + plot.getWidth() * x, py);
                }
                g.setColour (vce == 0 ? edit() : edit().withAlpha (0.25f + 0.1f * (float) vce));
                g.strokePath (p, juce::PathStrokeType (u * (vce == 0 ? 0.3f : 0.16f)));
            }
            caption (g, area.withHeight (u * 1.8f), valueText (ids[0]) + "  " + valueText (ids[1]), juce::Justification::topRight, u);
        }
        drawModBar (g, { plot.getX(), plot.getBottom() }, { plot.getRight(), plot.getBottom() }, ids[0]);
        drawModBar (g, { plot.getRight(), plot.getBottom() }, { plot.getRight(), plot.getY() }, ids[1]);
    }

private:
    Kind kind;
    juce::Point<float> anchor;
    float x0 = 0, y0 = 0;
};

// ---------------------------------------------------------------- pitch & level: tuning ring + level meter
class PitchLevelTile final : public Tile
{
public:
    PitchLevelTile (OhpeProcessor& p, int prt, std::string tune, std::string fine, std::string vol) : Tile (p, prt, { tune, fine, vol }) {}

    bool animating() const override { return meter() > 0.0005f || shownMeter > 0.0005f; }
    std::string paramAt (juce::Point<float> p) const override { return p.x > (float) getWidth() * 0.7f ? ids[2] : ids[0]; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        target = paramAt (e.position);
        anchor = e.position;
        start = norm (target);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        set (target, start - (e.position.y - anchor.y) / (float) getHeight() * (target == ids[0] ? 0.5f : 1.0f) * k);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override { reset (paramAt (e.position)); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { nudge (ids[1], w.deltaY * 0.05f); }

protected:
    uint64_t stateHash() const override { return mix (Tile::stateHash(), shownMeter); }
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto ringArea = area.withTrimmedRight (area.getWidth() * 0.3f).withTrimmedTop (u * 1.2f);
        const float r = juce::jmin (ringArea.getWidth(), ringArea.getHeight()) * 0.38f;
        const auto c = ringArea.getCentre();
        const float a0 = -twoPi * 0.375f, a1 = twoPi * 0.375f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
        g.setColour (colour::displayGrid.brighter (0.35f));
        g.strokePath (track, juce::PathStrokeType (u * 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float tn = normAt (0), mid = (a0 + a1) * 0.5f, at = a0 + (a1 - a0) * tn;
        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (mid, at), juce::jmax (mid, at), true);
        g.setColour (edit());
        g.strokePath (val, juce::PathStrokeType (u * 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // fine tuning: a small tick on an inner ring
        const float fa = a0 + (a1 - a0) * normAt (1);
        g.setColour (edit().withAlpha (0.7f));
        g.drawLine ({ c.getPointOnCircumference (r * 0.62f, fa), c.getPointOnCircumference (r * 0.82f, fa) }, u * 0.3f);
        g.setColour (colour::displayText);
        g.setFont (monoFont (r * 0.42f));
        g.drawText (valueText (ids[0]), juce::Rectangle<float> (r * 2.0f, r * 0.6f).withCentre (c), juce::Justification::centred, false);
        g.setColour (colour::displayDim);
        g.setFont (monoFont (r * 0.26f));
        g.drawText (valueText (ids[1]), juce::Rectangle<float> (r * 2.0f, r * 0.4f).withCentre (c.translated (0.0f, r * 0.42f)), juce::Justification::centred, false);
        drawModBar (g, c.translated (-r, r + u * 0.8f), c.translated (r, r + u * 0.8f), ids[0]);
        // level: the fader position and the part's output meter (green)
        const auto m = area.withTrimmedLeft (area.getWidth() * 0.74f).withTrimmedTop (u * 1.6f).withTrimmedBottom (u * 0.4f);
        const float lw = m.getWidth() * 0.28f;
        const auto fader = juce::Rectangle<float> (lw, m.getHeight()).withX (m.getX());
        const auto meterR = juce::Rectangle<float> (lw, m.getHeight()).withX (m.getX() + lw * 1.6f);
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.fillRoundedRectangle (fader, lw * 0.3f);
        g.fillRoundedRectangle (meterR, lw * 0.3f);
        g.setColour (edit());
        g.fillRoundedRectangle (fader.withTrimmedTop (fader.getHeight() * (1.0f - normAt (2))), lw * 0.3f);
        shownMeter = juce::jmax (meter(), shownMeter * 0.82f);
        const float db = shownMeter > 1.0e-5f ? 20.0f * std::log10 (shownMeter) : -100.0f;
        const float mn = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (play());
        g.fillRoundedRectangle (meterR.withTrimmedTop (meterR.getHeight() * (1.0f - mn)), lw * 0.3f);
        drawModBar (g, { fader.getX() - u * 0.6f, fader.getBottom() }, { fader.getX() - u * 0.6f, fader.getY() }, ids[2]);
        caption (g, area.withHeight (u * 1.6f), valueText (ids[2]), juce::Justification::topRight, u);
    }

private:
    float meter() const { return proc.core().mixer().meter (part); }
    mutable float shownMeter = 0.0f;
    std::string target;
    juce::Point<float> anchor;
    float start = 0.0f;
};

// ---------------------------------------------------------------- FX slot: a mini visual of the effect type (chain of cards)
class FxTile final : public Tile
{
public:
    FxTile (OhpeProcessor& p, int prt, std::vector<std::string> v, int slotNumber, juce::String typeHint = {})
        : Tile (p, prt, std::move (v)), slot (slotNumber), hint (std::move (typeHint)) {}

    std::string paramAt (juce::Point<float> p) const override
    {
        if (ids.size() > 3 && (p.y < (float) getHeight() * 0.5f || ! has (ids[1]))) return ids[3];
        return ids.size() > 1 ? ids[1] : ids[0];
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        anchor = e.position;
        m0 = ids.size() > 1 ? normAt (1) : 0.0f;
        p0 = ids.size() > 3 ? normAt (3) : 0.0f;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float k = isFineDrag (e.mods) ? 0.1f : 1.0f;
        if (ids.size() > 1) set (ids[1], m0 + (e.position.x - anchor.x) / (float) getWidth() * k);
        if (ids.size() > 3) set (ids[3], p0 - (e.position.y - anchor.y) / (float) getHeight() * k);
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f && hint.isEmpty()) stepChoice (ids[0], w.deltaY > 0.0f ? 1 : -1);
    }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto type = hint.isNotEmpty() ? hint : choiceText (ids[0]);
        const bool off = type == "Off";
        const bool bypassed = ids.size() > 2 && normAt (2) >= 0.5f;
        const float mixv = ids.size() > 1 && has (ids[1]) ? normAt (1) : 1.0f;
        const float pv = ids.size() > 3 ? normAt (3) : 0.5f;
        const auto plot = area.withTrimmedTop (u * 1.8f).reduced (u * 0.4f, u * 0.3f);
        const auto c = off || bypassed ? colour::displayDim : edit();
        // chain arrows: in -> [effect] -> out
        g.setColour (colour::displayGrid.brighter (0.4f));
        g.drawArrow ({ plot.getX(), plot.getCentreY(), plot.getX() + u * 1.4f, plot.getCentreY() }, u * 0.15f, u * 0.6f, u * 0.6f);
        g.drawArrow ({ plot.getRight() - u * 1.4f, plot.getCentreY(), plot.getRight(), plot.getCentreY() }, u * 0.15f, u * 0.6f, u * 0.6f);
        const auto box = plot.reduced (u * 2.0f, 0.0f);
        juce::Path p;
        const auto t = type.toLowerCase();
        if (off)
        {
            g.setColour (colour::displayDim);
            g.drawLine ({ box.getX(), box.getCentreY(), box.getRight(), box.getCentreY() }, u * 0.2f);
        }
        else if (t.contains ("delay") || t.contains ("echo"))
        {
            for (int i = 0; i < 7; ++i)
            {
                const float x = box.getX() + box.getWidth() * (0.06f + 0.14f * (float) i);
                const float h = box.getHeight() * 0.9f * std::pow (0.35f + 0.6f * pv, (float) i);
                g.setColour (c.withAlpha (0.9f - 0.1f * (float) i));
                g.fillRoundedRectangle (juce::Rectangle<float> (u * 0.7f, h).withCentre ({ x, box.getCentreY() }), u * 0.3f);
            }
        }
        else if (t.contains ("reverb") || t.contains ("hall") || t.contains ("space") || t.contains ("send"))
        {
            for (int i = 0; i <= 80; ++i)
            {
                const float x = (float) i / 80.0f;
                const float env = std::exp (-x * (6.0f - 4.5f * pv)) * (0.6f + 0.4f * std::sin ((float) i * 1.7f) * std::sin ((float) i * 0.37f));
                const float y = box.getCentreY() - box.getHeight() * 0.45f * env;
                if (i == 0) p.startNewSubPath (box.getX(), y);
                else p.lineTo (box.getX() + box.getWidth() * x, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (u * 0.22f));
        }
        else if (t.contains ("distort") || t.contains ("crush") || t.contains ("drive") || t.contains ("compress") || t.contains ("multiband"))
        {
            // a transfer curve: input across, output up
            for (int i = 0; i <= 48; ++i)
            {
                const float x = (float) i / 48.0f * 2.0f - 1.0f;
                float y = std::tanh (x * (1.0f + 6.0f * pv)) / std::tanh (1.0f + 6.0f * pv);
                if (t.contains ("crush")) y = std::round (y * (2.0f + 10.0f * (1.0f - pv))) / (2.0f + 10.0f * (1.0f - pv));
                const float px = box.getCentreX() + box.getHeight() * 0.48f * x, py = box.getCentreY() - box.getHeight() * 0.45f * y;
                if (i == 0) p.startNewSubPath (px, py);
                else p.lineTo (px, py);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (u * 0.26f));
        }
        else if (t.contains ("filter") || t == "eq" || t.contains ("formant") || t.contains ("wobble") || t.contains ("comb"))
        {
            for (int i = 0; i <= 64; ++i)
            {
                const float x = (float) i / 64.0f;
                float y = 0.5f;
                if (t.contains ("comb")) y = 0.5f + 0.35f * std::cos (x * twoPi * (3.0f + 8.0f * pv));
                else y = 0.75f / (1.0f + std::pow (juce::jmax (0.0f, x - pv) * 10.0f, 2.0f)) + (std::abs (x - pv) < 0.06f ? 0.15f : 0.0f);
                const float py = box.getBottom() - box.getHeight() * juce::jlimit (0.0f, 1.0f, y);
                if (i == 0) p.startNewSubPath (box.getX(), py);
                else p.lineTo (box.getX() + box.getWidth() * x, py);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (u * 0.26f));
        }
        else // modulation effects (chorus, phaser, flanger, hyper, tremolo, width, mid/side): wobbling lines
        {
            for (int k = 0; k < 3; ++k)
            {
                juce::Path w;
                for (int i = 0; i <= 64; ++i)
                {
                    const float x = (float) i / 64.0f;
                    const float y = box.getCentreY() + box.getHeight() * (0.12f * (float) (k - 1) + 0.18f * pv * std::sin (x * twoPi * 2.0f + (float) k * 1.3f + time * 2.0f));
                    if (i == 0) w.startNewSubPath (box.getX(), y);
                    else w.lineTo (box.getX() + box.getWidth() * x, y);
                }
                g.setColour (c.withAlpha (0.45f + 0.25f * (float) k));
                g.strokePath (w, juce::PathStrokeType (u * 0.2f));
            }
        }
        // mix bar
        g.setColour (colour::displayGrid.brighter (0.3f));
        g.fillRoundedRectangle (juce::Rectangle<float> (box.getX(), plot.getBottom() - u * 0.5f, box.getWidth(), u * 0.4f), u * 0.2f);
        g.setColour (c);
        g.fillRoundedRectangle (juce::Rectangle<float> (box.getX(), plot.getBottom() - u * 0.5f, box.getWidth() * mixv, u * 0.4f), u * 0.2f);
        if (ids.size() > 1) drawModBar (g, { box.getX(), plot.getBottom() - u * 1.1f }, { box.getRight(), plot.getBottom() - u * 1.1f }, ids[1]);
        juce::String head = slot > 0 ? juce::String (slot) + "  " : juce::String();
        head << (hint.isNotEmpty() ? i18n::upper (tr (hint)) : i18n::upper (valueText (ids[0])));
        if (bypassed) head << "  (" << i18n::upper (tr ("Bypass")) << ")";
        caption (g, area.withHeight (u * 1.8f), head, juce::Justification::topLeft, u, off ? colour::displayDim : colour::displayText);
        if (ids.size() > 3 && has (ids[3])) caption (g, area.withHeight (u * 1.8f), valueText (ids[3]), juce::Justification::topRight, u);
    }

private:
    int slot;
    juce::String hint; ///< sends: "Delay" / "Reverb" (no type parameter)
    juce::Point<float> anchor;
    float m0 = 0, p0 = 0;
};

// ---------------------------------------------------------------- 3D: the head from above, the source as a dot
class SpaceTile final : public Tile
{
public:
    SpaceTile (OhpeProcessor& p, int prt, std::string az, std::string dist, std::string elev, std::string on)
        : Tile (p, prt, { az, dist, elev, on }) {}

    std::string paramAt (juce::Point<float> p) const override
    {
        return p.getDistanceFrom (centre()) < radius() * 0.3f ? ids[1] : ids[0];
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showControlMenu (proc, juce::String (paramAt (e.position)), this); return; }
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto c = centre();
        const auto d = e.position - c;
        const float ang = std::atan2 (d.x, -d.y) * 180.0f / juce::MathConstants<float>::pi; // 0 = front (up), clockwise = right
        const auto& reg = proc.core().params();
        const int ai = reg.indexOf (ids[0]), di = reg.indexOf (ids[1]);
        if (ai >= 0) set (ids[0], reg.descriptor (ai).toNormalised (ang));
        if (di >= 0)
        {
            const auto& dd = reg.descriptor (di);
            const float frac = juce::jlimit (0.0f, 1.0f, d.getDistanceFromOrigin() / radius());
            set (ids[1], juce::jlimit (0.0f, 1.0f, frac));
            juce::ignoreUnused (dd);
        }
    }
    void mouseUp (const juce::MouseEvent&) override { endAll(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { reset (ids[0]); reset (ids[1]); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { nudge (ids[2], w.deltaY * 0.1f); }

protected:
    void paintTile (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const float u = unit();
        const auto c = centre();
        const float r = radius();
        const bool on = ! has (ids[3]) || normAt (3) >= 0.5f;
        g.setColour (colour::displayGrid.brighter (0.3f));
        for (float k : { 0.33f, 0.66f, 1.0f }) g.drawEllipse (juce::Rectangle<float> (r * 2.0f * k, r * 2.0f * k).withCentre (c), 1.0f);
        g.drawLine ({ c.x, c.y - r, c.x, c.y + r }, 0.6f);
        g.drawLine ({ c.x - r, c.y, c.x + r, c.y }, 0.6f);
        // the head: a circle with a nose (front = up) and ears
        const float hr = r * 0.16f;
        g.setColour (colour::displayText.withAlpha (0.85f));
        g.drawEllipse (juce::Rectangle<float> (hr * 2.0f, hr * 2.0f).withCentre (c), u * 0.25f);
        juce::Path nose;
        nose.addTriangle (c.x - hr * 0.3f, c.y - hr * 0.95f, c.x + hr * 0.3f, c.y - hr * 0.95f, c.x, c.y - hr * 1.4f);
        g.fillPath (nose);
        g.fillRoundedRectangle (juce::Rectangle<float> (hr * 0.3f, hr * 0.7f).withCentre ({ c.x - hr * 1.05f, c.y }), hr * 0.1f);
        g.fillRoundedRectangle (juce::Rectangle<float> (hr * 0.3f, hr * 0.7f).withCentre ({ c.x + hr * 1.05f, c.y }), hr * 0.1f);
        // the source
        const auto& reg = proc.core().params();
        const float az = plainOf (proc, ids[0]) * juce::MathConstants<float>::pi / 180.0f;
        const float dn = normAt (1) + mod (ids[1]);
        const auto s = c + juce::Point<float> (std::sin (az), -std::cos (az)) * (r * juce::jlimit (0.05f, 1.0f, dn));
        // modulation range of the azimuth: a violet arc
        if (const auto rg = range (ids[0]); ! rg.isEmpty())
        {
            const int ai = reg.indexOf (ids[0]);
            const auto& d = reg.descriptor (ai);
            const float lo = d.fromNormalised (juce::jlimit (0.0f, 1.0f, normAt (0) + rg.getStart())) * juce::MathConstants<float>::pi / 180.0f;
            const float hi = d.fromNormalised (juce::jlimit (0.0f, 1.0f, normAt (0) + rg.getEnd())) * juce::MathConstants<float>::pi / 180.0f;
            juce::Path arc;
            const float rr = r * juce::jlimit (0.05f, 1.0f, normAt (1));
            arc.addCentredArc (c.x, c.y, rr, rr, 0.0f, lo, hi, true);
            g.setColour (modc().withAlpha (0.7f));
            g.strokePath (arc, juce::PathStrokeType (u * 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour ((on ? edit() : colour::displayDim).withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (u * 3.0f, u * 3.0f).withCentre (s));
        g.setColour (on ? edit() : colour::displayDim);
        g.fillEllipse (juce::Rectangle<float> (u * 1.6f, u * 1.6f).withCentre (s));
        caption (g, area.withHeight (u * 1.8f), valueText (ids[0]) + "  " + valueText (ids[1]), juce::Justification::topRight, u);
        if (! on) caption (g, area.withHeight (u * 1.8f), i18n::upper (tr ("Off")), juce::Justification::topLeft, u);
    }

private:
    juce::Point<float> centre() const
    {
        const auto b = getLocalBounds().toFloat();
        return { b.getCentreX(), b.getCentreY() + unit() * 0.6f };
    }
    float radius() const { return juce::jmax (10.0f, juce::jmin ((float) getWidth(), (float) getHeight() - unit() * 3.0f) * 0.44f); }
};

/** The first ID of `g` that ends with `.<tail>` (or empty). */
std::string find (const ParamGroup& g, const char* tail)
{
    const std::string t = std::string (".") + tail;
    for (const auto& id : g.ids)
        if (endsWith (id, t.c_str())) return id;
    return {};
}
} // namespace

std::unique_ptr<Tile> makeTile (OhpeProcessor& proc, Mode mode, int part, const ParamGroup& g)
{
    const std::string p = "p" + std::to_string (juce::jlimit (0, Core::numParts - 1, part) + 1) + ".";
    const auto& t = g.title;
    if (t == "Engine")
    {
        const auto type = selectedEngine (proc.core(), part);
        const std::string e = p + engineTypeInfo (type).id + ".";
        switch (type)
        {
            case EngineType::FM: return std::make_unique<FmTile> (proc, part, e);
            case EngineType::Wavetable: return std::make_unique<WavetableTile> (proc, part, e);
            case EngineType::Grain: return std::make_unique<WaveTile> (proc, part, std::vector<std::string> { e + "pos", e + "size", e + "spray", e + "density" }, true);
            case EngineType::Sampler:
                return std::make_unique<WaveTile> (proc, part, std::vector<std::string> { e + "start", e + "loopstart", e + "loopend", e + "tone" }, false);
            case EngineType::Analog:
                return std::make_unique<OscTile> (proc, part, std::vector<std::string> { e + "shape", e + "detune", e + "sub", e + "oscmix" }, OscTile::Analog);
            case EngineType::Pluck:
                return std::make_unique<OscTile> (proc, part, std::vector<std::string> { e + "bright", e + "damping", e + "body", e + "excite" }, OscTile::Pluck);
            case EngineType::Cluster:
                return std::make_unique<OscTile> (proc, part, std::vector<std::string> { e + "detune", e + "spread", e + "stack", e + "shape" }, OscTile::Cluster);
            case EngineType::Vocal:
                return std::make_unique<OscTile> (proc, part, std::vector<std::string> { e + "vowel", e + "formant", e + "breath", e + "tension" }, OscTile::Vocal);
            default: return {};
        }
    }
    if (t == "Filter")
    {
        auto cut = find (g, "cutoff");
        if (cut.empty()) cut = find (g, "tone");
        if (cut.empty()) return {};
        return std::make_unique<FilterTile> (proc, part, cut, find (g, "reso"), find (g, "drive"), find (g, "ftype"));
    }
    if (t == "Amp Envelope") return std::make_unique<EnvelopeTile> (proc, part, std::vector<std::string> { p + "amp.a", p + "amp.d", p + "amp.s", p + "amp.r" }, 0);
    if (t == "Mod Envelope") return std::make_unique<EnvelopeTile> (proc, part, std::vector<std::string> { p + "mod.a", p + "mod.d", p + "mod.s", p + "mod.r" }, 1);
    if (t == "Env 3") return std::make_unique<EnvelopeTile> (proc, part, std::vector<std::string> { p + "env3.a", p + "env3.d", p + "env3.s", p + "env3.r" }, -1);
    if (t == "LFO") return std::make_unique<LfoTile> (proc, part, p + "lfo.rate", p + "lfo.depth", p + "lfo.shape", mod::SrcLfo1);
    if (t == "LFO 2") return std::make_unique<LfoTile> (proc, part, p + "lfo2.rate", p + "lfo2.depth", p + "lfo2.shape", mod::SrcLfo2);
    if (t == "LFO 3") return std::make_unique<LfoTile> (proc, part, p + "lfo3.rate", p + "lfo3.depth", p + "lfo3.shape", mod::SrcLfo3);
    if (t == "Pitch & Level") return std::make_unique<PitchLevelTile> (proc, part, p + "tune", p + "fine", p + "vol");
    if (mode == Mode::Effects && (t == "FX" || t == "Master FX") && g.ids.size() >= 3)
    {
        std::vector<std::string> v (g.ids.begin(), g.ids.begin() + 3);
        if (g.ids.size() > 3) v.push_back (g.ids[3]);
        return std::make_unique<FxTile> (proc, part, v, g.number);
    }
    if (mode == Mode::Effects && (t == "Delay (Send A)" || t == "Reverb (Send B)") && g.ids.size() >= 2)
    {
        const bool delay = t == "Delay (Send A)";
        return std::make_unique<FxTile> (proc, part, std::vector<std::string> { std::string(), std::string(), std::string(), delay && g.ids.size() > 2 ? g.ids[2] : g.ids[0] }, 0,
                                         delay ? "Delay" : "Reverb");
    }
    if (t == "3D Space")
    {
        const auto az = find (g, "azimuth");
        if (az.empty()) return {};
        return std::make_unique<SpaceTile> (proc, part, az, find (g, "distance"), find (g, "elevation"), find (g, "on"));
    }
    return {};
}

// ---------------------------------------------------------------- SourceChip
SourceChip::SourceChip (OhpeProcessor& p, int source) : proc (p), src (source)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::String SourceChip::getTooltip()
{
    const auto& s = mod::sourceTable()[(size_t) src];
    return i18n::pick (s.nameEn, s.nameDe) + " - "
           + i18n::pick ("drag onto a picture to modulate it", "auf ein Bild ziehen, um es zu modulieren");
}

void SourceChip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float live = proc.core().part (part).modMatrix().uiSourceValue (src);
    g.setColour (modc().withAlpha (0.18f + 0.25f * juce::jlimit (0.0f, 1.0f, std::abs (live))));
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (modc().brighter (0.3f));
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
    g.setColour (colour::displayText);
    g.setFont (labelFont (b.getHeight() * 0.55f));
    juce::String name;
    switch (src)
    {
        case mod::SrcModEnv: name = i18n::pick ("Mod Env", "Mod-HK"); break;
        case mod::SrcEnv3: name = i18n::pick ("Env 3", "HK 3"); break;
        case mod::SrcModWheel: name = i18n::pick ("Wheel", "Mod-Rad"); break;
        default: name = i18n::pick (mod::sourceTable()[(size_t) src].nameEn, mod::sourceTable()[(size_t) src].nameDe); break;
    }
    g.drawText (i18n::upper (name), b, juce::Justification::centred, true);
}

void SourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! c->isDragAndDropActive()) c->startDragging (sourceDragDescription (src), this);
}
} // namespace ohpe::ui::pro
