# Third-party software

Ohpe is licensed under the AGPL-3.0-or-later. It uses the following third-party software; each
keeps its own licence.

| Library | Use | Licence |
|---|---|---|
| [JUCE](https://juce.com) 8 | plug-in formats, standalone app, UI | AGPLv3 (or commercial) |
| [nlohmann/json](https://github.com/nlohmann/json) | state, presets, MIDI maps | MIT |
| [doctest](https://github.com/doctest/doctest) | unit tests (not shipped) | MIT |
| [Ableton Link](https://github.com/Ableton/link) 4.1 | network tempo sync in the standalone app (build option `OHPE_WITH_LINK`, default on) | GPL-2.0-or-later (compatible with the AGPL-3.0-or-later) |
| Space Grotesk | UI font | SIL Open Font License 1.1 |

Ableton Link is fetched at build time with its bundled standalone Asio (Boost Software License
1.0). "Ableton" and "Link" are trademarks of their owner; Ohpe is not affiliated with it.
