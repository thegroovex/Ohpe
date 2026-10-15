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
    midi/MidiMap.cpp)

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
