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
# Tests keep whole cores on the stack: give the Windows main thread 8 MB (default 1 MB) so a growing core never overflows it
if(MSVC)
    target_link_options(ohpe_core_tests PRIVATE /STACK:8388608)
endif()
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
# p37: the RANDOM key (constrained rolls, loudness normalisation)
target_sources(ohpe_core_tests PRIVATE RandomSoundTests.cpp)
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
# p45: chord naming, key spelling, Scale Lock
target_sources(ohpe_core_tests PRIVATE KeyChordTests.cpp)
# p27: the in-app updater (version order, manifest, settings, SHA-256, resumable download, content packs)
target_sources(ohpe_core_tests PRIVATE UpdateTests.cpp)
# content packs: shared loader base (pack.json, kind folders, limits, quota, content references)
target_sources(ohpe_core_tests PRIVATE ContentPackTests.cpp)
# p32: pressure glide (aftertouch scales the glide time)
target_sources(ohpe_core_tests PRIVATE GlidePressTests.cpp)
# p31: the SUB layer per part
target_sources(ohpe_core_tests PRIVATE SubTests.cpp)
target_sources(ohpe_core_tests PRIVATE ExpressionTests.cpp) # p33: expressive playing
# p32: the user's settings
target_sources(ohpe_core_tests PRIVATE UserSettingsTests.cpp)
# p35: CPU benchmark (percent of one core per scenario; informational, CI prints it, never gates on timing)
add_executable(ohpe_bench ${CMAKE_SOURCE_DIR}/tools/bench/OhpeBench.cpp)
target_link_libraries(ohpe_bench PRIVATE ohpe_core)
target_sources(ohpe_core_tests PRIVATE PerfTests.cpp) # p35: the performance pass
# p39: metronome, count-in, tap tempo
target_sources(ohpe_core_tests PRIVATE MetronomeTests.cpp)
# p46: Grid-Controller (Profil 1) - display SysEx, text folding, pad layout and colours, settings
target_sources(ohpe_core_tests PRIVATE GridProtocolTests.cpp)
target_sources(ohpe_core_tests PRIVATE ShowcasePresetTests.cpp) # p44: the showcase factory presets
# p50: Fingerboard mode / MPE HD (14/21-bit)
target_sources(ohpe_core_tests PRIVATE FingerboardTests.cpp)
# p49: Grid-Controller in a plug-in - ownership, automatic ports, settings
target_sources(ohpe_core_tests PRIVATE ControllerPortsTests.cpp)
target_sources(ohpe_core_tests PRIVATE BendRetriggerTests.cpp) # p53
target_sources(ohpe_core_tests PRIVATE SustainPedalTests.cpp) # p58: sustain / sostenuto from every input
target_sources(ohpe_core_tests PRIVATE GridLayoutTests.cpp) # p59: grid keyboard layouts
target_sources(ohpe_core_tests PRIVATE ArpSustainTests.cpp) # p60: sustain / sostenuto hold the arpeggiator
target_sources(ohpe_core_tests PRIVATE RobustnessTests.cpp) # p63: loaders survive damaged files
target_sources(ohpe_core_tests PRIVATE AudioStressTests.cpp) # p63: sample rates, buffer sizes, denormals, output guard, soak
target_sources(ohpe_core_tests PRIVATE FormatTests.cpp) # p62: format freeze (fixtures in tests/formats)
target_sources(ohpe_core_tests PRIVATE VoiceControlTests.cpp) # p68: Voice Control (live input -> modulation sources)
