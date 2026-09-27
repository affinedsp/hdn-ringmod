#pragma once

namespace affine
{
/**
    A vertical LED ladder for peak level. Segments light from the bottom; the top
    segment fades in proportionally so the column moves smoothly.
*/
class LadderMeter : public juce::Component
{
public:
    LadderMeter();

    void setTheme (const Theme&);
    void setRange (float minimumDb, float maximumDb, int segmentCount);
    /** Level colours: segments at or above `hotDb` use attention, at or above `dangerDb` use danger. */
    void setZones (float hotDb, float dangerDb);
    void setLevel (float decibels, bool live);
    /** Lights the separate clip cell at the top of the column. */
    void setClip (bool clipped);

    /** In parent coordinates: the span from `maximum` (top) to `minimum` (bottom), for a printed scale. */
    juce::Rectangle<float> getScaleSpan() const;
    /** In parent coordinates: the centre of the clip cell. */
    juce::Point<float> getClipCentre() const;

    void paint (juce::Graphics&) override;

private:
    struct Layout
    {
        juce::Rectangle<float> clipCell, column;
        float segmentHeight = 0.0f;
    };

    Layout getLayout() const;
    juce::Colour colourFor (float segmentDb) const;

    Theme theme;
    float minimum = -60.0f, maximum = 0.0f, hot = -12.0f, danger = -3.0f, level = -100.0f;
    int segments = 24;
    bool live = false, clip = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LadderMeter)
};
} // namespace affine
