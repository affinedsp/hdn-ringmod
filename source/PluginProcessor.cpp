#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include <cmath>

HdnRingmodAudioProcessor::HdnRingmodAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    mixParam         = apvts.getRawParameterValue(ParameterIDs::mix);
    rateMultParam    = apvts.getRawParameterValue(ParameterIDs::rateMultiplier);
    manualRateParam  = apvts.getRawParameterValue(ParameterIDs::manualRate);
    modeParam        = apvts.getRawParameterValue(ParameterIDs::mode);
    smoothingParam   = apvts.getRawParameterValue(ParameterIDs::smoothing);
    sensitivityParam = apvts.getRawParameterValue(ParameterIDs::sensitivity);
    waveformParam    = apvts.getRawParameterValue(ParameterIDs::waveform);
}

juce::AudioProcessorValueTreeState::ParameterLayout HdnRingmodAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::mix, 1),
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::rateMultiplier, 1),
        "Rate Multiplier",
        juce::NormalisableRange<float>(0.1f, 8.0f, 0.01f, 0.5f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("x")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::manualRate, 1),
        "Manual Rate",
        juce::NormalisableRange<float>(20.0f, 5000.0f, 0.1f, 0.3f),
        440.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParameterIDs::mode, 1),
        "Mode",
        juce::StringArray{ "Pitch Track", "Manual" },
        0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::smoothing, 1),
        "Smoothing",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::sensitivity, 1),
        "Sensitivity",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParameterIDs::waveform, 1),
        "Waveform",
        juce::StringArray{ "Sine", "Triangle", "Square", "Saw" },
        0));

    return layout;
}

void HdnRingmodAudioProcessor::prepareToPlay(double sampleRate, int)
{
    pitchDetector.prepare(sampleRate);
    oscillator.prepare(sampleRate);
    pitchSmoother.prepare(sampleRate);

    smoothedMix.reset(sampleRate, 0.02);
    smoothedMix.setCurrentAndTargetValue(mixParam->load() / 100.0f);
    smoothedRateMult.reset(sampleRate, 0.02);
    smoothedRateMult.setCurrentAndTargetValue(rateMultParam->load());
    smoothedManualRate.reset(sampleRate, 0.02);
    smoothedManualRate.setCurrentAndTargetValue(manualRateParam->load());

    float tau = 0.005f;
    trackEnableAlpha = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * tau));
    smoothedTrackEnable = 0.0f;
}

void HdnRingmodAudioProcessor::releaseResources()
{
}

bool HdnRingmodAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void HdnRingmodAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = juce::jmin(buffer.getNumChannels(), 2);

    if (numChannels == 0 || numSamples == 0)
        return;

    smoothedMix.setTargetValue(mixParam->load() / 100.0f);
    smoothedRateMult.setTargetValue(rateMultParam->load());
    smoothedManualRate.setTargetValue(manualRateParam->load());

    int mode = static_cast<int>(modeParam->load());
    float smoothing = smoothingParam->load() / 100.0f;
    float sensitivity = sensitivityParam->load() / 100.0f;
    int waveformIdx = static_cast<int>(waveformParam->load());

    pitchSmoother.setSmoothingAmount(smoothing);
    pitchSmoother.setSensitivity(sensitivity);
    oscillator.setWaveform(static_cast<Oscillator::Waveform>(waveformIdx));

    const float* channelReadPtrs[2] = {};
    float* channelPtrs[2] = {};
    for (int ch = 0; ch < numChannels; ++ch)
    {
        channelReadPtrs[ch] = buffer.getReadPointer(ch);
        channelPtrs[ch] = buffer.getWritePointer(ch);
    }

    float carrierHz = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float mix = smoothedMix.getNextValue();
        float effectiveMix = mix;

        if (mode == 0)
        {
            float monoSample = channelReadPtrs[0][i];
            if (numChannels > 1)
                monoSample = (monoSample + channelReadPtrs[1][i]) * 0.5f;

            pitchDetector.feedSample(monoSample);

            auto result = pitchDetector.getResult();
            float smoothedFreq = pitchSmoother.process(result.frequency, result.confidence);

            // Stay dry until the tracker has a pitch, then ramp straight into the tracked carrier.
            if (smoothedFreq > 0.0f)
                smoothedTrackEnable += trackEnableAlpha * (1.0f - smoothedTrackEnable);
            else
                smoothedTrackEnable = 0.0f;

            float oscFreq = smoothedFreq * smoothedRateMult.getNextValue();

            if (oscFreq > 0.0f)
                oscillator.setFrequency(oscFreq);

            effectiveMix *= smoothedTrackEnable;
            carrierHz = smoothedTrackEnable > 0.0f ? oscFreq : 0.0f;
        }
        else
        {
            smoothedTrackEnable = 1.0f;
            carrierHz = smoothedManualRate.getNextValue();
            oscillator.setFrequency(carrierHz);
        }

        float oscSample = oscillator.nextSample();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float dry = channelPtrs[ch][i];
            float wet = dry * oscSample;
            channelPtrs[ch][i] = dry * (1.0f - effectiveMix) + wet * effectiveMix;

            if (!std::isfinite(channelPtrs[ch][i]))
                channelPtrs[ch][i] = 0.0f;
        }
    }

    currentCarrierHz.store(carrierHz, std::memory_order_relaxed);

    if (mode == 0)
    {
        auto result = pitchDetector.getResult();
        currentPitchHz.store(result.frequency, std::memory_order_relaxed);
        currentConfidence.store(result.confidence, std::memory_order_relaxed);
    }
    else
    {
        currentPitchHz.store(0.0f, std::memory_order_relaxed);
        currentConfidence.store(0.0f, std::memory_order_relaxed);
    }

    processedBlocks.fetch_add(1, std::memory_order_release);
}

juce::AudioProcessorEditor* HdnRingmodAudioProcessor::createEditor()
{
    return new HdnRingmodAudioProcessorEditor(*this);
}

bool HdnRingmodAudioProcessor::hasEditor() const { return true; }

const juce::String HdnRingmodAudioProcessor::getName() const { return JucePlugin_Name; }

bool HdnRingmodAudioProcessor::acceptsMidi() const { return false; }
bool HdnRingmodAudioProcessor::producesMidi() const { return false; }
bool HdnRingmodAudioProcessor::isMidiEffect() const { return false; }
double HdnRingmodAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int HdnRingmodAudioProcessor::getNumPrograms() { return 1; }
int HdnRingmodAudioProcessor::getCurrentProgram() { return 0; }
void HdnRingmodAudioProcessor::setCurrentProgram(int) {}
const juce::String HdnRingmodAudioProcessor::getProgramName(int) { return {}; }
void HdnRingmodAudioProcessor::changeProgramName(int, const juce::String&) {}

void HdnRingmodAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xmlState = apvts.copyState().createXml())
        copyXmlToBinary(*xmlState, destData);
}

void HdnRingmodAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HdnRingmodAudioProcessor();
}
