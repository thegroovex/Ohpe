# JUCE plug-in (VST3 / AU / Standalone), UI, plug-in tests and the screenshot tool.

if(OHPE_JUCE_PATH)
    add_subdirectory(${OHPE_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(ohpe_resources
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        ${CMAKE_SOURCE_DIR}/resources/fonts/SpaceGrotesk-500.ttf
        ${CMAKE_SOURCE_DIR}/resources/fonts/SpaceGrotesk-700.ttf)
set_target_properties(ohpe_resources PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(OHPE_PLUGIN_SOURCES
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/ParameterBridge.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/Localisation.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Palette.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Pictograms.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Pages.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Display.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OhpeLookAndFeel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/standalone/AudioSettingsPage.cpp)
# phase 2, engines B: sample files from disk
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/SampleLoader.cpp)
# p20: audio to wavetable (user wavetables, analysis worker, library)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/UserTables.cpp)

set(OHPE_DEFINES OHPE_VERSION="${PROJECT_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
set(OHPE_JUCE_MODULES juce::juce_audio_utils juce::juce_audio_devices juce::juce_gui_extra ohpe_resources)

# phase 2, engines B: factory drum kit (CC0 recordings, see THIRD-PARTY.md), embedded as FLAC
file(GLOB OHPE_DRUMKIT_FILES ${CMAKE_SOURCE_DIR}/resources/drumkit/*.flac)
juce_add_binary_data(ohpe_drumkit
    HEADER_NAME OhpeDrumKit.h
    NAMESPACE OhpeDrumKit
    SOURCES ${OHPE_DRUMKIT_FILES})
set_target_properties(ohpe_drumkit PROPERTIES POSITION_INDEPENDENT_CODE ON)
list(APPEND OHPE_JUCE_MODULES ohpe_drumkit)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/FactoryKit.cpp)

# 0.4 (package F): factory sample bank - Sampler / Grain instruments and the drum kits Tight, Room,
# Lo-Fi, Hybrid (CC0 recordings, see THIRD-PARTY.md; built by tools/samples/build_samples.py)
file(GLOB_RECURSE OHPE_SAMPLE_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/resources/samples/*.flac)
juce_add_binary_data(ohpe_samples
    HEADER_NAME OhpeSamples.h
    NAMESPACE OhpeSamples
    SOURCES ${CMAKE_SOURCE_DIR}/resources/samples/bank.json ${OHPE_SAMPLE_FILES})
set_target_properties(ohpe_samples PROPERTIES POSITION_INDEPENDENT_CODE ON)
list(APPEND OHPE_JUCE_MODULES ohpe_samples)

# 0.3 (package E): pro view, preset library browser, preset manager and the factory preset bank.
# Package F (factory sounds) only adds *.ohpepreset files to resources/presets/factory; they are
# embedded as binary data (namespace OhpePresets) and appear in the library's factory bank.
list(APPEND OHPE_PLUGIN_SOURCES
    ${CMAKE_SOURCE_DIR}/src/ui/ProView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LibraryBrowser.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PresetManager.cpp)
# 0.7: display views of the modulation matrix and the FX racks
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/ModFxViews.cpp)
file(GLOB OHPE_FACTORY_PRESETS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/resources/presets/factory/*.ohpepreset)
juce_add_binary_data(ohpe_presets
    HEADER_NAME OhpePresets.h
    NAMESPACE OhpePresets
    SOURCES ${OHPE_FACTORY_PRESETS})
set_target_properties(ohpe_presets PROPERTIES POSITION_INDEPENDENT_CODE ON)
list(APPEND OHPE_JUCE_MODULES ohpe_presets)

# 0.7: display visuals (stereo scope, spectrum terrain)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/Visuals.cpp)
# 1.0: the 3D space view, the headphones pictogram / 3D badge
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/SpaceView.cpp)
# p16: the morph pad (LFO mode, view Morph)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/MorphView.cpp)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/UserTableView.cpp) # p20
# p21: pro view 2.0 - graphic tiles, the Discover strip
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/ProTiles.cpp ${CMAKE_SOURCE_DIR}/src/ui/DiscoverStrip.cpp)

set(OHPE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OHPE_FORMATS AU)
endif()

juce_add_plugin(Ohpe
    PRODUCT_NAME "Ohpe"
    COMPANY_NAME "groovex"
    BUNDLE_ID "com.groovex.ohpe"
    PLUGIN_MANUFACTURER_CODE Grvx
    PLUGIN_CODE Ohpe
    FORMATS ${OHPE_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${OHPE_COPY_PLUGIN}
    DESCRIPTION "Ohpe - synthesizer, sampler and tape workstation (designed by groovex)")

target_sources(Ohpe PRIVATE ${OHPE_PLUGIN_SOURCES} ${CMAKE_SOURCE_DIR}/src/standalone/StandaloneApp.cpp)
target_compile_definitions(Ohpe PUBLIC ${OHPE_DEFINES} JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_include_directories(Ohpe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Ohpe
    PRIVATE
        ohpe_core
        ${OHPE_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
if(OHPE_LTO)
    target_link_libraries(Ohpe PUBLIC juce::juce_recommended_lto_flags)
endif()

if(OHPE_BUILD_TESTS)
    # processor / editor tests (JUCE, no host)
    juce_add_console_app(ohpe_plugin_tests PRODUCT_NAME "ohpe_plugin_tests")
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/PluginTests.cpp ${OHPE_PLUGIN_SOURCES})
    target_include_directories(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(ohpe_plugin_tests PRIVATE ${OHPE_DEFINES} OHPE_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ohpe_plugin_tests PRIVATE ohpe_core doctest::doctest ${OHPE_JUCE_MODULES} juce::juce_recommended_config_flags)
    add_test(NAME plugin_tests COMMAND ohpe_plugin_tests)
    set_tests_properties(plugin_tests PROPERTIES ENVIRONMENT "OHPE_LANGUAGE=en")

    # loads the built VST3 like a DAW does
    juce_add_console_app(ohpe_hosttest PRODUCT_NAME "ohpe_hosttest")
    target_sources(ohpe_hosttest PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/HostTest.cpp)
    target_compile_definitions(ohpe_hosttest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(ohpe_hosttest PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
    add_dependencies(ohpe_hosttest Ohpe_VST3)
    add_test(NAME vst3_host_test COMMAND ohpe_hosttest "${CMAKE_BINARY_DIR}/Ohpe_artefacts/$<CONFIG>/VST3/Ohpe.vst3")

    # renders the editor into PNGs for the documentation
    juce_add_console_app(ohpe_screenshot PRODUCT_NAME "ohpe_screenshot")
    target_sources(ohpe_screenshot PRIVATE ${CMAKE_SOURCE_DIR}/tools/Screenshot.cpp ${OHPE_PLUGIN_SOURCES})
    target_include_directories(ohpe_screenshot PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(ohpe_screenshot PRIVATE ${OHPE_DEFINES} OHPE_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ohpe_screenshot PRIVATE ohpe_core ${OHPE_JUCE_MODULES} juce::juce_recommended_config_flags)
endif()

# ---------------------------------------------------------------- 0.2.0 package C: tape I/O (state, bounce)
target_sources(Ohpe PRIVATE ${CMAKE_SOURCE_DIR}/src/plugin/TapeIO.cpp)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/plugin/TapeIO.cpp ${CMAKE_SOURCE_DIR}/tests/plugin/TapeExportTests.cpp)
    target_sources(ohpe_screenshot PRIVATE ${CMAKE_SOURCE_DIR}/src/plugin/TapeIO.cpp)
endif()

# ---------------------------------------------------------------- 0.3 package E: UI, preset library
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/UiLibraryTests.cpp)
endif()

# ---------------------------------------------------------------- 0.4 package F: factory sounds
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/FactorySoundTests.cpp)
    target_include_directories(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tools)
endif()

# ---------------------------------------------------------------- 0.7: stereo FX / display visuals
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/StereoDisplayTests.cpp)
endif()

# ---------------------------------------------------------------- p11: tape transport UI, overlays
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TapeTransportUiTests.cpp)
endif()

# ---------------------------------------------------------------- p14: redesigned plate, wheels, glide
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/UiLayoutTests.cpp)
endif()

# ---------------------------------------------------------------- p16: the morph pad (LFO mode, view Morph)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/MorphUiTests.cpp)
endif()

# ---------------------------------------------------------------- p20: audio to wavetable (import, library, state)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/UserTableUiTests.cpp)
endif()

# ---------------------------------------------------------------- p21: pro view 2.0 (pictures, Discover, Simple / All)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/ProViewTwoTests.cpp)
endif()
