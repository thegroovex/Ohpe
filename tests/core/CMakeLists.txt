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
# 0.6 (sound revision 2): portamento, pitch envelope, crackle, master low-pass, Boom category
target_sources(ohpe_core_tests PRIVATE Revision06Tests.cpp)
# 0.8 (sound revision 3): new insert effects, FX slots 2 / 3, layer engine, unison stack, keyboard instruments
target_sources(ohpe_core_tests PRIVATE Revision08Tests.cpp)
# p15: life per part (drift, chaos LFOs, grain scatter, filter breathing)
target_sources(ohpe_core_tests PRIVATE LifeTests.cpp)
# p21: pro view 2.0 - character macros, Discover genome
target_sources(ohpe_core_tests PRIVATE DiscoverTests.cpp)
target_include_directories(ohpe_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/tools)
target_compile_definitions(ohpe_core_tests PRIVATE OHPE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# 0.7: FX rack and modulation matrix
target_sources(ohpe_core_tests PRIVATE ModFxTests.cpp)
target_sources(ohpe_core_tests PRIVATE RackFxTests.cpp)
# 0.7: Mid/Side stereo effect, scope FIFO, stereo / spectrum analysis
target_sources(ohpe_core_tests PRIVATE StereoTests.cpp)
find_package(Threads REQUIRED)
target_link_libraries(ohpe_core_tests PRIVATE Threads::Threads)
# 1.0: the 3D spatialiser (Space 3D)
target_sources(ohpe_core_tests PRIVATE SpaceTests.cpp)
# p19: the resonator body
target_sources(ohpe_core_tests PRIVATE ResonatorTests.cpp)
# p16: the morph space
target_sources(ohpe_core_tests PRIVATE MorphTests.cpp)
# p20: audio to wavetable
target_sources(ohpe_core_tests PRIVATE UserTableTests.cpp)
# p18: preset breeding
target_sources(ohpe_core_tests PRIVATE BreedTests.cpp)
# p23: bounce to engine
target_sources(ohpe_core_tests PRIVATE BounceTests.cpp)
# p22: the motion recorder
target_sources(ohpe_core_tests PRIVATE MotionTests.cpp)

# p25: the performance pads (stutter, tape stop, filter sweep, reverb freeze)
target_sources(ohpe_core_tests PRIVATE PadsTests.cpp)
# p26: the living display's object
target_sources(ohpe_core_tests PRIVATE LivingShapeTests.cpp)
# p24: the harmony brain
target_sources(ohpe_core_tests PRIVATE HarmonyTests.cpp)
# p27: the in-app updater (version order, manifest, settings, SHA-256, resumable download, content packs)
target_sources(ohpe_core_tests PRIVATE UpdateTests.cpp)
# p32: pressure glide (aftertouch scales the glide time)
target_sources(ohpe_core_tests PRIVATE GlidePressTests.cpp)
# p32: the user's settings
target_sources(ohpe_core_tests PRIVATE UserSettingsTests.cpp)
