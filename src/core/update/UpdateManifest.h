// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Version.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ohpe::update
{
/**
    p27: the update manifest - what a release offers to the in-app updater.

    Every GitHub release of the project carries an asset "ohpe-update.json" (written by tools/update/make_manifest.py
    in the release-assets workflow):

        { "format": 1, "app": "Ohpe", "version": "1.0.0", "tag": "v1.0.0", "channel": "stable", "date": "2026-11-02",
          "page": "https://github.com/.../releases/tag/v1.0.0",
          "notes": { "en": "...", "de": "..." },
          "assets": [ { "platform": "macos", "kind": "installer", "part": "all", "name": "...dmg", "url": "...",
                        "size": 123, "sha256": "..." }, ... ],
          "content": [ { "id": "factory-keys", "version": "1.1.0", "kind": "presets", "name": { "en": "...", "de": "..." },
                         "minApp": "1.0.0", "url": "...", "size": 123, "sha256": "..." } ] }

    Releases without a manifest (older ones) are read from the GitHub release itself: the body's "## English" /
    "## Deutsch" sections become the notes, the asset names give platform and part, SHA256SUMS.txt the checksums.
*/
enum class Channel { Stable = 0, Preview };
const char* channelName (Channel c) noexcept;
Channel channelFromName (const std::string& s) noexcept;

/** The platform this build runs on: "macos", "windows" or "linux". */
const char* currentPlatform() noexcept;

struct Asset
{
    std::string platform; ///< "macos", "windows", "linux"
    std::string kind;     ///< "installer" (launched after the download) or "archive" (opened / revealed)
    std::string part;     ///< "all", "app" (standalone), "plugin" (VST3 / AU)
    std::string name, url, sha256;
    uint64_t size = 0;
};

struct ContentPack
{
    std::string id, version, kind; ///< kind: "presets", "samples", "wavetables", ...
    std::string nameEn, nameDe;
    std::string minApp; ///< oldest app version that can use the pack (empty = any)
    std::string url, sha256;
    uint64_t size = 0;
};

struct Release
{
    Version version;
    std::string tag, date, page;
    Channel channel = Channel::Stable;
    std::string notesEn, notesDe;
    std::vector<Asset> assets;
    std::vector<ContentPack> content;

    /** The asset to download for `platform` and `part` ("app" / "plugin"): an exact part first, then "all".
        Installers win over archives. nullptr if the release has nothing for this platform. */
    const Asset* assetFor (const std::string& platform, const std::string& part) const;
};

/** Parses a manifest; false (with `error`) if it is not one (wrong format, no valid version). */
bool parseManifest (const std::string& json, Release& out, std::string* error = nullptr);
std::string manifestToJson (const Release& r);

/** One release of the GitHub list (GET /repos/<owner>/<repo>/releases). */
struct ReleaseRef
{
    Version version;
    bool prerelease = false;
    std::string manifestUrl; ///< the "ohpe-update.json" asset, empty if the release has none
    std::string sumsUrl;     ///< "SHA256SUMS.txt", empty if none
    Release fallback;        ///< the release as read from GitHub (used without a manifest; checksums from sumsUrl)
};

constexpr const char* manifestAssetName = "ohpe-update.json";
constexpr const char* sumsAssetName = "SHA256SUMS.txt";

/** Parses the GitHub release list. Drafts and tags that are no version are left out. `ok` (optional) = the text was
    a JSON array (false: not JSON, an object such as an error message, a cut-off answer - not "no releases"). */
std::vector<ReleaseRef> parseReleaseList (const std::string& json, bool* ok = nullptr);

/** Splits a bilingual release body at its "## English" / "## Deutsch" headings (both = the whole text without them). */
void splitNotes (const std::string& body, std::string& en, std::string& de);

/** "<sha256>  <file name>" lines (sha256sum format, "*" binary marker allowed) -> file name -> lower-case digest. */
std::map<std::string, std::string> parseSha256Sums (const std::string& text);
/** Fills empty asset checksums from a SHA256SUMS.txt text. */
void applySums (Release& r, const std::string& sumsText);

/** Index into `list` of the release to offer: the newest of `channel` (stable sees releases only, preview also
    pre-releases) that is newer than `current`; -1 if there is none or it is the version the user skipped. */
int pickUpdate (const std::vector<ReleaseRef>& list, Channel channel, const Version& current, const std::string& skipped = {});
/** Index of the newest release older than `current` (roll back to the previous version); -1 if none.
    A stable build (1.0 and later) prefers the previous stable release. */
int pickPrevious (const std::vector<ReleaseRef>& list, const Version& current);
} // namespace ohpe::update
