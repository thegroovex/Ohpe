# Core unit tests: no JUCE, build + run in seconds.
#   cmake -S . -B build-core -G Ninja -DOHPE_BUILD_PLUGIN=OFF && cmake --build build-core && ctest --test-dir build-core
add_executable(ohpe_core_tests
    TestMain.cpp
    TestHelpers.h
    ParamRegistryTests.cpp
    EnvelopeTests.cpp
    AnalogEngineTests.cpp
    FmEngineTests.cpp
    VoiceTests.cpp
    MixerTests.cpp
    SequencerTests.cpp
    MidiMapTests.cpp
    StateTests.cpp
    CoreTests.cpp
    WavetableEngineTests.cpp
    PluckEngineTests.cpp
    ClusterEngineTests.cpp)
target_link_libraries(ohpe_core_tests PRIVATE ohpe_core doctest::doctest)
target_compile_definitions(ohpe_core_tests PRIVATE OHPE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
# phase 2, engines B
target_sources(ohpe_core_tests PRIVATE
    SampleTestHelpers.h
    GrainEngineTests.cpp
    VocalEngineTests.cpp
    SamplerEngineTests.cpp
    DrumKitTests.cpp)
add_test(NAME core_tests COMMAND ohpe_core_tests)

# parameter table dump (golden file / documentation)
add_executable(ohpe_param_dump ${CMAKE_SOURCE_DIR}/tools/ParamDump.cpp)
target_link_libraries(ohpe_param_dump PRIVATE ohpe_core)

# 0.2.0 package C: tape machine, effects / mixer dynamics, real-time allocation check
target_sources(ohpe_core_tests PRIVATE TapeTests.cpp FxTests.cpp AllocationTests.cpp)
# 0.2 (package D): sequencers, scale / chord, timing, MIDI clock, MPE, parts
target_sources(ohpe_core_tests PRIVATE SeqMidiTests.cpp)
# 0.3 (package E): presets and the library
target_sources(ohpe_core_tests PRIVATE PresetTests.cpp)
# 0.4 (package F): offline preset renderer (factory-sound quality loop, see tools/render)
add_executable(ohpe_render ${CMAKE_SOURCE_DIR}/tools/render/OhpeRender.cpp)
target_include_directories(ohpe_render PRIVATE ${CMAKE_SOURCE_DIR}/tools)
target_link_libraries(ohpe_render PRIVATE ohpe_core)
# 0.4 (package F): factory sample bank, sequencers -> drums, part gain, factory presets render
target_sources(ohpe_core_tests PRIVATE FactorySoundTests.cpp)
# 0.5 (sound revision 1): Orbit / Euclid swing and humanise, poly glide, the Boom kit
target_sources(ohpe_core_tests PRIVATE Revision05Tests.cpp)
target_include_directories(ohpe_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/tools)
target_compile_definitions(ohpe_core_tests PRIVATE OHPE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
