#pragma once

#include "PluginProcessor.h"
#include <affine_ui/affine_ui.h>

namespace RingmodTheme
{
// Stealth finish: matte black powder coat, white LED rings and soft keys, and one amber screen.
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
    void paintTuner(juce::Graphics&, juce::Rectangle<float>);
    void paintSignal(juce::Graphics&, juce::Rectangle<float>);

    affine::Theme theme;
    affine::render::GlowText noteGlow;
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
    /** The source mode and the input pitch the carrier follows (0 without one); they set the scope's time window. */
    void setReference(bool pitchTracking, float inputPitchHz);
    void paint(juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    static juce::String formatFrequency(float hz);
    juce::String getReadoutText() const { return formatFrequency(carrier); }

private:
    class ValueInterface;
    void paintScope(juce::Graphics&, juce::Rectangle<float>);

    affine::Theme theme;
    affine::render::GlowText readout;
    float carrier = 0.0f;
    float reference = 0.0f;
    int waveform = 0;
    bool tracking = true;
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

    affine::Knob smoothingKnob, sensitivityKnob, rateMultKnob, manualRateKnob, mixKnob;
    affine::SelectorKeys modeKeys, waveformKeys;

    uint32_t lastBlockCount = 0;
    double lastBlockMs = 0.0;
    int lastModeIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HdnRingmodAudioProcessorEditor)
};
