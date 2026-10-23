// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "LivingView.h"
#include "Pages.h"
#include "SpaceView.h"
#include "KeyControl.h"
#include "TempoControl.h"
#include "Visuals.h"
#include "Widgets.h"
#include "plugin/PluginProcessor.h"

#include <array>
#include <functional>

namespace ohpe::ui
{
/**
    The central display: mode header, the animated pictogram character of the current engine /
    mode, the four encoder rows in their colours and a context strip (voices, steps, mixer,
    tape tracks, drum pads) with soft keys.

    0.3: the parameter bars are controls themselves - drag a bar (right / up = more; Shift, Cmd
    or Ctrl = fine), click on a bar to jump, double-click to reset, right-click for MIDI learn.
*/
class Display : public juce::Component,
                public juce::TooltipClient
{
public:
    explicit Display (OhpeProcessor& p);
    ~Display() override;

    /** 0.7: what the left half of the display shows. The stereo scope also appears by itself
        on the Mid/Side FX slot pages and the master stereo pages. */
    enum class Visual { Character = 0, Scope, Terrain, Space, Living, numVisuals }; ///< 1.0: Space = the 3D view; p26: Living = the sound as a 3D object
    void setVisual (Visual v);
    Visual visual() const noexcept { return userVisual; }
    Visual shownVisual() const;
    /** 0.7 setting: animated 3D visuals (spectrum terrain, parallax). Off = static display. */
    void setAnimatedVisuals (bool on);
    /** p32: the skin changed - the cached bezel / glass layers are drawn again. */
    void skinChanged() { baseLayer = {}; gridLayer = {}; glassLayer = {}; repaint(); }
    bool animatedVisuals() const noexcept { return animated; }
    /** The scope tap source the display needs right now (ScopeTap::Source). */
    int scopeSource() const;
    const DisplayVisuals& visualsState() const noexcept { return visuals; }
    /** Paints the stereo scope of the current scope source into `area` (for other views, e.g. an
        FX rack slot visual of a Mid/Side slot). */
    void paintStereoScope (juce::Graphics& g, juce::Rectangle<float> area) const;
    /** p33: the Expression pages - the response of each shown velocity / aftertouch slot and the live input. */
    bool showsExpression() const;
    void paintExpression (juce::Graphics& g, juce::Rectangle<float> area) const;
    /** p26: the living object (state, tests) and the region the last tick asked to repaint for it (empty = it rested:
        nothing of the object was repainted). */
    const ohpe::living::Shape& livingShape() const noexcept { return livingObj; }
    juce::Rectangle<float> livingRepaintArea() const noexcept { return livingDirty; }
    bool livingOnScreen() const;
    /** Called when the user changes the view or the animation setting on the display. */
    std::function<void()> onVisualSettingsChanged;

    struct SoftKey
    {
        int id = 0;
        juce::String label; ///< translated
        bool enabled = true;
        bool active = false;
    };

    void setMode (Mode m, int page, int numPagesInMode, const std::vector<juce::String>& encoderIds, const juce::String& pageTitle = {});
    void setVirtuals (const std::array<VirtualPtr, 4>& v) { virtuals = v; repaint(); }
    void setContext (const PageContext& c)
    {
        if (c.modRouting != ctx.modRouting || c.part != ctx.part) modHistory.clear(); // 0.7
        ctx = c;
        repaint();
    }
    void setMixerChannel (int ch) { ctx.mixerChannel = ch; repaint(); }
    void setStepPage (int p) { stepPage = p; repaint(); }
    void setLockStep (int step) { lockStep = step; repaint(); } ///< -1 = no step selected
    void setSoftKeys (std::vector<SoftKey> keys) { softKeys = std::move (keys); repaint(); }
    const std::vector<SoftKey>& softKeyList() const noexcept { return softKeys; } ///< p42: tests
    void setStatus (const juce::String& text) { status = text; repaint(); }
    void flash (const juce::String& message); ///< short message on the hint line
    void tick (float seconds); ///< advances the animation (called by the editor's timer)

    std::function<void (int delta)> onPageChange;
    std::function<void (int step)> onStepClicked;   ///< toggle
    std::function<void (int step)> onStepSelected;  ///< Shift-click / right-click: edit + lock
    std::function<void (int page)> onStepPageClicked;
    std::function<void (int channel)> onChannelClicked;
    std::function<void (SeqView view)> onSeqViewClicked;
    std::function<void (int track)> onTrackClicked;
    std::function<void (int pad)> onPadClicked;
    std::function<void (int id)> onSoftKey;
    std::function<void()> onLockBadgeClicked;
    std::function<void (int view)> onViewTabClicked;     ///< 0.7: LFO / Effects mode view tabs
    std::function<void (int routing)> onModRoutingClicked; ///< 0.7: a line of the routing list
    std::function<void (int slot)> onFxSlotClicked;      ///< 0.7: a block of the FX chain
    std::function<void (int from, int to)> onFxSlotMoved; ///< 0.7: a block dropped on another one
    std::function<void()> onFxVisualClicked;              ///< p55: the selected slot's picture (opens the FX editor)
    /** p55: the selected slot's picture on the strip of the Rack / Master views (empty in other views). */
    juce::Rectangle<float> fxVisualArea() const;
    std::function<void()> onGravityClicked;              ///< p70: the Key Gravity ring on the header (opens its page)
    std::function<void()> onSpaceBadgeClicked;           ///< 1.0: the 3D badge in the header (opens the Space view)
    std::function<void (int corner)> onMorphCornerClicked; ///< p16: a corner chip of the morph pad (assign a sound)
    /** p16: the morph pad and a corner chip (tests / screenshots). */
    juce::Rectangle<float> morphPadArea() const;
    juce::Rectangle<float> morphCornerArea (int corner) const;
    /** 1.0: where the 3D badge is (header; tests / screenshots). */
    juce::Rectangle<float> spaceBadgeArea() const;
    /** p75: the header's sizes (fractions of the header height): title font, the part badge and the 3D badge with the
        gaps around them, the page arrows. One row: title, badges ... gravity ring, key, TAP, tempo, page arrows. */
    static constexpr float headerTitleFont = 0.56f, headerGap = 0.3f, headerPartBadgeW = 1.2f, header3dBadgeW = 1.4f,
                           headerArrowW = 0.95f, headerRightTrim = 3.4f, headerTapW = 1.2f;
    /** p75: the title the header shows now, the width it needs (with the real font) and the room it has: it never
        shortens with an ellipsis (tests check needed <= room for every mode, engine, language and UI scale). */
    struct HeaderTitleFit { juce::String text; float needed = 0.0f, room = 0.0f; bool compact = false; };
    HeaderTitleFit headerTitleFit();
    /** p11: the transport buttons of the tape page (TransportButton) and the arm dots of the tracks. */
    std::function<void (int button)> onTransportClicked;
    std::function<void (int track)> onTrackArmClicked;
    /** p42: the loop readout left of the tape page's keys (opens / closes the loop keys); the loop edges on the lanes
        can be dragged (Shift: off the grid). */
    std::function<void()> onLoopReadoutClicked;
    void setTapeLoopKeys (bool shown) { tapeLoopKeysShown = shown; repaint(); }
    juce::Rectangle<float> loopReadoutArea() const;
    juce::Rectangle<float> tapeLanesArea() const; ///< the lanes without their labels (time axis of the tape)
    float tapeLoopEdgeX (bool inEdge) const;      ///< x of loop in / out on the lanes
    int tapeLoopEdgeAt (juce::Point<float> p) const noexcept; ///< 1 = loop in, 2 = loop out, 0 = none
    enum TransportButton { TxToStart = 0, TxStop, TxPlay, TxRecord, TxLoop, numTransportButtons };
    /** p11: shortcuts shown in the tooltips (standalone app only). */
    void setShowShortcuts (bool show) { shortcuts = show; }

    juce::String getTooltip() override; ///< p11: depends on what the mouse is over (1.0: the 3D badge too)

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override; ///< p39: the count-in countdown
    void resized() override;
    /** p39: the tempo on the header (editable) and the TAP key next to it; refreshed by the editor's timer. */
    TempoReadout& headerTempo() noexcept { return tempoHeader; }
    TapKey& headerTap() noexcept { return tapHeader; }
    KeyReadout& headerKey() noexcept { return keyHeader; } ///< p45: key, scale, Scale Lock on the header
    GravityControl& headerGravity() noexcept { return gravHeader; } ///< p70: Key Gravity ring next to the key
    /** p45: the voice strip of the default pages (tests / screenshots); the chord shown under the dots. */
    VoiceStripLayout voiceStripLayout() const;
    juce::String shownChord() const { return chordText; }
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    // layout (also used by tests)
    juce::Rectangle<float> rowArea (int i) const;
    juce::Rectangle<float> barArea (int i) const;
    /** p41: one text line of the strip below the rows (what is drawn where; tests check that none overlap). */
    struct StripLine
    {
        juce::String text;
        juce::Rectangle<float> area;
        juce::Font font;
        juce::Colour colour;
        float minScale = 1.0f; ///< drawFittedText's minimum horizontal scale
    };
    /** p41: the strip lines of the 3D Space view (motion or the switch-on hint, monitoring). */
    std::vector<StripLine> spaceStripLines() const;
    /** p41: segment `segment` of `numSegments` of a segmented switch row (the 3D switch). */
    juce::Rectangle<float> segmentArea (int row, int segment, int numSegments) const;
    int segmentAt (int row, juce::Point<float> p) const; ///< -1 = not on a segment of a switch row
    juce::Rectangle<float> stepCell (int i) const;
    juce::Rectangle<float> softKeyArea (int index) const;
    int softKeyRows() const noexcept; ///< p38: 2 on the tape page with more than eight keys
    juce::Rectangle<float> padCell (int pad) const;
    juce::Rectangle<float> trackLane (int track) const;
    juce::Rectangle<float> seqTab (int view) const;
    juce::Rectangle<float> characterArea() const; ///< 0.7: the character / scope / terrain area
    juce::Rectangle<float> transportButton (int b) const; ///< p11: tape page transport buttons
    juce::Rectangle<float> trackArmArea (int track) const; ///< p11: the arm dot of a track lane
    /** p11: the one-line hint of the tape page ("Pick a track, press REC"), translated. */
    juce::String tapeHint() const;

private:
    juce::Rectangle<float> headerArea() const;
    juce::Rectangle<float> stripArea() const;
    juce::Rectangle<float> midArea() const;
    juce::Rectangle<float> rowsArea() const;
    juce::Rectangle<float> captionArea() const;
    juce::Rectangle<float> stepPageTab (int i) const;
    juce::Rectangle<float> channelCell (int i) const;
    juce::Rectangle<float> pageArrow (bool right) const;
    juce::Rectangle<float> softKeyRow() const;
    bool hasSoftKeys() const noexcept { return ! softKeys.empty(); }

    int numViewTabs() const noexcept;
    juce::String viewTabName (int v) const;
    int selectedViewTab() const noexcept;
    bool showsModView() const noexcept;
    bool showsRackView() const noexcept;
    bool showsSpaceView() const noexcept; ///< 1.0: Effects mode, view Space
    bool showsBodyView() const noexcept;  ///< p19: Effects mode, view Body (the resonator)
    bool showsMorphView() const noexcept; ///< p16: LFO mode, view Morph
    bool showsMotionView() const noexcept; ///< p22: LFO mode, view Motion
    bool showsPadsView() const noexcept;   ///< p25: Effects mode, view Pads
    bool showsHarmonyView() const noexcept; ///< p24: LFO mode, view Harmony
    bool showsVoiceView() const noexcept;   ///< p68: LFO mode, view Voice
    void setMorphPosition (juce::Point<float> p); ///< p16: pad drag
    juce::Rectangle<float> visualArea() const; ///< 0.7: left of the rows (routing list / FX chain)
    FxRack& shownRack() const;
    void paintRows (juce::Graphics& g);
    void paintStrip (juce::Graphics& g, juce::Rectangle<float> area);
    void paintStepStrip (juce::Graphics& g, juce::Rectangle<float> area);
    void paintSeqViewStrip (juce::Graphics& g, juce::Rectangle<float> area);
    void paintTapeStrip (juce::Graphics& g, juce::Rectangle<float> area);
    void paintTapeTransport (juce::Graphics& g, juce::Rectangle<float> area); ///< p11
    void paintTapeHeaderStatus (juce::Graphics& g, juce::Rectangle<float> rightOf, float height); ///< p11: on every page
    void paintDrumStrip (juce::Graphics& g, juce::Rectangle<float> area);
    void paintSoftKeys (juce::Graphics& g);
    void paintHint (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text);
    void paintSegments (juce::Graphics& g, int row, const ParamDescriptor& d, int selected, juce::Colour c); ///< p41
    void updateTapeOverview();
    void renderLayers();
    void showVisualMenu();
    int forcedScopeSource() const;

    int rowAt (juce::Point<float> p) const;
    float rowValue (int row) const;
    void setRowValue (int row, float norm);
    void resetRow (int row);

    OhpeProcessor& proc;
    Mode mode = Mode::Synth;
    PageContext ctx;
    int page = 0, pages = 1, stepPage = 0, lockStep = -1;
    std::vector<juce::String> ids;
    std::array<VirtualPtr, 4> virtuals {};
    juce::String pageTitle, status, flashText;
    float flashTime = 0.0f;
    std::vector<SoftKey> softKeys;
    float time = 0.0f, energy = 0.0f;

    int dragRow = -1;
    int dragSlot = -1;              ///< 0.7: FX block being dragged
    bool morphDrag = false;         ///< p16: dragging the morph pad
    int heldPad = -1;               ///< p25: the performance pad held by the mouse
    int loopDrag = 0;               ///< p42: 1 = dragging loop in, 2 = loop out
    int64_t loopDragShownEnd = 0;   ///< p42: the time axis stays put during a drag
    bool tapeLoopKeysShown = false; ///< p42
    int64_t tapeShownEndFrames() const;
    void paintLoopReadout (juce::Graphics& g);
    std::vector<float> modHistory;  ///< 0.7: recent values of the selected routing
    BarDrag drag;

    // 0.7: visuals and depth
    DisplayVisuals visuals;
    space3d::Scene spaceScene; ///< 1.0
    ohpe::living::Shape livingObj;               ///< p26
    juce::Rectangle<float> livingDirty, livingLast; ///< p26: last repaint region, last object bounds
    juce::Rectangle<float> badgeRect; ///< 1.0: the 3D badge (header, set while painting)
    Visual userVisual = Visual::Character;
    bool animated = true;
    int lastSource = -2;
    juce::Point<float> parallax, parallaxTarget;
    juce::Image baseLayer, gridLayer, glassLayer; ///< bezel + glass, dot grid (parallax), inner shadow + sheen

    // tape overview (peaks per track over the used length), refreshed about twice a second
    static constexpr int overviewPoints = 96;
    std::array<std::array<float, overviewPoints>, TapeMachine::numTracks> overview {};
    int64_t overviewFrames = 0;
    float overviewAge = 1.0e9f;

    // p11: tooltips (last mouse position), shortcut hints
    juce::Point<float> lastMouse;
    bool shortcuts = false;

    // p39: tempo on the header, TAP
    TempoReadout tempoHeader { proc, TempoReadout::Style::Header };
    TapKey tapHeader { proc };
    KeyReadout keyHeader { proc }; // p45
    GravityControl gravHeader { proc }; // p70
    juce::String chordText, chordDetail; ///< p45: the chord that sounds (or sounded last) and its position
    bool chordSounding = false, keyHost = false;
    void updateChord();
    void placeTempo();
    juce::String headerTitle (bool compact); ///< p45: "SYNTH / VOCAL" (p75 compact: "VOCAL", "MIXER 3", "TAPE 2", "DRUM 7")
    int headerLeftX() const { return gravHeader.isVisible() ? gravHeader.getX() : keyHeader.getX(); } ///< p70: the leftmost header control
    float headerBadgesWidth() const;                 ///< p45: part badge + 3D badge right of the title
    juce::String keyLaidOut;                         ///< p45: key text + title the header was laid out for
    int shownCountIn = 0;
    // p79: idle display - nothing sounds, runs or changes: repaint at 10 instead of 30 frames per second
    uint32_t lastChangeCounter = 0;
    int idleTicks = 0;
    bool idleFrameSkip (bool settled);
};
} // namespace ohpe::ui
