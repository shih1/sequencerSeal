#pragma once

#include <JuceHeader.h>

class StepSequencerAudioProcessor : public juce::AudioProcessor
{
public:
    StepSequencerAudioProcessor();
    ~StepSequencerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String &newName) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState &getValueTreeState() { return apvts; }

    // Get current step for UI
    int getCurrentStep() const { return currentStep; }
    bool getIsPlaying() const { return isNoteOn; }

private:
    static constexpr int NUM_STEPS = 8;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Synth state
    bool isNoteOn = false;
    int baseNote = 60;
    float phase = 0.0f;
    float currentFrequency = 440.0f;
    float targetFrequency = 440.0f;
    float glideRate = 0.0f;

    // Sequencer state
    int currentStep = 0;
    double samplesUntilNextStep = 0.0;
    double stepLengthInSamples = 0.0;
    double gateOffSamples = 0.0;
    bool gateIsOn = false;

    // Tempo sync
    juce::AudioPlayHead::PositionInfo lastPosInfo;

    void advanceStep();
    void resetSequencer();
    void updateFrequency();
    double calculateStepLength(double sampleRate, float rateParam);

    std::atomic<float> currentBpm{120.0f}; // Default to 120 BPM

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerAudioProcessor)
};