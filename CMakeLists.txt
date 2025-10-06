cmake_minimum_required(VERSION 3.15)

project(StepSequencer VERSION 1.0.0)

# Add JUCE as a subdirectory (assuming you have JUCE in a folder called JUCE)
add_subdirectory(ext/juce)
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

# Create our plugin
juce_add_plugin(StepSequencer
    PLUGIN_MANUFACTURER_CODE Shih
    PLUGIN_CODE Seq8
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "8 Step Sequencer"
    COMPANY_NAME "YourCompany"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER "YourCompany"
)

# Add source files
juce_generate_juce_header(StepSequencer)

target_sources(StepSequencer
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp)

# Link required JUCE modules 
target_link_libraries(StepSequencer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Binary data if needed (for resources)
# juce_add_binary_data(StepSequencerData SOURCES icon.png)
# target_link_libraries(StepSequencer PRIVATE StepSequencerData)