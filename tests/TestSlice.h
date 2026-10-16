// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Long test cases that loop over many independent items (every factory preset, every engine x
// target ...) can be split over several processes. Such a test case is tagged with
// doctest::test_suite ("sliced") and asks testSlice().has (n) for its n-th item.
// scripts/ci/run-tests.py runs every sliced test case once per slice with OHPE_TEST_SLICE=k/count;
// every item belongs to exactly one slice. Without the variable (plain ctest, a local run) a
// test case does all of its items, as before.
#pragma once

#include <cstdlib>
#include <cstdio>

namespace ohpe::test
{
struct Slice
{
    int index = 0, count = 1;
    bool has (long long item) const { return count <= 1 || (int) (item % count) == index; }
};

inline Slice testSlice()
{
    Slice s;
    if (const char* env = std::getenv ("OHPE_TEST_SLICE"))
    {
        int k = 0, n = 1;
        if (std::sscanf (env, "%d/%d", &k, &n) == 2 && n >= 1 && k >= 0 && k < n)
        {
            s.index = k;
            s.count = n;
        }
    }
    return s;
}
} // namespace ohpe::test
