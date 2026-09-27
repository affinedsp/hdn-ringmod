#pragma once

namespace affine
{
namespace render
{
/** A panel LED behind a domed lens. `level` 0 is dark, 1 is fully lit. */
void lamp (juce::Graphics&, juce::Point<float> centre, float diameter, juce::Colour colour, float level);

/** A glass display window cut into the faceplate. */
void glass (juce::Graphics&, juce::Rectangle<float> area, const Palette&, float corner = metrics::displayCorner);
} // namespace render

/**
    A label that presents itself as a small glowing display with an optional
    status lamp. The text stays the accessible text; the display shows it in capitals.
*/
class DisplayLabel : public juce::Label
{
public:
    DisplayLabel();

    void setTheme (const Theme&);
    void setEmission (juce::Colour colour, float lampLevel);
    /** Lamp colour when it should differ from the text's emission. */
    void setLampColour (juce::Colour);
    void setShowsLamp (bool);
    void setGlassVisible (bool);
    void setDisplayFont (const juce::Font&);

    void paint (juce::Graphics&) override;

private:
    Theme theme;
    juce::Colour emission, lampColour;
    juce::Font font;
    float lampLevel = 0.0f;
    bool showsLamp = true, glassVisible = true;
    render::GlowText glow;
};
} // namespace affine
