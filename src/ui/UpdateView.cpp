// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UpdateView.h"

#include "Palette.h"
#include "plugin/Localisation.h"

#include <cmath>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

juce::String sizeText (uint64_t bytes)
{
    if (bytes == 0) return {};
    return juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB";
}

juce::String dateText (int64_t unixSeconds)
{
    if (unixSeconds <= 0) return tr ("never");
    return juce::Time (unixSeconds * 1000).formatted ("%Y-%m-%d %H:%M");
}
} // namespace

UpdateView::UpdateView (Updater& u) : updater (u)
{
    updater.addChangeListener (this);
    setWantsKeyboardFocus (true);
}

UpdateView::~UpdateView() { updater.removeChangeListener (this); }

void UpdateView::open()
{
    scroll = 0.0f;
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    repaint();
}

void UpdateView::close()
{
    if (! isVisible()) return;
    setVisible (false);
    if (onClosed) onClosed();
}

float UpdateView::unit() const { return (float) getWidth() / 114.0f; }

juce::String UpdateView::plainNotes (const juce::String& markdown)
{
    juce::StringArray out;
    auto lines = juce::StringArray::fromLines (markdown.replace ("\r", ""));
    for (auto line : lines)
    {
        line = line.replace ("**", "").replace ("__", "").replace ("`", "");
        auto t = line.trimStart();
        if (t.startsWith ("#"))
        {
            t = t.trimCharactersAtStart ("#").trim();
            if (! out.isEmpty() && out[out.size() - 1].isNotEmpty()) out.add ({});
            out.add (i18n::upper (t));
            continue;
        }
        if (t.startsWith ("- ") || t.startsWith ("* "))
            t = juce::String::fromUTF8 ("\xe2\x80\xa2 ") + t.substring (2).trim();
        if (t.isEmpty() && (out.isEmpty() || out[out.size() - 1].isEmpty())) continue;
        out.add (t);
    }
    while (! out.isEmpty() && out[out.size() - 1].isEmpty()) out.remove (out.size() - 1);
    return out.joinIntoString ("\n");
}

juce::String UpdateView::primaryAction() const
{
    const auto s = updater.snapshot();
    if (s.state == Updater::State::Downloading) return "cancel";
    if (s.state == Updater::State::Ready) return "install";
    if (s.hasRelease) return "update";
    return "check";
}

void UpdateView::runPrimary()
{
    const auto a = primaryAction();
    if (a == "cancel") updater.cancel();
    else if (a == "install") install();
    else if (a == "update") updater.downloadUpdate();
    else updater.checkNow();
    repaint();
}

void UpdateView::install()
{
    if (! updater.openDownloaded()) { repaint(); return; }
    if (standalone)
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("Ohpe")
                                          .withMessage (tr ("The installer is open. Quit Ohpe now so it can replace the program?"))
                                          .withButton (tr ("Quit Ohpe")).withButton (tr ("Later")).withAssociatedComponent (this),
                                      [] (int r) {
                                          if (r == 1)
                                              if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
                                      });
    else
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon).withTitle ("Ohpe")
                                          .withMessage (tr ("The installer is open. Save your project and quit the host program before the installer continues."))
                                          .withButton ("OK").withAssociatedComponent (this),
                                      nullptr);
}

void UpdateView::chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent,
                       std::function<void()> action, bool enabled)
{
    const float u = unit();
    g.setColour (on ? accent : colour::displayGrid.brighter (0.15f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour ((on ? colour::display : colour::displayText).withAlpha (enabled ? 1.0f : 0.35f));
    g.setFont (labelFont (juce::jmin (r.getHeight() * 0.52f, u * 1.5f)));
    g.drawFittedText (text, r.reduced (u * 0.5f, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    if (enabled && action) hits.push_back ({ r, std::move (action) });
}

void UpdateView::paintNotes (juce::Graphics& g, juce::Rectangle<float> area, const Updater::Snapshot& s)
{
    const float u = unit();
    const auto& r = s.release;
    const juce::String notes = plainNotes (i18n::current() == i18n::Language::German ? str (r.notesDe) : str (r.notesEn));
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.setLineSpacing (u * 0.25f);
    for (const auto& line : juce::StringArray::fromLines (notes))
    {
        const bool heading = line.isNotEmpty() && line == i18n::upper (line) && ! line.startsWith (juce::String::fromUTF8 ("\xe2\x80\xa2"));
        as.append (line + "\n", heading ? brandFont (u * 1.45f) : labelFont (u * 1.4f), heading ? colour::displayText : colour::displayText.withAlpha (0.78f));
    }
    juce::TextLayout layout;
    layout.createLayout (as, area.getWidth());
    maxScroll = juce::jmax (0.0f, layout.getHeight() - area.getHeight());
    scroll = juce::jlimit (0.0f, maxScroll, scroll);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());
    layout.draw (g, area.withHeight (layout.getHeight() + 1.0f).translated (0, -scroll));
    if (maxScroll > 0.0f)
    {
        const float h = area.getHeight() * area.getHeight() / layout.getHeight();
        const float y = area.getY() + (area.getHeight() - h) * (scroll / maxScroll);
        g.setColour (colour::displayDim.withAlpha (0.6f));
        g.fillRoundedRectangle ({ area.getRight() - u * 0.35f, y, u * 0.3f, h }, u * 0.15f);
    }
}

void UpdateView::paint (juce::Graphics& g)
{
    hits.clear();
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    const float du = (float) getWidth() / 1152.0f;
    g.setGradientFill (juce::ColourGradient (colour::bezel, 0, b.getY(), colour::bezelLow, 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, radius::panel * du);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), radius::inset * du);

    const auto s = updater.snapshot();
    const auto prefs = updater.settings();
    const auto cur = Updater::currentVersion();
    const bool busy = s.state == Updater::State::Checking || s.state == Updater::State::Downloading;

    // ---- top row: title, installed version, status, close
    g.setColour (colour::displayText);
    g.setFont (brandFont (u * 2.4f));
    g.drawText (i18n::upper (tr ("Updates")), juce::Rectangle<float> (u * 2.0f, u * 1.4f, u * 16.0f, u * 3.2f), juce::Justification::centredLeft, true);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.5f));
    g.drawText (i18n::upper (tr ("Installed")) + " " + str (cur.toString()), juce::Rectangle<float> (u * 18.0f, u * 1.4f, u * 20.0f, u * 3.2f),
                juce::Justification::centredLeft, true);
    if (s.message.isNotEmpty())
    {
        const bool alert = s.state == Updater::State::Failed || s.state == Updater::State::Offline;
        g.setColour (alert ? colour::signalRed : s.state == Updater::State::Available || s.state == Updater::State::Ready ? colour::sunYellow : colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        g.drawFittedText (tr (s.message) + (s.detail.isNotEmpty() && s.state != Updater::State::Downloading ? "  " + s.detail : juce::String()),
                          juce::Rectangle<float> (u * 38.0f, u * 1.4f, u * 61.0f, u * 3.2f).toNearestInt(), juce::Justification::centredRight, 1, 0.8f);
    }
    chip (g, { b.getWidth() - u * 12.5f, u * 1.4f, u * 10.5f, u * 3.2f }, i18n::upper (tr ("Close")), false, colour::displayText, [this] { close(); });

    // ---- left: the version on offer and its notes
    const juce::Rectangle<float> card { u * 2.0f, u * 6.0f, u * 60.0f, b.getHeight() - u * 8.0f };
    g.setColour (colour::displayGrid);
    g.fillRoundedRectangle (card, u * 0.8f);
    auto inner = card.reduced (u * 1.6f, u * 1.2f);
    if (s.hasRelease)
    {
        const auto& r = s.release;
        g.setColour (colour::sunYellow);
        g.fillRoundedRectangle ({ inner.getX(), inner.getY() + u * 0.5f, u * 0.5f, u * 3.0f }, u * 0.25f); // yellow: something to act on
        g.setColour (colour::displayText);
        g.setFont (brandFont (u * 3.2f));
        g.drawText (str (r.version.toString()), juce::Rectangle<float> (inner.getX() + u * 1.4f, inner.getY(), u * 24.0f, u * 4.0f), juce::Justification::centredLeft, true);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        const juce::String meta = i18n::upper (r.channel == update::Channel::Preview ? tr ("Preview") : tr ("Stable")) + (r.date.empty() ? juce::String() : "   " + str (r.date));
        g.drawText (meta, juce::Rectangle<float> (inner.getX() + u * 26.0f, inner.getY(), inner.getWidth() - u * 26.0f, u * 4.0f), juce::Justification::centredRight, true);
        paintNotes (g, inner.withTrimmedTop (u * 5.4f), s);
    }
    else
    {
        g.setColour (colour::displayText);
        g.setFont (brandFont (u * 2.2f));
        const juce::String headline = s.state == Updater::State::UpToDate ? tr ("Ohpe is up to date")
                                    : s.state == Updater::State::Checking ? tr ("Checking for updates")
                                    : s.state == Updater::State::Offline  ? tr ("No connection to the update server")
                                                                          : tr ("Not checked yet");
        g.drawFittedText (headline, inner.withHeight (u * 4.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        g.drawFittedText (tr ("Ohpe looks for new versions in the public release list of the project. Nothing about you or your music is sent.")
                              + "\n\n" + tr ("Check now, or switch on the daily check on the right."),
                          inner.withTrimmedTop (u * 5.4f).withHeight (u * 12.0f).toNearestInt(), juce::Justification::topLeft, 6, 1.0f);
    }

    // ---- right: actions
    const float x = u * 65.0f, w = b.getWidth() - x - u * 2.0f;
    float y = u * 6.0f;
    const float h = u * 3.4f;
    if (s.state == Updater::State::Downloading)
    {
        const juce::Rectangle<float> bar { x, y, w - u * 12.0f, h };
        g.setColour (colour::displayGrid.brighter (0.15f));
        g.fillRoundedRectangle (bar, u * 0.6f);
        g.setColour (colour::ultraviolet); // violet: running
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (u * 1.2f, bar.getWidth() * (float) juce::jlimit (0.0, 1.0, s.progress))), u * 0.6f);
        g.setColour (colour::displayText);
        g.setFont (labelFont (u * 1.4f));
        g.drawText (juce::String (juce::roundToInt (s.progress * 100.0)) + " %   " + s.detail, bar.reduced (u * 1.0f, 0), juce::Justification::centredLeft, true);
        chip (g, { x + w - u * 11.0f, y, u * 11.0f, h }, i18n::upper (tr ("Cancel")), false, colour::signalRed, [this] { updater.cancel(); });
    }
    else
    {
        chip (g, { x, y, u * 14.0f, h }, i18n::upper (tr ("Check now")), false, colour::displayText, [this] { updater.checkNow(); }, ! busy);
        if (s.state == Updater::State::Ready)
            chip (g, { x + u * 15.0f, y, w - u * 15.0f, h }, i18n::upper (tr ("Install")) + "  " + s.detail, true, colour::sunYellow, [this] { install(); });
        else if (s.hasRelease)
        {
            const auto* a = s.release.assetFor (update::currentPlatform(), standalone ? "app" : "plugin");
            chip (g, { x + u * 15.0f, y, w - u * 15.0f, h },
                  i18n::upper (tr ("Update to")) + " " + str (s.release.version.toString()) + (a != nullptr ? "   " + sizeText (a->size) : juce::String()), true,
                  colour::sunYellow, [this] { updater.downloadUpdate(); }, ! busy);
        }
    }
    y += h + u * 1.0f;
    const bool offer = s.hasRelease && s.state != Updater::State::Downloading;
    chip (g, { x, y, (w - u * 1.0f) * 0.5f, h * 0.85f }, i18n::upper (tr ("Skip this version")), false, colour::displayText, [this] { updater.skipThisVersion(); }, offer);
    chip (g, { x + (w + u * 1.0f) * 0.5f, y, (w - u * 1.0f) * 0.5f, h * 0.85f }, i18n::upper (tr ("Remind me later")), false, colour::displayText,
          [this] { updater.remindLater(); }, offer);

    // settings
    y += h + u * 1.6f;
    auto legend = [&] (const juce::String& text, float yy) {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.2f));
        g.drawText (i18n::upper (text), juce::Rectangle<float> (x, yy, w, u * 1.6f), juce::Justification::centredLeft, true);
    };
    auto label = [&] (const juce::String& text, float yy) {
        g.setColour (colour::displayText);
        g.setFont (labelFont (u * 1.4f));
        g.drawFittedText (text, juce::Rectangle<float> (x, yy, w - u * 21.0f, h * 0.85f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    };
    legend (tr ("Settings"), y);
    y += u * 2.0f;
    label (tr ("Check automatically (once a day)"), y);
    chip (g, { x + w - u * 20.0f, y, u * 9.5f, h * 0.85f }, i18n::upper (tr ("Off")), ! prefs.autoCheck, colour::displayText, [this] { updater.setAutoCheck (false); });
    chip (g, { x + w - u * 9.5f, y, u * 9.5f, h * 0.85f }, i18n::upper (tr ("On")), prefs.autoCheck, colour::sunYellow, [this] { updater.setAutoCheck (true); });
    y += h;
    label (tr ("Channel"), y);
    chip (g, { x + w - u * 20.0f, y, u * 9.5f, h * 0.85f }, i18n::upper (tr ("Stable")), prefs.channel == update::Channel::Stable, colour::sunYellow,
          [this] { updater.setChannel (update::Channel::Stable); });
    chip (g, { x + w - u * 9.5f, y, u * 9.5f, h * 0.85f }, i18n::upper (tr ("Preview")), prefs.channel == update::Channel::Preview, colour::sunYellow,
          [this] { updater.setChannel (update::Channel::Preview); });
    y += h;
    label (tr ("Last check"), y);
    g.setColour (colour::displayDim);
    g.drawText (dateText (prefs.lastCheck), juce::Rectangle<float> (x + w - u * 20.0f, y, u * 20.0f, h * 0.85f), juce::Justification::centredRight, true);

    // content packs
    y += h + u * 0.8f;
    legend (tr ("Content packs"), y);
    y += u * 2.0f;
    if (s.packs.empty())
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.3f));
        g.drawText (tr ("No content packs offered"), juce::Rectangle<float> (x, y, w, h * 0.85f), juce::Justification::centredLeft, true);
        y += h;
    }
    for (size_t i = 0; i < s.packs.size() && i < 3; ++i)
    {
        const auto& row = s.packs[i];
        g.setColour (colour::displayText);
        g.setFont (labelFont (u * 1.4f));
        const juce::String name = i18n::current() == i18n::Language::German ? str (row.pack.nameDe) : str (row.pack.nameEn);
        juce::String info = str (row.pack.version);
        if (row.installedVersion.isNotEmpty() && row.state == update::PackState::UpdateAvailable) info = row.installedVersion + juce::String::fromUTF8 (" \xe2\x86\x92 ") + info;
        g.drawFittedText (name + "  " + info, juce::Rectangle<float> (x, y, w - u * 21.0f, h * 0.85f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        const juce::String id = str (row.pack.id);
        const bool thisBusy = s.busyPack == id;
        switch (row.state)
        {
            case update::PackState::NotInstalled:
                chip (g, { x + w - u * 9.5f, y, u * 9.5f, h * 0.85f }, i18n::upper (thisBusy ? tr ("Loading") : tr ("Install")), false, colour::sunYellow,
                      [this, id] { updater.installPack (id); }, ! busy);
                break;
            case update::PackState::UpdateAvailable:
                chip (g, { x + w - u * 9.5f, y, u * 9.5f, h * 0.85f }, i18n::upper (thisBusy ? tr ("Loading") : tr ("Update")), true, colour::sunYellow,
                      [this, id] { updater.installPack (id); }, ! busy);
                break;
            case update::PackState::Installed:
                g.setColour (colour::displayDim);
                g.drawText (tr ("Installed"), juce::Rectangle<float> (x + w - u * 9.5f, y, u * 9.5f, h * 0.85f), juce::Justification::centred, true);
                break;
            case update::PackState::NeedsNewerApp:
                g.setColour (colour::displayDim);
                g.drawFittedText (tr ("Needs a newer Ohpe"), juce::Rectangle<float> (x + w - u * 20.0f, y, u * 20.0f, h * 0.85f).toNearestInt(),
                                  juce::Justification::centredRight, 1, 0.8f);
                break;
        }
        if (row.canRollBack && row.state != update::PackState::NeedsNewerApp)
            chip (g, { x + w - u * 20.0f, y, u * 10.0f, h * 0.85f }, i18n::upper (tr ("Roll back")), false, colour::displayText,
                  [this, id] { updater.rollbackPack (id); }, ! busy);
        y += h;
    }

    // rollback of the program, footer
    const float footY = b.getHeight() - u * 5.6f;
    if (s.hasPrevious)
        chip (g, { x, footY - h, w, h * 0.85f }, i18n::upper (tr ("Previous version")) + " " + str (s.previous.version.toString()), false,
              colour::displayText, [this] { updater.downloadPrevious(); }, ! busy);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.2f));
    g.drawFittedText (tr ("No telemetry: Ohpe only reads the public release list. Your presets and settings are never touched."),
                      juce::Rectangle<float> (x, footY + u * 0.4f, w, u * 3.2f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
}

void UpdateView::mouseDown (const juce::MouseEvent& e)
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            auto action = it->action; // the action may repaint and rebuild the hit list
            if (action) action();
            repaint();
            return;
        }
}

void UpdateView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    scroll = juce::jlimit (0.0f, maxScroll, scroll - w.deltaY * unit() * 12.0f);
    repaint();
}

bool UpdateView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    if (k == juce::KeyPress::returnKey) { runPrimary(); return true; }
    return false;
}
} // namespace ohpe::ui
