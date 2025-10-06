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

// Define NoteDivision struct
struct NoteDivision
{
    const char *label;
    float multiplier; // in beats relative to quarter note
};

// List of musical note divisions, relative to 1 quarter note
static const NoteDivision noteDivisions[] = {
    {"1/64", 0.0625f},
    {"1/64T", 0.0417f},
    {"1/32", 0.125f},
    {"1/32T", 0.0833f},
    {"1/16", 0.25f},
    {"1/16T", 0.1667f},
    {"1/8", 0.5f},
    {"1/8T", 0.333f},
    {"1/4", 1.0f},
    {"1/4T", 0.666f},
    {"1/2", 2.0f},
    {"1/2T", 1.333f},
    {"1 bar", 4.0f}};

// Convert ms to musical label based on BPM
juce::String getMusicalLabel(float ms, float bpm)
{
    // Convert ms to beats
    const float beatDurationMs = 60000.0f / bpm;
    const float beats = ms / beatDurationMs;

    const NoteDivision *closest = nullptr;
    float minError = std::numeric_limits<float>::max();

    for (const auto &div : noteDivisions)
    {
        float error = std::abs(div.multiplier - beats);
        if (error < minError)
        {
            minError = error;
            closest = &div;
        }
    }

    if (closest)
        return juce::String(ms, 1) + " ms (" + closest->label + ")";
    else
        return juce::String(ms, 1) + " ms";
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
                                             { return juce::String(value, 1); })));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("rate", 1),
        "Rate",
        juce::NormalisableRange<float>(10.0f, 500.0f, 0.1f), // reduced max to 500 ms
        100.0f,                                              // default value somewhere in the lower range
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([this](float value, int)
                                         {
            float bpm = currentBpm.load();
            return getMusicalLabel(value, bpm); })));

    // Gate length (up to 100%, but we'll extend it slightly for glide when at max)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gate", 1),
        "Gate",
        juce::NormalisableRange<float>(0.01f, 1.0f, 0.01f),
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

    if (auto *playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            lastPosInfo = *posInfo;

            if (auto bpmOpt = lastPosInfo.getBpm())
            {
                currentBpm.store(*bpmOpt); // Use atomic store
            }
        }
    }

    double bpm = currentBpm.load();
    double sampleRate = getSampleRate();

    auto rateParam = apvts.getRawParameterValue("rate")->load();
    auto gateParam = apvts.getRawParameterValue("gate")->load();
    auto glideEnable = apvts.getRawParameterValue("glide_enable")->load() > 0.5f;
    auto glideTimeMs = apvts.getRawParameterValue("glide_time")->load();

    stepLengthInSamples = calculateStepLength(sampleRate, rateParam);

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

double StepSequencerAudioProcessor::calculateStepLength(double sampleRate, float rateMs)
{
    // Convert ms to seconds
    double seconds = rateMs / 1000.0;

    // Convert to samples
    return seconds * sampleRate;
}

juce::AudioProcessorEditor *StepSequencerAudioProcessor::createEditor()
{
    return new StepSequencerAudioProcessorEditor(*this);
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