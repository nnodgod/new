cmake_minimum_required(VERSION 3.22)
project(VowelSynth VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

option(VM_COPY "Copy plugin to system folder after build" ON)

juce_add_plugin(VowelSynth
    COMPANY_NAME "Me"
    PLUGIN_MANUFACTURER_CODE Memm
    PLUGIN_CODE Vsyn
    FORMATS VST3 Standalone
    PRODUCT_NAME "VowelSynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD ${VM_COPY})

target_sources(VowelSynth PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(VowelSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VowelSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
