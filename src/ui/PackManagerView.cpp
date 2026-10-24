// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PackManagerView.h"

#include "Palette.h"
#include "plugin/Localisation.h"
#include "update/PackFormat.h"

namespace ohpe::ui
{
using i18n::tr;

namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
} // namespace

PackManagerView::PackManagerView (Updater& u) : updater (u) { updater.addChangeListener (this); }

PackManagerView::~PackManagerView() { updater.removeChangeListener (this); }

juce::String PackManagerView::sizeText (uint64_t bytes)
{
    if (bytes < 1024 * 1024) return juce::String ((int) ((bytes + 1023) / 1024)) + " KB";
    return juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB";
}

void PackManagerView::chooseAndInstall()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Install pack"), juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                   juce::String ("*") + update::packFileExtension + ";*.zip");
    juce::Component::SafePointer<PackManagerView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile()) safe->install (f);
    });
}

void PackManagerView::chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent,
                            std::function<void()> action, bool enabled)
{
    const float u = unit();
    g.setColour (on ? accent : colour::displayGrid.brighter (0.15f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour ((on ? colour::display : colour::displayText).withAlpha (enabled ? 1.0f : 0.35f));
    g.setFont (labelFont (juce::jmin (r.getHeight() * 0.52f, u * 1.4f)));
    g.drawFittedText (text, r.reduced (u * 0.4f, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    if (enabled && action) hits.push_back ({ r, text, std::move (action), tipNow });
}

void PackManagerView::paint (juce::Graphics& g)
{
    hits.clear();
    tipNow = {};
    rows.clear();
    const float u = unit(), h = u * 3.2f, row = u * 3.9f;
    const auto a = getLocalBounds().toFloat();
    const auto s = updater.snapshot();
    const bool busy = updater.busy();
    const bool german = i18n::current() == i18n::Language::German;
    float y = a.getY();

    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.2f));
    g.drawText (i18n::upper (tr ("Content packs")), juce::Rectangle<float> (a.getX(), y, a.getWidth() * 0.5f, u * 1.6f), juce::Justification::centredLeft, true);
    // the last pack message (install / remove / why it failed)
    const bool packMessage = s.busyPack.isNotEmpty() || s.message.containsIgnoreCase ("pack") || s.message == "Install cancelled";
    if (packMessage && s.message.isNotEmpty())
    {
        g.setColour (s.state == Updater::State::Failed ? colour::alertInk.brighter (0.6f) : colour::sunYellow);
        g.setFont (labelFont (u * 1.3f));
        g.drawFittedText (tr (s.message) + (s.detail.isNotEmpty() ? "  " + s.detail : juce::String()),
                          juce::Rectangle<float> (a.getCentreX() - u * 10.0f, y, a.getWidth() * 0.5f + u * 10.0f, u * 1.6f).toNearestInt(),
                          juce::Justification::centredRight, 1, 0.8f);
    }
    y += u * 2.2f;

    const auto packs = updater.installedPacks();
    uint64_t used = 0;
    for (const auto& p : packs) used += p.bytes;
    rowTip ("Install a content pack file (.ohpepack)", "Eine Inhaltspaket-Datei (.ohpepack) installieren");
    chip (g, { a.getX(), y, u * 20.0f, h }, i18n::upper (busy && s.busyPack.isNotEmpty() ? tr ("Installing") : tr ("Install pack") + juce::String::fromUTF8 (" \xe2\x80\xa6")),
          true, colour::sunYellow, [this] { chooseAndInstall(); }, ! busy);
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawFittedText (tr ("or drop an .ohpepack file on the window"), juce::Rectangle<float> (a.getX() + u * 21.5f, y, u * 36.0f, h).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);
    g.drawText (sizeText (used) + " / " + sizeText (update::contentQuotaBytes), juce::Rectangle<float> (a.getRight() - u * 26.0f, y, u * 26.0f, h),
                juce::Justification::centredRight, true);
    y += row + u * 0.6f;

    // header
    const float cName = a.getX(), cVer = a.getX() + u * 30.0f, cSize = a.getX() + u * 38.0f, cItems = a.getX() + u * 46.0f,
                cLic = a.getX() + u * 58.0f, cDel = a.getRight() - u * 13.0f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.1f));
    auto head = [&] (const juce::String& t, float x, float w) { g.drawText (i18n::upper (t), juce::Rectangle<float> (x, y, w, u * 1.6f), juce::Justification::centredLeft, true); };
    head (tr ("Pack"), cName, u * 29.0f);
    head (tr ("Version"), cVer, u * 8.0f);
    head (tr ("Size"), cSize, u * 8.0f);
    head (tr ("Content"), cItems, u * 12.0f);
    head (tr ("License"), cLic, u * 16.0f);
    y += u * 2.0f;
    g.setColour (colour::displayGrid);
    g.fillRect (juce::Rectangle<float> (a.getX(), y - u * 0.3f, a.getWidth(), 1.0f));

    if (packs.empty())
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        g.drawText (tr ("No content packs installed"), juce::Rectangle<float> (a.getX(), y + u * 0.4f, a.getWidth(), h), juce::Justification::centredLeft, true);
        y += row;
    }
    for (const auto& p : packs)
    {
        if (y + h > a.getBottom() - u * 5.0f) break;
        const auto& info = p.info;
        const auto name = str (german ? info.nameDe : info.nameEn);
        const auto kind = update::kindFromName (info.kind);
        const auto items = juce::String (p.items) + " " + (kind == update::PackKind::Wavetables ? tr ("tables") : tr ("presets"));
        const auto lic = (info.license.paid() ? tr ("Paid") : tr ("Free")) + "  /  " + (info.signature.present() ? tr ("Signed") : tr ("Unsigned"));
        rows.add (name + "  " + str (info.version) + "  " + sizeText (p.bytes) + "  " + items + "  " + lic);

        g.setColour (colour::displayText);
        g.setFont (labelFont (u * 1.5f));
        g.drawFittedText (name, juce::Rectangle<float> (cName, y, u * 29.0f, h * 0.6f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.15f));
        const auto desc = str (german ? info.descriptionDe : info.descriptionEn);
        g.drawFittedText (desc.isNotEmpty() ? desc : str (info.id), juce::Rectangle<float> (cName, y + h * 0.58f, u * 29.0f, h * 0.42f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (colour::displayText);
        g.setFont (labelFont (u * 1.35f));
        g.drawText (str (info.version), juce::Rectangle<float> (cVer, y, u * 8.0f, h), juce::Justification::centredLeft, true);
        g.drawText (sizeText (p.bytes), juce::Rectangle<float> (cSize, y, u * 8.0f, h), juce::Justification::centredLeft, true);
        g.drawText (items, juce::Rectangle<float> (cItems, y, u * 12.0f, h), juce::Justification::centredLeft, true);
        g.setColour (info.license.paid() ? colour::sunYellow : colour::displayText);
        g.drawFittedText (lic, juce::Rectangle<float> (cLic, y, cDel - cLic - u * 1.0f, h).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        const auto id = str (info.id);
        const bool armed = armedRemove == id;
        rowTip ("Remove this pack (click twice)", "Dieses Paket entfernen (zweimal klicken)");
        chip (g, { cDel, y + u * 0.2f, u * 13.0f, h - u * 0.4f }, i18n::upper (armed ? tr ("Remove?") : tr ("Remove")), armed, colour::signalRed,
              [this, id, armed] {
                  if (! armed) { armedRemove = id; repaint(); return; }
                  armedRemove.clear();
                  updater.removePack (id);
                  repaint();
              },
              ! busy);
        y += row;
    }

    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.25f));
    g.drawFittedText (tr ("Packs are read-only: your own presets and wavetables are never touched. License and signature are shown as the pack states them; they are not checked yet."),
                      juce::Rectangle<float> (a.getX(), a.getBottom() - u * 4.2f, a.getWidth(), u * 4.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
}

void PackManagerView::mouseDown (const juce::MouseEvent& e)
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            auto action = it->action;
            if (action) action();
            repaint();
            return;
        }
    if (armedRemove.isNotEmpty())
    {
        armedRemove.clear();
        repaint();
    }
}

juce::StringArray PackManagerView::chipTexts()
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    juce::Graphics g (img);
    paint (g);
    juce::StringArray out;
    for (const auto& h : hits) out.add (h.text);
    return out;
}

bool PackManagerView::clickChip (const juce::String& text)
{
    chipTexts();
    for (const auto& h : hits)
        if (h.text == text)
        {
            auto action = h.action;
            if (action) action();
            repaint();
            return true;
        }
    return false;
}
// ---------------------------------------------------------------- p82: tooltips of the painted controls
juce::String PackManagerView::getTooltip()
{
    const auto p = getMouseXYRelative().toFloat();
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (p)) return it->tip;
    return {};
}

std::vector<tips::Region> PackManagerView::tipRegions()
{
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
        juce::Graphics g (img);
        paint (g); // the hit list of what is shown now
    }
    std::vector<tips::Region> r;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) r.push_back ({ it->area, it->tip });
    return r;
}

} // namespace ohpe::ui
