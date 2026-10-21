// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace ohpe::jsonutil
{
/** p63: deepest nesting of objects / arrays a document may have. Ohpe's own documents stay below 16; a damaged or
    hostile file nested thousands deep would overflow the stack in the recursive parts of the JSON library
    (dump(), copies), so it is refused before it is parsed. */
constexpr int maxDepth = 256;

/** True when no object / array in `text` is nested deeper than `limit` (strings are skipped). O(n), no allocation. */
inline bool depthWithin (const std::string& text, int limit = maxDepth) noexcept
{
    int depth = 0;
    bool inString = false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (inString)
        {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{' || c == '[') { if (++depth > limit) return false; }
        else if (c == '}' || c == ']') --depth;
    }
    return true;
}

/** Parses without exceptions (like json::parse (text, nullptr, false)); a document nested deeper than maxDepth is
    discarded as well. Check is_discarded() / the type before use, and read values type-checked. */
inline nlohmann::json parse (const std::string& text)
{
    if (! depthWithin (text)) return nlohmann::json (nlohmann::json::value_t::discarded);
    return nlohmann::json::parse (text, nullptr, false);
}
} // namespace ohpe::jsonutil
