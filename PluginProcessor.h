// PluginProcessor.h
#pragma once

#include <JuceHeader.h>

class PitchVelocityProcessor : public juce::AudioProcessor
{
public:
    PitchVelocityProcessor();
    ~PitchVelocityProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    // Parameter access
    juce::AudioProcessorValueTreeState &getParameters() { return parameters; }

    // Parameter IDs
    static const juce::String MIN_VELOCITY_ID;
    static const juce::String MAX_VELOCITY_ID;
    static const juce::String CURVE_ID;
    static const juce::String BYPASS_ID;

    // helper to editor
    void processMidiForDisplay(juce::MidiKeyboardState &keyboardState, int numSamples);

    juce::MidiKeyboardState keyboardState; // Add this public member
    std::array<std::atomic<int>, 128> lastNoteVelocities;

private:
    juce::MidiBuffer currentMidiMessages;
    juce::CriticalSection midiMonitorLock;
    juce::AudioProcessorValueTreeState parameters;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchVelocityProcessor)
};
