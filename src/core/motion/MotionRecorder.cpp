// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MotionRecorder.h"
#include "../model/FormatVersions.h"

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace ohpe
{
using nlohmann::json;

namespace
{
bool startsWith (const std::string& s, const char* prefix)
{
    const size_t n = std::char_traits<char>::length (prefix);
    return s.size() >= n && s.compare (0, n, prefix) == 0;
}
constexpr double holdSeconds = 0.2; ///< a touch lasts this long after the last change
} // namespace

MotionRecorder::Lane::Lane()
{
    for (auto& p : points) p.store (-1.0f, std::memory_order_relaxed);
}

MotionRecorder::MotionRecorder()
{
    for (auto& a : audio)
    {
        a.applied.fill (-1);
        a.lastPoint.fill (-1);
        for (auto& t : a.uiTouch) t.store (false, std::memory_order_relaxed);
    }
}

void MotionRecorder::registerParams (ParamRegistry& r, int parts)
{
    using namespace pd;
    for (int p = 1; p <= parts; ++p)
    {
        const auto n = std::to_string (p);
        const auto pp = "p" + n + ".motion.";
        const std::string P = "Part " + n + " ";
        auto add = [&] (ParamDescriptor d) { d.group = "part" + n + "/motion"; r.add (std::move (d)); };
        add (toggle (pp + "on", P + "Motion Play", P + "Bewegung abspielen", false));
        add (toggle (pp + "rec", P + "Motion Record", P + "Bewegung aufnehmen", false));
        add (integer (pp + "len", P + "Motion Length", P + "Bewegung Länge", 1, maxSteps, 16));
        add (continuous (pp + "amt", P + "Motion Depth", P + "Bewegung Tiefe", 0.0f, 1.0f, 1.0f, ParamUnit::Percent));
        add (toggle (pp + "clear", P + "Motion Clear", P + "Bewegung löschen", false));
    }
}

bool MotionRecorder::isExcluded (const std::string& id) noexcept
{
    return startsWith (id, "motion.") || id == "midich" || id == "quant" || id == "chord" || id == "mpe" || id == "mpe.bend"
        || id == "mpe.press" || id == "mpe.timbre";
}

void MotionRecorder::bind (const ParamRegistry& r, ParamSnapshot& s)
{
    reg = &r;
    snap = &s;
    for (int p = 0; p < numParts; ++p)
    {
        auto& pi = idx[(size_t) p];
        pi.prefix = "p" + std::to_string (p + 1) + ".";
        pi.on = r.indexOf (pi.prefix + "motion.on");
        pi.rec = r.indexOf (pi.prefix + "motion.rec");
        pi.len = r.indexOf (pi.prefix + "motion.len");
        pi.amt = r.indexOf (pi.prefix + "motion.amt");
        pi.clear = r.indexOf (pi.prefix + "motion.clear");
        pi.recordable.clear();
        for (int i = 0; i < r.size(); ++i)
        {
            const auto& d = r.descriptor (i);
            if (d.kind != ParamKind::Continuous || ! startsWith (d.id, pi.prefix.c_str())) continue;
            if (isExcluded (d.id.substr (pi.prefix.size()))) continue;
            pi.recordable.push_back (i);
        }
        audio[(size_t) p].prev.assign (pi.recordable.size(), 0.0f);
    }
}

void MotionRecorder::reset() noexcept
{
    for (int p = 0; p < numParts; ++p)
    {
        auto& a = audio[(size_t) p];
        a.touch.fill (0);
        a.lastPoint.fill (-1);
        a.source.fill (0.0f);
        for (auto& t : a.uiTouch) t.store (false, std::memory_order_relaxed);
        a.uiPos.store (-1.0f, std::memory_order_relaxed);
        clearOverlay (p);
    }
}

int MotionRecorder::lengthPoints (int part) const noexcept
{
    if (reg == nullptr) return 16 * pointsPerStep;
    const int i = idx[(size_t) part].len;
    const int steps = i >= 0 ? (int) std::lround (reg->get (i)) : 16;
    return std::clamp (steps, 1, maxSteps) * pointsPerStep;
}

float MotionRecorder::point (int part, int lane, int index) const noexcept
{
    if (part < 0 || part >= numParts || lane < 0 || lane >= numLanes || index < 0 || index >= maxPoints) return -1.0f;
    return lanes[(size_t) part][(size_t) lane].points[(size_t) index].load (std::memory_order_relaxed);
}

bool MotionRecorder::anyLane (int part) const noexcept
{
    for (int l = 0; l < numLanes; ++l)
        if (laneTarget (part, l) >= 0) return true;
    return false;
}

bool MotionRecorder::anyLane() const noexcept
{
    for (int p = 0; p < numParts; ++p)
        if (anyLane (p)) return true;
    return false;
}

void MotionRecorder::clearLane (int part, int lane) noexcept
{
    if (part < 0 || part >= numParts || lane < 0 || lane >= numLanes) return;
    auto& ln = lanes[(size_t) part][(size_t) lane];
    ln.target.store (-1, std::memory_order_release);
    for (auto& v : ln.points) v.store (-1.0f, std::memory_order_relaxed);
}

void MotionRecorder::clearPart (int part) noexcept
{
    for (int l = 0; l < numLanes; ++l) clearLane (part, l);
}

void MotionRecorder::clearAll() noexcept
{
    for (int p = 0; p < numParts; ++p) clearPart (p);
}

void MotionRecorder::setLane (int part, int lane, int registryIndex, const std::vector<float>& pts) noexcept
{
    if (part < 0 || part >= numParts || lane < 0 || lane >= numLanes) return;
    auto& ln = lanes[(size_t) part][(size_t) lane];
    ln.target.store (-1, std::memory_order_release);
    for (int i = 0; i < maxPoints; ++i)
    {
        float v = i < (int) pts.size() ? pts[(size_t) i] : -1.0f;
        if (! (v >= 0.0f)) v = -1.0f; // also NaN
        ln.points[(size_t) i].store (std::min (v, 1.0f), std::memory_order_relaxed);
    }
    ln.target.store (registryIndex, std::memory_order_release);
}

void MotionRecorder::clearOverlay (int part) noexcept
{
    auto& a = audio[(size_t) part];
    for (auto& ap : a.applied)
    {
        if (ap >= 0 && snap != nullptr) snap->clearMotion (ap, reg->get (ap));
        ap = -1;
    }
}

float MotionRecorder::valueAt (const Lane& lane, double pos, int lenPoints) const noexcept
{
    const double fl = std::floor (pos);
    const int i0 = ((int) fl % lenPoints + lenPoints) % lenPoints;
    const int i1 = (i0 + 1) % lenPoints;
    const float v0 = lane.points[(size_t) i0].load (std::memory_order_relaxed);
    if (v0 < 0.0f) return -1.0f;
    const float v1 = lane.points[(size_t) i1].load (std::memory_order_relaxed);
    if (v1 < 0.0f) return v0;
    const float f = (float) (pos - fl);
    return v0 + (v1 - v0) * f;
}

void MotionRecorder::writePoints (Lane& lane, int from, int to, float fromValue, float toValue, int lenPoints) noexcept
{
    // from (exclusive, -1 = only `to`) .. to (inclusive), forward around the loop
    int dist = from < 0 ? 0 : ((to - from) % lenPoints + lenPoints) % lenPoints;
    if (dist > lenPoints / 2 || dist > 4 * pointsPerStep) dist = 0; // a jump of the transport: no bridge
    if (dist == 0)
    {
        lane.points[(size_t) to].store (toValue, std::memory_order_relaxed);
        return;
    }
    for (int k = 1; k <= dist; ++k)
    {
        const float v = fromValue + (toValue - fromValue) * (float) k / (float) dist;
        lane.points[(size_t) ((from + k) % lenPoints)].store (v, std::memory_order_relaxed);
    }
}

void MotionRecorder::recordPart (int part, int pointIndex, int lenPoints, int numSamples) noexcept
{
    auto& pi = idx[(size_t) part];
    auto& a = audio[(size_t) part];
    auto& pl = lanes[(size_t) part];
    const int hold = (int) (holdSeconds * sr);

    // which parameters moved since the last block (a bulk change - preset, breeding - is no knob movement)
    std::array<int, bulkChange + 1> changed {};
    int numChanged = 0;
    for (size_t k = 0; k < pi.recordable.size(); ++k)
    {
        const float v = reg->get (pi.recordable[k]);
        if (! (v == a.prev[k]))
        {
            if (numChanged <= bulkChange) changed[(size_t) std::min (numChanged, bulkChange)] = pi.recordable[k];
            ++numChanged;
            a.prev[k] = v;
        }
    }
    if (numChanged > bulkChange) numChanged = 0;
    for (int c = 0; c < numChanged; ++c)
    {
        const int target = changed[(size_t) c];
        int lane = -1;
        for (int l = 0; l < numLanes && lane < 0; ++l)
            if (pl[(size_t) l].target.load (std::memory_order_relaxed) == target) lane = l;
        for (int l = 0; l < numLanes && lane < 0; ++l)
            if (pl[(size_t) l].target.load (std::memory_order_relaxed) < 0)
            {
                lane = l;
                for (auto& v : pl[(size_t) l].points) v.store (-1.0f, std::memory_order_relaxed);
                pl[(size_t) l].target.store (target, std::memory_order_release);
                a.lastPoint[(size_t) l] = -1;
            }
        if (lane >= 0) a.touch[(size_t) lane] = hold;
    }
    // touched lanes take the knob's value at the play position
    for (int l = 0; l < numLanes; ++l)
    {
        if (a.touch[(size_t) l] <= 0) { a.lastPoint[(size_t) l] = -1; continue; }
        auto& ln = pl[(size_t) l];
        const int target = ln.target.load (std::memory_order_relaxed);
        if (target < 0) { a.touch[(size_t) l] = 0; continue; }
        const float v = reg->getNormalised (target);
        const int last = a.lastPoint[(size_t) l] < lenPoints ? a.lastPoint[(size_t) l] : -1;
        writePoints (ln, last, pointIndex, a.lastNorm[(size_t) l], v, lenPoints);
        a.lastPoint[(size_t) l] = pointIndex;
        a.lastNorm[(size_t) l] = v;
        a.touch[(size_t) l] -= numSamples;
    }
}

void MotionRecorder::process (const Transport& tr, int numSamples) noexcept
{
    if (reg == nullptr || snap == nullptr) return;
    for (int p = 0; p < numParts; ++p)
    {
        auto& pi = idx[(size_t) p];
        auto& a = audio[(size_t) p];
        auto& pl = lanes[(size_t) p];
        const bool on = reg->get (pi.on) >= 0.5f, rec = reg->get (pi.rec) >= 0.5f, clear = reg->get (pi.clear) >= 0.5f;
        if (clear && ! a.wasClear) clearPart (p);
        a.wasClear = clear;
        if (rec && ! a.wasRec) // armed: what the knobs show now is not a movement
        {
            for (size_t k = 0; k < pi.recordable.size(); ++k) a.prev[k] = reg->get (pi.recordable[k]);
            a.touch.fill (0);
            a.lastPoint.fill (-1);
        }
        a.wasRec = rec;

        const int lenPoints = lengthPoints (p);
        const bool run = tr.playing && (on || rec);
        if (! run)
        {
            if (! rec) { a.touch.fill (0); a.lastPoint.fill (-1); }
            clearOverlay (p);
            a.source.fill (0.0f);
            for (auto& t : a.uiTouch) t.store (false, std::memory_order_relaxed);
            a.uiPos.store (-1.0f, std::memory_order_relaxed);
            continue;
        }
        // position in points: sixteenths x pointsPerStep, looping over the part's length (polymetric)
        double pos = std::fmod (std::max (0.0, tr.ppqAtBlockStart) * 4.0 * pointsPerStep, (double) lenPoints);
        if (! (pos >= 0.0 && pos < (double) lenPoints)) pos = 0.0;
        const int pointIndex = std::clamp ((int) pos, 0, lenPoints - 1);
        a.uiPos.store ((float) pos, std::memory_order_relaxed);
        if (rec) recordPart (p, pointIndex, lenPoints, numSamples);

        const float amt = std::clamp (reg->get (pi.amt), 0.0f, 1.0f);
        for (int l = 0; l < numLanes; ++l)
        {
            const auto& ln = pl[(size_t) l];
            const int target = ln.target.load (std::memory_order_acquire);
            const bool touched = rec && a.touch[(size_t) l] > 0;
            a.uiTouch[(size_t) l].store (touched, std::memory_order_relaxed);
            if (a.applied[(size_t) l] >= 0 && a.applied[(size_t) l] != target)
            {
                snap->clearMotion (a.applied[(size_t) l], reg->get (a.applied[(size_t) l]));
                a.applied[(size_t) l] = -1;
            }
            const float v = target >= 0 ? valueAt (ln, pos, lenPoints) : -1.0f;
            a.source[(size_t) l] = v >= 0.0f ? v : 0.0f;
            if (target < 0 || touched || v < 0.0f)
            {
                if (a.applied[(size_t) l] >= 0) snap->clearMotion (a.applied[(size_t) l], reg->get (a.applied[(size_t) l]));
                a.applied[(size_t) l] = -1;
                continue;
            }
            const auto& d = reg->descriptor (target);
            const float knob = d.toNormalised (reg->get (target));
            snap->setMotion (target, d.fromNormalised (knob + (v - knob) * amt));
            a.applied[(size_t) l] = target;
        }
    }
}

// ---------------------------------------------------------------- state
std::string MotionRecorder::toJson() const
{
    json parts = json::array();
    for (int p = 0; p < numParts; ++p)
    {
        json ls = json::array();
        for (int l = 0; l < numLanes; ++l)
        {
            const int t = laneTarget (p, l);
            if (t < 0 || reg == nullptr || t >= reg->size()) { ls.push_back (nullptr); continue; }
            const auto& id = reg->descriptor (t).id;
            json pts = json::array();
            int used = 0;
            for (int i = 0; i < maxPoints; ++i)
                if (point (p, l, i) >= 0.0f) used = i + 1;
            for (int i = 0; i < used; ++i)
            {
                const float v = point (p, l, i);
                pts.push_back (v >= 0.0f ? std::round (v * 10000.0f) / 10000.0f : -1.0f);
            }
            ls.push_back ({ { "target", id.substr (idx[(size_t) p].prefix.size()) }, { "points", pts } });
        }
        parts.push_back ({ { "lanes", ls } });
    }
    return json ({ { "version", formats::motion }, { "parts", parts } }).dump();
}

bool MotionRecorder::fromJson (const std::string& text)
{
    clearAll();
    if (reg == nullptr) return false;
    const auto j = json::parse (text, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || ! j.contains ("parts") || ! j["parts"].is_array()) return false;
    const auto& parts = j["parts"];
    for (int p = 0; p < numParts && p < (int) parts.size(); ++p)
    {
        const auto& pj = parts[(size_t) p];
        if (! pj.is_object() || ! pj.contains ("lanes") || ! pj["lanes"].is_array()) continue;
        const auto& ls = pj["lanes"];
        for (int l = 0; l < numLanes && l < (int) ls.size(); ++l)
        {
            const auto& lj = ls[(size_t) l];
            if (! lj.is_object() || ! lj.contains ("target") || ! lj["target"].is_string()) continue;
            const auto local = lj["target"].get<std::string>();
            const int t = reg->indexOf (idx[(size_t) p].prefix + local);
            if (t < 0 || isExcluded (local) || reg->descriptor (t).kind != ParamKind::Continuous) continue;
            std::vector<float> pts;
            if (lj.contains ("points") && lj["points"].is_array())
                for (const auto& v : lj["points"])
                    pts.push_back (v.is_number() ? v.get<float>() : -1.0f);
            setLane (p, l, t, pts);
        }
    }
    return true;
}
} // namespace ohpe
