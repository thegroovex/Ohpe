// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ContentLibrary.h"

#include "update/Sha256.h"

#include <algorithm>

namespace ohpe::content
{
namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
std::string std8 (const juce::String& s) { return s.toStdString(); }

uint64_t folderBytes (const juce::File& dir)
{
    uint64_t n = 0;
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*"))
        n += (uint64_t) juce::jmax ((juce::int64) 0, f.getSize());
    return n;
}

bool isHidden (const juce::File& f) { return f.getFileName().startsWithChar ('.'); }

InstallResult failed (const juce::String& error, const juce::String& detail = {})
{
    InstallResult r;
    r.error = error;
    r.detail = detail;
    return r;
}

enum class Extract { Ok = 0, Failed, Checksum };

/** Copies one zip entry to `target`, at most `limit` bytes (a zip that lies about its sizes is refused); checks the
    SHA-256 when `sha256` is given. */
Extract extract (juce::ZipFile& zip, int index, const juce::File& target, uint64_t limit, const std::string& sha256,
                 const std::atomic<bool>* cancel)
{
    std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));
    if (in == nullptr) return Extract::Failed;
    if (! target.getParentDirectory().createDirectory()) return Extract::Failed;
    target.deleteFile();
    juce::FileOutputStream out (target);
    if (! out.openedOk()) return Extract::Failed;
    juce::HeapBlock<char> buffer (65536);
    update::Sha256 hash;
    uint64_t written = 0;
    for (;;)
    {
        if (cancel != nullptr && cancel->load()) return Extract::Failed;
        const int got = in->read (buffer.get(), 65536);
        if (got < 0) return Extract::Failed;
        if (got == 0) break;
        written += (uint64_t) got;
        if (written > limit) return Extract::Failed;
        hash.update (buffer.get(), (size_t) got);
        if (! out.write (buffer.get(), (size_t) got)) return Extract::Failed;
    }
    out.flush();
    if (! out.getStatus().wasOk()) return Extract::Failed;
    if (! sha256.empty() && ! update::Sha256::sameDigest (hash.finishHex(), sha256)) return Extract::Checksum;
    return Extract::Ok;
}
} // namespace

uint64_t usedBytes (const juce::File& contentFolder, const juce::String& exceptId)
{
    uint64_t n = 0;
    if (! contentFolder.isDirectory()) return 0;
    for (const auto& d : contentFolder.findChildFiles (juce::File::findDirectories, false))
        if (! isHidden (d) && d.getFileName() != exceptId) n += folderBytes (d);
    return n;
}

InstallResult installZip (const juce::File& zipFile, const juce::File& contentFolder, const update::PackInfo& expectedIn,
                          update::ContentRegistry& registry, const InstallOptions& options)
{
    const auto* cancel = options.cancel;
    juce::ZipFile zip (zipFile);
    if (zip.getNumEntries() == 0) return failed ("The content pack is damaged", zipFile.getFileName());

    std::vector<update::ZipItem> items;
    std::string packJson;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* e = zip.getEntry (i);
        if (e->isSymbolicLink) return failed ("The content pack is damaged", e->filename);
        update::ZipItem it;
        it.path = std8 (e->filename);
        it.directory = e->filename.endsWithChar ('/') || e->filename.endsWithChar ('\\');
        it.size = (uint64_t) juce::jmax ((juce::int64) 0, e->uncompressedSize);
        if (it.path == update::packInfoFile && it.size <= update::maxNoteBytes)
        {
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
            if (in != nullptr) packJson = std8 (in->readEntireStreamAsString());
        }
        items.push_back (std::move (it));
    }

    auto expected = expectedIn;
    if (expected.id.empty() && packJson.empty())
    {
        std::string id, version; // a local old pack: "Ohpe-content-<id>-<version>.zip"
        if (update::parsePackFileName (std8 (zipFile.getFileName()), id, version))
        {
            expected.id = id;
            expected.version = version;
        }
    }
    // the pack's own installed version becomes the backup: it does not count against the quota
    juce::String ownId = str (expected.id);
    if (ownId.isEmpty() && ! packJson.empty())
    {
        update::PackInfo peek;
        if (update::parsePackInfo (packJson, peek)) ownId = str (peek.id);
    }
    const auto plan = update::planPack (items, packJson, expected, usedBytes (contentFolder, ownId), options.requireChecksums);
    if (! plan.ok) return failed (str (plan.error), str (plan.detail));
    if (! plan.info.minApp.empty() && options.app.valid() && options.app < update::Version::of (plan.info.minApp))
        return failed ("This pack needs a newer Ohpe", str (plan.info.minApp));
    const auto& gate = options.gate != nullptr ? *options.gate : update::openGate();
    if (gate.access (plan.info, update::canonicalPackJson (packJson)) != update::PackAccess::Allowed)
        return failed ("This pack is locked", str (plan.info.license.productId));

    const auto id = str (plan.info.id);
    const auto staging = contentFolder.getChildFile (".staging").getChildFile (id);
    staging.deleteRecursively();
    if (! staging.createDirectory()) return failed ("Could not unpack the content pack", staging.getFullPathName());
    const auto limits = update::limitsFor (plan.kind);
    for (const auto& f : plan.files)
    {
        const auto target = staging.getChildFile (str (f.target));
        const auto res = target.isAChildOf (staging) // belt and braces: planPack already refused such paths
                             ? extract (zip, f.index, target, juce::jmin (f.size, limits.maxPackBytes), f.sha256, cancel)
                             : Extract::Failed;
        if (res != Extract::Ok)
        {
            staging.deleteRecursively(); // nothing installed changes: the old version stays
            if (res == Extract::Checksum) return failed ("A file of the content pack is damaged (checksum)", str (f.target));
            return failed (cancel != nullptr && cancel->load() ? "Install cancelled" : "Could not unpack the content pack", str (f.target));
        }
    }
    if (! plan.hasPackInfo) // old packs get their description written, so the library knows their name
        staging.getChildFile (update::packInfoFile).replaceWithText (str (update::packInfoToJson (plan.info)));

    // back up the installed version (one step), then swap the new one in
    const auto target = contentFolder.getChildFile (id);
    const auto backup = contentFolder.getChildFile (".backup").getChildFile (id);
    if (target.isDirectory())
    {
        backup.deleteRecursively();
        backup.getParentDirectory().createDirectory();
        if (! target.moveFileTo (backup))
        {
            staging.deleteRecursively();
            return failed ("Could not back up the installed pack", target.getFullPathName());
        }
    }
    if (! staging.moveFileTo (target))
    {
        if (backup.isDirectory()) backup.moveFileTo (target); // put the old one back
        staging.deleteRecursively();
        return failed ("Could not install the content pack", target.getFullPathName());
    }
    registry.installed (plan.info.id, plan.info.version);
    contentFolder.getChildFile ("installed.json").replaceWithText (str (registry.toJson()));

    InstallResult r;
    r.ok = true;
    r.info = plan.info;
    r.flat = plan.flat;
    r.skipped = plan.skipped;
    r.files = (int) plan.files.size();
    return r;
}

bool removePack (const juce::File& contentFolder, const juce::String& id, update::ContentRegistry& registry)
{
    if (! update::isSafePackId (std8 (id))) return false;
    const auto dir = contentFolder.getChildFile (id), backup = contentFolder.getChildFile (".backup").getChildFile (id);
    if (! dir.isDirectory() || ! dir.isAChildOf (contentFolder)) return false;
    // move aside first, so a half-deleted pack never shows up in the library
    const auto trash = contentFolder.getChildFile (".staging").getChildFile (id + "-removed");
    trash.deleteRecursively();
    trash.getParentDirectory().createDirectory();
    if (! dir.moveFileTo (trash)) return false;
    trash.deleteRecursively();
    backup.deleteRecursively();
    registry.removed (std8 (id));
    contentFolder.getChildFile ("installed.json").replaceWithText (str (registry.toJson()));
    return true;
}

uint64_t packBytes (const juce::File& contentFolder, const juce::String& id)
{
    const auto dir = contentFolder.getChildFile (id);
    return dir.isDirectory() ? folderBytes (dir) : 0;
}

update::PackInfo installedInfo (const juce::File& contentFolder, const juce::String& id, const update::ContentRegistry* registry)
{
    update::PackInfo info;
    const auto f = contentFolder.getChildFile (id).getChildFile (update::packInfoFile);
    if (f.existsAsFile() && f.getSize() <= (juce::int64) update::maxNoteBytes && update::parsePackInfo (std8 (f.loadFileAsString()), info)
        && info.id == std8 (id))
        return info;
    info = {};
    info.id = std8 (id);
    info.nameEn = info.nameDe = info.id;
    info.kind = "presets";
    if (registry != nullptr)
        if (const auto* p = registry->find (info.id)) info.version = p->version;
    return info;
}

juce::File kindFolder (const juce::File& contentFolder, const juce::String& packId, update::PackKind kind)
{
    const auto pack = contentFolder.getChildFile (packId);
    if (kind == update::PackKind::Presets && ! pack.getChildFile (update::kindFolder (kind)).isDirectory()) return pack; // old flat pack
    return pack.getChildFile (update::kindFolder (kind));
}

update::ContentIndex scan (const juce::File& contentFolder)
{
    update::ContentIndex index;
    if (! contentFolder.isDirectory()) return index;
    update::ContentRegistry reg;
    const auto regFile = contentFolder.getChildFile ("installed.json");
    if (regFile.existsAsFile()) reg = update::ContentRegistry::fromJson (std8 (regFile.loadFileAsString()));
    auto dirs = contentFolder.findChildFiles (juce::File::findDirectories, false);
    dirs.sort();
    for (const auto& d : dirs)
    {
        if (isHidden (d) || ! update::isSafePackId (std8 (d.getFileName()))) continue;
        const auto info = installedInfo (contentFolder, d.getFileName(), &reg);
        update::ContentIndex::Pack p;
        p.id = info.id;
        p.version = info.version;
        p.nameEn = info.nameEn;
        p.nameDe = info.nameDe;
        for (auto kind : { update::PackKind::Presets, update::PackKind::Wavetables })
        {
            const auto folder = kindFolder (contentFolder, d.getFileName(), kind);
            if (! folder.isDirectory()) continue;
            for (const auto& ext : update::kindExtensions (kind))
            {
                auto files = folder.findChildFiles (juce::File::findFiles, true, "*" + str (ext));
                files.sort();
                for (const auto& f : files)
                {
                    auto rel = f.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
                    rel = rel.dropLastCharacters (f.getFileExtension().length());
                    p.items.emplace_back (kind, std8 (rel));
                }
            }
        }
        index.packs.push_back (std::move (p));
    }
    return index;
}

juce::File fileFor (const juce::File& contentFolder, const update::ContentRef& ref, update::PackKind kind)
{
    if (! ref.valid()) return {};
    const auto exts = update::kindExtensions (kind);
    if (exts.empty()) return {};
    const auto folder = kindFolder (contentFolder, str (ref.pack), kind);
    for (const auto& ext : exts)
    {
        const auto f = folder.getChildFile (str (ref.item) + str (ext));
        if (f.isAChildOf (folder) && f.existsAsFile()) return f;
    }
    return folder.getChildFile (str (ref.item) + str (exts.front()));
}
} // namespace ohpe::content
