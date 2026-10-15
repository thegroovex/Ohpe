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
    midi/MidiMap.cpp
    engines/WavetableEngine.cpp
    engines/PluckEngine.cpp
    engines/ClusterEngine.cpp)

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
    fx/InsertEffects.cpp
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
