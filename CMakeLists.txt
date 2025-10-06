cmake_minimum_required(VERSION 3.15)

project(PitchVelocityRemapper VERSION 1.0.0)

# Add JUCE as a subdirectory (assuming you have JUCE in a folder called JUCE)
add_subdirectory(ext/juce)
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

# Create our plugin
juce_add_plugin(PitchVelocityRemapper
    PLUGIN_MANUFACTURER_CODE Shih
    PLUGIN_CODE bttr
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "BetterVelocity"
    COMPANY_NAME "YourCompany"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER "YourCompany"
)

# Add source files
juce_generate_juce_header(PitchVelocityRemapper)

target_sources(PitchVelocityRemapper
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp)

# Link required JUCE modules 
target_link_libraries(PitchVelocityRemapper
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Binary data if needed (for resources)
# juce_add_binary_data(PitchVelocityRemapperData SOURCES icon.png)
# target_link_libraries(PitchVelocityRemapper PRIVATE PitchVelocityRemapperData)