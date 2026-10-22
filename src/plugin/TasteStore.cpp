// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TasteStore.h"

#include "PluginProcessor.h"
#include "Updater.h"

namespace ohpe
{
namespace
{
constexpr double minPlayed = 3.0;     ///< seconds with notes that count as "played"
constexpr double skipWindow = 8.0;    ///< replaced within this (and played less than minPlayed) = "skipped"
constexpr double snapshotEvery = 2.0; ///< the edit part's genome is read this often while a sound is current
constexpr double saveEvery = 5.0;
} // namespace

TasteStore::TasteStore (OhpeProcessor& p) : proc (p) { load(); }

TasteStore::~TasteStore()
{
    if (dirty) save();
}

juce::File TasteStore::file() const
{
    const auto base = testFolder != juce::File() ? testFolder : Updater::dataFolder();
    return base.getChildFile ("Taste").getChildFile ("taste.json");
}

void TasteStore::setFolderForTests (const juce::File& f)
{
    testFolder = f;
    load();
}

void TasteStore::load()
{
    profile = {};
    dirty = false;
    if (! filesEnabled.load() && testFolder == juce::File()) return;
    const auto f = file();
    if (f.existsAsFile()) profile.fromJson (f.loadFileAsString().toStdString()); // damaged: an empty profile
    dirty = false;
}

void TasteStore::save()
{
    dirty = false;
    if (! filesEnabled.load() && testFolder == juce::File()) return;
    const auto f = file();
    if (f.getParentDirectory().createDirectory()) f.replaceWithText (juce::String::fromUTF8 (profile.toJson().c_str()));
    dirty = false;
}

const breed::Schema& TasteStore::schema()
{
    if (sch == nullptr) sch = std::make_unique<breed::Schema> (breed::Schema::forPart (proc.core().params(), 0));
    return *sch;
}

breed::Genome TasteStore::editGenome() { return breed::fromRegistry (schema(), proc.core().params(), proc.presets().editPart()); }

std::shared_ptr<const breed::TasteBias> TasteStore::bias()
{
    if (! profile.learning || profile.empty()) return {};
    auto b = std::make_shared<breed::TasteBias> (profile.bias (schema()));
    return b->engines.empty() ? nullptr : b;
}

void TasteStore::setLearning (bool on)
{
    profile.learning = on;
    save();
}

void TasteStore::setCuriosity (float c)
{
    profile.curiosity = juce::jlimit (0.0f, 1.0f, std::isfinite (c) ? c : 0.35f);
    save();
}

void TasteStore::reset()
{
    profile.reset();
    key.clear();
    genome.clear();
    played = 0.0;
    notesSeen = false;
    save();
}

void TasteStore::commit (double now)
{
    if (key.empty() || ! profile.learning) return;
    const auto& s = schema();
    if (played >= minPlayed)
        profile.record (s, key, genome, breed::Signal::Played, (float) played);
    else if (notesSeen && now - loadedAt < skipWindow)
        profile.record (s, key, genome, breed::Signal::Skipped);
    else
        return;
    dirty = true;
}

void TasteStore::soundChanged (const std::string& newKey, const breed::Genome& g)
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    commit (now);
    key = newKey;
    genome = g;
    loadedAt = lastSnapshot = now;
    played = 0.0;
    notesSeen = false;
    // a library entry loaded again counts once its genome is known (a bred child is new every time)
    const auto* known = profile.sound (newKey);
    pendingReload = profile.learning && known != nullptr && newKey.rfind ("breed:", 0) != 0;
    if (pendingReload && ! genome.empty())
    {
        profile.record (schema(), key, genome, breed::Signal::Reloaded);
        pendingReload = false;
        dirty = true;
    }
}

void TasteStore::signal (const std::string& k, breed::Signal s, const breed::Genome& g)
{
    if (! profile.learning || k.empty()) return;
    auto gen = g;
    if (gen.empty() && k == key) gen = genome.empty() ? editGenome() : genome;
    if (gen.empty()) profile.recordKeyOnly (k, s);
    else profile.record (schema(), k, gen, s);
    dirty = true;
}

void TasteStore::tick (double now)
{
    if (! profile.learning)
    {
        lastTick = now;
        if (dirty && now - lastSave > saveEvery) { save(); lastSave = now; }
        return;
    }
    // the library entry in the edit part changed: a new sound
    const auto uid = proc.presets().currentUid (-1);
    if (! uidKnown || uid != lastUid)
    {
        const bool first = ! uidKnown;
        uidKnown = true;
        lastUid = uid;
        if (! first && ! uid.empty()) soundChanged (uid);
    }
    const double dt = lastTick >= 0.0 ? juce::jlimit (0.0, 0.5, now - lastTick) : 0.0;
    lastTick = now;
    if (! key.empty())
    {
        const auto notes = proc.core().soundingNotes (proc.presets().editPart());
        if ((notes[0] | notes[1]) != 0)
        {
            played += dt;
            notesSeen = true;
        }
        if (genome.empty() ? now - loadedAt > 0.3 : now - lastSnapshot > snapshotEvery)
        {
            genome = editGenome();
            lastSnapshot = now;
            if (pendingReload)
            {
                profile.record (schema(), key, genome, breed::Signal::Reloaded);
                pendingReload = false;
                dirty = true;
            }
        }
    }
    if (dirty && now - lastSave > saveEvery)
    {
        save();
        lastSave = now;
    }
}
} // namespace ohpe
