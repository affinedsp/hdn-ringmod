#include <catch2/catch_test_macros.hpp>
#include "PluginLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace
{
float relativeLuminance(juce::Colour colour)
{
    auto linearise = [](float value)
    {
        return value <= 0.04045f ? value / 12.92f
                                 : std::pow((value + 0.055f) / 1.055f, 2.4f);
    };

    return 0.2126f * linearise(colour.getFloatRed())
         + 0.7152f * linearise(colour.getFloatGreen())
         + 0.0722f * linearise(colour.getFloatBlue());
}

float contrastRatio(juce::Colour first, juce::Colour second)
{
    auto lighter = std::max(relativeLuminance(first), relativeLuminance(second));
    auto darker = std::min(relativeLuminance(first), relativeLuminance(second));
    return (lighter + 0.05f) / (darker + 0.05f);
}

int countPixelsNear(juce::Image& image, juce::Colour target)
{
    int count = 0;
    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            auto pixel = image.getPixelAt(x, y);
            auto distance = std::abs(pixel.getFloatRed() - target.getFloatRed())
                          + std::abs(pixel.getFloatGreen() - target.getFloatGreen())
                          + std::abs(pixel.getFloatBlue() - target.getFloatBlue());
            if (distance < 0.08f)
                ++count;
        }
    }
    return count;
}
}

TEST_CASE("Plugin theme: interface colours retain readable contrast")
{
    REQUIRE(contrastRatio(PluginTheme::primaryText, PluginTheme::canvas) >= 4.5f);
    REQUIRE(contrastRatio(PluginTheme::secondaryText, PluginTheme::surface) >= 4.5f);
    REQUIRE(contrastRatio(PluginTheme::accent, PluginTheme::canvas) >= 4.5f);
}

TEST_CASE("Plugin look and feel: rotary value is visible as a proportional accent arc")
{
    PluginLookAndFeel lookAndFeel;
    juce::Slider slider;
    slider.setLookAndFeel(&lookAndFeel);

    auto render = [&](float position)
    {
        juce::Image image(juce::Image::RGB, 120, 120, true);
        juce::Graphics graphics(image);
        lookAndFeel.drawRotarySlider(graphics, 10, 10, 100, 100, position,
                                     juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f,
                                     slider);
        return image;
    };

    auto minimum = render(0.0f);
    auto maximum = render(1.0f);

    REQUIRE(countPixelsNear(maximum, PluginTheme::accent)
            > countPixelsNear(minimum, PluginTheme::accent) + 100);
}

TEST_CASE("Plugin look and feel: rotary value field is centred without shrinking the dial")
{
    PluginLookAndFeel lookAndFeel;
    juce::Slider slider;
    slider.setBounds(0, 0, 120, 140);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);

    auto layout = lookAndFeel.getSliderLayout(slider);

    REQUIRE(layout.sliderBounds == slider.getLocalBounds());
    REQUIRE(layout.textBoxBounds.getCentre() == slider.getLocalBounds().getCentre());

    std::unique_ptr<juce::Label> valueLabel(lookAndFeel.createSliderTextBox(slider));
    REQUIRE(valueLabel->findColour(juce::Label::outlineColourId).isTransparent());
    REQUIRE(valueLabel->findColour(juce::Label::backgroundWhenEditingColourId).isTransparent());
    REQUIRE(valueLabel->findColour(juce::Label::outlineWhenEditingColourId).isTransparent());
    REQUIRE(valueLabel->findColour(juce::TextEditor::focusedOutlineColourId).isTransparent());

    bool labelInterceptsClicks = true;
    bool editorInterceptsClicks = false;
    valueLabel->getInterceptsMouseClicks(labelInterceptsClicks, editorInterceptsClicks);
    REQUIRE_FALSE(labelInterceptsClicks);
    REQUIRE(editorInterceptsClicks);
}

TEST_CASE("Plugin theme: bundled display font is available in both weights")
{
    auto regular = PluginTheme::makeFont(18.0f);
    auto semibold = PluginTheme::makeFont(18.0f, true);

    REQUIRE(regular.getTypefaceName() == "Barlow Condensed");
    REQUIRE(semibold.getTypefaceName() == "Barlow Condensed");
    REQUIRE(regular.getTypefaceStyle() != semibold.getTypefaceStyle());
}

TEST_CASE("Parameter slider: double click opens exact value entry")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PluginLookAndFeel lookAndFeel;
    ParameterSlider slider;
    slider.setLookAndFeel(&lookAndFeel);
    slider.setBounds(0, 0, 120, 140);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);

    auto now = juce::Time::getCurrentTime();
    juce::MouseEvent doubleClick(
        juce::Desktop::getInstance().getMainMouseSource(), { 60.0f, 70.0f },
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f, &slider, &slider, now, { 60.0f, 70.0f },
        now, 2, false);

    slider.mouseDoubleClick(doubleClick);

    auto* valueLabel = dynamic_cast<juce::Label*>(slider.getChildComponent(0));
    REQUIRE(valueLabel != nullptr);
    REQUIRE(valueLabel->isBeingEdited());
    slider.hideTextBox(true);
}

TEST_CASE("Parameter slider: inherited theme configures the live value field")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PluginLookAndFeel lookAndFeel;
    juce::Component parent;
    ParameterSlider slider;
    parent.setLookAndFeel(&lookAndFeel);
    parent.setBounds(0, 0, 120, 140);
    parent.addAndMakeVisible(slider);
    slider.setBounds(parent.getLocalBounds());
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);

    auto* valueLabel = dynamic_cast<juce::Label*>(slider.getChildComponent(0));
    REQUIRE(valueLabel != nullptr);
    REQUIRE(valueLabel->getFont().getTypefaceName() == "Barlow Condensed");
    REQUIRE(valueLabel->findColour(juce::Label::outlineColourId).isTransparent());

    bool labelInterceptsClicks = true;
    bool editorInterceptsClicks = false;
    valueLabel->getInterceptsMouseClicks(labelInterceptsClicks, editorInterceptsClicks);
    REQUIRE_FALSE(labelInterceptsClicks);
    REQUIRE(editorInterceptsClicks);

    auto valueCentre = lookAndFeel.getSliderLayout(slider).textBoxBounds.getCentre();
    REQUIRE(slider.getComponentAt(valueCentre) == &slider);
}
