#include "PluginProcessor.h"
#include "PluginEditor.h"

StepSequencerAudioProcessorEditor::StepSequencerAudioProcessorEditor(StepSequencerAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(800, 400);

    // Setup step sliders
    for (int i = 0; i < NUM_STEPS; ++i)
    {
        auto &slider = stepSliders[i];
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        slider.setTextValueSuffix(" st");
        addAndMakeVisible(slider);

        stepAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getValueTreeState(), "step" + juce::String(i), slider);

        auto &label = stepLabels[i];
        label.setText(juce::String(i + 1), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&slider, false);
        addAndMakeVisible(label);
    }

    // Setup rate slider
    rateSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(rateSlider);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "rate", rateSlider);

    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.setJustificationType(juce::Justification::centred);
    rateLabel.attachToComponent(&rateSlider, false);
    addAndMakeVisible(rateLabel);

    // Setup gate slider
    gateSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(gateSlider);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "gate", gateSlider);

    gateLabel.setText("Gate", juce::dontSendNotification);
    gateLabel.setJustificationType(juce::Justification::centred);
    gateLabel.attachToComponent(&gateSlider, false);
    addAndMakeVisible(gateLabel);

    // Setup glide toggle
    glideToggle.setButtonText("Glide");
    addAndMakeVisible(glideToggle);
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getValueTreeState(), "glide_enable", glideToggle);

    // Setup glide time slider
    glideTimeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    glideTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(glideTimeSlider);
    glideTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "glide_time", glideTimeSlider);

    glideTimeLabel.setText("Glide Time", juce::dontSendNotification);
    glideTimeLabel.setJustificationType(juce::Justification::centred);
    glideTimeLabel.attachToComponent(&glideTimeSlider, false);
    addAndMakeVisible(glideTimeLabel);

    // Start timer for LED updates (30 FPS)
    startTimerHz(30);
}

StepSequencerAudioProcessorEditor::~StepSequencerAudioProcessorEditor()
{
    stopTimer();
}

void StepSequencerAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::darkgrey);

    // Draw sequencer section background
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRect(10, 40, getWidth() - 20, 180);

    // Draw config section background
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRect(10, 240, getWidth() - 20, 150);

    // Draw section titles
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("SEQUENCER", 20, 20, 200, 20, juce::Justification::left);
    g.drawText("CONTROLS", 20, 220, 200, 20, juce::Justification::left);

    // Draw LED indicators for each step
    int stepWidth = (getWidth() - 40) / NUM_STEPS;
    for (int i = 0; i < NUM_STEPS; ++i)
    {
        int x = 20 + i * stepWidth + (stepWidth - 40) / 2;
        int y = 190;

        // Highlight current step
        if (i == lastDisplayedStep)
            g.setColour(juce::Colours::lime);
        else
            g.setColour(juce::Colours::darkgrey);

        g.fillRoundedRectangle(x, y, 40, 20, 4.0f);

        // Border
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(x, y, 40, 20, 4.0f, 1.0f);
    }
}

void StepSequencerAudioProcessorEditor::resized()
{
    int stepWidth = (getWidth() - 40) / NUM_STEPS;

    // Layout step sliders in sequencer section
    for (int i = 0; i < NUM_STEPS; ++i)
    {
        int x = 20 + i * stepWidth;
        stepSliders[i].setBounds(x, 60, stepWidth - 10, 100);
    }

    // Layout config section controls
    int configY = 260;
    int controlSpacing = 120;
    int startX = 40;

    rateSlider.setBounds(startX, configY, 100, 100);
    gateSlider.setBounds(startX + controlSpacing, configY, 100, 100);
    glideToggle.setBounds(startX + controlSpacing * 2, configY + 20, 80, 30);
    glideTimeSlider.setBounds(startX + controlSpacing * 3, configY, 100, 100);
}

void StepSequencerAudioProcessorEditor::timerCallback()
{
    // Update current step display
    int currentStep = audioProcessor.getCurrentStep();
    if (currentStep != lastDisplayedStep)
    {
        lastDisplayedStep = currentStep;
        repaint();
    }
}