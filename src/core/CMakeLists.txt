# ohpe_core: all DSP and state, pure C++17, no JUCE. Fast to build and test.
add_library(ohpe_core STATIC
    OhpeCore.cpp
    ParameterLayout.cpp
    model/ParamRegistry.cpp
    model/StateSerializer.cpp
    engines/AnalogEngine.cpp
    engines/FmEngine.cpp
    engines/EngineFactory.cpp
    engines/Part.cpp
    mixer/Mixer.cpp
    fx/Delay.cpp
    fx/Reverb.cpp
    seq/StepSequencer.cpp
    seq/Metronome.cpp
    midi/MidiMap.cpp
    engines/WavetableEngine.cpp
    engines/PluckEngine.cpp
    engines/ClusterEngine.cpp
    breed/SoundGenome.cpp
    breed/RandomSound.cpp)

# phase 2, engines B: Grain, Vocal, Sampler, drum kit, sample model
target_sources(ohpe_core PRIVATE
    engines/GrainEngine.cpp
    engines/VocalEngine.cpp
    engines/SamplerEngine.cpp
    drum/DrumVoice.cpp
    drum/DrumKit.cpp
    sample/SampleData.cpp
    sample/Slicer.cpp
    sample/SampleState.cpp)

# 0.2.0 package C: tape, insert effects
target_sources(ohpe_core PRIVATE
    tape/TapeMachine.cpp
    tape/TapeFile.cpp
    tape/TapeLoop.cpp
    fx/InsertEffects.cpp
    fx/InsertEffects2.cpp
    fx/Space3D.cpp
    fx/Resonator.cpp
    fx/InsertSlot.cpp)

target_include_directories(ohpe_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ohpe_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(ohpe_core PUBLIC cxx_std_17)
set_target_properties(ohpe_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(ohpe_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ohpe_core PRIVATE -Wall -Wextra -Wshadow -Wno-unused-parameter)
endif()

# 0.2 (package D): sequencers, scale / chord mode, MIDI clock, MPE
target_sources(ohpe_core PRIVATE
    seq/EuclidSequencer.cpp
    seq/Arpeggiator.cpp
    seq/OrbitSequencer.cpp
    midi/MidiClock.cpp)

# 0.3 (package E): preset format and library (filter / sort / user data)
target_sources(ohpe_core PRIVATE
    preset/Preset.cpp
    preset/PresetLibrary.cpp)

# p16: the morph space (XY pad between four complete sounds per part)
target_sources(ohpe_core PRIVATE morph/MorphSpace.cpp)

# p22: the motion recorder (knob movements per part as looping lanes)
target_sources(ohpe_core PRIVATE motion/MotionRecorder.cpp)

# p25: the performance pads (stutter, tape stop, filter sweep, reverb freeze on the main mix)
target_sources(ohpe_core PRIVATE perform/PerformancePads.cpp)
# p26: the living display's object (the sound as a deformable 3D shape; the drawing is src/ui/LivingView)
target_sources(ohpe_core PRIVATE visual/LivingShape.cpp)
# p24: the harmony brain (scale and chord memory of all parts)
target_sources(ohpe_core PRIVATE harmony/HarmonyBrain.cpp harmony/KeyScale.cpp)
# p27: the in-app updater (versions, manifest, settings, SHA-256, resumable download, content packs)
target_sources(ohpe_core PRIVATE
    update/Version.cpp
    update/Sha256.cpp
    update/UpdateManifest.cpp
    update/UpdateSettings.cpp
    update/Download.cpp
    update/ContentPacks.cpp
    update/PackFormat.cpp)
# p32: the user's settings (language, help, skin, display, animations - per user, not per project)
target_sources(ohpe_core PRIVATE settings/UserSettings.cpp)

# 0.4 (package F): factory sample bank (instruments, drum kits)
target_sources(ohpe_core PRIVATE sample/FactoryBank.cpp)

# 0.7: FX rack (serial insert slots per part + master) and modulation matrix
target_sources(ohpe_core PRIVATE
    fx/FxRack.cpp
    fx/RackEffects.cpp
    mod/ModMatrix.cpp
    mod/Character.cpp
    discover/Discover.cpp)

# 0.7: Mid/Side stereo effect (FX rack type)
target_sources(ohpe_core PRIVATE fx/MidSide.cpp)

# p20: audio to wavetable (user wavetables, analysis, table files, state)
target_sources(ohpe_core PRIVATE
    wavetable/UserTable.cpp
    wavetable/AudioAnalysis.cpp
    wavetable/TableFile.cpp
    wavetable/UserTableState.cpp)

# p23: bounce to engine (tape / live input -> Grain or Sampler)
target_sources(ohpe_core PRIVATE bounce/Bounce.cpp)
# p67: tape memory (rolling output buffer -> Sampler instrument)
target_sources(ohpe_core PRIVATE tapemem/TapeMemory.cpp)

# p46: Grid-Controller (Profil 1) - wire protocol, display text, pad layout
target_sources(ohpe_core PRIVATE control/GridProtocol.cpp control/ControllerPorts.cpp) # p46, p49
