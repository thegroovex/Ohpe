// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

/**
    p82: tooltips and the help search.

    Every interactive control carries a short EN / DE tooltip: what it does, plus the MIDI learn hint where a right-click
    learns it. Components are TooltipClients; views that paint their controls themselves (the display, the settings, the
    library, the breeding view ...) are RegionProviders - one tooltip per painted control - and answer getTooltip() from
    the region under the mouse. A plugin test walks the component tree of every view and fails on a control without an
    English and a German tooltip.

    The help search (Settings > Help, Ctrl / Cmd + F) finds help topics, parameters and controls by English or German
    words - case, umlauts, word prefixes and one-letter typos do not matter - and jumps to the page or control.
*/
namespace ohpe::ui::tips
{
/** The text for the current language (both given here, so a tooltip never misses its translation). */
juce::String t (const char* en, const char* de);
/** "Right-click: MIDI learn" in the current language. */
juce::String learnHint();
/** `tip` and the MIDI learn hint on its own line. */
juce::String withLearn (const juce::String& tip);

/** One painted control of a RegionProvider: where it is (component pixels) and its tooltip (current language). */
struct Region
{
    juce::Rectangle<float> area;
    juce::String tip;
};

class RegionProvider
{
public:
    virtual ~RegionProvider() = default;
    /** Every interactive area the component shows now, most specific first (the first hit wins). */
    virtual std::vector<Region> tipRegions() = 0;
};

/** The tooltip of the first region that contains `p` (empty = none). */
juce::String regionTipAt (const std::vector<Region>& regions, juce::Point<float> p);

/** A component that takes mouse clicks but is no control itself (a backdrop, a container): the tooltip test skips it. */
void markPassive (juce::Component& c);
bool isPassive (const juce::Component& c);

// ---------------------------------------------------------------- controls of the plate (tooltips + search)
struct Control
{
    const char* key;              ///< stable key ("key.play"); the editor finds the component for it
    const char* nameEn, *nameDe;  ///< short name (search results)
    const char* tipEn, *tipDe;    ///< the tooltip (what it does)
};
const std::vector<Control>& controls();
const Control* control (const juce::String& key);
/** The control's tooltip in the current language (empty for an unknown key). */
juce::String controlTip (const char* key);
/** The control's name in the current language. */
juce::String controlName (const char* key);

// ---------------------------------------------------------------- the help search
enum class Kind { Topic = 0, Parameter, Control };

struct Entry
{
    Kind kind = Kind::Topic;
    juce::String titleEn, titleDe; ///< what the result list shows
    juce::String textEn, textDe;   ///< more words to match (description, keywords, where it is)
    juce::String target;           ///< Topic: help topic index; Parameter: parameter ID; Control: control key
    juce::String whereEn, whereDe; ///< where it is ("Synth > Filter"; shown under the title, matched like the text)
};

struct Result
{
    Entry entry;
    float score = 0.0f;
};

/** Lower case, umlauts folded (ä = ae, ß = ss), punctuation to spaces, single spaces. */
juce::String fold (const juce::String& text);
/** Damerau-Levenshtein distance (adjacent swaps count one), capped at `cap` + 1. */
int editDistance (const juce::String& a, const juce::String& b, int cap = 3);
/** How well `query` matches `text` (0 = no match, 1 = exact): every word of the query must match a word of the text -
    exactly, as its start, inside it, or with a typo (one letter for words of 4+ letters, two for 8+). */
float matchScore (const juce::String& text, const juce::String& query);
/** The best entries for `query` (titles count more than texts; both languages), best first. */
std::vector<Result> search (const std::vector<Entry>& index, const juce::String& query, int maxResults = 14);
/** "Help" / "Parameter" / "Control" in the current language (the tag on a result). */
juce::String kindName (Kind k);
} // namespace ohpe::ui::tips
