// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "update/ContentPacks.h"
#include "update/PackFormat.h"

#include <juce_core/juce_core.h>

#include <atomic>

namespace ohpe
{
/**
    Content packs on disk (<user data>/groovex/Ohpe/Content): the one place that unpacks a pack zip - downloaded by
    the updater or picked with "Install pack ..." - and that reads what is installed.

    Unpacking follows update::planPack: every entry path checked, one fixed folder per kind (flat old packs moved
    into it), size limits per kind and the total quota of Content/, pack.json read (and kept in the pack folder).
    The pack is unpacked into Content/.staging/<id>/ first; the installed version moves to Content/.backup/<id>/
    (one step of rollback) and the new one takes its place. Nothing outside Content/<id>/ is ever written.
    Any thread but the audio thread; one install at a time (the updater's thread).
*/
namespace content
{
struct InstallResult
{
    bool ok = false;
    juce::String error;  ///< English UI text (translated by the UI)
    juce::String detail; ///< not translated
    update::PackInfo info;
    bool flat = false;
    int skipped = 0;
    int files = 0;
};

/**
    Installs `zip` into `contentFolder`. `expected`: what the caller knows (manifest entry; or nothing for a local
    file - then the file name "Ohpe-content-<id>-<version>.zip" fills id / version if the zip has no pack.json).
    Updates `registry` and writes Content/installed.json on success.
*/
InstallResult installZip (const juce::File& zip, const juce::File& contentFolder, const update::PackInfo& expected,
                          update::ContentRegistry& registry, const std::atomic<bool>* cancel = nullptr);

/** Bytes used by the installed packs (without .backup / .staging), leaving out pack `exceptId`. */
uint64_t usedBytes (const juce::File& contentFolder, const juce::String& exceptId = {});

/** pack.json of an installed pack (or, for old packs, id / version from installed.json and the name = id). */
update::PackInfo installedInfo (const juce::File& contentFolder, const juce::String& id, const update::ContentRegistry* registry = nullptr);

/** The installed packs and their content (pack folders without a valid id are left out), sorted by id. */
update::ContentIndex scan (const juce::File& contentFolder);

/** The folder of a kind inside an installed pack. Old flat packs (installed before kind folders) keep presets at
    their root: presets are read from the whole pack folder. */
juce::File kindFolder (const juce::File& contentFolder, const juce::String& packId, update::PackKind kind);

/** The file a reference points at (Content/<pack>/wavetables/<item>.wav); juce::File() if the reference is not valid. */
juce::File fileFor (const juce::File& contentFolder, const update::ContentRef& ref, update::PackKind kind);
} // namespace content
} // namespace ohpe
