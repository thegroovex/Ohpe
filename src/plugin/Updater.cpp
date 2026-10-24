// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Updater.h"
#include "ContentLibrary.h"

#include <cmath>

#ifndef OHPE_VERSION
 #define OHPE_VERSION "0.0.0"
#endif

namespace ohpe
{
namespace
{
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
std::string std8 (const juce::String& s) { return s.toStdString(); }

juce::String userAgent() { return "Ohpe/" OHPE_VERSION; }

/** p81: aborts a connection that makes no progress. Some network stacks do not honour their receive timeout
    (seen as a download that hangs for good on Windows when a connection breaks or a server answers a ranged
    request with the whole file); closing the stream from here unblocks the read, and the core then continues in
    a new connection. Also aborts a blocked read at once when the user cancels or the updater shuts down. */
class StallWatchdog final : private juce::Thread
{
public:
    explicit StallWatchdog (std::function<bool()> abortNow) : juce::Thread ("Ohpe download watchdog"), abort (std::move (abortNow)) {}
    ~StallWatchdog() override { stopThread (4000); }

    void watch (juce::WebInputStream* s, int stallMs)
    {
        {
            std::lock_guard<std::mutex> g (m);
            stream = s;
            limitMs = juce::jmax (500, stallMs);
            lastMs = juce::Time::getMillisecondCounter();
            fired = false;
        }
        if (! isThreadRunning()) startThread (juce::Thread::Priority::low);
    }
    void unwatch() { std::lock_guard<std::mutex> g (m); stream = nullptr; }
    void progressed() { std::lock_guard<std::mutex> g (m); lastMs = juce::Time::getMillisecondCounter(); }
    bool hasFired() { std::lock_guard<std::mutex> g (m); return fired; }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            wait (100);
            std::lock_guard<std::mutex> g (m);
            if (stream == nullptr) continue;
            const bool stalled = juce::Time::getMillisecondCounter() - lastMs > (juce::uint32) limitMs;
            if (stalled || (abort && abort()))
            {
                fired = true;
                stream->cancel(); // under the lock: the transport cannot destroy the stream meanwhile
                stream = nullptr;
            }
        }
    }

    std::function<bool()> abort;
    std::mutex m;
    juce::WebInputStream* stream = nullptr;
    int limitMs = 30000;
    juce::uint32 lastMs = 0;
    bool fired = false;
};

/** juce::URL as the core's download transport (https; file:// for tests). */
class UrlTransport final : public update::Transport
{
public:
    UrlTransport (juce::Thread& t, int timeoutMs, std::function<bool()> cancelled)
        : thread (t), timeout (timeoutMs),
          watchdog ([&t, c = std::move (cancelled)] { return t.threadShouldExit() || (c && c()); }) {}
    ~UrlTransport() override { close(); }

    bool open (const std::string& urlText, uint64_t offset, uint64_t& total, bool& fromOffset) override
    {
        close();
        const juce::URL url (str (urlText));
        if (url.isLocalFile())
        {
            auto f = std::make_unique<juce::FileInputStream> (url.getLocalFile());
            if (! f->openedOk()) return false;
            total = (uint64_t) juce::jmax ((juce::int64) 0, f->getTotalLength());
            fromOffset = f->setPosition ((juce::int64) offset);
            file = std::move (f);
            return true;
        }
        juce::StringPairArray response;
        int status = 0;
        auto request = [&] (uint64_t from) -> std::unique_ptr<juce::WebInputStream> {
            juce::String headers = "User-Agent: " + userAgent() + "\r\nAccept: application/octet-stream";
            if (from > 0) headers << "\r\nRange: bytes=" << (juce::int64) from << "-";
            auto w = std::make_unique<juce::WebInputStream> (url, false);
            w->withExtraHeaders (headers).withConnectionTimeout (timeout).withNumRedirectsToFollow (8);
            // the connection itself is watched too (a server that accepts and never answers)
            watchdog.watch (w.get(), stallLimit());
            const bool ok = w->connect (nullptr) && ! watchdog.hasFired();
            watchdog.unwatch();
            if (! ok) return nullptr;
            status = w->getStatusCode();
            response = w->getResponseHeaders();
            return w;
        };
        auto stream = request (offset);
        bool restarted = false;
        if (stream != nullptr && offset > 0 && status == 416)
        {
            // p81: the partial file is not a prefix the server can continue (stale, as long or longer than the
            // file): ask for the whole file - fromOffset = false makes the download start over instead of
            // failing on every attempt
            stream.reset();
            stream = request (0);
            restarted = true;
        }
        if (stream == nullptr) return false;
        if (status == 206 && ! restarted)
        {
            fromOffset = true;
            // Content-Range: bytes 100-999/1000
            for (const auto& key : response.getAllKeys())
                if (key.equalsIgnoreCase ("Content-Range"))
                {
                    const auto v = response[key];
                    const auto slash = v.lastIndexOfChar ('/');
                    if (slash >= 0) total = (uint64_t) juce::jmax ((juce::int64) 0, v.substring (slash + 1).trim().getLargeIntValue());
                }
        }
        else if (status == 200 || status == 0)
        {
            fromOffset = offset == 0 && ! restarted;
            const auto len = stream->getTotalLength();
            total = len > 0 ? (uint64_t) len : 0;
        }
        else
            return false;
        web = std::move (stream);
        watchdog.watch (web.get(), stallLimit());
        return true;
    }

    int64_t read (uint8_t* buffer, size_t n) override
    {
        if (thread.threadShouldExit()) return -1;
        juce::InputStream* in = web != nullptr ? static_cast<juce::InputStream*> (web.get()) : file.get();
        if (in == nullptr) return -1;
        const int got = in->read (buffer, (int) juce::jmin (n, (size_t) 65536));
        if (web != nullptr && watchdog.hasFired()) return -1; // stalled and aborted: continue in a new connection
        if (got < 0) return -1;
        if (got == 0) return in->isExhausted() ? 0 : -1;
        if (web != nullptr) watchdog.progressed();
        return got;
    }

    void close() override
    {
        watchdog.unwatch();
        web.reset();
        file.reset();
    }

private:
    /** No byte for this long = stalled. Twice the stack's own timeout, so that one normally fires first. */
    int stallLimit() const { return juce::jmax (2000, 2 * timeout); }

    juce::Thread& thread;
    int timeout = 20000;
    StallWatchdog watchdog;
    std::unique_ptr<juce::WebInputStream> web;
    std::unique_ptr<juce::InputStream> file;
};

class LocalPartialFile final : public update::PartialFile
{
public:
    explicit LocalPartialFile (juce::File f) : file (std::move (f)) {}
    ~LocalPartialFile() override { out.reset(); }

    uint64_t size() override
    {
        if (out != nullptr) out->flush();
        return file.existsAsFile() ? (uint64_t) file.getSize() : 0;
    }
    bool append (const uint8_t* d, size_t n) override
    {
        if (out == nullptr)
        {
            file.getParentDirectory().createDirectory();
            out = std::make_unique<juce::FileOutputStream> (file); // appends to an existing file
            if (! out->openedOk()) { out.reset(); return false; }
        }
        return out->write (d, n);
    }
    bool clear() override
    {
        out.reset();
        return ! file.exists() || file.deleteFile();
    }
    bool readAll (const std::function<void (const uint8_t*, size_t)>& sink) override
    {
        if (out != nullptr) { out->flush(); out.reset(); }
        juce::FileInputStream in (file);
        if (! in.openedOk()) return false;
        juce::HeapBlock<uint8_t> buf (65536);
        for (;;)
        {
            const int n = in.read (buf.get(), 65536);
            if (n <= 0) break;
            sink (buf.get(), (size_t) n);
        }
        return true;
    }
    void finish() { out.reset(); }

private:
    juce::File file;
    std::unique_ptr<juce::FileOutputStream> out;
};

juce::String safeFileName (const std::string& name, const char* fallback)
{
    auto n = juce::File::createLegalFileName (str (name));
    return n.isEmpty() ? juce::String (fallback) : n;
}
} // namespace

Updater::Updater() : juce::Thread ("Ohpe updater")
{
    loadSettings();
}

Updater::~Updater()
{
    cancelFlag = true;
    stopThread (8000);
    cancelPendingUpdate();
}

update::Version Updater::currentVersion() { return update::Version::of (OHPE_VERSION); }

update::Version Updater::appVersion() const
{
    if (testVersion.isNotEmpty())
        if (const auto v = update::Version::of (std8 (testVersion)); v.valid()) return v;
    return currentVersion();
}

void Updater::setAppVersionForTests (const juce::String& v)
{
    testVersion = v;
    loadSettings();
}

juce::File Updater::dataFolder()
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("OHPE_DATA_DIR", {});
    if (env.isNotEmpty()) return juce::File (env);
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile ("Application Support");
#endif
    return base.getChildFile ("groovex").getChildFile ("Ohpe");
}

juce::File Updater::contentFolder() { return dataFolder().getChildFile ("Content"); }

juce::File Updater::folder() const { return testFolder != juce::File() ? testFolder : dataFolder(); }

void Updater::setDataFolderForTests (const juce::File& f)
{
    testFolder = f;
    loadSettings();
}

void Updater::loadSettings()
{
    const auto f = folder().getChildFile ("Settings").getChildFile ("update.json");
    const auto defaults = update::Settings::defaults (appVersion());
    const auto reg = folder().getChildFile ("Content").getChildFile ("installed.json");
    std::lock_guard<std::mutex> g (lock);
    prefs = f.existsAsFile() ? update::Settings::fromJson (std8 (f.loadFileAsString()), defaults) : defaults;
    registry = reg.existsAsFile() ? update::ContentRegistry::fromJson (std8 (reg.loadFileAsString())) : update::ContentRegistry {};
}

void Updater::saveSettings()
{
    std::string text;
    {
        std::lock_guard<std::mutex> g (lock);
        text = prefs.toJson();
    }
    const auto f = folder().getChildFile ("Settings").getChildFile ("update.json");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (str (text));
}

Updater::Snapshot Updater::snapshot() const
{
    std::lock_guard<std::mutex> g (lock);
    return snap;
}

update::Settings Updater::settings() const
{
    std::lock_guard<std::mutex> g (lock);
    return prefs;
}

void Updater::setAutoCheck (bool on)
{
    { std::lock_guard<std::mutex> g (lock); prefs.autoCheck = on; }
    saveSettings();
    sendChangeMessage();
}

void Updater::setChannel (update::Channel c)
{
    { std::lock_guard<std::mutex> g (lock); prefs.channel = c; }
    saveSettings();
    sendChangeMessage();
}

void Updater::setState (State s, const juce::String& msg, const juce::String& detail)
{
    {
        std::lock_guard<std::mutex> g (lock);
        snap.state = s;
        snap.message = msg;
        snap.detail = detail;
    }
    triggerAsyncUpdate();
}

void Updater::handleAsyncUpdate()
{
    sendChangeMessage();
}

void Updater::startJob (std::function<void()> j)
{
    if (isThreadRunning()) return;
    job = std::move (j);
    cancelFlag = false;
    startThread (juce::Thread::Priority::low);
}

void Updater::run()
{
    if (auto j = std::move (job)) j();
    triggerAsyncUpdate();
}

bool Updater::waitIdle (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (isThreadRunning())
    {
        if (juce::Time::getMillisecondCounter() > end) return false;
        juce::Thread::sleep (10);
    }
    return true;
}

void Updater::checkNow() { startJob ([this] { doCheck (false); }); }

void Updater::maybeAutoCheck (bool playing)
{
    if (isThreadRunning()) return;
    const auto now = juce::Time::currentTimeMillis() / 1000;
    if (update::autoCheckDue (settings(), now, playing)) startJob ([this] { doCheck (true); });
}

juce::String Updater::fetchText (const juce::String& urlText, int* statusOut, bool* tooLargeOut)
{
    if (tooLargeOut != nullptr) *tooLargeOut = false;
    if (statusOut != nullptr) *statusOut = 0;
    const juce::URL url (urlText);
    if (url.isLocalFile())
    {
        const auto f = url.getLocalFile();
        if (! f.existsAsFile()) return {};
        if (statusOut != nullptr) *statusOut = 200;
        if (f.getSize() > maxTextBytes) { if (tooLargeOut != nullptr) *tooLargeOut = true; return {}; }
        return f.loadFileAsString();
    }
    // p81: the same stall watchdog as downloads - a stack that ignores its timeout must not hang the check
    StallWatchdog watchdog ([this] { return threadShouldExit() || cancelFlag.load(); });
    const int stallMs = juce::jmax (2000, 2 * netTimeoutMs);
    juce::WebInputStream in (url, false);
    in.withExtraHeaders ("User-Agent: " + userAgent() + "\r\nAccept: application/vnd.github+json")
      .withConnectionTimeout (netTimeoutMs)
      .withNumRedirectsToFollow (8);
    watchdog.watch (&in, stallMs);
    const bool connected = in.connect (nullptr) && ! watchdog.hasFired();
    const int status = connected ? in.getStatusCode() : 0;
    if (statusOut != nullptr) *statusOut = status;
    if (! connected || (status != 200 && status != 0)) { watchdog.unwatch(); return {}; }
    // p81: never read more than maxTextBytes (a wrong URL or a hostile server could stream without end)
    juce::MemoryBlock mb;
    juce::HeapBlock<char> buf (16384);
    bool broken = false;
    while (mb.getSize() <= (size_t) maxTextBytes)
    {
        const int n = in.read (buf.get(), 16384);
        if (watchdog.hasFired()) { broken = true; break; }
        if (n <= 0) break;
        mb.append (buf.get(), (size_t) n);
        watchdog.progressed();
    }
    watchdog.unwatch();
    if (broken) { if (statusOut != nullptr) *statusOut = 0; return {}; }
    if (mb.getSize() > (size_t) maxTextBytes) { if (tooLargeOut != nullptr) *tooLargeOut = true; return {}; }
    return juce::String::fromUTF8 ((const char*) mb.getData(), (int) mb.getSize());
}

void Updater::doCheck (bool automatic)
{
    setState (State::Checking, "Checking for updates");
    int status = 0;
    bool tooLarge = false;
    const auto text = fetchText (listUrl, &status, &tooLarge);
    if (threadShouldExit()) return;
    if (status != 0 && status != 200)
    {
        // p81: the server answered, but with an error (rate limit, moved, down) - not "offline"
        setState (State::Failed, "The update server answered with an error", "HTTP " + juce::String (status));
        return;
    }
    if (text.isEmpty() && ! tooLarge)
    {
        setState (State::Offline, automatic ? "Could not check for updates (offline?)" : "No connection to the update server", {});
        return;
    }
    bool listOk = false;
    const auto list = tooLarge ? std::vector<update::ReleaseRef> {} : update::parseReleaseList (std8 (text), &listOk);
    if (! listOk)
    {
        // p81: a cut-off, malformed or foreign answer (captive portal page, error object) used to read as an empty
        // list and say "Ohpe is up to date"
        setState (State::Failed, "The update server sent an answer Ohpe cannot read", {});
        return;
    }
    const auto cur = appVersion();
    update::Settings s = settings();
    const int pick = update::pickUpdate (list, s.channel, cur, s.skipped);
    const int prev = update::pickPrevious (list, cur);

    auto resolve = [this] (const update::ReleaseRef& ref) {
        update::Release r = ref.fallback;
        if (! ref.manifestUrl.empty())
        {
            update::Release m;
            if (update::parseManifest (std8 (fetchText (str (ref.manifestUrl))), m) && m.version == ref.version)
            {
                if (m.page.empty()) m.page = r.page;
                return m;
            }
        }
        if (! ref.sumsUrl.empty()) update::applySums (r, std8 (fetchText (str (ref.sumsUrl))));
        return r;
    };

    // content packs: offered by the newest release of the channel (also when the app is up to date)
    int contentFrom = pick;
    if (contentFrom < 0)
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].version == cur && ! list[(size_t) i].manifestUrl.empty()) contentFrom = i;

    update::Release offered, previous, contentSource;
    const bool has = pick >= 0;
    if (has) offered = resolve (list[(size_t) pick]);
    if (prev >= 0) previous = resolve (list[(size_t) prev]);
    if (contentFrom >= 0) contentSource = contentFrom == pick ? offered : resolve (list[(size_t) contentFrom]);

    const auto now = juce::Time::currentTimeMillis() / 1000;
    {
        std::lock_guard<std::mutex> g (lock);
        prefs.lastCheck = now;
        snap.hasRelease = has && (! automatic || update::shouldOffer (prefs, offered.version, now));
        snap.release = offered;
        snap.hasPrevious = prev >= 0;
        snap.previous = previous;
        snap.packs.clear();
        for (const auto& p : contentSource.content)
        {
            if (! update::isSafePackId (p.id)) continue;
            PackRow row;
            row.pack = p;
            snap.packs.push_back (row);
        }
    }
    saveSettings();
    refreshPacks();
    if (snapshot().hasRelease)
        setState (State::Available, "New version available", str (offered.version.toString()));
    else
        setState (State::UpToDate, "Ohpe is up to date", str (cur.toString()));
}

void Updater::refreshPacks()
{
    const auto cur = appVersion();
    std::lock_guard<std::mutex> g (lock);
    for (auto& row : snap.packs)
    {
        row.state = update::packState (row.pack, registry, cur);
        const auto* inst = registry.find (row.pack.id);
        row.installedVersion = inst != nullptr ? str (inst->version) : juce::String();
        row.canRollBack = inst != nullptr && ! inst->previousVersion.empty()
                          && folder().getChildFile ("Content").getChildFile (".backup").getChildFile (str (row.pack.id)).isDirectory();
    }
}

update::DownloadReport Updater::fetchFile (const juce::String& url, const juce::File& target, const std::string& sha256, uint64_t size)
{
    UrlTransport net (*this, netTimeoutMs, [this] { return cancelFlag.load(); });
    LocalPartialFile part (target.withFileExtension (target.getFileExtension() + ".part"));
    update::DownloadOptions o;
    o.url = std8 (url);
    o.sha256 = sha256;
    o.expectedSize = size;
    o.maxBytes = maxDownloadBytes;
    o.cancel = &cancelFlag;
    o.waitBeforeRetry = [this] (int attempt) {
        const int ms = (int) juce::jmin (16.0 * retryBaseMs_, retryBaseMs_ * std::pow (2.0, attempt - 1));
        for (int t = 0; t < ms && ! cancelFlag && ! threadShouldExit(); t += 50) juce::Thread::sleep (50);
    };
    o.progress = [this] (uint64_t done, uint64_t total) {
        {
            std::lock_guard<std::mutex> g (lock);
            const double p = total > 0 ? (double) done / (double) total : 0.0;
            if (std::abs (p - snap.progress) < 0.004 && p < 1.0) return;
            snap.progress = p;
        }
        triggerAsyncUpdate();
    };
    auto rep = update::download (net, part, o);
    part.finish();
    if (rep.result == update::DownloadResult::Ok)
    {
        target.deleteFile();
        if (! target.withFileExtension (target.getFileExtension() + ".part").moveFileTo (target)) rep.result = update::DownloadResult::WriteFailed;
    }
    return rep;
}

void Updater::downloadUpdate() { startJob ([this] { doDownload (false); }); }
void Updater::downloadPrevious() { startJob ([this] { doDownload (true); }); }

void Updater::doDownload (bool previous)
{
    update::Release r;
    {
        std::lock_guard<std::mutex> g (lock);
        if (previous ? ! snap.hasPrevious : ! snap.hasRelease) return;
        r = previous ? snap.previous : snap.release;
        snap.progress = 0.0;
        snap.downloadingPrevious = previous;
        snap.downloaded = juce::File();
    }
    const auto* asset = r.assetFor (update::currentPlatform(), std8 (part));
    if (asset == nullptr)
    {
        setState (State::Failed, "No download for this system - see the release page", str (r.page));
        return;
    }
    if (asset->sha256.empty())
    {
        setState (State::Failed, "The release has no checksum - not installed", str (asset->name));
        return;
    }
    if (asset->size > maxDownloadBytes)
    {
        setState (State::Failed, "The download is larger than allowed and was refused", str (asset->name));
        return;
    }
    setState (State::Downloading, previous ? "Downloading the previous version" : "Downloading the update", str (asset->name));
    const auto target = folder().getChildFile ("Updates").getChildFile (safeFileName (asset->name, "Ohpe-update"));
    const auto rep = fetchFile (str (asset->url), target, asset->sha256, asset->size);
    switch (rep.result)
    {
        case update::DownloadResult::Ok:
        {
            std::lock_guard<std::mutex> g (lock);
            snap.downloaded = target;
            snap.progress = 1.0;
            break;
        }
        case update::DownloadResult::Cancelled: setState (State::Available, "Download paused - it continues where it stopped", str (asset->name)); return;
        case update::DownloadResult::ChecksumMismatch:
        case update::DownloadResult::SizeMismatch: setState (State::Failed, "The download failed the checksum test and was deleted", str (asset->name)); return;
        case update::DownloadResult::WriteFailed: setState (State::Failed, "Could not write the download", target.getFullPathName()); return;
        case update::DownloadResult::NetworkFailed: setState (State::Failed, "Download interrupted - try again to continue it", str (asset->name)); return;
        case update::DownloadResult::TooLarge: setState (State::Failed, "The download is larger than allowed and was refused", str (asset->name)); return;
    }
    setState (State::Ready, asset->kind == "installer" ? "Verified - ready to install" : "Verified - ready to unpack", str (asset->name));
}

void Updater::cancel() { cancelFlag = true; }

bool Updater::openDownloaded()
{
    const auto s = snapshot();
    if (s.state != State::Ready || ! s.downloaded.existsAsFile()) return false;
    const auto* asset = (s.downloadingPrevious ? s.previous : s.release).assetFor (update::currentPlatform(), std8 (part));
    // check again right before opening: the file must still be the verified one
    LocalPartialFile f (s.downloaded);
    if (asset == nullptr || ! update::verifyFile (f, asset->sha256, asset->size))
    {
        s.downloaded.deleteFile();
        setState (State::Failed, "The download failed the checksum test and was deleted", s.downloaded.getFileName());
        return false;
    }
    if (asset->kind == "installer")
        return s.downloaded.startAsProcess();
    s.downloaded.revealToUser();
    return true;
}

void Updater::skipThisVersion()
{
    {
        std::lock_guard<std::mutex> g (lock);
        if (! snap.hasRelease) return;
        update::skipVersion (prefs, snap.release.version);
        snap.hasRelease = false;
        snap.state = State::UpToDate;
        snap.message = "Version skipped";
        snap.detail = str (prefs.skipped);
    }
    saveSettings();
    sendChangeMessage();
}

void Updater::remindLater()
{
    {
        std::lock_guard<std::mutex> g (lock);
        if (! snap.hasRelease) return;
        update::remindLater (prefs, snap.release.version, juce::Time::currentTimeMillis() / 1000);
        snap.message = "I will remind you in three days";
    }
    saveSettings();
    sendChangeMessage();
}

void Updater::installPack (const juce::String& id) { startJob ([this, id] { doInstallPack (id); }); }

void Updater::doInstallPack (juce::String id)
{
    update::ContentPack pack;
    {
        std::lock_guard<std::mutex> g (lock);
        bool found = false;
        for (const auto& row : snap.packs)
            if (str (row.pack.id) == id) { pack = row.pack; found = true; }
        if (! found || ! update::isSafePackId (pack.id)) return;
        snap.busyPack = id;
        snap.progress = 0.0;
    }
    auto done = [this] (State s, const juce::String& msg, const juce::String& detail) {
        { std::lock_guard<std::mutex> g (lock); snap.busyPack.clear(); }
        setState (s, msg, detail);
    };
    if (pack.sha256.empty()) { done (State::Failed, "The release has no checksum - not installed", id); return; }
    if (pack.size > maxDownloadBytes) { done (State::Failed, "The download is larger than allowed and was refused", str (pack.nameEn)); return; }
    bool needsNewer = false;
    {
        std::lock_guard<std::mutex> g (lock);
        needsNewer = update::packState (pack, registry, appVersion()) == update::PackState::NeedsNewerApp;
    }
    if (needsNewer)
    {
        done (State::Failed, "This pack needs a newer Ohpe", str (pack.minApp)); // p81: not downloaded at all
        return;
    }
    const auto prevState = snapshot().state;
    setState (State::Downloading, "Downloading the content pack", str (pack.nameEn));
    const auto content = folder().getChildFile ("Content");
    const auto zipFile = folder().getChildFile ("Updates").getChildFile ("content").getChildFile (id + "-" + str (pack.version) + ".zip");
    const auto rep = fetchFile (str (pack.url), zipFile, pack.sha256, pack.size);
    if (rep.result != update::DownloadResult::Ok)
    {
        done (State::Failed, rep.result == update::DownloadResult::ChecksumMismatch || rep.result == update::DownloadResult::SizeMismatch
                                 ? "The download failed the checksum test and was deleted"
                             : rep.result == update::DownloadResult::TooLarge ? "The download is larger than allowed and was refused"
                                                                              : "Download interrupted - try again to continue it",
              str (pack.nameEn));
        return;
    }
    // unpack (shared with "Install pack ...": path-safe, kind folders, size limits, quota, one step of backup)
    update::PackInfo expected;
    expected.id = pack.id;
    expected.version = pack.version;
    expected.kind = pack.kind;
    expected.nameEn = pack.nameEn;
    expected.nameDe = pack.nameDe;
    expected.minApp = pack.minApp;
    update::ContentRegistry reg;
    {
        std::lock_guard<std::mutex> g (lock);
        reg = registry;
    }
    content::InstallOptions opt;
    opt.app = appVersion();
    opt.cancel = &cancelFlag;
    const auto res = content::installZip (zipFile, content, expected, reg, opt);
    if (! res.ok)
    {
        done (State::Failed, res.error, res.detail.isNotEmpty() ? res.detail : str (pack.nameEn));
        return;
    }
    zipFile.deleteFile();
    {
        std::lock_guard<std::mutex> g (lock);
        registry = reg;
    }
    refreshPacks();
    ++contentGen;
    done (prevState == State::Available ? State::Available : State::UpToDate, "Content pack installed", str (pack.nameEn) + " " + str (pack.version));
}

void Updater::installPackFile (const juce::File& zip) { startJob ([this, zip] { doInstallPackFile (zip); }); }

void Updater::doInstallPackFile (juce::File zip)
{
    const auto prevState = snapshot().state;
    auto done = [this] (State s, const juce::String& msg, const juce::String& detail) {
        { std::lock_guard<std::mutex> g (lock); snap.busyPack.clear(); }
        setState (s, msg, detail);
    };
    const bool ohpepack = zip.hasFileExtension (update::packFileExtension);
    if (! zip.existsAsFile() || ! (ohpepack || zip.hasFileExtension ("zip")))
    {
        done (State::Failed, "Choose a content pack (.ohpepack)", zip.getFileName());
        return;
    }
    {
        std::lock_guard<std::mutex> g (lock);
        snap.busyPack = zip.getFileName();
        snap.progress = 0.0;
    }
    update::ContentRegistry reg;
    {
        std::lock_guard<std::mutex> g (lock);
        reg = registry;
    }
    content::InstallOptions opt;
    opt.requireChecksums = ohpepack; // old .zip packs (flat, no checksums) still install
    opt.app = appVersion();
    opt.cancel = &cancelFlag;
    const auto res = content::installZip (zip, folder().getChildFile ("Content"), {}, reg, opt);
    if (! res.ok) { done (State::Failed, res.error, res.detail.isNotEmpty() ? res.detail : zip.getFileName()); return; }
    {
        std::lock_guard<std::mutex> g (lock);
        registry = reg;
    }
    refreshPacks();
    ++contentGen;
    done (prevState == State::Available ? State::Available : (prevState == State::Failed || prevState == State::Idle ? State::UpToDate : prevState),
          "Content pack installed", str (res.info.nameEn) + " " + str (res.info.version));
}

bool Updater::removePack (const juce::String& id)
{
    if (isThreadRunning()) return false;
    bool ok = false;
    {
        std::lock_guard<std::mutex> g (lock);
        ok = content::removePack (folder().getChildFile ("Content"), id, registry);
    }
    if (! ok) return false;
    refreshPacks();
    {
        std::lock_guard<std::mutex> g (lock);
        snap.message = "Content pack removed";
        snap.detail = id;
    }
    ++contentGen;
    sendChangeMessage();
    return true;
}

std::vector<Updater::InstalledPack> Updater::installedPacks() const
{
    update::ContentRegistry reg;
    {
        std::lock_guard<std::mutex> g (lock);
        reg = registry;
    }
    std::vector<InstalledPack> out;
    const auto dir = folder().getChildFile ("Content");
    for (const auto& p : content::scan (dir).packs)
    {
        InstalledPack row;
        row.info = content::installedInfo (dir, str (p.id), &reg);
        row.bytes = content::packBytes (dir, str (p.id));
        row.items = (int) p.items.size();
        out.push_back (std::move (row));
    }
    return out;
}

bool Updater::rollbackPack (const juce::String& id)
{
    if (isThreadRunning() || ! update::isSafePackId (std8 (id))) return false;
    const auto content = folder().getChildFile ("Content");
    const auto target = content.getChildFile (id), backup = content.getChildFile (".backup").getChildFile (id);
    if (! backup.isDirectory()) return false;
    // p81: the registry changes only after the folders moved (a failed move used to leave it pointing at the
    // backup version while the files were still the new ones)
    update::ContentRegistry next;
    {
        std::lock_guard<std::mutex> g (lock);
        next = registry;
    }
    if (! next.rolledBack (std8 (id))) return false;
    const auto regText = next.toJson();
    const auto old = content.getChildFile (".staging").getChildFile (id + "-old");
    old.deleteRecursively();
    old.getParentDirectory().createDirectory();
    if (target.isDirectory() && ! target.moveFileTo (old)) return false;
    if (! backup.moveFileTo (target))
    {
        old.moveFileTo (target);
        return false;
    }
    old.deleteRecursively();
    content.getChildFile ("installed.json").replaceWithText (str (regText));
    {
        std::lock_guard<std::mutex> g (lock);
        registry = next;
    }
    refreshPacks();
    {
        std::lock_guard<std::mutex> g (lock);
        snap.message = "Content pack rolled back";
        snap.detail = id;
    }
    ++contentGen;
    sendChangeMessage();
    return true;
}

void Updater::showReleaseForTests (const update::Release& r, State s, double progress)
{
    {
        std::lock_guard<std::mutex> g (lock);
        snap.release = r;
        snap.hasRelease = true;
        snap.progress = progress;
        snap.packs.clear();
        for (const auto& p : r.content)
        {
            PackRow row;
            row.pack = p;
            snap.packs.push_back (row);
        }
    }
    refreshPacks();
    {
        std::lock_guard<std::mutex> g (lock);
        snap.state = s;
        snap.message = s == State::Downloading ? "Downloading the update" : "New version available";
        snap.detail = str (r.version.toString());
    }
    sendChangeMessage();
}
} // namespace ohpe
