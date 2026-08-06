#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace PluginTheme
{
inline const juce::Colour canvas { 0xff0b0e11 };
inline const juce::Colour surface { 0xff12171c };
inline const juce::Colour surfaceRaised { 0xff171d23 };
inline const juce::Colour border { 0xff2a333c };
inline const juce::Colour primaryText { 0xffe8edf1 };
inline const juce::Colour secondaryText { 0xff89939d };
inline const juce::Colour accent { 0xffb8f24a };
inline const juce::Colour warning { 0xffe7a84b };
inline const juce::Colour inactiveControl { 0xff66717b };
inline constexpr float cornerRadius = 10.0f;

juce::Font makeFont(float size, bool semibold = false, float tracking = 0.0f);
}

class ParameterSlider final : public juce::Slider
{
public:
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
};

class PluginLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PluginLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosition, float startAngle, float endAngle,
                          juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                      juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
};
