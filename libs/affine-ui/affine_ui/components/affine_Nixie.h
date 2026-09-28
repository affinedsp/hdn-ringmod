#pragma once

namespace affine
{
/**
    A row of Nixie tubes, as on a laboratory frequency counter. Each tube holds
    the ten numeral cathodes stacked behind a honeycomb anode; unlit cathodes stay
    faintly visible, exactly as in the real part.

    Numeral tubes right-align their text and support 0-9, '.', '-' and ' ';
    a '.' lights the decimal cathode of the preceding tube. Alphanumeric tubes
    carry fourteen neon segments, as in the B-7971, and left-align their text.
*/
class NixieDisplay : public juce::Component
{
public:
    enum class Characters { numerals, alphanumeric };

    NixieDisplay();

    void setTheme (const Theme&);
    void setCharacters (Characters);
    void setNumTubes (int);
    void setText (const juce::String&);
    /** 0 = tubes dark (no reading), 1 = fully lit. */
    void setIntensity (float);

    const juce::String& getText() const noexcept { return text; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell
    {
        juce::juce_wchar digit = ' ';
        bool point = false;
    };

    std::vector<Cell> layoutCells() const;
    juce::Rectangle<float> tubeBounds (int index) const;
    void renderTubes (float scale);
    void renderFront (float scale);

    Theme theme;
    int tubes = 5;
    juce::String text;
    float intensity = 1.0f;

    juce::Image glassLayer, frontLayer;
    float glassScale = 0.0f;

    struct Sprite
    {
        juce::Image crisp, glow;
    };
    const Sprite& spriteFor (juce::juce_wchar, float scale, juce::Rectangle<float> glyphArea);

    Characters characters = Characters::numerals;
    std::map<juce::juce_wchar, Sprite> sprites;
    float spriteScale = 0.0f;
    int spriteMargin = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NixieDisplay)
};
} // namespace affine
