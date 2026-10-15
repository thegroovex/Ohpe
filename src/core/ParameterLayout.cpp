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
namespace
{
/** Registers descriptors [from, to) of a sequencer's late list (local indices continue after numParams). */
void addLateSeq (ParamRegistry& reg, const std::vector<ParamDescriptor>& list, size_t from, size_t to, const char* prefix, const char* group)
{
    for (size_t i = from; i < std::min (to, list.size()); ++i)
    {
        auto d = list[i];
        d.id = std::string (prefix) + d.id;
        d.group = group;
        reg.add (std::move (d));
    }
}
} // namespace

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
    // ---- phase 2, engines B: Grain, Vocal, Sampler engines of part 1, drum kit ---------------
    parts[0]->registerEngineParams (reg, EngineType::Grain);
    parts[0]->registerEngineParams (reg, EngineType::Vocal);
    parts[0]->registerEngineParams (reg, EngineType::Sampler);
    drum::DrumKit::registerParams (reg);

    // ---- 0.2.0: mixer dynamics, tape machine, part 1 FX slot (package C) ---------------
    mixer.registerDynamicsParams (reg);
    TapeMachine::registerParams (reg);
    InsertSlot::registerParams (reg, parts[0]->prefix() + "fx.", "part1/fx");

    // ---- 0.2.0 (package D): step patterns / song --------------------------------------
    for (auto d : stepSeq.getChainParamDescriptors())
    {
        d.id = std::string (Core::Ids::stepPrefix) + d.id;
        d.group = "sequencer/step";
        reg.add (std::move (d));
    }

    // ---- 0.2.0 (package D): parts 2-4 (common, then engines FM and Analog) -------------
    //      New engines: register them for EVERY part, e.g.
    //      for (auto& p : parts) p->registerEngineParams (reg, EngineType::Wavetable);
    for (size_t i = 1; i < parts.size(); ++i)
    {
        parts[i]->registerCommonParams (reg);
        parts[i]->registerEngineParams (reg, EngineType::FM);
        parts[i]->registerEngineParams (reg, EngineType::Analog);
    }

    // ---- 0.2.0 (package D): per-part MIDI channel, scale / chord, MPE ------------------
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const auto& p = *parts[i];
        const auto n = std::to_string (i + 1);
        const auto group = "part" + n + "/midi";
        auto add = [&] (ParamDescriptor d) { d.group = group; reg.add (std::move (d)); };
        add (integer (p.id ("midich"), "Part " + n + " MIDI Channel", "Part " + n + " MIDI-Kanal", 0, 16, (int) i + 1));
        add (toggle (p.id ("quant"), "Part " + n + " Scale Quantise", "Part " + n + " Skala quantisieren", false));
        add (choice (p.id ("chord"), "Part " + n + " Chord", "Part " + n + " Akkord", scales::chordNamesEn(), scales::chordNamesDe(), scales::Off));
        add (toggle (p.id ("mpe"), "Part " + n + " MPE", "Part " + n + " MPE", false));
        add (integer (p.id ("mpe.bend"), "Part " + n + " MPE Bend Range", "Part " + n + " MPE-Bend-Bereich", 0, 96, 48, ParamUnit::Semitones));
        add (choice (p.id ("mpe.press"), "Part " + n + " Pressure Target", "Part " + n + " Druck-Ziel", MpeInput::targetNamesEn(),
                     MpeInput::targetNamesDe(), MpeInput::Amp));
        add (choice (p.id ("mpe.timbre"), "Part " + n + " Timbre Target", "Part " + n + " Timbre-Ziel", MpeInput::targetNamesEn(),
                     MpeInput::targetNamesDe(), MpeInput::Timbre));
    }

    // ---- 0.2.0 (package D): key (scale / chord mode) -----------------------------------
    {
        auto root = choice (Core::Ids::keyRoot, "Key", "Tonart", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
                            { "C", "Cis", "D", "Dis", "E", "F", "Fis", "G", "Gis", "A", "Ais", "H" }, 0);
        root.group = "global";
        reg.add (root);
        auto sc = choice (Core::Ids::keyScale, "Scale", "Skala", scales::namesEn(), scales::namesDe(), scales::Major);
        sc.group = "global";
        reg.add (sc);
    }

    // ---- 0.2.0 (package D): Euclid, Arp, Orbit sequencers ------------------------------
    {
        auto addSeq = [&] (const std::vector<ParamDescriptor>& list, const char* prefix, const char* group) {
            for (auto d : list)
            {
                d.id = std::string (prefix) + d.id;
                d.group = group;
                reg.add (std::move (d));
            }
        };
        addSeq (EuclidSequencer().getParamDescriptors(), Core::Ids::euclidPrefix, "sequencer/euclid");
        addSeq (Arpeggiator().getParamDescriptors(), Core::Ids::arpPrefix, "sequencer/arp");
        addSeq (OrbitSequencer().getParamDescriptors(), Core::Ids::orbitPrefix, "sequencer/orbit");
    }

    // ---- 0.2.0 (package D): tempo sync, MIDI out ---------------------------------------
    {
        auto add = [&] (ParamDescriptor d) { d.group = "global/sync"; reg.add (std::move (d)); };
        add (choice (Core::Ids::syncSource, "Sync Source", "Sync-Quelle", { "Internal", "MIDI Clock", "Link" },
                     { "Intern", "MIDI-Clock", "Link" }, Core::SyncInternal));
        add (continuous (Core::Ids::syncSmoothing, "Clock Smoothing", "Clock-Glättung", 0.0f, 1.0f, 0.5f));
        add (toggle (Core::Ids::clockOut, "MIDI Clock Out", "MIDI-Clock senden", false));
        add (toggle (Core::Ids::notesOut, "Sequencer Notes Out", "Sequencer-Noten senden", false));
    }

    // ---- 0.2.0: engines Wavetable, Pluck, Cluster, Grain, Vocal, Sampler for parts 2-4 -------
    for (size_t i = 1; i < parts.size(); ++i)
        for (auto type : { EngineType::Wavetable, EngineType::Pluck, EngineType::Cluster,
                           EngineType::Grain, EngineType::Vocal, EngineType::Sampler })
            parts[i]->registerEngineParams (reg, type);

    // ---- 0.3 (package E): remote controls --------------------------------------------------
    // Momentary triggers and selectors for UI functions, so every function can be MIDI-mapped:
    // a trigger fires when its value rises to "on" (CC >= 64). Not part of presets.
    {
        auto add = [&] (ParamDescriptor d) { d.group = "remote"; reg.add (std::move (d)); };
        add (toggle (Core::Ids::libOpen, "Library", "Bibliothek", false));
        add (toggle (Core::Ids::libPrev, "Previous Preset", "Vorheriges Preset", false));
        add (toggle (Core::Ids::libNext, "Next Preset", "Nächstes Preset", false));
        add (integer (Core::Ids::editPart, "Edit Part", "Bearbeiteter Part", 1, Core::numParts, 1));
        add (integer (Core::Ids::tapeEditTrack, "Tape Edit Track", "Tape-Bearbeitungsspur", 1, TapeMachine::numTracks, 1));
        add (toggle (Core::Ids::tapeLift, "Tape Lift", "Tape Heben", false));
        add (toggle (Core::Ids::tapeCut, "Tape Cut", "Tape Schneiden", false));
        add (toggle (Core::Ids::tapeDrop, "Tape Drop", "Tape Einsetzen", false));
        add (toggle (Core::Ids::tapeSplice, "Tape Splice", "Tape Einfügen", false));
        add (toggle (Core::Ids::tapeUndo, "Tape Undo", "Tape Rückgängig", false));
        add (toggle (Core::Ids::tapeRedo, "Tape Redo", "Tape Wiederholen", false));
        add (toggle (Core::Ids::tapeRtz, "Tape Return to Zero", "Tape zum Anfang", false));
    }

    // ---- 0.4 (package F): factory instruments (Sampler, Grain) per part, factory drum kits --
    for (auto& p : parts)
    {
        p->registerLateEngineParams (reg, EngineType::Sampler);
        p->registerLateEngineParams (reg, EngineType::Grain);
    }
    drum::DrumKit::registerLateParams (reg);

    // ---- 0.4 (package F): sequencers can play the drum kit instead of their target part ----
    {
        auto add = [&] (ParamDescriptor d, const char* group) { d.group = group; reg.add (std::move (d)); };
        add (toggle (Core::Ids::stepDrums, "Step Seq to Drums", "Step-Sequenzer an Drums", false), "sequencer/step");
        add (toggle (Core::Ids::euclidDrums, "Euclid to Drums", "Euklid an Drums", false), "sequencer/euclid");
        add (toggle (Core::Ids::orbitDrums, "Orbit to Drums", "Orbit an Drums", false), "sequencer/orbit");
    }

    // ---- 0.4 (package F): part gain in dB (level matching of presets, on top of the volume) --
    for (auto& p : parts) p->registerLateCommonParams (reg);

    // ---- 0.5: swing / humanise for Orbit and Euclid, poly glide per part ----------------------
    {
        addLateSeq (reg, OrbitSequencer().getLateParamDescriptors(), 0, 2, Core::Ids::orbitPrefix, "sequencer/orbit");
        addLateSeq (reg, EuclidSequencer().getLateParamDescriptors(), 0, 2, Core::Ids::euclidPrefix, "sequencer/euclid");
    }
    for (auto& p : parts) p->registerLate05Params (reg);
    // seq.euclid.rN.vel (0 = the Euclid Velocity), seq.orbit.lN.part (0 = the Orbit target), synced delay
    addLateSeq (reg, EuclidSequencer().getLateParamDescriptors(), 2, 6, Core::Ids::euclidPrefix, "sequencer/euclid");
    addLateSeq (reg, OrbitSequencer().getLateParamDescriptors(), 2, 6, Core::Ids::orbitPrefix, "sequencer/orbit");
    mixer.registerLate05Params (reg);

    // ---- 0.6: glide mode / timing and pitch envelope per part, Analog osc level + crackle, master low-pass --
    for (auto& p : parts) p->registerLate06Params (reg);
    for (auto& p : parts) p->registerLateEngineParams (reg, EngineType::Analog);
    mixer.registerLate06Params (reg);
}
} // namespace ohpe
