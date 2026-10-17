// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "plugin/Updater.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ohpe::ui
{
/**
    p27: the update view - an overlay on the plate like the library, opened by a click on the version legend /
    wordmark in the header.

    Left: the version on offer (number, channel, date) and its release notes in the UI language (scroll with the
    wheel). Right: CHECK NOW, UPDATE (download with a violet progress bar, CANCEL; then INSTALL / OPEN), SKIP THIS
    VERSION, REMIND LATER; the settings (automatic check once a day - off until switched on -, channel stable /
    preview, last check); the content packs on offer (INSTALL / UPDATE / ROLL BACK); PREVIOUS VERSION (rollback).
*/
class UpdateView : public juce::Component, private juce::ChangeListener
{
public:
    explicit UpdateView (Updater& u);
    ~UpdateView() override;

    void open();
    void close();
    std::function<void()> onClosed;
    /** The standalone may offer to quit after the installer opened; a plug-in asks to close the host. */
    void setStandalone (bool s) { standalone = s; }

    void paint (juce::Graphics& g) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;

    /** Release notes as plain lines ("### Heading" -> heading, "- item" -> bullet, emphasis marks removed). */
    static juce::String plainNotes (const juce::String& markdown);
    /** The action the primary key runs now ("check", "update", "cancel", "install"), for tests. */
    juce::String primaryAction() const;
    void runPrimary();

private:
    struct Hit
    {
        juce::Rectangle<float> area;
        std::function<void()> action;
    };
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    float unit() const;
    void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent, std::function<void()> action,
               bool enabled = true);
    void paintNotes (juce::Graphics& g, juce::Rectangle<float> area, const Updater::Snapshot& s);
    void install();

    Updater& updater;
    std::vector<Hit> hits;
    float scroll = 0.0f, maxScroll = 0.0f;
    bool standalone = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateView)
};
} // namespace ohpe::ui
