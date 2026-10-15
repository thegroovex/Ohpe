// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Prints the parameter table of the core.
//   ohpe_param_dump --ids        one ID per line (compare with tests/core/golden/param_ids_v1.txt)
//   ohpe_param_dump --markdown   reference table for the documentation

#include "OhpeCore.h"

#include <cstring>
#include <iostream>

int main (int argc, char** argv)
{
    ohpe::Core core;
    const auto& reg = core.params();
    const bool markdown = argc > 1 && std::strcmp (argv[1], "--markdown") == 0;
    if (markdown)
        std::cout << "| # | ID | English | Deutsch | Range | Default |\n|---|---|---|---|---|---|\n";
    for (int i = 0; i < reg.size(); ++i)
    {
        const auto& d = reg.descriptor (i);
        if (markdown)
        {
            std::cout << "| " << i << " | `" << d.id << "` | " << d.nameEn << " | " << d.nameDe << " | ";
            if (d.kind == ohpe::ParamKind::Choice)
            {
                for (size_t c = 0; c < d.choices.size(); ++c) std::cout << (c ? ", " : "") << d.choices[c];
            }
            else
                std::cout << d.minValue << " … " << d.maxValue;
            std::cout << " | " << d.defaultValue << " |\n";
        }
        else
            std::cout << d.id << "\n";
    }
    return 0;
}
