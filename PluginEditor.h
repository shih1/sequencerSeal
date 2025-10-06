#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class StepSequencerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    StepSequencerAudioProcessorEditor(StepSequencerAudioProcessor &);
    ~StepSequencerAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void timerCallback() override;

private:
    StepSequencerAudioProcessor &audioProcessor;

    // Step sequencer knobs and LEDs
    static constexpr int NUM_STEPS = 8;
    std::array<juce::Slider, NUM_STEPS> stepSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, NUM_STEPS> stepAttachments;
    std::array<juce::Label, NUM_STEPS> stepLabels;

    // Config section
    juce::Slider rateSlider;
    juce::Label rateLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttachment;

    juce::Slider gateSlider;
    juce::Label gateLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;

    juce::ToggleButton glideToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> glideAttachment;

    juce::Slider glideTimeSlider;
    juce::Label glideTimeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideTimeAttachment;

    // Current step indicator (for LED)
    int lastDisplayedStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerAudioProcessorEditor)
};