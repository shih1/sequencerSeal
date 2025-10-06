// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

// Parameter IDs
const juce::String PitchVelocityProcessor::MIN_VELOCITY_ID = "minVel";
const juce::String PitchVelocityProcessor::MAX_VELOCITY_ID = "maxVel";
const juce::String PitchVelocityProcessor::CURVE_ID = "curve";
const juce::String PitchVelocityProcessor::BYPASS_ID = "bypass";

PitchVelocityProcessor::PitchVelocityProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    for (auto &vel : lastNoteVelocities)
        vel.store(0);
}

PitchVelocityProcessor::~PitchVelocityProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchVelocityProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{MIN_VELOCITY_ID, 1},
        "Min Velocity",
        juce::NormalisableRange<float>(1.0f, 127.0f, 1.0f),
        10.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{MAX_VELOCITY_ID, 1},
        "Max Velocity",
        juce::NormalisableRange<float>(1.0f, 127.0f, 1.0f),
        127.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{CURVE_ID, 1},
        "Curve",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), // Added skew for better control
        1.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{BYPASS_ID, 1},
        "Bypass",
        false));

    return layout;
}

const juce::String PitchVelocityProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PitchVelocityProcessor::acceptsMidi() const
{
    return true;
}

bool PitchVelocityProcessor::producesMidi() const
{
    return true;
}

bool PitchVelocityProcessor::isMidiEffect() const
{
    return true;
}

double PitchVelocityProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PitchVelocityProcessor::getNumPrograms()
{
    return 1;
}

int PitchVelocityProcessor::getCurrentProgram()
{
    return 0;
}

void PitchVelocityProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String PitchVelocityProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void PitchVelocityProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

void PitchVelocityProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void PitchVelocityProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PitchVelocityProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    // For Ableton compatibility, always accept stereo layouts
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif
void PitchVelocityProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Get parameter values
    bool bypass = false;
    float minVel = 10.0f;
    float maxVel = 127.0f;
    float curveValue = 1.0f;

    if (auto *bypassParam = dynamic_cast<juce::AudioParameterBool *>(parameters.getParameter(BYPASS_ID)))
        bypass = bypassParam->get();

    if (auto *minVelParam = dynamic_cast<juce::AudioParameterFloat *>(parameters.getParameter(MIN_VELOCITY_ID)))
        minVel = minVelParam->get();

    if (auto *maxVelParam = dynamic_cast<juce::AudioParameterFloat *>(parameters.getParameter(MAX_VELOCITY_ID)))
        maxVel = maxVelParam->get();

    if (auto *curveParam = dynamic_cast<juce::AudioParameterFloat *>(parameters.getParameter(CURVE_ID)))
        curveValue = curveParam->get();

    // Update keyboard state with incoming MIDI
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    if (bypass || midiMessages.isEmpty())
        return;

    juce::MidiBuffer processedMidi;

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            int noteNumber = message.getNoteNumber();

            float normalized = noteNumber / 127.0f;
            normalized = std::pow(normalized, curveValue);

            int newVelocity = static_cast<int>(minVel + (maxVel - minVel) * normalized);
            newVelocity = juce::jlimit(1, 127, newVelocity);

            // STORE THE VELOCITY FOR THE EDITOR
            lastNoteVelocities[noteNumber].store(newVelocity);

            auto newMessage = juce::MidiMessage::noteOn(
                message.getChannel(),
                noteNumber,
                static_cast<juce::uint8>(newVelocity));

            processedMidi.addEvent(newMessage, metadata.samplePosition);
        }
        else if (message.isNoteOff())
        {
            // Clear velocity on note off
            lastNoteVelocities[message.getNoteNumber()].store(0);
            processedMidi.addEvent(message, metadata.samplePosition);
        }
        else
        {
            processedMidi.addEvent(message, metadata.samplePosition);
        }
    }

    midiMessages.swapWith(processedMidi);

    // Store MIDI for the editor to read
    {
        const juce::ScopedLock sl(midiMonitorLock);
        currentMidiMessages.clear();
        currentMidiMessages.addEvents(midiMessages, 0, buffer.getNumSamples(), 0);
    }
}

void PitchVelocityProcessor::processMidiForDisplay(juce::MidiKeyboardState &keyboardState, int numSamples)
{
    const juce::ScopedLock sl(midiMonitorLock);
    keyboardState.processNextMidiBuffer(currentMidiMessages, 0, numSamples, true);
}

bool PitchVelocityProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *PitchVelocityProcessor::createEditor()
{
    return new PitchVelocityEditor(*this);
}

void PitchVelocityProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PitchVelocityProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PitchVelocityProcessor();
}
