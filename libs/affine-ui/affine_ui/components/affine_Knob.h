#pragma once

namespace affine
{
/**
    The family rotary control: a rendered knob with a calibrated printed scale,
    a silkscreened label and a glass readout.

    Interaction contract (identical in every product):
    - drag anywhere on the control; up or right increases; Shift for fine;
    - double-click or Return for exact entry; Return commits, Escape cancels,
      malformed text never changes the value;
    - Alt/Option-click or Home resets to the default;
    - arrow keys step; right-click opens the parameter menu, including host items.
    Every edit is one delimited host gesture.
*/
class Knob : public juce::Slider
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& parameterID, const juce::String& help = {});
    ~Knob() override;

    juce::RangedAudioParameter& parameter;

    void setTheme (const Theme&);
    void setDiameter (float logicalDiameter);
    float getDiameter() const noexcept { return diameter; }
    void setLabel (const juce::String&);

    /** Printed scale marks at plain parameter values, placed through the parameter's own mapping. */
    void setScale (std::vector<double> plainValues, std::function<juce::String (double)> formatter = {});

    /** Step for arrow keys, in plain units. Defaults to the parameter interval or 1% of the range. */
    void setKeyboardStep (double plainStep) { keyboardStep = plainStep; }

    /** Inactive but editable: the control does not currently affect the sound. */
    void setInactive (bool);
    bool isInactive() const noexcept { return inactive; }

    /** A lamp beside the label that reports whether the control currently acts on the sound. */
    void setShowsActivityLamp (bool shouldShow);

    /** A ring of LEDs around the knob, lit up to the current value, instead of printed ticks. */
    void setLedRing (bool shouldShow);

    juce::Point<int> getPreferredSize() const;
    /** Preferred bounds, in the parent, that put the knob's axis at `knobCentre`. */
    juce::Rectangle<int> getBoundsForCentre (juce::Point<int> knobCentre) const;
    juce::Point<float> getKnobCentre() const;

    void beginEntry();
    void cancelInteraction();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void enablementChanged() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    struct Geometry
    {
        juce::Point<float> centre;
        float radius = 0.0f;
        juce::Rectangle<float> label, readout;
    };

    Geometry getGeometry() const;
    void finishEntry (bool commit);
    void setWithGesture (double plainValue);
    void showMenu();
    double defaultValue() const;
    void paintScale (juce::Graphics&, const Geometry&);
    void paintReadout (juce::Graphics&, const Geometry&);

    Theme theme;
    KnobRenderer renderer;
    render::GlowText valueGlow;
    juce::String label;
    float diameter = 64.0f;
    std::vector<double> scaleValues;
    std::function<juce::String (double)> scaleFormatter;
    double keyboardStep = 0.0;
    bool inactive = false, activityLamp = false, ledRing = false;

    juce::TextEditor entry;
    juce::Point<float> lastDrag;
    double dragProportion = 0.0;
    std::unique_ptr<juce::Slider::ScopedDragNotification> drag;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
} // namespace affine
