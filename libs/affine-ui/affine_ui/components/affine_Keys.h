#pragma once

namespace affine
{
/**
    A latching key or switch for engaged/disengaged states. Space and Return
    operate it; the printed legend sits below the key (above and below a toggle).
*/
class KeyButton : public juce::Button
{
public:
    enum class Style
    {
        cap,           // anodised key cap with an LED window
        pianoKey,      // interlocking push key that stays down while engaged
        toggle,        // chrome bat-handle toggle: up is engaged
        silverSquare,  // polished square push button whose legend lights while engaged
        softKey        // rubber key with an LED strip
    };

    explicit KeyButton (const juce::String& name);

    void setTheme (const Theme&);
    void setStyle (Style);
    /** Colour of a piano key's top. */
    void setKeyColour (juce::Colour);
    void setLampColour (juce::Colour);
    void setLegend (const juce::String& lineOne, const juce::String& lineTwo = {});
    /** A glyph printed on the key cap, fitted inside its lower half. */
    void setGlyph (const juce::Path&);
    /** Maximum key cap size; the key is centred above the legend. */
    void setKeySize (float width, float height);
    juce::Rectangle<float> getKeyBounds() const;

    /** Arrow keys, for moving within a group of keys: -1 or +1. */
    std::function<void (int)> onArrow;

    bool keyPressed (const juce::KeyPress&) override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    void paintCap (juce::Graphics&, bool highlighted, bool down);
    void paintPianoKey (juce::Graphics&, bool highlighted, bool down);
    void paintToggle (juce::Graphics&, bool highlighted, bool down);
    void paintSilverSquare (juce::Graphics&, bool highlighted, bool down);
    void paintSoftKey (juce::Graphics&, bool highlighted, bool down);
    void paintGlyph (juce::Graphics&, juce::Rectangle<float> cap, juce::Colour, float width);
    void paintLegend (juce::Graphics&, bool lit);

    Theme theme;
    Style style = Style::cap;
    juce::Colour lampColour, keyColour { 0xffeae3d0 };
    juce::String legendOne, legendTwo;
    juce::Path glyph;
    float keyWidth = 58.0f, keyHeight = 40.0f;
};

/**
    Illuminated keys bound to one choice parameter: one key per choice, the
    selected key lit. Every press is one complete host gesture.
*/
class SelectorKeys : public juce::Component
{
public:
    SelectorKeys (juce::AudioProcessorValueTreeState&, const juce::String& parameterID);
    ~SelectorKeys() override;

    juce::RangedAudioParameter& parameter;

    void setTheme (const Theme&);
    void setLegends (const juce::StringArray&);
    void setGlyphs (const std::vector<juce::Path>&);
    void setKeySize (float width, float height);
    void setStyle (KeyButton::Style);
    void setKeyColour (juce::Colour);
    /** Keys are laid out left to right, wrapping after this many columns. */
    void setColumns (int);
    int getSelectedIndex() const noexcept { return selected; }
    KeyButton* getKey (int index) const { return keys[index]; }
    int getNumKeys() const noexcept { return keys.size(); }

    std::function<void (int)> onChange;

    void resized() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    void select (int index);

    juce::OwnedArray<KeyButton> keys;
    int selected = -1, columns = 0;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectorKeys)
};

/** Unit-square glyphs for common oscillator shapes, for stroking. */
namespace glyphs
{
juce::Path sine();
juce::Path triangle();
juce::Path square();
juce::Path saw();
} // namespace glyphs
} // namespace affine
