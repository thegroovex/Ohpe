// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>

namespace ohpe::update
{
/**
    p27: a release version "MAJOR.MINOR.PATCH[-PRERELEASE]" (a leading "v" is accepted, build metadata after "+" is
    ignored). Ordering follows semantic versioning: a pre-release sorts before its release (1.0.0-preview.2 < 1.0.0),
    pre-release identifiers compare dot by dot, numeric ones numerically (preview.10 > preview.9).
*/
struct Version
{
    int major = 0, minor = 0, patch = 0;
    std::string pre; ///< pre-release identifiers without the "-" ("preview.2"), empty for a release

    /** Parses `text`; returns false (and leaves `out` unchanged) for anything that is not a version. */
    static bool parse (const std::string& text, Version& out);
    /** Parsed version, or 0.0.0 with valid() == false. */
    static Version of (const std::string& text);

    bool valid() const noexcept { return ok; }
    bool isPreRelease() const noexcept { return ! pre.empty(); }
    std::string toString() const;

    /** < 0, 0, > 0 like strcmp. */
    static int compare (const Version& a, const Version& b);
    friend bool operator< (const Version& a, const Version& b) { return compare (a, b) < 0; }
    friend bool operator> (const Version& a, const Version& b) { return compare (a, b) > 0; }
    friend bool operator<= (const Version& a, const Version& b) { return compare (a, b) <= 0; }
    friend bool operator>= (const Version& a, const Version& b) { return compare (a, b) >= 0; }
    friend bool operator== (const Version& a, const Version& b) { return compare (a, b) == 0; }
    friend bool operator!= (const Version& a, const Version& b) { return compare (a, b) != 0; }

    bool ok = false;
};
} // namespace ohpe::update
