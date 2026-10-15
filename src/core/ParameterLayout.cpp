// Ohpe - designed by groovex
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// THE canonical parameter order. Host automation in every saved project refers to these
// parameters, MIDI maps and presets refer to their IDs.
//
//   * Never remove, rename, reorder or change the meaning / range of an existing entry.
//   * Add new parameters ONLY at the end of this function (bottom section).
//   * tests/core/golden/param_ids_v1.txt must stay a prefix of the generated ID list
//     (test "parameter IDs are append-only"); extend the golden file when you append.

#include "OhpeCore.h"

namespace ohpe
{
void buildParameterLayout (ParamRegistry& reg, const std::array<std::unique_ptr<Part>, Core::numParts>& parts, const Mixer& mixer,
                           const StepSequencer& stepSeq)
{
    using namespace pd;

    // ---- 0.1.0: global ------------------------------------------------------------------
    {
        auto t = continuous (Core::Ids::tempo, "Tempo", "Tempo", 20.0f, 300.0f, 120.0f, ParamUnit::Bpm);
        t.group = "global";
        reg.add (t);
        auto run = toggle (Core::Ids::seqRun, "Sequencer Run", "Sequencer Start", false);
        run.group = "global";
        reg.add (run);
    }

    // ---- 0.1.0: part 1 (common, then engines FM and Analog) -----------------------------
    parts[0]->registerCommonParams (reg);
    parts[0]->registerEngineParams (reg, EngineType::FM);
    parts[0]->registerEngineParams (reg, EngineType::Analog);

    // ---- 0.1.0: mixer, send effects -----------------------------------------------------
    mixer.registerParams (reg);

    // ---- 0.1.0: step sequencer ----------------------------------------------------------
    for (auto d : stepSeq.getParamDescriptors())
    {
        d.id = std::string (Core::Ids::stepPrefix) + d.id;
        d.group = "sequencer/step";
        reg.add (std::move (d));
    }

    // ---- append new parameters below this line (phase 2+), never above ------------------

    // ---- 0.2.0: part 1 engines Wavetable, Pluck, Cluster --------------------------------
    parts[0]->registerEngineParams (reg, EngineType::Wavetable);
    parts[0]->registerEngineParams (reg, EngineType::Pluck);
    parts[0]->registerEngineParams (reg, EngineType::Cluster);
}
} // namespace ohpe
