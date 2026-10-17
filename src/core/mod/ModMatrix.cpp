// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ModMatrix.h"

#include <cmath>

namespace ohpe::mod
{
static_assert (numBuiltinSources <= numSources, "source list");
const std::vector<SourceInfo>& sourceTable()
{
    using S = SourceScope;
    static const std::vector<SourceInfo> t {
        { "off", "Off", "Aus", false, S::Part },
        { "lfo1", "LFO 1", "LFO 1", true, S::Voice },
        { "lfo2", "LFO 2", "LFO 2", true, S::Voice },
        { "lfo3", "LFO 3", "LFO 3", true, S::Voice },
        { "ampenv", "Amp Env", "Amp-Hüllkurve", false, S::Voice },
        { "modenv", "Mod Env", "Mod-Hüllkurve", false, S::Voice },
        { "env3", "Env 3", "Hüllkurve 3", false, S::Voice },
        { "velocity", "Velocity", "Anschlag", false, S::Voice },
        { "key", "Key Track", "Tastatur", true, S::Voice },
        { "modwheel", "Mod Wheel", "Modulationsrad", false, S::Part },
        { "bend", "Pitch Bend", "Pitchbend", true, S::Part },
        { "chpressure", "Channel Pressure", "Kanaldruck", false, S::Part },
        { "pressure", "Note Pressure", "Notendruck", false, S::Voice },
        { "slide", "Slide (Timbre)", "Slide (Klangfarbe)", true, S::Voice },
        { "notebend", "Note Bend", "Noten-Bend", true, S::Voice },
        { "random", "Random", "Zufall", true, S::Voice },
        { "macro1", "Macro 1", "Makro 1", false, S::Part },
        { "macro2", "Macro 2", "Makro 2", false, S::Part },
        { "macro3", "Macro 3", "Makro 3", false, S::Part },
        { "macro4", "Macro 4", "Makro 4", false, S::Part },
        { "seqlane", "Seq Lane", "Seq-Spur", false, S::Part },
        { "morphx", "Morph X", "Morph X", false, S::Part }, // p16: the morph pad
        { "morphy", "Morph Y", "Morph Y", false, S::Part },
        { "motion1", "Motion 1", "Bewegung 1", false, S::Part }, // p22: the motion recorder's lanes
        { "motion2", "Motion 2", "Bewegung 2", false, S::Part },
        { "motion3", "Motion 3", "Bewegung 3", false, S::Part },
        { "motion4", "Motion 4", "Bewegung 4", false, S::Part },
    };
    return t;
}

const std::vector<DestInfo>& destTable()
{
    static const std::vector<DestInfo> t = [] {
        std::vector<DestInfo> v;
        auto add = [&] (const char* key, std::string en, std::string de, DestKind k, int arg, const char* suffix = nullptr) {
            v.push_back ({ key, std::move (en), std::move (de), k, arg, suffix });
        };
        add ("off", "Off", "Aus", DestKind::None, 0);
        add ("pitch", "Pitch", "Tonhöhe", DestKind::Voice, DstPitch);
        add ("level", "Level", "Pegel", DestKind::Voice, DstLevel);
        add ("pan", "Pan", "Panorama", DestKind::Voice, DstPan);
        add ("timbre", "Timbre", "Klangfarbe", DestKind::Voice, DstTimbre);
        static const char* engineKeys[numEngineDests] = { "engine1", "engine2", "engine3", "engine4", "engine5", "engine6", "engine7",
                                                          "engine8", "engine9", "engine10", "engine11", "engine12", "engine13", "engine14",
                                                          "engine15", "engine16", "engine17", "engine18", "engine19", "engine20",
                                                          "engine21", "engine22", "engine23", "engine24" };
        for (int i = 0; i < numEngineDests; ++i)
            add (engineKeys[i], "Engine " + std::to_string (i + 1), "Engine " + std::to_string (i + 1), DestKind::EngineParam, i);
        add ("volume", "Volume", "Lautstärke", DestKind::PartParam, 0, "vol");
        add ("lfo1rate", "LFO 1 Rate", "LFO 1 Tempo", DestKind::PartParam, 0, "lfo.rate");
        add ("lfo1depth", "LFO 1 Depth", "LFO 1 Tiefe", DestKind::PartParam, 0, "lfo.depth");
        add ("lfo2rate", "LFO 2 Rate", "LFO 2 Tempo", DestKind::PartParam, 0, "lfo2.rate");
        add ("lfo2depth", "LFO 2 Depth", "LFO 2 Tiefe", DestKind::PartParam, 0, "lfo2.depth");
        add ("lfo3rate", "LFO 3 Rate", "LFO 3 Tempo", DestKind::PartParam, 0, "lfo3.rate");
        add ("lfo3depth", "LFO 3 Depth", "LFO 3 Tiefe", DestKind::PartParam, 0, "lfo3.depth");
        add ("modenvamt", "Mod Env Amount", "Mod-Hüllkurve Menge", DestKind::PartParam, 0, "mod.amt");
        static const char* mixKeys[numFxSlots] = { "fx1mix", "fx2mix", "fx3mix", "fx4mix", "fx5mix", "fx6mix" };
        for (int k = 0; k < numFxSlots; ++k)
            add (mixKeys[k], "FX " + std::to_string (k + 1) + " Mix", "FX " + std::to_string (k + 1) + " Mischung", DestKind::FxMix, k);
        static const char* fxKeys[numFxSlots][numFxDestParams] = {
            { "fx1p1", "fx1p2", "fx1p3", "fx1p4" }, { "fx2p1", "fx2p2", "fx2p3", "fx2p4" }, { "fx3p1", "fx3p2", "fx3p3", "fx3p4" },
            { "fx4p1", "fx4p2", "fx4p3", "fx4p4" }, { "fx5p1", "fx5p2", "fx5p3", "fx5p4" }, { "fx6p1", "fx6p2", "fx6p3", "fx6p4" } };
        for (int k = 0; k < numFxSlots; ++k)
            for (int j = 0; j < numFxDestParams; ++j)
                add (fxKeys[k][j], "FX " + std::to_string (k + 1) + " Param " + std::to_string (j + 1),
                     "FX " + std::to_string (k + 1) + " Parameter " + std::to_string (j + 1), DestKind::FxParam, k * numFxDestParams + j);
        // reserved for the planned per-part 3D spatialiser (1.0): inert until its parameters "pN.space.*" exist,
        // and hidden in the UI until then
        add ("azimuth", "Azimuth", "Azimut", DestKind::PartParam, 0, "space.azimuth");
        add ("elevation", "Elevation", "Elevation", DestKind::PartParam, 0, "space.elevation");
        add ("distance", "Distance", "Entfernung", DestKind::PartParam, 0, "space.distance");
        add ("pathrate", "Motion Path Rate", "Bewegungspfad Tempo", DestKind::PartParam, 0, "space.pathrate");
        add ("pathphase", "Motion Path Phase", "Bewegungspfad Phase", DestKind::PartParam, 0, "space.pathphase");
        add ("life", "Life", "Leben", DestKind::PartParam, 0, "life"); // p15
        // p16: the morph pad's position
        add ("morphx", "Morph X", "Morph X", DestKind::PartParam, 0, "morph.x");
        add ("morphy", "Morph Y", "Morph Y", DestKind::PartParam, 0, "morph.y");
        // p19: the resonator body stage of the part
        add ("resotune", "Body Tune", "Körper-Stimmung", DestKind::PartParam, 0, "reso.tune");
        add ("resosize", "Body Size", "Körper-Größe", DestKind::PartParam, 0, "reso.size");
        add ("resomaterial", "Body Material", "Körper-Material", DestKind::PartParam, 0, "reso.material");
        add ("resodecay", "Body Decay", "Körper-Ausklang", DestKind::PartParam, 0, "reso.decay");
        add ("resoposition", "Body Position", "Körper-Position", DestKind::PartParam, 0, "reso.position");
        add ("resomix", "Body Mix", "Körper-Mischung", DestKind::PartParam, 0, "reso.mix");
        // p21: the character macros
        add ("charbright", "Bright", "Hell", DestKind::PartParam, 0, "char.bright");
        add ("charhard", "Hard", "Hart", DestKind::PartParam, 0, "char.hard");
        add ("charalive", "Alive", "Lebendig", DestKind::PartParam, 0, "char.alive");
        add ("charwide", "Wide", "Weit", DestKind::PartParam, 0, "char.wide");
        return v;
    }();
    return t;
}

int sourceIndexOfKey (const std::string& key) noexcept
{
    const auto& t = sourceTable();
    for (size_t i = 0; i < t.size(); ++i)
        if (key == t[i].key) return (int) i;
    return -1;
}

int destIndexOfKey (const std::string& key) noexcept
{
    const auto& t = destTable();
    for (size_t i = 0; i < t.size(); ++i)
        if (key == t[i].key) return (int) i;
    return -1;
}

void migrate (int fromSchema, std::map<std::string, float>& params)
{
    // schema 1 (0.7) is the first matrix schema; presets without matrix parameters (0) need nothing either
    (void) fromSchema;
    (void) params;
}

std::vector<std::string> sourceNamesEn()
{
    std::vector<std::string> v;
    for (const auto& s : sourceTable()) v.emplace_back (s.nameEn);
    return v;
}
std::vector<std::string> sourceNamesDe()
{
    std::vector<std::string> v;
    for (const auto& s : sourceTable()) v.emplace_back (s.nameDe);
    return v;
}
std::vector<std::string> destNamesEn()
{
    std::vector<std::string> v;
    for (const auto& d : destTable()) v.push_back (d.nameEn);
    return v;
}
std::vector<std::string> destNamesDe()
{
    std::vector<std::string> v;
    for (const auto& d : destTable()) v.push_back (d.nameDe);
    return v;
}
std::vector<std::string> curveNamesEn() { return { "Linear", "Exponential", "Logarithmic", "S-Curve", "Steps" }; }
std::vector<std::string> curveNamesDe() { return { "Linear", "Exponentiell", "Logarithmisch", "S-Kurve", "Stufen" }; }
std::vector<std::string> syncNamesEn()
{
    return { "Free", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2.", "1/4.", "1/8.", "1/16.", "1/2T", "1/4T", "1/8T", "1/16T" };
}
std::vector<std::string> syncNamesDe()
{
    auto v = syncNamesEn();
    v[0] = "Frei";
    return v;
}

void PartMatrix::registerParams (ParamRegistry& reg, const std::string& pp, int partNumber)
{
    using namespace pd;
    const auto n = std::to_string (partNumber);
    const auto group = "part" + n + "/mod";
    auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
    const std::string pe = "Part " + n + " ", pg = "Part " + n + " ";
    for (int k = 1; k <= numSlots; ++k)
    {
        const auto s = pp + "mod.s" + std::to_string (k) + ".";
        const auto m = "Mod " + std::to_string (k) + " ";
        add (choice (s + "src", pe + m + "Source", pg + m + "Quelle", sourceNamesEn(), sourceNamesDe(), SrcOff));
        add (choice (s + "via", pe + m + "Via", pg + m + "Via", sourceNamesEn(), sourceNamesDe(), SrcOff));
        add (choice (s + "dst", pe + m + "Target", pg + m + "Ziel", destNamesEn(), destNamesDe(), DstOff));
        add (continuous (s + "amt", pe + m + "Amount", pg + m + "Menge", -1.0f, 1.0f, 0.0f, ParamUnit::Percent));
        add (choice (s + "curve", pe + m + "Curve", pg + m + "Kurve", curveNamesEn(), curveNamesDe(), CurveLinear));
    }
    for (int l = 2; l <= 3; ++l)
    {
        const auto s = pp + "lfo" + std::to_string (l) + ".";
        const auto m = "LFO " + std::to_string (l) + " ";
        add (continuous (s + "rate", pe + m + "Rate", pg + m + "Tempo", 0.01f, 40.0f, 1.0f, ParamUnit::Hertz, 0.3f));
        add (choice (s + "sync", pe + m + "Sync", pg + m + "Sync", syncNamesEn(), syncNamesDe(), 0));
        add (choice (s + "shape", pe + m + "Shape", pg + m + "Form",
                     { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random" },
                     { "Sinus", "Dreieck", "Sägezahn auf", "Sägezahn ab", "Rechteck", "S&H", "Weicher Zufall" }, ShapeSine));
        add (continuous (s + "phase", pe + m + "Phase", pg + m + "Phase", 0.0f, 1.0f, 0.0f));
        add (continuous (s + "fade", pe + m + "Fade In", pg + m + "Einblenden", 0.0f, 10.0f, 0.0f, ParamUnit::Seconds, 0.4f));
        add (choice (s + "mode", pe + m + "Mode", pg + m + "Modus", { "Free-Running", "Retrigger", "One-Shot" },
                     { "Frei laufend", "Neustart", "Einmalig" }, ModeRetrigger));
        add (continuous (s + "depth", pe + m + "Depth", pg + m + "Tiefe", 0.0f, 1.0f, 1.0f));
    }
    {
        const auto s = pp + "env3.";
        add (continuous (s + "delay", pe + "Env 3 Delay", pg + "Hüllkurve 3 Verzögerung", 0.0f, 4.0f, 0.0f, ParamUnit::Seconds, 0.4f));
        add (continuous (s + "a", pe + "Env 3 Attack", pg + "Hüllkurve 3 Attack", 0.001f, 10.0f, 0.5f, ParamUnit::Seconds, 0.3f));
        add (continuous (s + "hold", pe + "Env 3 Hold", pg + "Hüllkurve 3 Halten", 0.0f, 4.0f, 0.0f, ParamUnit::Seconds, 0.4f));
        add (continuous (s + "d", pe + "Env 3 Decay", pg + "Hüllkurve 3 Decay", 0.001f, 10.0f, 1.0f, ParamUnit::Seconds, 0.3f));
        add (continuous (s + "s", pe + "Env 3 Sustain", pg + "Hüllkurve 3 Sustain", 0.0f, 1.0f, 0.5f));
        add (continuous (s + "r", pe + "Env 3 Release", pg + "Hüllkurve 3 Release", 0.001f, 10.0f, 1.0f, ParamUnit::Seconds, 0.3f));
    }
    for (int k = 1; k <= 4; ++k)
        add (continuous (pp + "macro" + std::to_string (k), pe + "Macro " + std::to_string (k), pg + "Makro " + std::to_string (k), 0.0f, 1.0f, 0.0f));
    add (continuous (pp + "seqlane", pe + "Seq Lane", pg + "Seq-Spur", 0.0f, 1.0f, 0.0f));
}

void PartMatrix::bind (const ParamRegistry& reg, const std::string& pp)
{
    for (int k = 0; k < numSlots; ++k)
    {
        const auto s = pp + "mod.s" + std::to_string (k + 1) + ".";
        idx[(size_t) k] = { reg.indexOf (s + "src"), reg.indexOf (s + "via"), reg.indexOf (s + "dst"), reg.indexOf (s + "amt"),
                            reg.indexOf (s + "curve") };
    }
    static const char* lfoKeys[7] = { "rate", "sync", "shape", "phase", "fade", "mode", "depth" };
    for (int l = 0; l < 2; ++l)
        for (int k = 0; k < 7; ++k) lfoIdx[(size_t) l][(size_t) k] = reg.indexOf (pp + "lfo" + std::to_string (l + 2) + "." + lfoKeys[k]);
    static const char* envKeys[6] = { "delay", "a", "hold", "d", "s", "r" };
    for (int k = 0; k < 6; ++k) env3Idx[(size_t) k] = reg.indexOf (pp + "env3." + envKeys[k]);
    for (int k = 0; k < 4; ++k) macroIdx[(size_t) k] = reg.indexOf (pp + "macro" + std::to_string (k + 1));
    laneIdx = reg.indexOf (pp + "seqlane");
}

void PartMatrix::prepare (double sampleRate) noexcept
{
    sr = (float) (sampleRate > 0.0 ? sampleRate : 48000.0);
    ampEnv.prepare (sr);
    modEnv.prepare (sr);
    lfo1.prepare (sr);
    reset();
}

void PartMatrix::reset() noexcept
{
    ampEnv.reset();
    modEnv.reset();
    env3.reset();
    held.fill (false);
    heldCount = 0;
    lastNote = -1;
    fresh.fill (true);
    value.fill (0.0f);
    for (auto s : { SrcLfo1, SrcLfo2, SrcLfo3, SrcAmpEnv, SrcModEnv, SrcEnv3, SrcRandom, SrcNotePressure, SrcSlide, SrcNoteBend })
        mono[(size_t) s] = 0.0f;
}

void PartMatrix::pull (const ParamSnapshot& s, const dsp::AdsrParams& amp, const dsp::AdsrParams& modP, float lfo1Rate, int lfo1Shape) noexcept
{
    numParamRoutes = 0;
    program.numRoutes = 0;
    for (int k = 0; k < numSlots; ++k)
    {
        const auto& ix = idx[(size_t) k];
        Route r;
        if (ix.src >= 0)
        {
            r.src = std::clamp (s.getInt (ix.src), 0, numSources - 1);
            r.via = std::clamp (s.getInt (ix.via), 0, numSources - 1);
            r.dst = std::clamp (s.getInt (ix.dst), 0, numDests - 1);
            r.amount = std::clamp (s[ix.amt], -1.0f, 1.0f);
            r.curve = std::clamp (s.getInt (ix.curve), 0, numCurves - 1);
        }
        auto& old = slots[(size_t) k];
        if (old.src != r.src || old.dst != r.dst) fresh[(size_t) k] = true;
        old = r;
        const bool on = r.src != SrcOff && r.dst != DstOff && r.amount != 0.0f;
        isParam[(size_t) k] = on && ! isVoiceDest (r.dst);
        if (! on) value[(size_t) k] = target[(size_t) k] = 0.0f;
        if (isParam[(size_t) k]) ++numParamRoutes;
        else if (on) program.routes[(size_t) program.numRoutes++] = r;
    }
    for (int l = 0; l < 2; ++l)
    {
        const auto& ix = lfoIdx[(size_t) l];
        if (ix[0] < 0) continue;
        auto& ls = program.lfo[(size_t) l];
        ls.rate = s[ix[0]];
        ls.sync = std::clamp (s.getInt (ix[1]), 0, numSyncDivisions - 1);
        ls.shape = std::clamp (s.getInt (ix[2]), 0, numLfoShapes - 1);
        ls.phase = dsp::clamp01 (s[ix[3]]);
        ls.fade = std::max (0.0f, s[ix[4]]);
        ls.mode = std::clamp (s.getInt (ix[5]), 0, numLfoModes - 1);
        ls.depth = dsp::clamp01 (s[ix[6]]);
    }
    if (env3Idx[0] >= 0)
        program.env3 = { s[env3Idx[0]], s[env3Idx[1]], s[env3Idx[2]], s[env3Idx[3]], s[env3Idx[4]], s[env3Idx[5]] };
    for (int k = 0; k < 4; ++k) mono[(size_t) SrcMacro1 + (size_t) k] = macroIdx[(size_t) k] >= 0 ? dsp::clamp01 (s[macroIdx[(size_t) k]]) : 0.0f;
    mono[SrcSeqLane] = laneIdx >= 0 ? dsp::clamp01 (s[laneIdx]) : 0.0f;
    ampParams = amp;
    modParams = modP;
    lfo1.setRate (lfo1Rate);
    lfo1.setShape (lfo1Shape);
    program.mono = mono;
    program.bpm = tempo;
}

void PartMatrix::noteOn (int note, float velocity) noexcept
{
    if (note < 0 || note > 127) return;
    if (! held[(size_t) note]) { held[(size_t) note] = true; ++heldCount; }
    lastNote = note;
    mono[SrcVelocity] = velocity;
    mono[SrcKeyTrack] = std::clamp (((float) note - 60.0f) / 60.0f, -1.0f, 1.0f);
    mono[SrcRandom] = rng.nextBipolar();
    mono[SrcNotePressure] = 0.0f;
    mono[SrcSlide] = 0.0f;
    mono[SrcNoteBend] = 0.0f;
    ampEnv.setParams (ampParams);
    modEnv.setParams (modParams);
    ampEnv.noteOn();
    modEnv.noteOn();
    lfo1.reset();
    ++noteCounter;
    program.seed = noteCounter;
    for (size_t i = 0; i < 2; ++i)
        if (program.lfo[i].mode != ModeFree) lfo[i].trigger (program.lfo[i], noteCounter + (uint32_t) i * 7919u);
    env3.noteOn();
    sinceNote = 0.0f;
}

void PartMatrix::noteOff (int note) noexcept
{
    if (note < 0 || note > 127 || ! held[(size_t) note]) return;
    held[(size_t) note] = false;
    if (--heldCount <= 0)
    {
        heldCount = 0;
        ampEnv.noteOff();
        modEnv.noteOff();
        env3.noteOff();
    }
}

void PartMatrix::allNotesOff() noexcept
{
    held.fill (false);
    heldCount = 0;
    ampEnv.noteOff();
    modEnv.noteOff();
    env3.noteOff();
}

void PartMatrix::setNoteExpression (int note, float pressure, float slide, float noteBend) noexcept
{
    if (note < 0) mono[SrcChannelPressure] = pressure;
    if (note < 0 || note == lastNote)
    {
        mono[SrcNotePressure] = pressure;
        mono[SrcSlide] = slide;
        mono[SrcNoteBend] = noteBend;
    }
}

void PartMatrix::advance (int n) noexcept
{
    if (n <= 0) return;
    ampEnv.setParams (ampParams);
    modEnv.setParams (modParams);
    for (int i = 0; i < n; ++i) { ampEnv.next(); modEnv.next(); }
    mono[SrcAmpEnv] = ampEnv.getLevel();
    mono[SrcModEnv] = modEnv.getLevel();
    mono[SrcLfo1] = lfo1.advance (n);
    const double beatsPerSample = tempo / 60.0 / (double) sr;
    for (size_t i = 0; i < 2; ++i)
    {
        const auto& ls = program.lfo[i];
        const double beats = syncBeats (ls.sync);
        if (ls.mode == ModeFree && beats > 0.0 && transportOn)
        {
            // locked to the song position: the same phase on every playback
            double ph = (ppqPos + beatsPerSample * n) / beats + ls.phase;
            ph -= std::floor (ph);
            lfo[i].setPhase ((float) ph, ls.shape);
        }
        else
            lfo[i].advance (ls, n, sr, tempo);
        mono[(size_t) SrcLfo2 + i] = lfo[i].value() * ls.depth;
    }
    ppqPos += beatsPerSample * n;
    mono[SrcEnv3] = env3.advance (program.env3, n, sr);
    sinceNote += (float) n / sr;
    program.mono = mono;
    program.bpm = tempo;
}

void PartMatrix::tickParamRoutes (int n) noexcept
{
    std::array<float, numSources> s = mono;
    s[SrcOff] = 0.0f;
    for (size_t i = 0; i < 2; ++i) s[(size_t) SrcLfo2 + i] *= fadeGain (program.lfo[i].fade, sinceNote);
    // ~1.5 ms glide: smooth enough for filters and FX, fast enough for snappy envelopes and steps
    const float a = 1.0f - std::exp (-(float) std::max (1, n) / (0.0015f * sr));
    for (int k = 0; k < numSlots; ++k)
    {
        const auto& r = slots[(size_t) k];
        if (r.src == SrcOff || r.dst == DstOff || r.amount == 0.0f) continue;
        const float v = evaluate (r, s.data());
        if (! isParam[(size_t) k]) { value[(size_t) k] = v; continue; }
        target[(size_t) k] = v;
        if (fresh[(size_t) k]) { value[(size_t) k] = v; fresh[(size_t) k] = false; }
        else value[(size_t) k] += (v - value[(size_t) k]) * a;
    }
}

void PartMatrix::publishUi() noexcept
{
    for (int k = 0; k < numSlots; ++k) uiValue[(size_t) k].store (value[(size_t) k], std::memory_order_relaxed);
    for (int s = 0; s < numSources; ++s) uiSource[(size_t) s].store (mono[(size_t) s], std::memory_order_relaxed);
    uiStage[0].store ((int) ampEnv.getStage(), std::memory_order_relaxed); // p21: the pro view's envelope playheads
    uiStage[1].store ((int) modEnv.getStage(), std::memory_order_relaxed);
}
} // namespace ohpe::mod
