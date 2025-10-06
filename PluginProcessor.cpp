#include "PluginProcessor.h"
#include "PluginEditor.h"

StepSequencerAudioProcessor::StepSequencerAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

StepSequencerAudioProcessor::~StepSequencerAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout StepSequencerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 8 step pitch parameters (±12 semitones)
    for (int i = 0; i < NUM_STEPS; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("step" + juce::String(i), 1),
            "Step " + juce::String(i + 1),
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("st")
                .withStringFromValueFunction([](float value, int)
                                             { return juce::String(value, 1) + " st"; })));
    }

    // Rate parameter (note divisions) - now includes triplets
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("rate", 1),
        "Rate",
        juce::NormalisableRange<float>(0.25f, 8.0f, 0.01f),
        2.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) -> juce::String
                                         {
                // Check for triplets first
                if (std::abs(value - 1.333f) < 0.02f) return juce::String("1/4T");
                if (std::abs(value - 2.666f) < 0.02f) return juce::String("1/8T");
                if (std::abs(value - 5.333f) < 0.02f) return juce::String("1/16T");
                
                // Regular divisions
                if (value >= 4.0f) return "1/" + juce::String((int)(value * 4)) + " note";
                else if (value >= 2.0f) return "1/" + juce::String((int)(value * 2)) + " note";
                else if (value >= 1.0f) return juce::String("1/4 note");
                else if (value >= 0.5f) return juce::String("1/2 note");
                else return juce::String("Whole"); })));

    // Gate length (can go over 100% for overlapping notes/glide)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gate", 1),
        "Gate",
        juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String((int)(value * 100)) + "%"; })));

    // Glide enable
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("glide_enable", 1),
        "Glide",
        false));

    // Glide time in milliseconds
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("glide_time", 1),
        "Glide Time",
        juce::NormalisableRange<float>(1.0f, 1000.0f, 1.0f, 0.3f),
        50.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("ms")
            .withStringFromValueFunction([](float value, int)
                                         { return juce::String((int)value) + " ms"; })));

    return {params.begin(), params.end()};
}

void StepSequencerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    phase = 0.0f;
    currentFrequency = 440.0f;
    targetFrequency = 440.0f;
    resetSequencer();
}

void StepSequencerAudioProcessor::releaseResources()
{
}

bool StepSequencerAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void StepSequencerAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Get tempo info
    if (auto *playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
            lastPosInfo = *posInfo;
    }

    double bpm = lastPosInfo.getBpm().orFallback(120.0);
    double sampleRate = getSampleRate();

    auto rateParam = apvts.getRawParameterValue("rate")->load();
    auto gateParam = apvts.getRawParameterValue("gate")->load();
    auto glideEnable = apvts.getRawParameterValue("glide_enable")->load() > 0.5f;
    auto glideTimeMs = apvts.getRawParameterValue("glide_time")->load();

    stepLengthInSamples = calculateStepLength(sampleRate, bpm, rateParam);

    // Calculate glide rate (frequency change per sample)
    if (glideEnable && glideTimeMs > 0.0f)
    {
        float glideTimeSamples = (glideTimeMs / 1000.0f) * sampleRate;
        glideRate = 1.0f / glideTimeSamples;
    }
    else
    {
        glideRate = 1.0f; // Instant change
    }

    // Process MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            isNoteOn = true;
            baseNote = msg.getNoteNumber();
            resetSequencer();
            gateIsOn = true;
            gateOffSamples = stepLengthInSamples * gateParam;
        }
        else if (msg.isNoteOff())
        {
            isNoteOn = false;
            gateIsOn = false;
        }
    }

    if (!isNoteOn)
        return;

    auto *outputData = buffer.getWritePointer(0);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Check if we need to advance to next step
        if (samplesUntilNextStep <= 0.0)
        {
            advanceStep();
            samplesUntilNextStep = stepLengthInSamples;
            gateOffSamples = stepLengthInSamples * gateParam;
            gateIsOn = true;
        }

        // Check gate
        if (gateOffSamples <= 0.0)
            gateIsOn = false;

        // Apply glide to frequency
        if (currentFrequency != targetFrequency)
        {
            if (glideRate >= 1.0f)
            {
                currentFrequency = targetFrequency;
            }
            else
            {
                float diff = targetFrequency - currentFrequency;
                currentFrequency += diff * glideRate;

                // Snap to target if very close
                if (std::abs(targetFrequency - currentFrequency) < 0.1f)
                    currentFrequency = targetFrequency;
            }
        }

        // Generate saw wave
        float output = 0.0f;
        if (gateIsOn)
        {
            output = (phase * 2.0f - 1.0f) * 0.3f; // Saw wave with volume scaling
        }

        outputData[sample] = output;

        // Update phase
        phase += currentFrequency / (float)sampleRate;
        if (phase >= 1.0f)
            phase -= 1.0f;

        samplesUntilNextStep -= 1.0;
        gateOffSamples -= 1.0;
    }
}

void StepSequencerAudioProcessor::advanceStep()
{
    currentStep = (currentStep + 1) % NUM_STEPS;
    updateFrequency();
}

void StepSequencerAudioProcessor::resetSequencer()
{
    currentStep = -1;           // Start at -1 so first advance goes to step 0
    samplesUntilNextStep = 0.0; // Trigger first step immediately
}

void StepSequencerAudioProcessor::updateFrequency()
{
    auto stepPitch = apvts.getRawParameterValue("step" + juce::String(currentStep))->load();
    float midiNote = baseNote + stepPitch;
    targetFrequency = 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);

    // If glide is off, snap immediately
    auto glideEnable = apvts.getRawParameterValue("glide_enable")->load() > 0.5f;
    if (!glideEnable)
        currentFrequency = targetFrequency;
}

double StepSequencerAudioProcessor::calculateStepLength(double sampleRate, double bpm, float rateParam)
{
    // Calculate samples per quarter note
    double samplesPerQuarterNote = (60.0 / bpm) * sampleRate;

    // rateParam: 2.0 = eighth note = 0.5 quarter notes
    // rateParam: 1.0 = quarter note = 1.0 quarter notes
    // rateParam: 0.5 = half note = 2.0 quarter notes
    double quarterNotes = 1.0 / rateParam;

    return samplesPerQuarterNote * quarterNotes;
}

juce::AudioProcessorEditor *StepSequencerAudioProcessor::createEditor()
{
    // Use generic editor which automatically creates UI for all APVTS parameters
    auto *editor = new juce::GenericAudioProcessorEditor(*this);
    editor->setSize(400, 600); // Make it taller so all params are visible
    return editor;
}

void StepSequencerAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void StepSequencerAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new StepSequencerAudioProcessor();
}