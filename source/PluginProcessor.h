#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/YinPitchDetector.h"
#include "dsp/Oscillator.h"
#include "dsp/PitchSmoother.h"
#include <atomic>

class HdnRingmodAudioProcessor : public juce::AudioProcessor
{
public:
    HdnRingmodAudioProcessor();
    ~HdnRingmodAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
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
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> currentPitchHz { 0.0f };
    std::atomic<float> currentConfidence { 0.0f };
    // Frequency the carrier actually ran at the end of the last block; 0 while the effect is dry.
    std::atomic<float> currentCarrierHz { 0.0f };
    std::atomic<uint32_t> processedBlocks { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    YinPitchDetector pitchDetector;
    Oscillator oscillator;
    PitchSmoother pitchSmoother;

    juce::SmoothedValue<float> smoothedMix;
    juce::SmoothedValue<float> smoothedRateMult;
    juce::SmoothedValue<float> smoothedManualRate;

    float smoothedTrackEnable = 1.0f;
    float trackEnableAlpha = 0.01f;

    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* rateMultParam = nullptr;
    std::atomic<float>* manualRateParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* smoothingParam = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* waveformParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HdnRingmodAudioProcessor)
};
