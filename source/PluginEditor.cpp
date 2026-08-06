#include "PluginEditor.h"
#include "NoteNames.h"
#include "ParameterIDs.h"

class PitchDisplay::ValueInterface final : public juce::AccessibilityValueInterface
{
public:
    explicit ValueInterface(PitchDisplay& displayToUse) : display(displayToUse) {}

    bool isReadOnly() const override { return true; }
    double getCurrentValue() const override
    {
        return display.trackingMode ? display.pitchHz : display.manualRate;
    }
    juce::String getCurrentValueAsString() const override
    {
        return display.getAccessibleValueText();
    }
    void setValue(double) override {}
    void setValueAsString(const juce::String&) override {}
    AccessibleValueRange getRange() const override { return {}; }

private:
    PitchDisplay& display;
};

void PitchDisplay::setState(float detectedPitchHz, float confidence, bool pitchTracking,
                            float manualRateHz)
{
    auto pitchChanged = juce::roundToInt(pitchHz * 10.0f)
                        != juce::roundToInt(detectedPitchHz * 10.0f);
    auto confidenceChanged = juce::roundToInt(confidenceValue * 100.0f)
                             != juce::roundToInt(confidence * 100.0f)
                          || (confidenceValue > 0.1f) != (confidence > 0.1f);
    auto modeChanged = trackingMode != pitchTracking;
    auto manualRateChanged = juce::roundToInt(manualRate * 10.0f)
                          != juce::roundToInt(manualRateHz * 10.0f);

    if (!pitchChanged && !confidenceChanged && !modeChanged && !manualRateChanged)
        return;

    auto previousAccessibleValue = getAccessibleValueText();
    pitchHz = detectedPitchHz;
    confidenceValue = confidence;
    trackingMode = pitchTracking;
    manualRate = manualRateHz;
    auto accessibleValue = getAccessibleValueText();
    setDescription(accessibleValue);
    repaint();

    if (accessibleValue != previousAccessibleValue)
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

juce::String PitchDisplay::getAccessibleValueText() const
{
    if (!trackingMode)
        return "Manual carrier, " + juce::String(manualRate, 1) + " hertz";

    if (pitchHz > 0.0f && confidenceValue > 0.1f)
        return juce::String(NoteNames::fromFrequency(pitchHz).c_str()) + ", "
             + juce::String(pitchHz, 1) + " hertz, locked, confidence "
             + juce::String(juce::roundToInt(confidenceValue * 100.0f)) + " percent";

    return "Listening for input pitch";
}

std::unique_ptr<juce::AccessibilityHandler> PitchDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

void PitchDisplay::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds();
    auto left = bounds.removeFromLeft(juce::roundToInt(static_cast<float>(bounds.getWidth()) * 0.58f));
    auto right = bounds.reduced(14, 2);
    auto hasPitch = trackingMode && pitchHz > 0.0f && confidenceValue > 0.1f;

    juce::String primary;
    juce::String secondary;
    juce::String status;
    juce::Colour statusColour;

    if (!trackingMode)
    {
        primary = "MANUAL";
        secondary = juce::String(manualRate, 1) + " Hz";
        status = "FIXED SOURCE";
        statusColour = PluginTheme::accent;
    }
    else if (hasPitch)
    {
        primary = NoteNames::fromFrequency(pitchHz).c_str();
        secondary = juce::String(pitchHz, 1) + " Hz";
        status = "LOCKED";
        statusColour = PluginTheme::accent;
    }
    else
    {
        primary = "LISTENING";
        secondary = "Play a note to begin";
        status = "LISTENING";
        statusColour = PluginTheme::warning;
    }

    graphics.setColour(PluginTheme::primaryText);
    graphics.setFont(PluginTheme::makeFont(trackingMode && hasPitch ? 50.0f : 34.0f, true));
    graphics.drawFittedText(primary, left.removeFromTop(54), juce::Justification::centredLeft, 1);

    graphics.setColour(PluginTheme::secondaryText);
    graphics.setFont(trackingMode && hasPitch ? PluginTheme::makeFont(18.0f, false, 0.03f) : PluginTheme::makeFont(16.0f));
    graphics.drawFittedText(secondary, left.removeFromTop(26), juce::Justification::centredLeft, 1);

    auto dividerX = static_cast<float>(bounds.getX() - 1);
    graphics.setColour(PluginTheme::border);
    graphics.drawVerticalLine(juce::roundToInt(dividerX), 2.0f,
                              static_cast<float>(getHeight() - 2));

    graphics.setColour(PluginTheme::secondaryText);
    graphics.setFont(PluginTheme::makeFont(11.5f, true, 0.10f));
    graphics.drawText("STATUS", right.removeFromTop(16), juce::Justification::centredLeft);

    auto statusRow = right.removeFromTop(28);
    graphics.setColour(statusColour);
    graphics.fillEllipse(static_cast<float>(statusRow.getX()),
                         static_cast<float>(statusRow.getCentreY() - 3), 6.0f, 6.0f);
    graphics.setFont(PluginTheme::makeFont(15.0f, true, 0.04f));
    graphics.drawText(status, statusRow.withTrimmedLeft(14), juce::Justification::centredLeft);

    if (trackingMode)
    {
        auto confidence = juce::jlimit(0.0f, 1.0f, confidenceValue);
        graphics.setColour(PluginTheme::secondaryText);
        graphics.setFont(PluginTheme::makeFont(11.0f, true, 0.08f));
        graphics.drawText("CONFIDENCE  " + juce::String(juce::roundToInt(confidence * 100.0f)) + "%",
                          right.removeFromTop(18), juce::Justification::centredLeft);

        auto meter = right.removeFromTop(4).toFloat();
        graphics.setColour(PluginTheme::border);
        graphics.fillRoundedRectangle(meter, 2.0f);
        meter.setWidth(meter.getWidth() * confidence);
        graphics.setColour(hasPitch ? PluginTheme::accent : PluginTheme::warning);
        graphics.fillRoundedRectangle(meter, 2.0f);
    }
    else
    {
        graphics.setColour(PluginTheme::secondaryText);
        graphics.setFont(PluginTheme::makeFont(11.0f, true, 0.08f));
        graphics.drawText("HOST-AUTOMATABLE", right.removeFromTop(18),
                          juce::Justification::centredLeft);
    }
}

HdnRingmodAudioProcessorEditor::HdnRingmodAudioProcessorEditor(HdnRingmodAudioProcessor& processor)
    : AudioProcessorEditor(processor), processorRef(processor)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label,
                              const juce::String& text)
    {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 24);
        slider.setWantsKeyboardFocus(true);
        slider.setName(text);
        slider.setTitle(text);

        label.setText(text.toUpperCase(), juce::dontSendNotification);
        label.setFont(PluginTheme::makeFont(12.0f, true, 0.08f));
        label.setColour(juce::Label::textColourId, PluginTheme::secondaryText);
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    };

    setupSlider(mixSlider, mixLabel, "Mix");
    setupSlider(rateMultSlider, rateMultLabel, "Rate Multiplier");
    setupSlider(manualRateSlider, manualRateLabel, "Manual Rate");
    setupSlider(smoothingSlider, smoothingLabel, "Smoothing");
    setupSlider(sensitivitySlider, sensitivityLabel, "Sensitivity");

    mixAttach = std::make_unique<SliderAttachment>(processor.apvts, ParameterIDs::mix, mixSlider);
    rateMultAttach = std::make_unique<SliderAttachment>(processor.apvts, ParameterIDs::rateMultiplier,
                                                        rateMultSlider);
    manualRateAttach = std::make_unique<SliderAttachment>(processor.apvts, ParameterIDs::manualRate,
                                                          manualRateSlider);
    smoothingAttach = std::make_unique<SliderAttachment>(processor.apvts, ParameterIDs::smoothing,
                                                         smoothingSlider);
    sensitivityAttach = std::make_unique<SliderAttachment>(processor.apvts, ParameterIDs::sensitivity,
                                                           sensitivitySlider);

    auto setupCombo = [this, &processor](juce::ComboBox& box, juce::Label& label,
                                        const juce::String& text, const juce::String& parameterID)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(
                processor.apvts.getParameter(parameterID)))
            box.addItemList(parameter->choices, 1);

        box.setJustificationType(juce::Justification::centredLeft);
        box.setName(text);
        box.setTitle(text);
        addAndMakeVisible(box);

        label.setText(text.toUpperCase(), juce::dontSendNotification);
        label.setFont(PluginTheme::makeFont(11.5f, true, 0.08f));
        label.setColour(juce::Label::textColourId, PluginTheme::secondaryText);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
    };

    setupCombo(modeBox, modeLabel, "Source Mode", ParameterIDs::mode);
    setupCombo(waveformBox, waveformLabel, "Carrier Shape", ParameterIDs::waveform);

    modeAttach = std::make_unique<ComboBoxAttachment>(processor.apvts, ParameterIDs::mode, modeBox);
    waveformAttach = std::make_unique<ComboBoxAttachment>(processor.apvts, ParameterIDs::waveform,
                                                          waveformBox);

    pitchDisplay.setTitle("Pitch monitor");
    pitchDisplay.setDescription("Displays the detected input pitch and tracking confidence");
    addAndMakeVisible(pitchDisplay);

    setSize(780, 480);
    updateModePresentation();
    timerCallback();
    startTimerHz(30);
}

HdnRingmodAudioProcessorEditor::~HdnRingmodAudioProcessorEditor()
{
    stopTimer();
    mixAttach.reset();
    rateMultAttach.reset();
    manualRateAttach.reset();
    smoothingAttach.reset();
    sensitivityAttach.reset();
    modeAttach.reset();
    waveformAttach.reset();
    setLookAndFeel(nullptr);
}

void HdnRingmodAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(PluginTheme::canvas);

    graphics.setColour(PluginTheme::surface);
    graphics.fillRect(0, 0, getWidth(), 62);
    graphics.setColour(PluginTheme::border);
    graphics.drawHorizontalLine(61, 0.0f, static_cast<float>(getWidth()));

    graphics.setColour(PluginTheme::accent);
    graphics.fillRoundedRectangle(18.0f, 17.0f, 4.0f, 28.0f, 2.0f);

    graphics.setColour(PluginTheme::primaryText);
    graphics.setFont(PluginTheme::makeFont(22.0f, true, 0.02f));
    graphics.drawText("HDN", 34, 10, 48, 25, juce::Justification::centredLeft);
    graphics.setColour(PluginTheme::secondaryText);
    graphics.drawText("/", 80, 10, 16, 25, juce::Justification::centred);
    graphics.setColour(PluginTheme::primaryText);
    graphics.drawText("RING MODULATOR", 96, 10, 220, 25, juce::Justification::centredLeft);

    graphics.setColour(PluginTheme::secondaryText);
    graphics.setFont(PluginTheme::makeFont(11.0f, true, 0.12f));
    graphics.drawText("PITCH-TRACKED SIGNAL PROCESSOR", 34, 34, 280, 15,
                      juce::Justification::centredLeft);

    auto iconCentre = juce::Point<float>(static_cast<float>(getWidth() - 42), 31.0f);
    graphics.setColour(PluginTheme::border);
    graphics.drawEllipse(iconCentre.x - 19.0f, iconCentre.y - 10.0f, 20.0f, 20.0f, 1.5f);
    graphics.setColour(PluginTheme::accent);
    graphics.drawEllipse(iconCentre.x - 1.0f, iconCentre.y - 10.0f, 20.0f, 20.0f, 1.5f);

    paintPanel(graphics, trackerBounds, "PITCH MONITOR");
    paintPanel(graphics, sourceBounds, "CONFIGURATION");
    paintPanel(graphics, carrierBounds, "CARRIER");
    paintPanel(graphics, trackingBounds, "TRACKING");
    paintPanel(graphics, outputBounds, "OUTPUT");
}

void HdnRingmodAudioProcessorEditor::paintPanel(juce::Graphics& graphics,
                                                 juce::Rectangle<int> bounds,
                                                 const juce::String& title) const
{
    auto panel = bounds.toFloat();
    graphics.setColour(PluginTheme::surface);
    graphics.fillRoundedRectangle(panel, PluginTheme::cornerRadius);
    graphics.setColour(PluginTheme::border);
    graphics.drawRoundedRectangle(panel.reduced(0.5f), PluginTheme::cornerRadius, 1.0f);

    graphics.setColour(PluginTheme::secondaryText);
    graphics.setFont(PluginTheme::makeFont(11.5f, true, 0.10f));
    graphics.drawText(title, bounds.getX() + 14, bounds.getY() + 8,
                      bounds.getWidth() - 28, 15, juce::Justification::centredLeft);

    graphics.setColour(PluginTheme::border.withAlpha(0.7f));
    graphics.drawHorizontalLine(bounds.getY() + 28,
                                static_cast<float>(bounds.getX() + 14),
                                static_cast<float>(bounds.getRight() - 14));
}

void HdnRingmodAudioProcessorEditor::resized()
{
    auto content = getLocalBounds();
    content.removeFromTop(62);
    content.reduce(18, 18);

    auto top = content.removeFromTop(154);
    trackerBounds = top.removeFromLeft(500);
    top.removeFromLeft(12);
    sourceBounds = top;

    content.removeFromTop(12);
    auto bottom = content;
    carrierBounds = bottom.removeFromLeft(290);
    bottom.removeFromLeft(12);
    trackingBounds = bottom.removeFromLeft(290);
    bottom.removeFromLeft(12);
    outputBounds = bottom;

    auto trackerContent = trackerBounds.reduced(16);
    trackerContent.removeFromTop(28);
    pitchDisplay.setBounds(trackerContent);

    auto sourceContent = sourceBounds.reduced(14);
    sourceContent.removeFromTop(26);
    modeLabel.setBounds(sourceContent.removeFromTop(14));
    modeBox.setBounds(sourceContent.removeFromTop(34));
    sourceContent.removeFromTop(6);
    waveformLabel.setBounds(sourceContent.removeFromTop(14));
    waveformBox.setBounds(sourceContent.removeFromTop(32));

    auto layoutPair = [](juce::Rectangle<int> panel, juce::Slider& firstSlider,
                         juce::Label& firstLabel, juce::Slider& secondSlider,
                         juce::Label& secondLabel)
    {
        auto controls = panel.reduced(12);
        controls.removeFromTop(28);
        auto first = controls.removeFromLeft(controls.getWidth() / 2);
        auto second = controls;
        firstLabel.setBounds(first.removeFromTop(18));
        secondLabel.setBounds(second.removeFromTop(18));
        firstSlider.setBounds(first.reduced(3, 0));
        secondSlider.setBounds(second.reduced(3, 0));
    };

    layoutPair(carrierBounds, rateMultSlider, rateMultLabel,
               manualRateSlider, manualRateLabel);
    layoutPair(trackingBounds, smoothingSlider, smoothingLabel,
               sensitivitySlider, sensitivityLabel);

    auto output = outputBounds.reduced(12);
    output.removeFromTop(28);
    mixLabel.setBounds(output.removeFromTop(18));
    mixSlider.setBounds(output.reduced(2, 0));
}

void HdnRingmodAudioProcessorEditor::updateModePresentation()
{
    auto modeIndex = modeBox.getSelectedItemIndex();
    if (modeIndex == lastModeIndex)
        return;

    lastModeIndex = modeIndex;
    auto pitchTracking = modeIndex != 1;
    auto setControlEmphasis = [](juce::Slider& slider, bool prominent)
    {
        slider.setColour(juce::Slider::rotarySliderFillColourId,
                         prominent ? PluginTheme::accent : PluginTheme::inactiveControl);
        slider.repaint();
    };

    setControlEmphasis(rateMultSlider, pitchTracking);
    setControlEmphasis(smoothingSlider, pitchTracking);
    setControlEmphasis(sensitivitySlider, pitchTracking);
    setControlEmphasis(manualRateSlider, !pitchTracking);
}

void HdnRingmodAudioProcessorEditor::timerCallback()
{
    auto pitchHz = processorRef.currentPitchHz.load(std::memory_order_relaxed);
    auto confidence = processorRef.currentConfidence.load(std::memory_order_relaxed);
    auto pitchTracking = modeBox.getSelectedItemIndex() != 1;

    pitchDisplay.setState(pitchHz, confidence, pitchTracking,
                          static_cast<float>(manualRateSlider.getValue()));
    updateModePresentation();
}
