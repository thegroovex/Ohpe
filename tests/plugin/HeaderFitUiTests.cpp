// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p75: the display's header never shortens a title. Every mode title and every engine name (EN / DE), the widest
// mixer channel / tape track / drum pad, and the full key ("C# PHRYG" ... every scale, own key and host key, internal
// tempo and MIDI clock) are measured with the real fonts at every UI scale: the title fits its room (no ellipsis),
// the key keeps its full form, nothing overlaps, and the gravity ring stays big enough to click / drag.

#include <doctest/doctest.h>

#include "core/engines/EngineFactory.h"
#include "core/seq/ScaleChord.h"
#include "core/settings/UserSettings.h"
#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/Palette.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
juce::String primaryTitle (ui::Mode m, EngineType t)
{
    // what the header must always show in full: the engine name on the Synth page, the mode name elsewhere
    if (m == ui::Mode::Synth) return i18n::upper (i18n::pick (engineTypeInfo (t).nameEn, engineTypeInfo (t).nameDe));
    return i18n::upper (tr (ui::modeInfo (m).nameEn));
}
} // namespace

TEST_CASE ("p75 header: no mode title or engine name is cut, the full key fits (EN / DE, every UI scale)")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<OhpeEditor*> (ed.get());
    REQUIRE (e != nullptr);
    auto& d = e->displayComponent();
    auto& reg = p.core().params();

    // the widest key texts (two-letter roots, five-letter scale names); every root / scale once at the widest title
    const std::vector<std::pair<int, int>> someKeys { { 1, scales::Phrygian }, { 1, scales::HarmonicMinor }, { 6, scales::MelodicMinor }, { 10, scales::Chromatic } };
    std::vector<std::pair<int, int>> allKeys;
    for (int r = 0; r < 12; ++r)
        for (int s = 0; s < scales::numScales; ++s) allKeys.push_back ({ r, s });

    int checked = 0;
    for (float scale : settings::windowScales)
    {
        e->setSize (juce::roundToInt (OhpeEditor::designWidth * scale), juce::roundToInt (OhpeEditor::designHeight * scale));
        for (auto lang : { i18n::Language::English, i18n::Language::German })
        {
            e->setLanguage (lang);
            for (bool clock : { false, true })
            {
                reg.set (Core::Ids::syncSource, clock ? (float) Core::SyncMidiClock : (float) Core::SyncInternal);
                for (bool host : { false, true })
                    for (int m = 0; m < ui::numModes; ++m)
                    {
                        const auto mode = (ui::Mode) m;
                        if (! ui::modeInfo (mode).available) continue;
                        const int engines = mode == ui::Mode::Synth ? numEngineTypes : 1;
                        for (int t = 0; t < engines; ++t)
                        {
                            e->selectPart (0);
                            e->selectMode (mode);
                            if (mode == ui::Mode::Synth) e->selectEngine ((EngineType) t);
                            if (mode == ui::Mode::Mixer && d.onChannelClicked) d.onChannelClicked (7);
                            if (mode == ui::Mode::Tape) e->selectTapeTrack (2);
                            if (mode == ui::Mode::Drum) e->selectDrumPad (23);
                            const bool widest = scale == 1.0f && clock && mode == ui::Mode::Synth && (EngineType) t == EngineType::Wavetable;
                            for (const auto& [root, sc] : widest ? allKeys : someKeys)
                            {
                                if (host) p.core().setHostKey (root, sc);
                                else
                                {
                                    p.core().setHostKey (-1, 0);
                                    p.setKeyFromUi (root, sc);
                                }
                                e->advanceAnimation (0.02f);
                                (void) d.createComponentSnapshot (d.getLocalBounds()); // places the badges while painting
                                const auto fit = d.headerTitleFit();
                                const auto want = primaryTitle (mode, (EngineType) t);
                                const auto where = (lang == i18n::Language::German ? juce::String ("DE ") : juce::String ("EN ")) + juce::String (scale)
                                                   + " " + fit.text + " | " + d.headerKey().paintedText() + (host ? " host" : "") + (clock ? " clock" : "");
                                INFO (where.toStdString());
                                // the title: whole, never with an ellipsis, inside its room
                                CHECK (fit.text.contains (want));
                                CHECK_FALSE (fit.text.contains (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"))));
                                CHECK (fit.needed <= fit.room + 0.5f);
                                // the key: full form, as wide as its text needs
                                auto& k = d.headerKey();
                                const float hh = (float) k.getHeight();
                                CHECK_FALSE (k.isCompact());
                                CHECK ((float) k.getWidth() + 1.0f >= k.neededWidth (hh, false));
                                const float keyText = juce::GlyphArrangement::getStringWidth (ui::monoFont (hh * k.textSizeFraction (false)), k.headerText());
                                CHECK (keyText <= (float) k.getWidth() - hh * 0.75f);
                                // nothing overlaps: title and badges | gravity ring | key | TAP | tempo
                                CHECK (d.spaceBadgeArea().getRight() <= (float) (d.headerGravity().isVisible() ? d.headerGravity().getX() : k.getX()) + 0.5f);
                                if (d.headerGravity().isVisible())
                                {
                                    CHECK (d.headerGravity().getRight() <= k.getX());
                                    CHECK ((float) d.headerGravity().getWidth() >= hh * 0.6f); // still easy to click / drag
                                }
                                if (d.headerTap().isVisible()) CHECK (k.getRight() <= d.headerTap().getX());
                                CHECK (k.getRight() <= d.headerTempo().getX());
                                ++checked;
                            }
                        }
                    }
            }
        }
    }
    CHECK (checked > 200);
    p.core().setHostKey (-1, 0);
    reg.set (Core::Ids::syncSource, (float) Core::SyncInternal);
    e->setLanguage (i18n::Language::English);
    ed.reset();
}
