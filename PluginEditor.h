// PluginEditor.h
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PitchVelocityEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    PitchVelocityEditor(PitchVelocityProcessor &);
    ~PitchVelocityEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawKeyboard(juce::Graphics &g, juce::Rectangle<float> area);
    void drawVelocityBars(juce::Graphics &g, juce::Rectangle<float> area);
    void drawCurveOverlay(juce::Graphics &g, juce::Rectangle<float> area);

    juce::Colour getVelocityColour(float normalizedVelocity);
    bool isBlackKey(int noteNumber);
    float getKeyX(int noteNumber, float keyboardWidth);

    PitchVelocityProcessor &audioProcessor;

    juce::Slider minVelocitySlider;
    juce::Slider maxVelocitySlider;
    juce::Slider curveSlider;
    juce::ToggleButton bypassButton;

    juce::Label minVelocityLabel;
    juce::Label maxVelocityLabel;
    juce::Label curveLabel;
    juce::Label titleLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> minVelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxVelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> curveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Active notes tracking
    struct NoteInfo
    {
        int velocity;
        float fadeAmount;
        NoteInfo() : velocity(0), fadeAmount(0.0f) {}
    };
    std::array<NoteInfo, 128> activeNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchVelocityEditor)
};