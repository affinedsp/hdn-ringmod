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
        float restPosition = 0.0f;  // where the needle rests without a reading: 0.5 for centre-zero meters
    };

    /** The printed card and the housing around it. */
    struct Face
    {
        enum class Bezel
        {
            moulded,  // black moulded housing standing proud of the plate
            chrome,   // black housing with a polished chrome ring, as on vintage panel meters
            flush     // behind a glass front: only a thin black frame
        };

        juce::Colour backlight { 0xfff2e4c4 };  // the lamp behind the card
        juce::Colour ink { 0xff1d1b18 };        // printing on the card
        juce::Colour zone { 0xffc23b2e };       // colour of the red zone
        float zoneFrom = 2.0f;                  // scale position where the red zone begins; above 1 for none
        bool mirror = true;                     // anti-parallax mirror band
        float vignette = 1.1f;                  // how much darker the card's corners are than its lit centre
        bool twinLamps = false;                 // two bulbs low behind the card instead of one
        Bezel bezel = Bezel::moulded;
        juce::String brand;                     // small print on the card, e.g. "VU"
    };

    NeedleMeter();
    ~NeedleMeter() override;

    void setTheme (const Theme&);
    void setScale (Scale);
    void setCaption (const juce::String&);
    void setFace (const Face&);

    /** Face backlight colour: the lamp behind the printed scale. */
    void setBacklight (juce::Colour);

    /** Movement: natural frequency in Hz and damping ratio. The default is a VU-like 2.1 Hz, 0.74. */
    void setBallistics (float frequencyHz, float damping);

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
    Face style;
    float naturalFrequency = 2.1f, dampingRatio = 0.74f;
    juce::Image face;
    float faceScale = 0.0f;
    float target = 0.0f, position = 0.0f, velocity = 0.0f, lamp = 0.0f, lampTarget = 0.0f;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeedleMeter)
};
} // namespace affine
