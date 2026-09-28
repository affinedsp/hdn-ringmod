#pragma once

namespace affine
{
/**
    Legends printed behind a glass front; the one that describes the current
    state lights up. The label's text stays the accessible state; the mapping
    chooses which legend it lights.
*/
class IlluminatedLegends : public juce::Label
{
public:
    IlluminatedLegends();

    void setTheme (const Theme&);
    void setLegends (const juce::StringArray& legends, std::function<int (const juce::String&)> legendForText);
    /** Colour of the lit legend, e.g. attention while auditioning. */
    void setLitColour (juce::Colour);

    void paint (juce::Graphics&) override;

private:
    Theme theme;
    juce::StringArray legends;
    std::function<int (const juce::String&)> legendFor;
    juce::Colour lit;
};

/**
    A radio-style tuning dial: a backlit logarithmic frequency scale with a
    pointer that glides to the reading and parks at the left stop when there is none.
*/
class TuningDial : public juce::Component, private juce::Timer
{
public:
    TuningDial();
    ~TuningDial() override;

    void setTheme (const Theme&);
    void setRange (float minimumHz, float maximumHz);
    /** Pointer colour; the scale glows in the theme accent. */
    void setPointerColour (juce::Colour);
    void setFrequency (float hz, bool live);
    /** Skips the remaining glide, e.g. before a still capture. */
    void settle();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    float positionFor (float hz) const;
    void renderScale (float scale);

    Theme theme;
    juce::Colour pointer { 0xffff5a3c };
    float minimum = 20.0f, maximum = 20000.0f;
    float target = 0.0f, position = 0.0f, lamp = 0.0f, lampTarget = 0.0f;
    double lastTick = 0.0;
    juce::Image scaleLayer;
    float scaleLayerScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningDial)
};
} // namespace affine
