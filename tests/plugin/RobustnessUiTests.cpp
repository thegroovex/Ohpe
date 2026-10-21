// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// p63: a damaged host / standalone state never takes the plug-in down (setStateInformation with truncated,
// corrupted and oversized blobs), and the crash-safe start's notice (restore offer, EN / DE).

#include <doctest/doctest.h>

#include <cmath>
#include <random>

#include "plugin/Localisation.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace ohpe;

namespace
{
bool processFinite (OhpeProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), 256);
    bool finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i) finite = finite && std::isfinite (buffer.getSample (c, i));
    }
    return finite;
}

juce::AudioProcessorParameter* param (OhpeProcessor& p, const juce::String& id)
{
    for (auto* x : p.getParameters())
        if (auto* w = dynamic_cast<juce::AudioProcessorParameterWithID*> (x); w != nullptr && w->paramID == id) return x;
    return nullptr;
}
} // namespace

TEST_CASE ("robustness: damaged host states are refused or loaded, the plug-in keeps running")
{
    OhpeProcessor src;
    src.prepareToPlay (48000.0, 256);
    REQUIRE (param (src, "p1.vol") != nullptr);
    param (src, "p1.vol")->setValueNotifyingHost (0.3f);
    juce::MemoryBlock good;
    src.getStateInformation (good);
    REQUIRE (good.getSize() > 100);

    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    p.setStateInformation (good.getData(), (int) good.getSize());
    CHECK_FALSE (p.lastStateLoadFailed());

    // nothing / garbage: refused
    p.setStateInformation (nullptr, 0);
    CHECK (p.lastStateLoadFailed());
    const char junk[] = "\x01\x02 not a state";
    p.setStateInformation (junk, (int) sizeof (junk));
    CHECK (p.lastStateLoadFailed());

    std::mt19937 rng (6304);
    auto pick = [&] (size_t n) { return n == 0 ? (size_t) 0 : std::uniform_int_distribution<size_t> (0, n - 1) (rng); };
    int failed = 0, loaded = 0;
    for (int i = 0; i < 60; ++i)
    {
        std::string blob (static_cast<const char*> (good.getData()), good.getSize());
        switch (i % 4)
        {
            case 0: blob.resize (pick (blob.size())); break; // truncated
            case 1: for (int k = 0; k < 16; ++k) blob[pick (blob.size())] = (char) pick (256); break; // corrupted
            case 2: blob.insert (pick (blob.size()), std::string (1 << 16, '[')); break; // oversized / deep
            default: // wrong types: a value replaced by an array / object / string
            {
                const auto at = blob.find (':', pick (blob.size()));
                if (at != std::string::npos) blob.insert (at + 1, i % 8 == 3 ? "[" : "{\"x\":");
                break;
            }
        }
        CHECK_NOTHROW (p.setStateInformation (blob.data(), (int) blob.size()));
        (p.lastStateLoadFailed() ? failed : loaded) += 1;
    }
    MESSAGE ("damaged states: " << failed << " refused, " << loaded << " loaded");
    CHECK (failed > 0);
    CHECK (processFinite (p, 20));
    // a good state still loads afterwards
    p.setStateInformation (good.getData(), (int) good.getSize());
    CHECK_FALSE (p.lastStateLoadFailed());
    CHECK (param (p, "p1.vol")->getValue() == doctest::Approx (0.3f).epsilon (0.01));
}

TEST_CASE ("crash-safe start: the restore notice (EN / DE), RESTORE runs the restore, START FRESH dismisses")
{
    OhpeProcessor p;
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto& e = *dynamic_cast<OhpeEditor*> (ed.get());
    auto& n = e.recoveryNoticeComponent();
    CHECK_FALSE (n.isOpen());
    e.showPendingRecoveryOffer(); // nothing offered: nothing shown
    CHECK_FALSE (n.isOpen());

    int restored = 0;
    p.offerRecovery (1, [&] { ++restored; });
    CHECK (p.hasRecoveryOffer());
    e.showPendingRecoveryOffer();
    CHECK_FALSE (p.hasRecoveryOffer());
    REQUIRE (n.isOpen());
    CHECK (n.reason() == ui::RecoveryNotice::Reason::UncleanExit);
    CHECK (n.numKeys() == 2);
    i18n::setCurrent (i18n::Language::German);
    CHECK (n.title() == juce::String::fromUTF8 ("Die letzte Sitzung wurde nicht richtig beendet"));
    CHECK (n.keyText (0) == "Wiederherstellen");
    CHECK (n.keyText (1) == "Neu beginnen");
    i18n::setCurrent (i18n::Language::English);
    CHECK (n.keyText (0) == "Restore");
    // the strip only takes clicks on itself: the plate stays usable around it
    const auto panel = n.panelArea();
    CHECK (n.hitTest ((int) panel.getCentreX(), (int) panel.getCentreY()));
    CHECK_FALSE (n.hitTest (5, n.getHeight() - 5));
    CHECK (panel.getWidth() < 0.6f * (float) n.getWidth());
    n.restore();
    CHECK (restored == 1);
    CHECK_FALSE (n.isOpen());

    p.offerRecovery (1, [&] { ++restored; });
    e.showPendingRecoveryOffer();
    n.dismiss();
    CHECK (restored == 1);
    CHECK_FALSE (n.isOpen());

    // a damaged state: only OK
    p.offerRecovery (2);
    e.showPendingRecoveryOffer();
    CHECK (n.numKeys() == 1);
    CHECK (n.keyText (0) == "OK");
    CHECK (n.message().contains ("start sound"));
    n.restore(); // = OK
    CHECK_FALSE (n.isOpen());
}
