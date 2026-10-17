// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>

namespace ohpe::i18n
{
enum class Language { English = 0, German };

/** Current UI language (process-wide). Default: German if the system language is German, else English. */
Language current() noexcept;
void setCurrent (Language lang) noexcept;
/** p32: back to the system language (OHPE_LANGUAGE still wins), as the settings' "System" choice. */
void followSystem() noexcept;
Language systemDefault();

/** Translates an English UI text into the current language (English text = key). */
juce::String tr (const char* english);
juce::String tr (const juce::String& english);

/** Picks the right one of two texts for the current language (for data that carries both, e.g. parameter names). */
inline juce::String pick (const std::string& en, const std::string& de)
{
    return juce::String::fromUTF8 ((current() == Language::German && ! de.empty() ? de : en).c_str());
}

/** Upper case that also maps German umlauts (juce::String::toUpperCase follows the C library,
    which leaves them unchanged in the "C" locale on Linux). */
juce::String upper (const juce::String& text);

/** All keys with a German translation (used by the localisation test). */
juce::StringArray allKeys();

/** Keys that appear more than once in the translation table (must stay empty; checked by a test). */
juce::StringArray duplicateKeys();
} // namespace ohpe::i18n
