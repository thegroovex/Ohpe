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

set(OHPE_DEFINES OHPE_VERSION="${PROJECT_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
set(OHPE_JUCE_MODULES juce::juce_audio_utils juce::juce_audio_devices juce::juce_gui_extra ohpe_resources)

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
