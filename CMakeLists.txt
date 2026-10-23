cmake_minimum_required(VERSION 3.22)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(HexQPrimes VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HexQPrimes
    COMPANY_NAME "HEX-Q"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Hexq
    PLUGIN_CODE Prms
    FORMATS VST3
    PRODUCT_NAME "HEX-Q PRIMES"
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion)

target_sources(HexQPrimes PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Diag.h
    Source/Parameters/PrimesParams.cpp
    Source/Parameters/PrimesParams.h
    Source/DSP/PrimesEngine.h
    Source/HexQ/HexQTheme.h
    Source/HexQ/HexQLookAndFeel.cpp
    Source/HexQ/HexQLookAndFeel.h
    Source/HexQ/HexQHeader.h
    Source/HexQ/HexQFooter.h
    Source/HexQ/HexQModeButton.h)

target_compile_definitions(HexQPrimes PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HexQPrimes
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Offline DSP self-test (pure C++, no JUCE): run with `ctest -C Release`
enable_testing()
add_executable(HexQPrimesDspTest tests/primes_dsp_test.cpp)
target_include_directories(HexQPrimesDspTest PRIVATE Source)
add_test(NAME primes_dsp_self_test COMMAND HexQPrimesDspTest)
