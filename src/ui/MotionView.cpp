// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MotionView.h"

#include "ModFxViews.h"
#include "Palette.h"
#include "plugin/Localisation.h"

#include <cmath>

namespace ohpe::ui::motionview
{
using i18n::tr;

namespace
{
std::string partId (int part, const char* local) { return "p" + std::to_string (part + 1) + "." + local; }
bool flag (Core& core, int part, const char* local) { return core.params().get (partId (part, local)) >= 0.5f; }
constexpr float labelShare = 0.26f; ///< width of the lane names / visual area width
} // namespace

std::vector<std::string> motionParams (int part)
{
    return { partId (part, "motion.on"), partId (part, "motion.rec"), partId (part, "motion.len"), partId (part, "motion.amt") };
}

std::vector<std::string> motionTitles() { return { "Motion" }; }

juce::Rectangle<float> laneArea (juce::Rectangle<float> v, int lane)
{
    const float h = v.getHeight() / (float) MotionRecorder::numLanes;
    return { v.getX(), v.getY() + h * (float) lane, v.getWidth(), h };
}

int laneAt (juce::Rectangle<float> v, juce::Point<float> p)
{
    for (int l = 0; l < MotionRecorder::numLanes; ++l)
        if (laneArea (v, l).contains (p)) return l;
    return -1;
}

juce::String laneTitle (Core& core, int part, int lane)
{
    const int t = core.motion().laneTarget (part, lane);
    if (t < 0 || t >= core.params().size()) return tr ("Empty");
    const auto& d = core.params().descriptor (t);
    return modfx::compactName (d.id, d.nameEn, d.nameDe);
}

void paint (juce::Graphics& g, juce::Rectangle<float> v, Core& core, int part, float time)
{
    auto& m = core.motion();
    const bool on = flag (core, part, "motion.on"), rec = flag (core, part, "motion.rec");
    const bool active = on || rec;
    const int len = m.lengthPoints (part);
    const int steps = len / MotionRecorder::pointsPerStep;
    const float pos = m.uiPosition (part);
    const float alpha = active ? 1.0f : 0.5f;

    for (int l = 0; l < MotionRecorder::numLanes; ++l)
    {
        auto row = laneArea (v, l).reduced (0.0f, v.getHeight() * 0.012f);
        const auto col = colour::encoder (l);
        const int target = m.laneTarget (part, l);
        const bool touched = rec && m.uiTouched (part, l);
        auto label = row.removeFromLeft (v.getWidth() * labelShare).reduced (row.getHeight() * 0.06f, 0.0f);
        auto lane = row.reduced (row.getHeight() * 0.04f, row.getHeight() * 0.06f);
        const float rad = juce::jmin (5.0f, lane.getHeight() * 0.2f);

        // name: the lane number in its colour, the parameter (or "Empty")
        auto num = label.withWidth (label.getHeight() * 0.62f).withSizeKeepingCentre (label.getHeight() * 0.62f, label.getHeight() * 0.62f);
        if (target >= 0) { g.setColour (col.withAlpha (alpha)); g.fillRoundedRectangle (num, rad * 0.8f); }
        else { g.setColour (col.withAlpha (0.7f * alpha)); g.drawRoundedRectangle (num, rad * 0.8f, 1.0f); }
        g.setColour (target >= 0 ? colour::display : col.withAlpha (alpha));
        g.setFont (brandFont (num.getHeight() * 0.72f));
        g.drawText (juce::String (l + 1), num, juce::Justification::centred, false);
        g.setColour ((target >= 0 ? colour::displayText : colour::displayDim).withAlpha (alpha));
        g.setFont (labelFont (label.getHeight() * 0.36f));
        g.drawText (laneTitle (core, part, l), label.withTrimmedLeft (num.getWidth() + label.getHeight() * 0.2f), juce::Justification::centredLeft, true);

        // the lane: recessed field, sixteenth grid (beats brighter)
        g.setColour (colour::displayGrid.withAlpha (0.9f));
        g.fillRoundedRectangle (lane, rad);
        for (int s = 1; s < steps; ++s)
        {
            const float x = lane.getX() + lane.getWidth() * (float) s / (float) steps;
            g.setColour (colour::displayDim.withAlpha ((s % 4 == 0 ? 0.45f : 0.15f) * alpha));
            g.drawVerticalLine ((int) std::round (x), lane.getY() + 2.0f, lane.getBottom() - 2.0f);
        }
        if (touched) // the lane takes the knob: red rim, breathing
        {
            g.setColour (colour::signalRed.withAlpha (0.55f + 0.35f * std::sin (time * 8.0f)));
            g.drawRoundedRectangle (lane, rad, 1.6f);
        }

        // the recorded movement: filled curve, segments where nothing is recorded stay empty
        if (target >= 0)
        {
            const auto in = lane.reduced (1.5f, lane.getHeight() * 0.1f);
            juce::Path curve, fill;
            bool open = false;
            float lastX = 0.0f;
            for (int i = 0; i < len; ++i)
            {
                const float val = m.point (part, l, i);
                const float x = in.getX() + in.getWidth() * ((float) i + 0.5f) / (float) len;
                if (val < 0.0f)
                {
                    if (open) { fill.lineTo (lastX, in.getBottom()); fill.closeSubPath(); }
                    open = false;
                    continue;
                }
                const float y = in.getBottom() - val * in.getHeight();
                if (! open) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, in.getBottom()); fill.lineTo (x, y); }
                else { curve.lineTo (x, y); fill.lineTo (x, y); }
                open = true;
                lastX = x;
            }
            if (open) { fill.lineTo (lastX, in.getBottom()); fill.closeSubPath(); }
            g.setColour (col.withAlpha (0.18f * alpha));
            g.fillPath (fill);
            g.setColour (col.withAlpha (alpha));
            g.strokePath (curve, juce::PathStrokeType (juce::jmax (1.2f, lane.getHeight() * 0.035f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // playhead: green while playing, red while recording
        if (pos >= 0.0f && active)
        {
            const float x = lane.getX() + lane.getWidth() * juce::jlimit (0.0f, 1.0f, pos / (float) len);
            g.setColour (rec ? colour::signalRed : colour::playGreenLit);
            g.fillRect (x - 1.0f, lane.getY(), 2.0f, lane.getHeight());
        }
    }
}

juce::String statusLine (Core& core, int part)
{
    auto& m = core.motion();
    const bool on = flag (core, part, "motion.on"), rec = flag (core, part, "motion.rec");
    const int steps = m.lengthPoints (part) / MotionRecorder::pointsPerStep;
    int lanes = 0;
    for (int l = 0; l < MotionRecorder::numLanes; ++l) lanes += m.laneTarget (part, l) >= 0 ? 1 : 0;
    const auto loop = juce::String (steps) + " " + tr ("steps");
    if (rec && ! core.transportPlaying()) return tr ("Armed - press Play, then turn a knob");
    if (rec) return lanes < MotionRecorder::numLanes ? tr ("Recording - turn a knob of this part") + " · " + loop
                                                      : tr ("Recording - all lanes taken, overdub them") + " · " + loop;
    if (lanes == 0) return tr ("Press Rec, play, and turn knobs - each takes a lane");
    if (! on) return tr ("Off - Play loops the lanes") + " · " + loop;
    return juce::String (lanes) + " " + tr ("lanes") + " · " + loop;
}
} // namespace ohpe::ui::motionview
