// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TasteProfile.h"

#include "../model/FormatVersions.h"
#include "../model/JsonParse.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>

namespace ohpe::breed
{
using nlohmann::json;

float fitnessDelta (Signal s, float amount) noexcept
{
    switch (s)
    {
        case Signal::Played:
        {
            const float sec = std::isfinite (amount) ? std::clamp (amount, 0.0f, 120.0f) : 0.0f;
            return sec / 60.0f; // up to +2 for two minutes of playing
        }
        case Signal::Saved: return 1.5f;
        case Signal::Reloaded: return 0.75f;
        case Signal::Favourite: return 2.0f;
        case Signal::Unfavourite: return -1.0f;
        case Signal::Kept: return 0.5f;
        case Signal::Skipped: return -1.0f;
    }
    return 0.0f;
}

bool learnable (const Gene& g) noexcept
{
    return g.drawsRandom && g.mutable_ && ! g.discrete() && g.group != Level && g.group != Fixed;
}

// ------------------------------------------------------------------------------------------------ TasteBias
const EngineBias* TasteBias::forEngine (int engine) const noexcept
{
    const auto it = engines.find (engine);
    return it != engines.end() && it->second.any ? &it->second : nullptr;
}

float TasteBias::pull() const noexcept
{
    const float c = std::isfinite (curiosity) ? std::clamp (curiosity, 0.0f, 1.0f) : 1.0f;
    return 0.6f * (1.0f - c);
}

float TasteBias::score (const Schema& s, const Genome& g) const
{
    if ((int) g.size() != s.size()) return 0.0f;
    const auto* eb = forEngine (engineOf (s, g));
    if (eb == nullptr) return 0.0f;
    double sum = 0.0, conf = 0.0;
    for (int i = 0; i < s.size(); ++i)
    {
        const float lc = eb->likeConf[(size_t) i], dc = eb->dislikeConf[(size_t) i];
        if (lc <= 0.0f && dc <= 0.0f) continue;
        const float n = s.gene (i).toNorm (g[(size_t) i]);
        if (lc > 0.0f)
        {
            const float d = n - eb->like[(size_t) i];
            sum -= lc * d * d;
        }
        if (dc > 0.0f)
        {
            const float d = n - eb->dislike[(size_t) i];
            sum += dc * d * d;
        }
        conf += std::max (lc, dc);
    }
    return conf > 0.0 ? (float) (sum / conf) : 0.0f;
}

int pickByTaste (const std::vector<float>& scores, float curiosity, float u)
{
    const int n = (int) scores.size();
    if (n <= 0) return -1;
    const float c = std::isfinite (curiosity) ? std::clamp (curiosity, 0.0f, 1.0f) : 1.0f;
    float best = -1.0e30f;
    for (const float v : scores)
        if (std::isfinite (v)) best = std::max (best, v);
    if (best < -1.0e29f) best = 0.0f;
    // softmax with a temperature that rises with curiosity, mixed with a uniform share
    const float beta = 40.0f * (1.0f - c);
    std::vector<double> w ((size_t) n);
    double tot = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float sc = std::isfinite (scores[(size_t) i]) ? scores[(size_t) i] : best - 10.0f;
        w[(size_t) i] = std::exp ((double) (beta * (sc - best)));
        tot += w[(size_t) i];
    }
    double acc = 0.0;
    const double target = std::clamp ((double) u, 0.0, 0.999999);
    for (int i = 0; i < n; ++i)
    {
        acc += c / (double) n + (1.0 - c) * w[(size_t) i] / tot;
        if (target < acc) return i;
    }
    return n - 1;
}

// ------------------------------------------------------------------------------------------------ TasteProfile
TasteProfile::Sound& TasteProfile::touch (const std::string& key)
{
    auto& snd = sounds[key];
    snd.used = ++clock;
    return snd;
}

void TasteProfile::prune()
{
    while ((int) sounds.size() > maxSounds)
    {
        auto oldest = sounds.begin();
        for (auto it = sounds.begin(); it != sounds.end(); ++it)
            if (it->second.used < oldest->second.used) oldest = it;
        sounds.erase (oldest);
    }
}

float TasteProfile::recordKeyOnly (const std::string& key, Signal sig, float amount)
{
    if (! learning || key.empty()) return 0.0f;
    const float d = fitnessDelta (sig, amount);
    auto& snd = touch (key);
    if (sig == Signal::Reloaded) ++snd.loads;
    snd.fitness = std::clamp (snd.fitness + d, -20.0f, 20.0f);
    prune();
    return d;
}

float TasteProfile::record (const Schema& s, const std::string& key, const Genome& g, Signal sig, float amount)
{
    if (! learning) return 0.0f;
    const float d = recordKeyOnly (key, sig, amount);
    if ((int) g.size() == s.size())
    {
        if (auto it = sounds.find (key); it != sounds.end()) it->second.engine = engineOf (s, g);
        learn (s, g, d);
    }
    return d;
}

void TasteProfile::learn (const Schema& s, const Genome& g, float delta)
{
    if (! learning || ! std::isfinite (delta) || delta == 0.0f || (int) g.size() != s.size() || ! valid (s, g)) return;
    const int engine = engineOf (s, g);
    if (engine < 0) return;
    auto& eng = engines[engine];
    const float w = std::min (std::fabs (delta), maxWeight);
    for (int i = 0; i < s.size(); ++i)
    {
        const auto& gene = s.gene (i);
        if (! learnable (gene)) continue;
        auto& st = delta > 0.0f ? eng[gene.id].like : eng[gene.id].dislike;
        const float x = std::clamp (gene.toNorm (g[(size_t) i]), 0.0f, 1.0f);
        st.mean += (x - st.mean) * w / (st.w + w);
        st.w = std::min (st.w + w, maxWeight);
    }
}

const TasteProfile::Sound* TasteProfile::sound (const std::string& key) const
{
    const auto it = sounds.find (key);
    return it != sounds.end() ? &it->second : nullptr;
}

float TasteProfile::fitness (const std::string& key) const
{
    const auto* snd = sound (key);
    return snd != nullptr ? snd->fitness : 0.0f;
}

const std::map<std::string, TasteProfile::GeneStats>* TasteProfile::engineStats (int engine) const
{
    const auto it = engines.find (engine);
    return it != engines.end() ? &it->second : nullptr;
}

TasteBias TasteProfile::bias (const Schema& s) const
{
    TasteBias b;
    b.curiosity = learning ? std::clamp (std::isfinite (curiosity) ? curiosity : 1.0f, 0.0f, 1.0f) : 1.0f;
    if (! learning) return b;
    const auto conf = [] (float w) { return w > 0.0f ? w / (w + confidenceHalf) : 0.0f; };
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const auto& [engine, genes] : engines)
    {
        EngineBias eb;
        const auto n = (size_t) s.size();
        eb.like.assign (n, nan);
        eb.dislike.assign (n, nan);
        eb.likeConf.assign (n, 0.0f);
        eb.dislikeConf.assign (n, 0.0f);
        for (const auto& [id, st] : genes)
        {
            const int i = s.indexOf (id);
            if (i < 0 || ! learnable (s.gene (i))) continue;
            if (st.like.w > 0.0f)
            {
                eb.like[(size_t) i] = st.like.mean;
                eb.likeConf[(size_t) i] = conf (st.like.w);
                eb.any = true;
            }
            if (st.dislike.w > 0.0f)
            {
                eb.dislike[(size_t) i] = st.dislike.mean;
                eb.dislikeConf[(size_t) i] = conf (st.dislike.w);
                eb.any = true;
            }
        }
        if (eb.any) b.engines[engine] = std::move (eb);
    }
    return b;
}

std::vector<TasteProfile::Trait> TasteProfile::traits (const Schema& s, int count) const
{
    std::vector<Trait> out;
    for (const auto& [engine, genes] : engines)
        for (const auto& [id, st] : genes)
        {
            const int i = s.indexOf (id);
            if (i < 0 || st.like.w <= 0.0f) continue;
            const auto& gene = s.gene (i);
            Trait t;
            t.engine = engine;
            t.gene = id;
            t.liked = st.like.mean;
            t.reference = std::clamp (gene.toNorm (gene.defaultValue), 0.0f, 1.0f);
            t.confidence = st.like.w / (st.like.w + confidenceHalf);
            if (std::fabs (t.liked - t.reference) < 0.08f) continue; // nothing learned beyond the default
            out.push_back (t);
        }
    std::stable_sort (out.begin(), out.end(), [] (const Trait& a, const Trait& b) {
        return a.confidence * std::fabs (a.liked - a.reference) > b.confidence * std::fabs (b.liked - b.reference);
    });
    if ((int) out.size() > count) out.resize ((size_t) std::max (0, count));
    return out;
}

void TasteProfile::reset()
{
    sounds.clear();
    engines.clear();
    clock = 0;
}

namespace
{
float r4 (float v) { return std::round (v * 10000.0f) / 10000.0f; }

float num (const json& j, const char* key, float def, float lo, float hi)
{
    const auto it = j.find (key);
    if (it == j.end() || ! it->is_number()) return def;
    const float v = it->get<float>();
    return std::isfinite (v) ? std::clamp (v, lo, hi) : def;
}
} // namespace

std::string TasteProfile::toJson() const
{
    json o = json::object();
    o["format"] = "ohpe-taste";
    o["version"] = formats::taste;
    o["learning"] = learning;
    o["curiosity"] = r4 (curiosity);
    json snd = json::object();
    for (const auto& [key, v] : sounds)
        snd[key] = { { "f", r4 (v.fitness) }, { "n", v.loads }, { "e", v.engine }, { "t", v.used } };
    o["sounds"] = std::move (snd);
    json eng = json::object();
    for (const auto& [engine, genes] : engines)
    {
        json gj = json::object();
        for (const auto& [id, st] : genes)
            gj[id] = json::array ({ r4 (st.like.w), r4 (st.like.mean), r4 (st.dislike.w), r4 (st.dislike.mean) });
        eng[std::to_string (engine)] = std::move (gj);
    }
    o["engines"] = std::move (eng);
    return o.dump (1);
}

bool TasteProfile::fromJson (const std::string& text)
try
{
    const bool keepLearning = learning;
    const float keepCuriosity = curiosity;
    reset();
    fileVersion = 0;
    const auto j = jsonutil::parse (text);
    const auto fmt = j.is_object() ? j.find ("format") : j.end();
    if (! j.is_object() || fmt == j.end() || ! fmt->is_string() || fmt->get<std::string>() != "ohpe-taste")
    {
        learning = keepLearning;
        curiosity = keepCuriosity;
        return false;
    }
    fileVersion = (int) num (j, "version", 0.0f, 0.0f, 1.0e6f);
    if (const auto it = j.find ("learning"); it != j.end() && it->is_boolean()) learning = it->get<bool>();
    curiosity = num (j, "curiosity", 0.35f, 0.0f, 1.0f);
    if (const auto it = j.find ("sounds"); it != j.end() && it->is_object())
        for (const auto& [key, v] : it->items())
        {
            if (! v.is_object() || key.empty() || key.size() > 256) continue;
            Sound snd;
            snd.fitness = num (v, "f", 0.0f, -20.0f, 20.0f);
            snd.loads = (int) num (v, "n", 0.0f, 0.0f, 1.0e6f);
            snd.engine = (int) num (v, "e", -1.0f, -1.0f, 1000.0f);
            const auto t = v.find ("t");
            snd.used = t != v.end() && t->is_number_unsigned() ? t->get<uint64_t>() : 0;
            clock = std::max (clock, snd.used);
            sounds[key] = snd;
        }
    if (const auto it = j.find ("engines"); it != j.end() && it->is_object())
        for (const auto& [ek, genes] : it->items())
        {
            char* end = nullptr;
            const long e = std::strtol (ek.c_str(), &end, 10);
            if (end == ek.c_str() || *end != 0 || e < 0 || e > 1000 || ! genes.is_object()) continue;
            for (const auto& [id, a] : genes.items())
            {
                if (! a.is_array() || a.size() != 4) continue;
                bool ok = true;
                float f[4] {};
                for (size_t k = 0; k < 4; ++k)
                {
                    ok = ok && a[k].is_number();
                    if (ok) f[k] = a[k].get<float>();
                    ok = ok && std::isfinite (f[k]);
                }
                if (! ok) continue;
                GeneStats gs;
                gs.like = { std::clamp (f[0], 0.0f, maxWeight), std::clamp (f[1], 0.0f, 1.0f) };
                gs.dislike = { std::clamp (f[2], 0.0f, maxWeight), std::clamp (f[3], 0.0f, 1.0f) };
                engines[(int) e][id] = gs;
            }
        }
    prune();
    return true;
}
catch (const std::exception&)
{
    reset();
    return false;
}
} // namespace ohpe::breed
