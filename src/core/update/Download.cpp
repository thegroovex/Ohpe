// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Download.h"

#include <algorithm>

#include "Sha256.h"

#include <vector>

namespace ohpe::update
{
const char* resultName (DownloadResult r) noexcept
{
    switch (r)
    {
        case DownloadResult::Ok: return "ok";
        case DownloadResult::NetworkFailed: return "network failed";
        case DownloadResult::ChecksumMismatch: return "checksum mismatch";
        case DownloadResult::SizeMismatch: return "size mismatch";
        case DownloadResult::WriteFailed: return "write failed";
        case DownloadResult::Cancelled: return "cancelled";
        case DownloadResult::TooLarge: return "too large";
    }
    return "?";
}

bool verifyFile (PartialFile& file, const std::string& sha256, uint64_t expectedSize, std::string* digest)
{
    if (expectedSize > 0 && file.size() != expectedSize) return false;
    Sha256 h;
    if (! file.readAll ([&h] (const uint8_t* d, size_t n) { h.update (d, n); })) return false;
    const auto got = h.finishHex();
    if (digest != nullptr) *digest = got;
    return Sha256::sameDigest (got, sha256);
}

DownloadReport download (Transport& net, PartialFile& file, const DownloadOptions& o)
{
    DownloadReport rep;
    auto cancelled = [&o] { return o.cancel != nullptr && o.cancel->load(); };
    std::vector<uint8_t> buffer (64 * 1024);
    int failuresInRow = 0;
    uint64_t total = o.expectedSize;
    bool complete = false;
    // p81: progress = getting further than ever before. An attempt that started over and broke off again below that
    // mark counts as a failure, so a server / network stack that keeps restarting cannot loop for ever
    uint64_t highWater = file.size();
    auto tooLarge = [&o] (uint64_t n) { return o.maxBytes > 0 && n > o.maxBytes; };
    if (tooLarge (total)) { file.clear(); rep.result = DownloadResult::TooLarge; return rep; }

    while (! complete)
    {
        if (cancelled()) { rep.result = DownloadResult::Cancelled; rep.bytes = file.size(); return rep; }
        if (failuresInRow >= o.maxAttempts) { rep.result = DownloadResult::NetworkFailed; rep.bytes = file.size(); return rep; }
        if (failuresInRow > 0 && o.waitBeforeRetry) o.waitBeforeRetry (failuresInRow);
        if (cancelled()) continue;

        uint64_t offset = file.size();
        if (total > 0 && offset > total) { file.clear(); offset = 0; } // a stale, longer file: start over
        if (total > 0 && offset == total) { complete = true; break; }  // already there (an earlier run)

        uint64_t reported = 0;
        bool fromOffset = false;
        ++rep.attempts;
        if (! net.open (o.url, offset, reported, fromOffset))
        {
            ++failuresInRow;
            continue;
        }
        if (offset > 0 && ! fromOffset)
        {
            if (! file.clear()) { net.close(); rep.result = DownloadResult::WriteFailed; return rep; }
            offset = 0;
        }
        else if (offset > 0)
            ++rep.resumes;
        if (total == 0 && reported > 0) total = reported; // the transport reports the full size
        if (tooLarge (total) || tooLarge (reported))
        {
            net.close();
            file.clear();
            rep.result = DownloadResult::TooLarge;
            return rep;
        }
        if (o.progress) o.progress (offset, total);

        bool progressed = false;
        for (;;)
        {
            if (cancelled()) break;
            const auto n = net.read (buffer.data(), buffer.size());
            if (n < 0) break; // broken connection: resume in the next attempt
            if (n == 0) { complete = true; break; }
            if (! file.append (buffer.data(), (size_t) n)) { net.close(); rep.result = DownloadResult::WriteFailed; rep.bytes = file.size(); return rep; }
            progressed = true;
            if (tooLarge (file.size()))
            {
                net.close();
                file.clear();
                rep.result = DownloadResult::TooLarge;
                return rep;
            }
            if (o.progress) o.progress (file.size(), total);
            if (total > 0 && file.size() >= total) { complete = true; break; }
        }
        net.close();
        if (complete && total > 0 && file.size() < total) complete = false; // the stream ended early: resume
        if (progressed && file.size() <= highWater) progressed = false;
        highWater = std::max (highWater, file.size());
        if (! complete) failuresInRow = progressed ? 1 : failuresInRow + 1;
    }

    rep.bytes = file.size();
    if (o.expectedSize > 0 && rep.bytes != o.expectedSize)
    {
        file.clear();
        rep.result = DownloadResult::SizeMismatch;
        return rep;
    }
    Sha256 h;
    if (! file.readAll ([&h] (const uint8_t* d, size_t n) { h.update (d, n); })) { rep.result = DownloadResult::WriteFailed; return rep; }
    rep.digest = h.finishHex();
    if (! o.sha256.empty() && ! Sha256::sameDigest (rep.digest, o.sha256))
    {
        file.clear(); // never hand a damaged or foreign file to the installer
        rep.bytes = 0;
        rep.result = DownloadResult::ChecksumMismatch;
        return rep;
    }
    rep.result = DownloadResult::Ok;
    return rep;
}
} // namespace ohpe::update
