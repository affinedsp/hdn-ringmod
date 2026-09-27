#pragma once

namespace affine
{
/**
    A backlit moving-coil panel meter with a mirror scale.

    The needle only ever shows a value the product supplied; the ballistics
    are a visual spring-damper and never feed back into processing. When the
    reading is not live the lamp dims and the needle returns to rest.
*/
class NeedleMeter : public juce::Component, private juce::Timer
{
public:
    struct Scale
    {
        std::function<float (float)> toPosition;   // value -> 0..1 along the arc
        std::vector<float> majors, minors;
        std::function<juce::String (float)> format;
        juce::String unit, caption;
    };

    NeedleMeter();
    ~NeedleMeter() override;

    void setTheme (const Theme&);
    void setScale (Scale);
    void setCaption (const juce::String&);

    /** Face backlight colour: the lamp behind the printed scale. */
    void setBacklight (juce::Colour);

    /** Sets the reading. When `live` is false the needle rests and the lamp dims. */
    void setReading (float value, bool live);

    /** Skips the remaining ballistics, e.g. before a still capture. */
    void settle();

    float getDisplayedPosition() const noexcept { return position; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderFace (float scale);
    juce::Point<float> pivot() const;
    float arcRadius() const;
    float angleFor (float position) const;
    juce::Rectangle<float> faceBounds() const;

    Theme theme;
    Scale scale;
    juce::Colour backlight { 0xfff3ead6 };
    juce::Image face;
    float faceScale = 0.0f;
    float target = 0.0f, position = 0.0f, velocity = 0.0f, lamp = 0.0f, lampTarget = 0.0f;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeedleMeter)
};
} // namespace affine
