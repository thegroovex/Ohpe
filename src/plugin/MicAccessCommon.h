// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
// p86: the test override shared by MicAccess.cpp / MicAccess.mm (internal).
#pragma once

#include "plugin/MicAccess.h"

#include <atomic>

namespace ohpe::mic::detail
{
inline std::atomic<int>& forced() { static std::atomic<int> o { -1 }; return o; }
inline bool overridden (Access& a)
{
    const int o = forced().load();
    if (o < 0) return false;
    a = (Access) o;
    return true;
}
} // namespace ohpe::mic::detail
