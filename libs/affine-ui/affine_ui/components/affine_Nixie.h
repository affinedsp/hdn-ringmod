#pragma once

namespace affine
{
/**
    A row of Nixie tubes, as on a laboratory frequency counter. Each tube holds
    the ten numeral cathodes stacked behind a honeycomb anode; unlit cathodes stay
    faintly visible, exactly as in the real part.

    Text is right-aligned into the tubes. Supported characters: 0-9, '.', '-', ' '.
    A '.' lights the decimal cathode of the preceding tube.
*/
class NixieDisplay : public juce::Component
{
public:
    NixieDisplay();

    void setTheme (const Theme&);
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
    void renderSprites (float scale, juce::Rectangle<float> glyphArea);

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
    std::array<Sprite, 11> sprites; // 0-9 and '-'
    float spriteScale = 0.0f;
    int spriteMargin = 0;
    juce::Rectangle<float> spriteArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NixieDisplay)
};
} // namespace affine
