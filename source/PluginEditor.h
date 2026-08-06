#pragma once

#include "PluginLookAndFeel.h"
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class PitchDisplay final : public juce::Component
{
public:
    void setState(float detectedPitchHz, float confidence, bool pitchTracking,
                  float manualRateHz);
    void paint(juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    class ValueInterface;
    juce::String getAccessibleValueText() const;

    float pitchHz = 0.0f;
    float confidenceValue = 0.0f;
    float manualRate = 440.0f;
    bool trackingMode = true;
};

class HdnRingmodAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit HdnRingmodAudioProcessorEditor(HdnRingmodAudioProcessor&);
    ~HdnRingmodAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModePresentation();
    void paintPanel(juce::Graphics&, juce::Rectangle<int>, const juce::String&) const;

    HdnRingmodAudioProcessor& processorRef;
    PluginLookAndFeel lookAndFeel;
    PitchDisplay pitchDisplay;

    ParameterSlider mixSlider, rateMultSlider, manualRateSlider, smoothingSlider, sensitivitySlider;
    juce::Label mixLabel, rateMultLabel, manualRateLabel, smoothingLabel, sensitivityLabel;

    juce::ComboBox modeBox, waveformBox;
    juce::Label modeLabel, waveformLabel;

    juce::Rectangle<int> trackerBounds, sourceBounds, carrierBounds, trackingBounds, outputBounds;
    int lastModeIndex = -1;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> mixAttach, rateMultAttach, manualRateAttach,
                                       smoothingAttach, sensitivityAttach;
    std::unique_ptr<ComboBoxAttachment> modeAttach, waveformAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HdnRingmodAudioProcessorEditor)
};
