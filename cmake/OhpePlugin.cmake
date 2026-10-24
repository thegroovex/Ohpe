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
        ${CMAKE_SOURCE_DIR}/resources/fonts/SpaceGrotesk-700.ttf
        ${CMAKE_SOURCE_DIR}/resources/icon/png/ohpe-icon-128.png) # p43: About header, standalone window icon
set_target_properties(ohpe_resources PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(OHPE_PLUGIN_SOURCES
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/ParameterBridge.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/Localisation.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Palette.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Pictograms.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Pages.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Accessibility.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Display.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TempoControl.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/KeyControl.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OhpeLookAndFeel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/standalone/AudioSettingsPage.cpp)
# phase 2, engines B: sample files from disk
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/SampleLoader.cpp)
# p20: audio to wavetable (user wavetables, analysis worker, library)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/UserTables.cpp)
# p23: bounce to engine (tape / live input -> Grain or Sampler, bounce library)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/BounceToEngine.cpp)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/TapeMemoryManager.cpp) # p67

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
# p69: learning breed (local taste profile)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/TasteStore.cpp)
# 0.7: display views of the modulation matrix and the FX racks
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/ModFxViews.cpp)
# p55: the graphical FX slot editor (Effects mode, Rack / Master)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/FxEditor.cpp)
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
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/ResoView.cpp) # p19
# p16: the morph pad (LFO mode, view Morph)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/MorphView.cpp)
# p22: the motion recorder's lanes (LFO mode, view Motion)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/MotionView.cpp)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/VoiceView.cpp) # p68
# p25: the performance pads (Effects mode, view Pads)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/PadsView.cpp)
# p26: the living display (the sound as a 3D object in the display's visual area)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/LivingView.cpp)
# p24: the harmony brain (LFO mode, view Harmony) and the isomorphic grid keyboard
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/HarmonyView.cpp)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/GridKeyboard.cpp)
# p46: Grid-Controller (Profil 1) - external pad grid with display (standalone app)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/GridController.cpp)
# p27: the in-app updater (release check, verified download, content packs) and its view
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/Updater.cpp ${CMAKE_SOURCE_DIR}/src/plugin/ContentLibrary.cpp ${CMAKE_SOURCE_DIR}/src/ui/UpdateView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PackManagerView.cpp)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/UserTableView.cpp) # p20
# p32: the settings menu (user settings store, settings view, help overlay)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/plugin/UserSettingsStore.cpp ${CMAKE_SOURCE_DIR}/src/ui/SettingsView.cpp)
# p18: preset breeding (the breeding view; the engine-agnostic core is src/core/breed)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/BreedView.cpp)
# p38: the confirmation over the plate (clear the whole tape, load a tape over material)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/ConfirmDialog.cpp)
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/RecoveryNotice.cpp) # p63: crash-safe start
list(APPEND OHPE_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/ui/FirstStartTour.cpp) # p64: first-start tour
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
    # p68: the standalone app's audio input (Voice Control, tape, bounce); localised in resources/macos/*.lproj
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Ohpe uses the audio input you choose so your voice can control the sound (Voice Control) and so you can record it to the tape. The input is analysed on your computer and never sent anywhere."
    PLIST_TO_MERGE "<plist><dict><key>CFBundleLocalizations</key><array><string>en</string><string>de</string></array></dict></plist>"
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${OHPE_COPY_PLUGIN}
    ICON_BIG ${CMAKE_SOURCE_DIR}/resources/icon/png/ohpe-icon-1024.png
    ICON_SMALL ${CMAKE_SOURCE_DIR}/resources/icon/png/ohpe-icon-32.png
    DESCRIPTION "Ohpe - synthesizer, sampler and tape workstation (designed by groovex)")

# p43: the app icon. JUCE derives its .icns / .ico from the two PNGs above (only a few sizes, rescaled);
# replace them with the hand-tuned files from resources/icon (every size, small sizes drawn for the
# pixel grid - see tools/app-icon/make_icons.py). JUCE writes them at configure time, before this line,
# and every bundle (app, VST3, AU) and the Windows resource script use exactly these paths.
get_target_property(OHPE_JUCE_GENERATED Ohpe JUCE_GENERATED_SOURCES_DIRECTORY)
if(APPLE)
    if(NOT EXISTS "${OHPE_JUCE_GENERATED}/AppIcon.icns")
        message(FATAL_ERROR "JUCE did not generate AppIcon.icns in ${OHPE_JUCE_GENERATED}")
    endif()
    configure_file(${CMAKE_SOURCE_DIR}/resources/icon/Ohpe.icns "${OHPE_JUCE_GENERATED}/AppIcon.icns" COPYONLY)
elseif(WIN32)
    if(NOT EXISTS "${OHPE_JUCE_GENERATED}/icon.ico")
        message(FATAL_ERROR "JUCE did not generate icon.ico in ${OHPE_JUCE_GENERATED}")
    endif()
    configure_file(${CMAKE_SOURCE_DIR}/resources/icon/Ohpe.ico "${OHPE_JUCE_GENERATED}/icon.ico" COPYONLY)
endif()

target_sources(Ohpe PRIVATE ${OHPE_PLUGIN_SOURCES} ${CMAKE_SOURCE_DIR}/src/standalone/StandaloneApp.cpp)
# p68: localised microphone permission text (EN / DE) inside the app bundle (copied before JUCE signs it)
if(APPLE AND TARGET Ohpe_Standalone)
    foreach(OHPE_LPROJ en de)
        set(OHPE_STRINGS ${CMAKE_SOURCE_DIR}/resources/macos/${OHPE_LPROJ}.lproj/InfoPlist.strings)
        set_source_files_properties(${OHPE_STRINGS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/${OHPE_LPROJ}.lproj HEADER_FILE_ONLY TRUE)
        target_sources(Ohpe_Standalone PRIVATE ${OHPE_STRINGS})
    endforeach()
endif()
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
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/FirstPresetTests.cpp) # p40
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/PartPresetNameTests.cpp) # p50
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/FxEditorTests.cpp) # p55
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TourUiTests.cpp) # p64
    target_include_directories(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    if(MSVC) # tests keep cores / processors on the stack: 8 MB main thread on Windows (default 1 MB)
        target_link_options(ohpe_plugin_tests PRIVATE /STACK:8388608)
    endif()
    target_compile_definitions(ohpe_plugin_tests PRIVATE ${OHPE_DEFINES} OHPE_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ohpe_plugin_tests PRIVATE ohpe_core doctest::doctest ${OHPE_JUCE_MODULES} juce::juce_recommended_config_flags)
    add_test(NAME plugin_tests COMMAND ohpe_plugin_tests)
    set_tests_properties(plugin_tests PROPERTIES ENVIRONMENT "OHPE_LANGUAGE=en")

    # loads the built VST3 like a DAW does
    juce_add_console_app(ohpe_hosttest PRODUCT_NAME "ohpe_hosttest")
    target_sources(ohpe_hosttest PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/HostTest.cpp)
    target_compile_definitions(ohpe_hosttest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                               JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ohpe_hosttest PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
    add_dependencies(ohpe_hosttest Ohpe_VST3)
    if(MSVC)
        target_link_options(ohpe_hosttest PRIVATE /STACK:8388608)
    endif()
    # p80 host matrix: one CTest per section (they run in parallel); the Audio Unit runs in the macOS auval job
    # (it has to be installed first). docs/en/HOST-TESTING.md
    add_test(NAME vst3_host_test COMMAND ohpe_hosttest "${CMAKE_BINARY_DIR}/Ohpe_artefacts/$<CONFIG>/VST3/Ohpe.vst3"
             --section basic --source "${CMAKE_SOURCE_DIR}")
    foreach(section lifecycle state automation bounce rates bypass latency multi editor transport)
        add_test(NAME vst3_host_${section} COMMAND ohpe_hosttest "${CMAKE_BINARY_DIR}/Ohpe_artefacts/$<CONFIG>/VST3/Ohpe.vst3"
                 --section ${section} --source "${CMAKE_SOURCE_DIR}")
        set_tests_properties(vst3_host_${section} PROPERTIES ENVIRONMENT "OHPE_LANGUAGE=en")
    endforeach()

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
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/SpaceSwitchUiTests.cpp) # p41: the 3D switch
endif()

# ---------------------------------------------------------------- p11: tape transport UI, overlays
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TapeTransportUiTests.cpp)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TapeEraseUiTests.cpp) # p38
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/RobustnessUiTests.cpp) # p63
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TapeLoopUiTests.cpp) # p42
endif()

# ---------------------------------------------------------------- p14: redesigned plate, wheels, glide
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/UiLayoutTests.cpp)
endif()

# ---------------------------------------------------------------- p16: the morph pad (LFO mode, view Morph)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/MorphUiTests.cpp)
endif()

# ---------------------------------------------------------------- p22: the motion recorder (LFO mode, view Motion)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/MotionUiTests.cpp)
endif()

# ---------------------------------------------------------------- p25: the performance pads (Effects mode, view Pads)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/PadsUiTests.cpp)
endif()

# ---------------------------------------------------------------- p26: the living display
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/LivingUiTests.cpp)
endif()

# ---------------------------------------------------------------- p24: the harmony brain (LFO mode, view Harmony), grid keyboard
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/HarmonyUiTests.cpp)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/GridControllerUiTests.cpp) # p46
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/GridPluginUiTests.cpp) # p49
endif()

# ---------------------------------------------------------------- p20: audio to wavetable (import, library, state)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/UserTableUiTests.cpp)
endif()

# ---------------------------------------------------------------- p18: preset breeding
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/BreedUiTests.cpp)
endif()

# ---------------------------------------------------------------- p33: expressive playing (Envelope mode: the Expression pages)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/ExpressionUiTests.cpp)
endif()

# ---------------------------------------------------------------- p21: pro view 2.0 (pictures, Discover, Simple / All)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/ProViewTwoTests.cpp)
endif()

# ---------------------------------------------------------------- p23: bounce to engine
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/BounceUiTests.cpp)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TapeMemoryUiTests.cpp) # p67
endif()

# ---------------------------------------------------------------- p27: the in-app updater
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/UpdaterTests.cpp)
endif()

# ---------------------------------------------------------------- content packs: shared loader (local install, kind folders, references)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/ContentLoaderTests.cpp)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/PackUiTests.cpp)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/WavetableBankTests.cpp)
endif()

# ---------------------------------------------------------------- p62: format freeze - the golden fixtures through the plug-in
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/FormatPluginTests.cpp)
    # only this file needs the fixture folder (a target-wide define would rebuild every plug-in test source)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/plugin/FormatPluginTests.cpp PROPERTIES
        COMPILE_DEFINITIONS OHPE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------- p31: the SUB layer per part (Synth page, pro view group)
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/SubUiTests.cpp)
endif()

# ---------------------------------------------------------------- p32: the settings menu
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/SettingsUiTests.cpp)
endif()

# ---------------------------------------------------------------- p39: tempo editable, metronome, count-in
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/TempoUiTests.cpp)
endif()

# ---------------------------------------------------------------- p45: chords, key on the header, Scale Lock
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/KeyChordUiTests.cpp)
endif()

# ---------------------------------------------------------------- p70: Key Gravity ring on the display's header
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/GravityHeaderUiTests.cpp)
endif()

# ---------------------------------------------------------------- p75: the display's header never cuts a title
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/HeaderFitUiTests.cpp)
endif()

# ---------------------------------------------------------------- p83: accessibility, keyboard operation, UI sizes
if(OHPE_BUILD_TESTS)
    target_sources(ohpe_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/plugin/AccessibilityUiTests.cpp)
endif()
