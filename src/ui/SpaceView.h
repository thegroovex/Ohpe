// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// 1.0: the 3D spatialiser on screen - the head-centred space view of the display (every part's source as a moving
// dot with its trail), the headphones pictogram / "3D" badge that lights up when a part plays binaural 3D, and the
// Space pages of the Effects mode.

#include "OhpeCore.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <string>
#include <vector>

namespace ohpe::ui::space3d
{
/** How a part uses the spatialiser: not at all, binaural (headphone monitoring) or for speakers. */
enum class State { Off = 0, Binaural, Speakers };
/** The part's stage is on, or one of its rack slots holds an active Space 3D effect. Message thread. */
bool partUsesSpace (Core& core, int part);
State partState (Core& core, int part);
/** Master monitoring: headphones (binaural) or speakers. */
bool speakerMonitoring (Core& core);
/** Tooltip of the badge / pictogram (translated). */
juce::String badgeTooltip (State s);

/** The headphones pictogram (own drawing in the line style of the characters): band, two cups, sound arcs when lit.
    `glow` 0..1 adds a soft halo (active, pulsing). */
void drawHeadphones (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, float glow);
/** The speakers pictogram for the speaker-monitoring state (two cabinets with cones). */
void drawSpeakers (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink);
/** The badge in the display header: pictogram + "3D"; lit (ultraviolet, pulsing) for binaural, outlined for speakers,
    dim when off. `time` drives the pulse. */
void drawBadge (juce::Graphics& g, juce::Rectangle<float> area, State s, float time);

/** Space view pages of the Effects mode for one part (four per page) and their titles. */
std::vector<std::string> spaceParams (int part);
std::vector<std::string> spaceTitles();

/**
    The head-centred 3D view: a perspective of the listener's sphere (floor rings, the median arc, the head - with
    headphones while monitoring binaurally, speakers in front otherwise), every part's source as a dot in its colour
    with a fading trail and a drop line to the floor, nearer = larger. UI thread.
*/
class Scene
{
public:
    /** Samples the positions of the four part stages (call ~30 times a second). */
    void update (Core& core, float seconds);
    void clear();
    /** `focus` = the edit part (drawn on top, labelled), `correlation` of the master (NaN = hidden). */
    void paint (juce::Graphics& g, juce::Rectangle<float> area, Core& core, int focus, juce::Point<float> parallax,
                float correlation) const;
    /** One source only (a Space 3D rack slot): position in metres, level 0..1. */
    static void paintSingle (juce::Graphics& g, juce::Rectangle<float> area, const space::Vec3& position, float level,
                             juce::Colour colour, bool speakers, float time);

    static constexpr int trailLength = 40;

private:
    struct Trail
    {
        std::array<space::Vec3, trailLength> p {};
        int head = 0, count = 0;
        float level = 0.0f;
        bool active = false;
    };
    std::array<Trail, Core::numParts> trails {};
    float time = 0.0f, yaw = 0.0f;
};
} // namespace ohpe::ui::space3d
