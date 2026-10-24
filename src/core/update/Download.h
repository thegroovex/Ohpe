// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace ohpe::update
{
/**
    p27: a resumable, verified download (no JUCE: the network and the file come in through two small interfaces, so
    the logic - resume after an interruption, retry with back-off, size and SHA-256 check - is tested in the core).

    The partial file is kept between attempts (and between runs: a download that broke off yesterday continues where
    it stopped). A server that ignores the byte range sends the whole file again: the partial file is started over.
    A checksum mismatch deletes the file - it is never handed to the installer.
*/
class Transport
{
public:
    virtual ~Transport() = default;
    /** Opens `url` from byte `offset` on. `total` = the full size if known (0 = unknown); `fromOffset` = the server
        honoured the range (false: the stream starts at byte 0). Returns false if the request failed. */
    virtual bool open (const std::string& url, uint64_t offset, uint64_t& total, bool& fromOffset) = 0;
    /** Reads up to `n` bytes: > 0 bytes read, 0 = end of the stream, < 0 = the connection broke. */
    virtual int64_t read (uint8_t* buffer, size_t n) = 0;
    virtual void close() {}
};

class PartialFile
{
public:
    virtual ~PartialFile() = default;
    virtual uint64_t size() = 0;
    virtual bool append (const uint8_t* data, size_t n) = 0;
    virtual bool clear() = 0;
    /** Feeds the whole file to `sink` in chunks; false if it cannot be read. */
    virtual bool readAll (const std::function<void (const uint8_t*, size_t)>& sink) = 0;
};

enum class DownloadResult { Ok = 0, NetworkFailed, ChecksumMismatch, SizeMismatch, WriteFailed, Cancelled, TooLarge };
const char* resultName (DownloadResult r) noexcept;

struct DownloadOptions
{
    std::string url;
    std::string sha256;      ///< expected digest (hex); empty = not checked (the updater refuses to install then)
    uint64_t expectedSize = 0; ///< 0 = unknown
    uint64_t maxBytes = 0;     ///< p81: hard limit (0 = none): a larger declared or streamed file is refused (TooLarge)
    int maxAttempts = 5;     ///< connection attempts in a row without progress (getting further than ever before)
    std::function<void (int attempt)> waitBeforeRetry; ///< back-off (the plug-in sleeps 1, 2, 4 ... s); may be empty
    std::function<void (uint64_t done, uint64_t total)> progress;
    const std::atomic<bool>* cancel = nullptr;
};

struct DownloadReport
{
    DownloadResult result = DownloadResult::NetworkFailed;
    uint64_t bytes = 0;     ///< final size of the file
    int attempts = 0;       ///< connections opened
    int resumes = 0;        ///< attempts that continued a partial file
    std::string digest;     ///< computed SHA-256 (when the download completed)
};

DownloadReport download (Transport& net, PartialFile& file, const DownloadOptions& options);

/** Verifies an existing file against `sha256` (and `expectedSize` when not 0). */
bool verifyFile (PartialFile& file, const std::string& sha256, uint64_t expectedSize, std::string* digest = nullptr);
} // namespace ohpe::update
