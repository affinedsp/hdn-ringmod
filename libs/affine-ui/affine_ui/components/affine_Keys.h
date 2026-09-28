#pragma once

namespace affine
{
/**
    A latching rubber key with an LED strip that lights while it is engaged,
    and a printed legend below it. Space and Return operate it.
*/
class KeyButton : public juce::Button
{
public:
    explicit KeyButton (const juce::String& name);

    void setTheme (const Theme&);
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
    void paintGlyph (juce::Graphics&, juce::Rectangle<float> cap, juce::Colour, float width);
    void paintLegend (juce::Graphics&);

    Theme theme;
    juce::Colour lampColour;
    juce::String legendOne, legendTwo;
    juce::Path glyph;
    float keyWidth = 58.0f, keyHeight = 40.0f;
};

/**
    Keys bound to one choice parameter: one key per choice, the selected key
    lit. Every press is one complete host gesture.
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
