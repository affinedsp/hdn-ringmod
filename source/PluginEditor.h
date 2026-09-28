#pragma once

#include "PluginProcessor.h"
#include <affine_ui/affine_ui.h>

namespace RingmodTheme
{
// Hi-Fi finish: a black glass front, a blue tuning dial and meters, green legends and silver knobs.
affine::Theme theme();
}

class PitchDisplay final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    PitchDisplay();

    void setTheme(const affine::Theme&);
    void setState(float detectedPitchHz, float confidence, bool pitchTracking, float manualRateHz,
                  float acceptanceThreshold, bool carrierRunning, bool audioLive);
    void paint(juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    juce::String getAccessibleValueText() const;
    bool hasPitch() const;
    /** Deviation of the detected pitch from the nearest equal-tempered note, in cents. */
    float getCents() const;

private:
    class ValueInterface;

    affine::Theme theme;
    affine::render::GlowText noteGlow, detailGlow;
    float pitchHz = 0.0f;
    float confidenceValue = 0.0f;
    float manualRate = 440.0f;
    float threshold = 0.5f;
    bool trackingMode = true;
    bool locked = false;
    bool live = false;
};

class CarrierDisplay final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    CarrierDisplay();

    void setTheme(const affine::Theme&);
    void setState(float carrierHz, int waveformIndex);
    void paint(juce::Graphics&) override;
    void resized() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    static juce::String formatFrequency(float hz);
    juce::String getReadoutText() const { return formatFrequency(carrier); }
    affine::TuningDial& getDial() { return dial; }

private:
    class ValueInterface;

    affine::Theme theme;
    affine::TuningDial dial;
    affine::render::GlowText readout, unit;
    float carrier = 0.0f;
    int waveform = 0;
};

class HdnRingmodAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit HdnRingmodAudioProcessorEditor(HdnRingmodAudioProcessor&);
    ~HdnRingmodAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    int getControlParameterIndex(juce::Component&) override;

    static constexpr int width = 960;
    static constexpr int height = 560;

private:
    void timerCallback() override;
    void updateModePresentation();
    void print(juce::Graphics&);

    HdnRingmodAudioProcessor& processorRef;
    affine::Theme theme;
    affine::LookAndFeel lookAndFeel;
    affine::Faceplate faceplate;
    PitchDisplay pitchDisplay;
    CarrierDisplay carrierDisplay;
    affine::NeedleMeter signalMeter, tuningMeter;

    affine::Knob smoothingKnob, sensitivityKnob, rateMultKnob, manualRateKnob, mixKnob;
    affine::SelectorKeys modeKeys, waveformKeys;

    uint32_t lastBlockCount = 0;
    double lastBlockMs = 0.0;
    int lastModeIndex = -1;
    float shownThreshold = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HdnRingmodAudioProcessorEditor)
};
