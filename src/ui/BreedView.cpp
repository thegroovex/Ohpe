// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "BreedView.h"

#include "Palette.h"
#include "engines/EngineFactory.h"
#include "plugin/Localisation.h"

#include <algorithm>
#include <cmath>

namespace ohpe::ui
{
using i18n::tr;

namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

juce::String engineName (int type)
{
    if (type < 0 || type >= numEngineTypes) return {};
    const auto& inf = engineTypeInfo ((EngineType) type);
    return i18n::pick (inf.nameEn, inf.nameDe);
}

const juce::String& heartFull()
{
    static const juce::String s = juce::String::fromUTF8 ("\xe2\x99\xa5");
    return s;
}
const juce::String& heartEmpty()
{
    static const juce::String s = juce::String::fromUTF8 ("\xe2\x99\xa1");
    return s;
}

/** Parent colours: A paper white, B violet; a blend lies between. */
juce::Colour parentColour (float fromB) { return colour::displayText.interpolatedWith (colour::ultraviolet, juce::jlimit (0.0f, 1.0f, fromB)); }

/** Brightness tint of a thumbnail: dark = violet, middle = rose, bright = yellow. */
juce::Colour brightnessColour (float b)
{
    b = juce::jlimit (0.0f, 1.0f, b);
    return b < 0.5f ? colour::ultraviolet.interpolatedWith (colour::rose, b * 2.0f) : colour::rose.interpolatedWith (colour::sunYellow, (b - 0.5f) * 2.0f);
}

const char* choiceLabel (breed::Choice c)
{
    switch (c)
    {
        case breed::Choice::A: return "A";
        case breed::Choice::B: return "B";
        case breed::Choice::Blend: return "Mix";
        default: return "Auto";
    }
}
} // namespace

// ------------------------------------------------------------------------------------------------ worker
class BreedView::Worker : public juce::Thread
{
public:
    struct Kid
    {
        uint64_t seed = 0;
        breed::Genome genome;
        bool match = true;
    };
    struct Job
    {
        int serial = 0;
        breed::Genome a, b;
        std::vector<Kid> kids;
    };
    struct Result
    {
        int serial = 0;
        uint64_t seed = 0;
        breed::Genome genome;
        breed::Audition::Sketch sketch;
        bool matched = false;
    };

    Worker() : juce::Thread ("Ohpe breeding") { startThread(); }
    ~Worker() override
    {
        signalThreadShouldExit();
        notify();
        stopThread (8000);
    }

    void submit (Job j)
    {
        {
            const juce::ScopedLock sl (lock);
            pending = std::move (j);
            hasJob = true;
        }
        notify();
    }

    std::vector<Result> take()
    {
        const juce::ScopedLock sl (lock);
        auto r = std::move (results);
        results.clear();
        return r;
    }

    bool idle() const
    {
        const juce::ScopedLock sl (lock);
        return ! hasJob && ! busy;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            Job job;
            {
                const juce::ScopedLock sl (lock);
                if (hasJob)
                {
                    job = std::move (pending);
                    hasJob = false;
                    busy = true;
                }
            }
            if (! busy)
            {
                wait (250);
                continue;
            }
            if (audition == nullptr) audition = std::make_unique<breed::Audition>();
            const auto sa = audition->render (job.a), sb = audition->render (job.b);
            float target = std::max (sa.rmsDb, sb.rmsDb);
            if (sa.rmsDb > -90.0f && sb.rmsDb > -90.0f) target = 0.5f * (sa.rmsDb + sb.rmsDb);
            for (auto& k : job.kids)
            {
                if (threadShouldExit()) break;
                {
                    const juce::ScopedLock sl (lock);
                    if (hasJob) break; // a newer generation: start over
                }
                Result r;
                r.serial = job.serial;
                r.seed = k.seed;
                r.genome = k.genome;
                r.matched = k.match && target > -90.0f;
                r.sketch = r.matched ? audition->matchLevel (r.genome, target) : audition->render (r.genome);
                const juce::ScopedLock sl (lock);
                results.push_back (std::move (r));
            }
            const juce::ScopedLock sl (lock);
            busy = false;
        }
    }

private:
    juce::CriticalSection lock;
    Job pending;
    bool hasJob = false, busy = false;
    std::vector<Result> results;
    std::unique_ptr<breed::Audition> audition;
};

// ------------------------------------------------------------------------------------------------ view
BreedView::BreedView (OhpeProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    ses = std::make_unique<breed::Session> (breed::Schema::forPart (proc.core().params(), 0));
    const auto saved = proc.core().stateSection ("breed");
    if (! saved.empty()) ses->fromJson (saved);
    setVisible (false);
}

BreedView::~BreedView()
{
    stopTimer();
    if (noteOn >= 0) proc.keyboardState.noteOff (noteChannel, noteOn, 0.0f);
}

float BreedView::unit() const { return (float) getWidth() / 114.0f; }

uint64_t BreedView::nextSeed()
{
    const juce::int64 base = seedBase != 0 ? seedBase : juce::Time::currentTimeMillis();
    return (uint64_t) base * 1000003ull + (uint64_t) (++seedCounter);
}

breed::Genome BreedView::currentGenome() const
{
    proc.editRegistry ([] (Core&) {}); // host values -> registry
    return breed::fromRegistry (ses->schema(), proc.core().params(), proc.presets().editPart());
}

void BreedView::open (int libraryEntry)
{
    auto& presets = proc.presets();
    presets.rescan();
    presets.beginPreview();
    before = currentGenome();
    beforeName = presets.currentName();
    status.clear();
    selected = -1;
    if (! ses->parent (0).set) ses->setParent (0, beforeName.toStdString(), before);
    if (libraryEntry >= 0)
    {
        setParentCurrent (0);
        setParentFromLibrary (1, libraryEntry);
        breedNew();
    }
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    generationChanged();
    startTimerHz (20);
}

void BreedView::close (bool keep)
{
    if (! isVisible()) return;
    if (noteOn >= 0) { proc.keyboardState.noteOff (noteChannel, noteOn, 0.0f); noteOn = -1; }
    auto& presets = proc.presets();
    if (keep) presets.endPreview();
    else presets.revertPreview();
    store();
    stopTimer();
    setVisible (false);
    if (onClosed) onClosed();
}

void BreedView::store() { proc.core().setStateSection ("breed", ses->toJson()); }

// ---- parents ----------------------------------------------------------------------------------------------------------
void BreedView::setParentCurrent (int which)
{
    if (before.empty()) before = currentGenome(), beforeName = proc.presets().currentName();
    ses->setParent (which, beforeName.toStdString(), before);
    repaint();
}

void BreedView::setParentFromLibrary (int which, int entry)
{
    auto& presets = proc.presets();
    if (entry < 0 || entry >= (int) presets.entries().size()) return;
    preset::Preset p;
    if (! preset::fromJson (presets.read (presets.entries()[(size_t) entry]), p) || p.meta.kind != preset::Kind::Part) return;
    ses->setParent (which, p.meta.name, breed::fromParams (ses->schema(), p.params));
    repaint();
}

void BreedView::setParentFromChild (int which, int child)
{
    ses->promote (child, which);
    repaint();
}

void BreedView::randomParent (int which)
{
    const auto& list = proc.presets().entries();
    std::vector<int> parts;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].meta.kind == preset::Kind::Part) parts.push_back (i);
    if (parts.empty()) return;
    breed::Rng rng (nextSeed());
    setParentFromLibrary (which, parts[(size_t) rng.below ((int) parts.size())]);
}

// ---- breeding ---------------------------------------------------------------------------------------------------------
void BreedView::breedNew()
{
    if (! ses->ready())
    {
        status = tr ("Choose parent B first (or roll the die).");
        repaint();
        return;
    }
    ses->breedParents (nextSeed());
    selected = -1;
    status.clear();
    generationChanged();
}

void BreedView::breedAgain()
{
    if (ses->empty()) { breedNew(); return; }
    ses->breedAgain (nextSeed());
    selected = -1;
    status.clear();
    generationChanged();
}

void BreedView::undo()
{
    ses->undo();
    selected = -1;
    generationChanged();
}

void BreedView::redo()
{
    ses->redo();
    selected = -1;
    generationChanged();
}

void BreedView::setAmount (float a)
{
    ses->amount = juce::jlimit (0.0f, 1.0f, a);
    repaint();
}

void BreedView::setGroupChoice (int group, breed::Choice c)
{
    if (group >= 0 && group < breed::numChoiceGroups) ses->options.choice[(size_t) group] = c;
    repaint();
}

void BreedView::generationChanged()
{
    static int serialCounter = 0;
    const int serial = ++serialCounter;
    if (const auto* g = ses->current())
    {
        Worker::Job job;
        job.serial = serial;
        job.a = g->a.genome;
        job.b = g->b.genome;
        for (const auto& c : g->children)
            if (sketches.count (c.seed) == 0) job.kids.push_back ({ c.seed, c.genome, ! c.matched });
        if (! job.kids.empty() && job.a.size() == job.b.size() && ! job.a.empty())
        {
            if (worker == nullptr) worker = std::make_unique<Worker>();
            worker->submit (std::move (job));
        }
    }
    store();
    repaint();
}

bool BreedView::hasSketch (int i) const
{
    const auto* g = ses->current();
    return g != nullptr && i >= 0 && i < (int) g->children.size() && sketches.count (g->children[(size_t) i].seed) > 0;
}

breed::Audition::Sketch BreedView::sketch (int i) const
{
    const auto* g = ses->current();
    if (g == nullptr || i < 0 || i >= (int) g->children.size()) return {};
    const auto it = sketches.find (g->children[(size_t) i].seed);
    return it != sketches.end() ? it->second : breed::Audition::Sketch {};
}

bool BreedView::waitForSketches (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        timerCallback();
        const auto* g = ses->current();
        bool all = g != nullptr;
        if (g != nullptr)
            for (int i = 0; i < (int) g->children.size(); ++i) all = all && hasSketch (i);
        if (all) return true;
        juce::Thread::sleep (20);
    }
    return false;
}

void BreedView::timerCallback()
{
    bool changed = false;
    if (noteOn >= 0 && --noteTicks <= 0)
    {
        proc.keyboardState.noteOff (noteChannel, noteOn, 0.0f);
        noteOn = -1;
        changed = true;
    }
    if (worker != nullptr)
    {
        bool reapply = false;
        for (auto& r : worker->take())
        {
            sketches[r.seed] = r.sketch;
            changed = true;
            if (auto* g = ses->current())
            {
                for (int i = 0; i < (int) g->children.size(); ++i)
                {
                    auto& c = g->children[(size_t) i];
                    if (c.seed != r.seed || ! r.matched || c.matched) continue;
                    c.genome = r.genome;
                    c.matched = true;
                    if (const int k = ses->schema().gainGene(); k >= 0) c.gainDb = r.genome[(size_t) k];
                    reapply |= i == selected;
                }
            }
        }
        if (changed) store();
        if (reapply) audition (selected, false); // the selected child plays at its matched level
    }
    if (changed) repaint();
}

// ---- children ---------------------------------------------------------------------------------------------------------
preset::Preset BreedView::childPreset (int i) const
{
    preset::Preset p;
    const auto* g = ses->current();
    if (g == nullptr || i < 0 || i >= (int) g->children.size()) return p;
    const auto& c = g->children[(size_t) i];
    p.meta.kind = preset::Kind::Part;
    p.meta.name = c.name;
    p.meta.engine = engineTypeInfo ((EngineType) breed::engineOf (ses->schema(), c.genome)).id;
    p.meta.tags = { "Bred" };
    p.meta.comment = "Bred from " + g->a.name + " x " + g->b.name;
    p.params = breed::toParams (ses->schema(), c.genome);
    return p;
}

void BreedView::audition (int i, bool playNote)
{
    const auto* g = ses->current();
    if (g == nullptr || i < 0 || i >= (int) g->children.size()) return;
    selected = i;
    proc.presets().apply (childPreset (i), -1);
    if (playNote && playNotes)
    {
        if (noteOn >= 0) proc.keyboardState.noteOff (noteChannel, noteOn, 0.0f);
        noteChannel = juce::jlimit (1, 16, (int) std::lround (proc.core().params().get ("p" + std::to_string (proc.presets().editPart() + 1) + ".midich")));
        noteOn = 60;
        noteTicks = 12; // 0.6 s at 20 Hz
        proc.keyboardState.noteOn (noteChannel, noteOn, 0.8f);
    }
    repaint();
}

void BreedView::auditionNext()
{
    const auto* g = ses->current();
    if (g == nullptr || g->children.empty()) return;
    audition ((selected + 1) % (int) g->children.size());
}

void BreedView::toggleKept (int i)
{
    const auto* g = ses->current();
    if (g == nullptr || i < 0 || i >= (int) g->children.size()) return;
    ses->setKept (i, ! g->children[(size_t) i].kept);
    store();
    repaint();
}

int BreedView::saveChild (int i)
{
    const auto* g = ses->current();
    if (g == nullptr || i < 0 || i >= (int) g->children.size()) return -1;
    audition (i, false);
    auto& presets = proc.presets();
    auto meta = childPreset (i).meta;
    meta.author = presets.author().toStdString();
    const auto file = presets.save (meta, false);
    const int index = file != juce::File() ? presets.indexOfFile (file) : -1;
    if (index < 0)
    {
        status = tr ("Could not save the preset in") + " " + presets.userFolder().getFullPathName();
        repaint();
        return -1;
    }
    presets.setFavourite (index, true);
    ses->setKept (i, true);
    store();
    status = tr ("Saved") + ": " + str (presets.entries()[(size_t) index].meta.name) + " " + heartFull();
    repaint();
    return index;
}

// ---- menus ------------------------------------------------------------------------------------------------------------
void BreedView::showParentMenu (int which)
{
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Parent") + (which == 0 ? " A" : " B"));
    m.addItem (1, tr ("Current sound") + " (" + beforeName + ")");
    m.addItem (2, tr ("Random preset"));
    if (const auto* g = ses->current(); g != nullptr && selected >= 0 && selected < (int) g->children.size())
        m.addItem (3, tr ("Selected child") + " (" + str (g->children[(size_t) selected].name) + ")");
    std::map<juce::String, juce::PopupMenu> byCategory;
    const auto& entries = proc.presets().entries();
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        if (e.meta.kind != preset::Kind::Part) continue;
        juce::String cat = tr ("Other");
        for (const auto& t : e.meta.tags)
            if (preset::isCategory (t))
            {
                for (const auto& c : preset::categories())
                    if (t == c.key) cat = i18n::pick (c.key, c.nameDe);
                break;
            }
        byCategory[cat].addItem (1000 + i, (e.favourite ? heartFull() + " " : juce::String()) + str (e.meta.name));
    }
    juce::PopupMenu library;
    for (auto& [cat, sub] : byCategory) library.addSubMenu (cat, sub);
    m.addSubMenu (tr ("Preset"), library);
    juce::Component::SafePointer<BreedView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, which] (int r) {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->setParentCurrent (which);
        else if (r == 2) safe->randomParent (which);
        else if (r == 3) safe->setParentFromChild (which, safe->selected);
        else safe->setParentFromLibrary (which, r - 1000);
    });
}

void BreedView::showChildMenu (int i)
{
    const auto* g = ses->current();
    if (g == nullptr || i < 0 || i >= (int) g->children.size()) return;
    juce::PopupMenu m;
    m.addSectionHeader (str (g->children[(size_t) i].name));
    m.addItem (1, tr ("Listen"));
    m.addItem (2, g->children[(size_t) i].kept ? tr ("Do not keep") : tr ("Keep for the next generation"));
    m.addItem (3, tr ("Make parent A"));
    m.addItem (4, tr ("Make parent B"));
    m.addSeparator();
    m.addItem (5, tr ("Save to the library"));
    juce::Component::SafePointer<BreedView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, i] (int r) {
        if (safe == nullptr) return;
        if (r == 1) safe->audition (i);
        else if (r == 2) safe->toggleKept (i);
        else if (r == 3) safe->setParentFromChild (0, i);
        else if (r == 4) safe->setParentFromChild (1, i);
        else if (r == 5) safe->saveChild (i);
    });
}

// ---- paint ------------------------------------------------------------------------------------------------------------
juce::String BreedView::parentLabel (int which) const
{
    const auto& p = ses->parent (which);
    if (! p.set) return tr ("Choose a sound");
    return str (p.name) + "  /  " + engineName (breed::engineOf (ses->schema(), p.genome));
}

juce::Rectangle<float> BreedView::amountArea() const
{
    const float u = unit();
    return { u * 66.0f, u * 5.6f, u * 20.5f, u * 3.6f };
}

juce::Rectangle<float> BreedView::tileArea (int i) const
{
    const float u = unit();
    const float top = u * 14.4f, bottom = (float) getHeight() - u * 3.6f;
    const float w = (u * 110.0f - u * 2.0f) / 3.0f, h = (bottom - top - u * 1.0f) / 2.0f;
    return { u * 2.0f + (float) (i % 3) * (w + u * 1.0f), top + (float) (i / 3) * (h + u * 1.0f), w, h };
}

void BreedView::chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, juce::Colour accent,
                      std::function<void()> action, std::function<void()> secondary)
{
    const float u = unit();
    g.setColour (on ? accent : colour::displayGrid.brighter (0.15f));
    g.fillRoundedRectangle (r, u * 0.6f);
    g.setColour (on ? colour::display : colour::displayText);
    g.setFont (labelFont (juce::jmin (r.getHeight() * 0.52f, u * 1.6f)));
    g.drawFittedText (text, r.reduced (u * 0.4f, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    hits.push_back ({ r, std::move (action), false, std::move (secondary) });
}

void BreedView::paint (juce::Graphics& g)
{
    hits.clear();
    const float u = unit();
    const auto b = getLocalBounds().toFloat();
    const float du = (float) getWidth() / 1152.0f;
    g.setGradientFill (juce::ColourGradient (colour::bezel, 0, b.getY(), colour::bezelLow, 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, radius::panel * du);
    g.setColour (colour::display);
    g.fillRoundedRectangle (b.reduced (u * 0.5f), radius::inset * du);

    // ---- top row: title, generation + undo / redo, status, keep / cancel
    g.setColour (colour::displayText);
    g.setFont (brandFont (u * 2.4f));
    g.drawText (i18n::upper (tr ("Breeding")), juce::Rectangle<float> (u * 2.0f, u * 1.4f, u * 15.0f, u * 3.2f), juce::Justification::centredLeft, true);
    const auto* gen = ses->current();
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.5f));
    const juce::String genText = gen != nullptr ? i18n::upper (tr ("Generation")) + " " + juce::String (gen->number) + "   ("
                                                      + juce::String (ses->currentIndex() + 1) + " / " + juce::String (ses->numGenerations()) + ")"
                                                : i18n::upper (tr ("No brood yet"));
    g.drawText (genText, juce::Rectangle<float> (u * 17.5f, u * 1.4f, u * 22.0f, u * 3.2f), juce::Justification::centredLeft, true);
    chip (g, { u * 40.0f, u * 1.4f, u * 4.0f, u * 3.2f }, juce::String::fromUTF8 ("\xe2\x80\xb9"), false, colour::displayText, [this] { undo(); });
    chip (g, { u * 44.6f, u * 1.4f, u * 4.0f, u * 3.2f }, juce::String::fromUTF8 ("\xe2\x80\xba"), false, colour::displayText, [this] { redo(); });
    chip (g, { u * 49.6f, u * 1.4f, u * 9.0f, u * 3.2f }, juce::String::fromUTF8 ("\xe2\x99\xaa ") + tr ("Note"), playNotes, colour::playGreenLit,
          [this] { playNotes = ! playNotes; repaint(); });
    if (status.isNotEmpty())
    {
        g.setColour (colour::sunYellow);
        g.setFont (labelFont (u * 1.4f));
        g.drawText (status, juce::Rectangle<float> (u * 59.5f, u * 1.4f, u * 29.0f, u * 3.2f), juce::Justification::centredRight, true);
    }
    chip (g, { b.getWidth() - u * 24.0f, u * 1.4f, u * 10.5f, u * 3.2f }, i18n::upper (tr ("Keep")), true, colour::sunYellow, [this] { close (true); });
    chip (g, { b.getWidth() - u * 12.5f, u * 1.4f, u * 10.5f, u * 3.2f }, i18n::upper (tr ("Cancel")), false, colour::signalRed, [this] { close (false); });

    // ---- second row: parents, die, mutation, BREED / AGAIN
    const float y2 = u * 5.6f, h2 = u * 3.6f;
    for (int w = 0; w < 2; ++w)
    {
        const juce::Rectangle<float> r { u * (2.0f + 30.5f * (float) w), y2, u * 27.0f, h2 };
        g.setColour (colour::displayGrid.brighter (0.15f));
        g.fillRoundedRectangle (r, u * 0.6f);
        g.setColour (parentColour ((float) w));
        g.fillRoundedRectangle (r.withWidth (u * 3.2f), u * 0.6f);
        g.setColour (colour::display);
        g.setFont (brandFont (u * 1.8f));
        g.drawText (w == 0 ? "A" : "B", r.withWidth (u * 3.2f), juce::Justification::centred);
        g.setColour (ses->parent (w).set ? colour::displayText : colour::displayDim);
        g.setFont (labelFont (u * 1.5f));
        g.drawFittedText (parentLabel (w) + juce::String::fromUTF8 ("  \xe2\x96\xbe"), r.withTrimmedLeft (u * 4.0f).reduced (u * 0.4f, 0).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
        hits.push_back ({ r, [this, w] { showParentMenu (w); }, false, {} });
    }
    g.setColour (colour::displayDim);
    g.setFont (brandFont (u * 2.0f));
    g.drawText (juce::String::fromUTF8 ("\xc3\x97"), juce::Rectangle<float> (u * 29.0f, y2, u * 3.5f, h2), juce::Justification::centred);
    chip (g, { u * 60.0f, y2, u * 5.0f, h2 }, juce::String::fromUTF8 ("\xe2\x9a\x83"), false, colour::sunYellow, [this] { randomParent (1); });
    {
        const auto r = amountArea();
        g.setColour (colour::displayGrid.brighter (0.15f));
        g.fillRoundedRectangle (r, u * 0.6f);
        g.setColour (colour::sunYellow.withAlpha (0.85f));
        g.fillRoundedRectangle (r.withWidth (juce::jmax (u * 0.6f, r.getWidth() * ses->amount)), u * 0.6f);
        g.setColour (ses->amount > 0.55f ? colour::display : colour::displayText);
        g.setFont (labelFont (u * 1.45f));
        g.drawText (i18n::upper (tr ("Mutation")) + " " + juce::String (juce::roundToInt (ses->amount * 100.0f)) + " %", r.reduced (u * 0.8f, 0),
                    juce::Justification::centredLeft, true);
    }
    chip (g, { u * 87.5f, y2, u * 12.0f, h2 }, i18n::upper (tr ("Breed")), ses->ready(), colour::sunYellow, [this] { breedNew(); });
    chip (g, { u * 100.5f, y2, u * 11.5f, h2 }, i18n::upper (tr ("Again")), gen != nullptr, colour::ultraviolet, [this] { breedAgain(); });

    // ---- third row: how the groups are inherited
    {
        const float y3 = u * 10.4f, h3 = u * 2.7f;
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.3f));
        g.drawText (i18n::upper (tr ("Inherit")), juce::Rectangle<float> (u * 2.0f, y3, u * 9.0f, h3), juce::Justification::centredLeft, true);
        for (int k = 0; k < breed::numChoiceGroups; ++k)
        {
            const auto c = ses->options.choice[(size_t) k];
            const juce::Rectangle<float> r { u * (11.0f + 15.2f * (float) k), y3, u * 14.6f, h3 };
            const auto accent = c == breed::Choice::B ? colour::ultraviolet : c == breed::Choice::Blend ? parentColour (0.5f) : colour::displayText;
            chip (g, r, tr (breed::groupName (k)) + ": " + tr (choiceLabel (c)), c != breed::Choice::Auto, accent, [this, k, c] {
                setGroupChoice (k, (breed::Choice) (((int) c + 1) % 4));
            });
        }
        g.setColour (colour::displayDim);
        g.drawFittedText (tr ("Click a child to hear it.") + " " + heartFull() + " " + tr ("keeps it for AGAIN."),
                          juce::Rectangle<float> (u * 88.0f, y3, u * 24.0f, h3).toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
    }

    // ---- the brood
    if (gen == nullptr)
    {
        auto r = tileArea (0).getUnion (tileArea (5));
        g.setColour (colour::displayGrid.withAlpha (0.6f));
        g.fillRoundedRectangle (r, u * 0.8f);
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.8f));
        g.drawFittedText (tr ("Choose two parents - the current sound, a library preset or the die - set the mutation and press BREED. "
                              "Six children appear here; keep the best ones and breed again."),
                          r.reduced (u * 8.0f).toNearestInt(), juce::Justification::centred, 4, 0.9f);
    }
    else
        for (int i = 0; i < (int) gen->children.size() && i < 6; ++i) paintTile (g, i, tileArea (i));

    // ---- foot: what SAVE does
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.3f));
    g.drawText (tr ("SAVE stores the selected child in the library as a favourite. RANDOM rolls a new parent B."),
                juce::Rectangle<float> (u * 2.0f, b.getHeight() - u * 3.2f, u * 110.0f, u * 2.4f), juce::Justification::centredLeft, true);
}

void BreedView::paintTile (juce::Graphics& g, int i, juce::Rectangle<float> r)
{
    const float u = unit();
    const auto& c = ses->current()->children[(size_t) i];
    const bool sel = i == selected;
    g.setColour (colour::displayGrid.withAlpha (sel ? 1.0f : 0.75f));
    g.fillRoundedRectangle (r, u * 0.8f);
    if (sel)
    {
        g.setColour (colour::sunYellow);
        g.drawRoundedRectangle (r.reduced (u * 0.1f), u * 0.8f, u * 0.25f);
    }
    hits.push_back ({ r, [this, i] { audition (i); }, false, [this, i] { showChildMenu (i); } });

    auto inner = r.reduced (u * 1.0f, u * 0.8f);
    // name + heart + save
    g.setColour (colour::displayText);
    g.setFont (labelFont (u * 1.6f));
    g.drawFittedText (str (c.name), inner.withHeight (u * 2.6f).withTrimmedRight (u * 14.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    chip (g, { inner.getRight() - u * 13.4f, inner.getY(), u * 4.0f, u * 2.6f }, c.kept ? heartFull() : heartEmpty(), c.kept, colour::rose,
          [this, i] { toggleKept (i); });
    chip (g, { inner.getRight() - u * 9.0f, inner.getY(), u * 9.0f, u * 2.6f }, i18n::upper (tr ("Save")), false, colour::sunYellow,
          [this, i] { saveChild (i); });

    // waveform thumbnail, tinted by brightness
    const auto wave = juce::Rectangle<float> (inner.getX(), inner.getY() + u * 3.2f, inner.getWidth(), inner.getHeight() - u * 7.6f);
    g.setColour (colour::display);
    g.fillRoundedRectangle (wave, u * 0.5f);
    if (hasSketch (i))
    {
        const auto s = sketch (i);
        float peak = 0.0f;
        for (float w : s.wave) peak = juce::jmax (peak, w);
        const float scale = peak > 1.0e-4f ? 0.92f / juce::jmax (peak, 0.25f) : 0.0f; // quiet sounds stay small
        const auto tint = brightnessColour (s.brightness);
        const float bw = wave.getWidth() / (float) breed::Audition::thumbPoints;
        for (int k = 0; k < breed::Audition::thumbPoints; ++k)
        {
            const float h = juce::jlimit (0.0f, 1.0f, s.wave[(size_t) k] * scale) * wave.getHeight() * 0.5f;
            g.setColour (tint.withAlpha (0.55f + 0.45f * (float) k / (float) breed::Audition::thumbPoints));
            g.fillRect (wave.getX() + (float) k * bw + bw * 0.15f, wave.getCentreY() - h, bw * 0.7f, juce::jmax (u * 0.08f, 2.0f * h));
        }
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.15f));
        const juce::String level = s.rmsDb > -90.0f ? juce::String (s.rmsDb, 1) + " dB" : tr ("silent");
        g.drawText (level + "   " + tr ("Bright") + " " + juce::String (juce::roundToInt (s.brightness * 100.0f)) + " %",
                    juce::Rectangle<float> (inner.getX(), inner.getBottom() - u * 3.6f, inner.getWidth(), u * 1.6f), juce::Justification::centredRight, true);
    }
    else
    {
        g.setColour (colour::displayDim);
        g.setFont (labelFont (u * 1.4f));
        g.drawText (tr ("listening") + " ...", wave, juce::Justification::centred);
    }

    // inheritance strip: one segment per group, paper = parent A, violet = parent B
    const float sy = inner.getBottom() - u * 3.6f;
    g.setColour (colour::displayDim);
    g.setFont (labelFont (u * 1.15f));
    g.drawText (engineName (breed::engineOf (ses->schema(), c.genome))
                    + (c.inheritance.hybrid ? " + " + engineName (breed::layerOf (ses->schema(), c.genome)) : juce::String()),
                juce::Rectangle<float> (inner.getX(), sy, inner.getWidth(), u * 1.6f), juce::Justification::centredLeft, true);
    const float segW = (inner.getWidth() - u * 0.4f * (float) (breed::numChoiceGroups - 1)) / (float) breed::numChoiceGroups;
    for (int k = 0; k < breed::numChoiceGroups; ++k)
    {
        const juce::Rectangle<float> seg { inner.getX() + (float) k * (segW + u * 0.4f), sy + u * 1.8f, segW, u * 1.8f };
        g.setColour (parentColour (c.inheritance.fromB[(size_t) k]));
        g.fillRoundedRectangle (seg, u * 0.35f);
        g.setColour (colour::display.withAlpha (0.8f));
        g.setFont (labelFont (u * 1.1f));
        g.drawText (tr (breed::groupName (k)), seg, juce::Justification::centred, true);
    }
    // light bar: yellow = selected (edit), green while its preview note plays
    if (sel)
    {
        g.setColour (noteOn >= 0 ? colour::playGreenLit : colour::sunYellow);
        g.fillRoundedRectangle (juce::Rectangle<float> (r.getCentreX() - u * 4.0f, r.getBottom() - u * 0.55f, u * 8.0f, u * 0.35f), u * 0.17f);
    }
}

// ---- input ------------------------------------------------------------------------------------------------------------
void BreedView::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && amountArea().contains (e.position))
    {
        draggingAmount = true;
        const auto r = amountArea();
        setAmount ((e.position.x - r.getX()) / r.getWidth());
        return;
    }
    draggingAmount = false;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            auto action = e.mods.isPopupMenu() ? it->secondary : it->action; // the action may repaint and rebuild the hit list
            if (action) action();
            return;
        }
}

void BreedView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingAmount) return;
    const auto r = amountArea();
    setAmount ((e.position.x - r.getX()) / r.getWidth());
}

bool BreedView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close (false); return true; }
    if (k == juce::KeyPress::returnKey) { close (true); return true; }
    const auto* g = ses->current();
    const int n = g != nullptr ? (int) g->children.size() : 0;
    if (n > 0 && (k == juce::KeyPress::rightKey || k == juce::KeyPress::leftKey))
    {
        audition (((selected < 0 ? (k == juce::KeyPress::rightKey ? -1 : 0) : selected) + (k == juce::KeyPress::rightKey ? 1 : -1) + n) % n);
        return true;
    }
    return false;
}
} // namespace ohpe::ui
