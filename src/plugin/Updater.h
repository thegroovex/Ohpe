// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "update/ContentPacks.h"
#include "update/PackFormat.h"
#include "update/Download.h"
#include "update/UpdateManifest.h"
#include "update/UpdateSettings.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <mutex>

namespace ohpe
{
/**
    p27: the in-app updater (standalone and plug-in; one per process, shared by every open instance through
    juce::SharedResourcePointer<Updater>).

    * Check: reads the public release list of the project on GitHub (no account, no telemetry - nothing about the
      user is sent), picks the newest release of the chosen channel (stable / preview) that is newer than this build,
      reads its manifest (ohpe-update.json; older releases: the release text and SHA256SUMS.txt). "Check now" or the
      opt-in automatic check (at most once a day, never while the transport plays - the editor asks).
    * Update: downloads the installer for this platform and format (macOS installer image; Windows: the app or the
      VST3 archive) on a worker thread with progress; resumes after a broken connection and retries with back-off
      (core: update::download); verifies size and SHA-256 - a file that fails the check is deleted, never opened.
      Then opens the installer (archives: shown in the file manager) - the editor offers to quit the standalone, or
      asks to close the host for the plug-in.
    * Skip this version / remind me later (three days); channel; previous version (rollback: the newest older
      release is downloaded the same way).
    * Content packs (factory presets / samples) from the manifest: downloaded, verified, unpacked into
      Content/<id>/ (old version kept in Content/.backup/<id>/, one step of rollback). User presets and settings are
      never touched.

    Files: <user data>/groovex/Ohpe/Settings/update.json (settings), .../Updates/ (downloads), .../Content/ (packs).
    OHPE_DATA_DIR overrides <user data>/groovex/Ohpe (tests, portable setups).

    All public functions are for the message thread; the work runs on the updater's own thread and every change is
    announced with sendChangeMessage() (on the message thread).
*/
class Updater : public juce::ChangeBroadcaster, private juce::Thread, private juce::AsyncUpdater
{
public:
    Updater();
    ~Updater() override;

    enum class State { Idle = 0, Checking, UpToDate, Available, Downloading, Ready, Failed, Offline };

    struct PackRow
    {
        update::ContentPack pack;
        update::PackState state = update::PackState::NotInstalled;
        juce::String installedVersion;
        bool canRollBack = false;
    };

    struct Snapshot
    {
        State state = State::Idle;
        juce::String message;       ///< English UI text (translate with tr) - the last result or error
        juce::String detail;        ///< not translated (version numbers, file names)
        double progress = 0.0;      ///< 0..1 while downloading
        bool hasRelease = false;    ///< `release` is the version offered
        update::Release release;
        bool hasPrevious = false;   ///< `previous` is the version a rollback installs
        update::Release previous;
        bool downloadingPrevious = false;
        juce::File downloaded;      ///< the verified installer / archive (State::Ready)
        std::vector<PackRow> packs;
        juce::String busyPack;      ///< id of the pack being installed
    };

    Snapshot snapshot() const;
    update::Settings settings() const;
    static update::Version currentVersion();
    /** The version update decisions are made for: currentVersion(), or the one set for tests. */
    update::Version appVersion() const;

    void setAutoCheck (bool on);
    void setChannel (update::Channel c);
    /** "app" (standalone) or "plugin": which download the update picks. */
    void setPart (const juce::String& p) { part = p; }

    void checkNow();
    /** Editor timer: runs the automatic check when it is due (never while `playing`). */
    void maybeAutoCheck (bool playing);
    void downloadUpdate();
    void downloadPrevious();
    void cancel();
    /** Opens the verified download (installer: launched; archive: shown in the file manager). */
    bool openDownloaded();
    void skipThisVersion();
    void remindLater();
    void installPack (const juce::String& id);
    bool rollbackPack (const juce::String& id);
    /** "Install pack ...": an .ohpepack file (picked or dropped on the window) - the same unpacking as a downloaded
        pack (pack.json, kind folders, size limits, quota, one step of backup) plus a SHA-256 for every file (a
        mismatch stops the install, the installed version stays). Old flat .zip packs without pack.json still install,
        named by their file name "Ohpe-content-<id>-<version>.zip". */
    void installPackFile (const juce::File& pack);
    /** Removes an installed pack (and its backup). False while busy or if it is not installed. */
    bool removePack (const juce::String& id);

    struct InstalledPack
    {
        update::PackInfo info; ///< pack.json (license / signature shown, not enforced)
        uint64_t bytes = 0;    ///< on disk
        int items = 0;         ///< presets / tables
    };
    /** The installed packs, sorted by id. */
    std::vector<InstalledPack> installedPacks() const;
    bool busy() const { return isThreadRunning(); }

    /** Counts content pack installs / rollbacks: an editor that sees it change rescans the library. */
    int contentGeneration() const noexcept { return contentGen.load(); }

    static juce::File dataFolder();
    static juce::File contentFolder();

    // tests / screenshot tool
    void setDataFolderForTests (const juce::File& f);
    void setReleaseListUrl (const juce::String& url) { listUrl = url; }
    /** p81 (end-to-end tests): pretend to be `v` (empty = the build's version) - also resets the settings defaults. */
    void setAppVersionForTests (const juce::String& v);
    /** p81: network timeout per request and the first retry pause (doubles per attempt, at most 16x). */
    void setNetworkTimingForTests (int timeoutMs, int retryBaseMs) { netTimeoutMs = timeoutMs; retryBaseMs_ = retryBaseMs; }
    /** p81: the largest download (installer / pack) and the largest text answer (release list, manifest, sums). */
    void setLimitsForTests (uint64_t maxDownload, int maxText) { maxDownloadBytes = maxDownload; maxTextBytes = maxText; }

    /** Size limits: a download larger than this is refused before (declared size) and while (streamed bytes) it runs;
        release list / manifest / checksum list answers larger than maxTextBytesDefault count as unreadable. */
    static constexpr uint64_t maxDownloadBytesDefault = 2048ull * 1024 * 1024;
    static constexpr int maxTextBytesDefault = 4 * 1024 * 1024;
    /** Shows a release as found by a check (no network): state Available or the given state. */
    void showReleaseForTests (const update::Release& r, State s = State::Available, double progress = 0.0);
    bool waitIdle (int timeoutMs);

private:
    void run() override;
    void handleAsyncUpdate() override;
    void startJob (std::function<void()> job);
    void doCheck (bool automatic);
    void doDownload (bool previous);
    void doInstallPack (juce::String id);
    void doInstallPackFile (juce::File zip);
    void setState (State s, const juce::String& msg, const juce::String& detail = {});
    void saveSettings();
    void loadSettings();
    void refreshPacks();
    juce::File folder() const;
    /** The answer's text; empty on failure. `status`: the HTTP status (0 = no answer); `tooLarge`: cut at maxTextBytes. */
    juce::String fetchText (const juce::String& url, int* status = nullptr, bool* tooLarge = nullptr);
    update::DownloadReport fetchFile (const juce::String& url, const juce::File& target, const std::string& sha256, uint64_t size);

    mutable std::mutex lock;
    Snapshot snap;
    update::Settings prefs;
    update::ContentRegistry registry;
    std::function<void()> job;
    std::atomic<bool> cancelFlag { false };
    juce::String part { "plugin" };
    juce::String listUrl { "https://api.github.com/repos/thegroovex/Ohpe/releases?per_page=30" };
    juce::File testFolder;
    juce::String testVersion;
    int netTimeoutMs = 15000, retryBaseMs_ = 1000;
    uint64_t maxDownloadBytes = maxDownloadBytesDefault;
    int maxTextBytes = maxTextBytesDefault;
    std::atomic<int> contentGen { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Updater)
};
} // namespace ohpe
