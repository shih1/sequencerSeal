// PluginEditor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchVelocityEditor::PitchVelocityEditor(PitchVelocityProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Title
    titleLabel.setText("Pitch → Velocity Remapper", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Min Velocity
    minVelocitySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    minVelocitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(minVelocitySlider);

    minVelocityLabel.setText("Min Velocity", juce::dontSendNotification);
    minVelocityLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(minVelocityLabel);

    // Max Velocity
    maxVelocitySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    maxVelocitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(maxVelocitySlider);

    maxVelocityLabel.setText("Max Velocity", juce::dontSendNotification);
    maxVelocityLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(maxVelocityLabel);

    // Curve
    curveSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    curveSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    curveSlider.setSkewFactorFromMidPoint(1.0);
    addAndMakeVisible(curveSlider);

    curveLabel.setText("Curve", juce::dontSendNotification);
    curveLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(curveLabel);

    // Bypass
    bypassButton.setButtonText("Bypass");
    addAndMakeVisible(bypassButton);

    // Create parameter attachments
    minVelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), PitchVelocityProcessor::MIN_VELOCITY_ID, minVelocitySlider);

    maxVelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), PitchVelocityProcessor::MAX_VELOCITY_ID, maxVelocitySlider);

    curveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), PitchVelocityProcessor::CURVE_ID, curveSlider);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), PitchVelocityProcessor::BYPASS_ID, bypassButton);

    // Initialize active notes
    for (auto &note : activeNotes)
    {
        note.velocity = 0;
        note.fadeAmount = 0.0f;
    }

    // Start timer for graphics updates
    startTimerHz(30);

    setSize(800, 300);
}

PitchVelocityEditor::~PitchVelocityEditor()
{
    stopTimer();
}
void PitchVelocityEditor::timerCallback()
{
    // Update from processor's MIDI
    audioProcessor.processMidiForDisplay(audioProcessor.keyboardState, 128);

    // Update active notes with velocities
    bool needsRepaint = false;

    for (int i = 0; i < 128; i++)
    {
        if (audioProcessor.keyboardState.isNoteOn(1, i))
        {
            int vel = audioProcessor.lastNoteVelocities[i].load();
            if (vel > 0)
            {
                activeNotes[i].velocity = vel; // THIS LINE WAS MISSING
                activeNotes[i].fadeAmount = 1.0f;
                needsRepaint = true;
            }
        }
    }

    // Fade out active notes
    for (auto &note : activeNotes)
    {
        if (note.fadeAmount > 0.0f)
        {
            note.fadeAmount -= 0.05f;
            if (note.fadeAmount < 0.0f)
                note.fadeAmount = 0.0f;
            needsRepaint = true;
        }
    }

    if (needsRepaint)
        repaint();
}

void PitchVelocityEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff1a1a1a)); // Dark background

    // Define visualization area
    juce::Rectangle<float> vizArea(20, 170, getWidth() - 40, 110);

    // Draw curve overlay at top
    juce::Rectangle<float> curveArea(vizArea.getX(), vizArea.getY(), vizArea.getWidth(), 25);
    drawCurveOverlay(g, curveArea);

    // Draw velocity bars
    juce::Rectangle<float> barsArea(vizArea.getX(), curveArea.getBottom() + 5, vizArea.getWidth(), 35);
    drawVelocityBars(g, barsArea);

    // Draw keyboard at bottom
    juce::Rectangle<float> keyboardArea(vizArea.getX(), barsArea.getBottom() + 5, vizArea.getWidth(), 40);
    drawKeyboard(g, keyboardArea);
}

void PitchVelocityEditor::drawCurveOverlay(juce::Graphics &g, juce::Rectangle<float> area)
{
    // Background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(area, 4.0f);

    // Draw velocity curve
    auto minVelParam = audioProcessor.getParameters().getRawParameterValue(PitchVelocityProcessor::MIN_VELOCITY_ID);
    auto maxVelParam = audioProcessor.getParameters().getRawParameterValue(PitchVelocityProcessor::MAX_VELOCITY_ID);
    auto curveParam = audioProcessor.getParameters().getRawParameterValue(PitchVelocityProcessor::CURVE_ID);

    if (minVelParam && maxVelParam && curveParam)
    {
        float minVel = *minVelParam;
        float maxVel = *maxVelParam;
        float curveVal = *curveParam;

        juce::Path path;
        for (int i = 0; i <= 127; i++)
        {
            float x = area.getX() + (i / 127.0f) * area.getWidth();
            float normalized = std::pow(i / 127.0f, curveVal);
            float velocity = minVel + (maxVel - minVel) * normalized;
            float y = area.getBottom() - (velocity / 127.0f) * area.getHeight();

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(juce::Colour(0xff00ffff).withAlpha(0.6f));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }
}

void PitchVelocityEditor::drawVelocityBars(juce::Graphics &g, juce::Rectangle<float> area)
{
    float whiteKeyWidth = area.getWidth() / 75.0f; // 75 white keys in 128 notes

    for (int i = 0; i < 128; i++)
    {
        if (activeNotes[i].fadeAmount > 0.0f)
        {
            float x = getKeyX(i, area.getWidth());
            float normalizedVel = activeNotes[i].velocity / 127.0f;
            float barHeight = normalizedVel * area.getHeight();

            juce::Colour barColour = getVelocityColour(normalizedVel);
            barColour = barColour.withAlpha(activeNotes[i].fadeAmount);

            g.setColour(barColour);
            float barWidth = isBlackKey(i) ? whiteKeyWidth * 0.6f : whiteKeyWidth * 0.8f;
            g.fillRect(area.getX() + x - barWidth / 2,
                       area.getBottom() - barHeight,
                       barWidth,
                       barHeight);
        }
    }
}

void PitchVelocityEditor::drawKeyboard(juce::Graphics &g, juce::Rectangle<float> area)
{
    float whiteKeyWidth = area.getWidth() / 75.0f;
    float blackKeyWidth = whiteKeyWidth * 0.6f;
    float blackKeyHeight = area.getHeight() * 0.6f;

    // Draw white keys first
    int whiteKeyIndex = 0;
    for (int i = 0; i < 128; i++)
    {
        if (!isBlackKey(i))
        {
            float x = area.getX() + whiteKeyIndex * whiteKeyWidth;

            // Determine if key is active
            bool isActive = activeNotes[i].fadeAmount > 0.0f;
            juce::Colour keyColour;

            if (isActive)
            {
                float normalizedVel = activeNotes[i].velocity / 127.0f;
                keyColour = getVelocityColour(normalizedVel).withAlpha(activeNotes[i].fadeAmount);
            }
            else
            {
                keyColour = juce::Colours::white;
            }

            g.setColour(keyColour);
            g.fillRect(x, area.getY(), whiteKeyWidth - 1, area.getHeight());

            g.setColour(juce::Colour(0xff1a1a1a));
            g.drawRect(x, area.getY(), whiteKeyWidth - 1, area.getHeight(), 1.0f);

            whiteKeyIndex++;
        }
    }

    // Draw black keys on top
    whiteKeyIndex = 0;
    for (int i = 0; i < 128; i++)
    {
        if (!isBlackKey(i))
        {
            whiteKeyIndex++;
        }
        else
        {
            float x = area.getX() + whiteKeyIndex * whiteKeyWidth - blackKeyWidth / 2;

            bool isActive = activeNotes[i].fadeAmount > 0.0f;
            juce::Colour keyColour;

            if (isActive)
            {
                float normalizedVel = activeNotes[i].velocity / 127.0f;
                keyColour = getVelocityColour(normalizedVel).withAlpha(activeNotes[i].fadeAmount);
            }
            else
            {
                keyColour = juce::Colours::black;
            }

            g.setColour(keyColour);
            g.fillRect(x, area.getY(), blackKeyWidth, blackKeyHeight);

            g.setColour(juce::Colour(0xff1a1a1a));
            g.drawRect(x, area.getY(), blackKeyWidth, blackKeyHeight, 1.0f);
        }
    }
}

juce::Colour PitchVelocityEditor::getVelocityColour(float normalizedVelocity)
{
    // Cool to hot color gradient
    if (normalizedVelocity < 0.25f)
    {
        // Blue to Cyan
        float t = normalizedVelocity / 0.25f;
        return juce::Colour(0xff0000ff).interpolatedWith(juce::Colour(0xff00ffff), t);
    }
    else if (normalizedVelocity < 0.5f)
    {
        // Cyan to Green
        float t = (normalizedVelocity - 0.25f) / 0.25f;
        return juce::Colour(0xff00ffff).interpolatedWith(juce::Colour(0xff00ff00), t);
    }
    else if (normalizedVelocity < 0.75f)
    {
        // Green to Yellow
        float t = (normalizedVelocity - 0.5f) / 0.25f;
        return juce::Colour(0xff00ff00).interpolatedWith(juce::Colour(0xffffff00), t);
    }
    else
    {
        // Yellow to Red
        float t = (normalizedVelocity - 0.75f) / 0.25f;
        return juce::Colour(0xffffff00).interpolatedWith(juce::Colour(0xffff0000), t);
    }
}

bool PitchVelocityEditor::isBlackKey(int noteNumber)
{
    int pitchClass = noteNumber % 12;
    return (pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
            pitchClass == 8 || pitchClass == 10);
}

float PitchVelocityEditor::getKeyX(int noteNumber, float keyboardWidth)
{
    int whiteKeysBefore = 0;
    for (int i = 0; i < noteNumber; i++)
    {
        if (!isBlackKey(i))
            whiteKeysBefore++;
    }

    float whiteKeyWidth = keyboardWidth / 75.0f;

    if (isBlackKey(noteNumber))
    {
        return whiteKeysBefore * whiteKeyWidth;
    }
    else
    {
        return whiteKeysBefore * whiteKeyWidth + whiteKeyWidth / 2;
    }
}

void PitchVelocityEditor::resized()
{
    titleLabel.setBounds(0, 10, getWidth(), 30);

    int knobSize = 80;
    int y = 50;
    int spacing = (getWidth() - 3 * knobSize) / 4;

    minVelocityLabel.setBounds(spacing, y, knobSize, 20);
    minVelocitySlider.setBounds(spacing, y + 20, knobSize, knobSize);

    maxVelocityLabel.setBounds(spacing * 2 + knobSize, y, knobSize, 20);
    maxVelocitySlider.setBounds(spacing * 2 + knobSize, y + 20, knobSize, knobSize);

    curveLabel.setBounds(spacing * 3 + knobSize * 2, y, knobSize, 20);
    curveSlider.setBounds(spacing * 3 + knobSize * 2, y + 20, knobSize, knobSize);

    bypassButton.setBounds(getWidth() / 2 - 40, 145, 80, 25);
}