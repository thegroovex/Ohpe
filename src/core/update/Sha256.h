// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ohpe::update
{
/** p27: SHA-256 (FIPS 180-4), incremental. Verifies downloaded installers and content packs. */
class Sha256
{
public:
    Sha256() { reset(); }
    void reset();
    void update (const void* data, size_t size);
    std::array<uint8_t, 32> finish();
    /** finish() as 64 lower-case hex digits. */
    std::string finishHex();

    static std::string hex (const std::array<uint8_t, 32>& digest);
    static std::string of (const std::string& data);
    /** Case-insensitive compare of two hex digests (false if either is not 64 hex digits). */
    static bool sameDigest (const std::string& a, const std::string& b);

private:
    void block (const uint8_t* p);
    std::array<uint32_t, 8> h {};
    std::array<uint8_t, 64> buf {};
    size_t used = 0;
    uint64_t total = 0;
};
} // namespace ohpe::update
