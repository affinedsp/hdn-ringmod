#include "PluginLookAndFeel.h"
#include "HdnRingmodAssets.h"

namespace
{
const juce::Typeface::Ptr& regularTypeface()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        HdnRingmodAssets::BarlowCondensedRegular_ttf,
        HdnRingmodAssets::BarlowCondensedRegular_ttfSize);
    return typeface;
}

const juce::Typeface::Ptr& semiboldTypeface()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        HdnRingmodAssets::BarlowCondensedSemiBold_ttf,
        HdnRingmodAssets::BarlowCondensedSemiBold_ttfSize);
    return typeface;
}
}

juce::Font PluginTheme::makeFont(float size, bool semibold, float tracking)
{
    auto typeface = semibold ? semiboldTypeface() : regularTypeface();
    return juce::Font(juce::FontOptions(typeface)
                          .withHeight(size)
                          .withKerningFactor(tracking));
}

void ParameterSlider::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (isEnabled() && isTextBoxEditable() && !event.mods.isPopupMenu())
        showTextBox();
}

bool ParameterSlider::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey && isEnabled() && isTextBoxEditable())
    {
        showTextBox();
        return true;
    }

    return juce::Slider::keyPressed(key);
}

PluginLookAndFeel::PluginLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, PluginTheme::accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, PluginTheme::border);
    setColour(juce::Slider::textBoxTextColourId, PluginTheme::primaryText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, PluginTheme::accent.withAlpha(0.25f));
    setColour(juce::ComboBox::textColourId, PluginTheme::primaryText);
    setColour(juce::ComboBox::backgroundColourId, PluginTheme::surfaceRaised);
    setColour(juce::ComboBox::outlineColourId, PluginTheme::border);
    setColour(juce::ComboBox::arrowColourId, PluginTheme::secondaryText);
    setColour(juce::ComboBox::focusedOutlineColourId, PluginTheme::accent);
    setColour(juce::PopupMenu::backgroundColourId, PluginTheme::surfaceRaised);
    setColour(juce::PopupMenu::textColourId, PluginTheme::primaryText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, PluginTheme::accent);
    setColour(juce::PopupMenu::highlightedTextColourId, PluginTheme::canvas);
    setColour(juce::Label::textColourId, PluginTheme::primaryText);
    setColour(juce::Label::textWhenEditingColourId, PluginTheme::primaryText);
    setColour(juce::Label::backgroundWhenEditingColourId, PluginTheme::surfaceRaised);
    setColour(juce::Label::outlineWhenEditingColourId, PluginTheme::accent);
}

void PluginLookAndFeel::drawRotarySlider(juce::Graphics& graphics, int x, int y,
                                         int width, int height, float sliderPosition,
                                         float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    auto size = static_cast<float>(juce::jmin(width, height));
    auto radius = size * 0.5f - 7.0f;
    auto centre = juce::Point<float>(static_cast<float>(x) + static_cast<float>(width) * 0.5f,
                                     static_cast<float>(y) + static_cast<float>(height) * 0.5f);
    auto angle = juce::jmap(sliderPosition, 0.0f, 1.0f, startAngle, endAngle);
    auto alpha = slider.isEnabled() ? 1.0f : 0.35f;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                        startAngle, endAngle, true);

    graphics.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId)
                           .withMultipliedAlpha(alpha));
    graphics.strokePath(track, juce::PathStrokeType(4.0f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

    if (sliderPosition > 0.0f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                            startAngle, angle, true);
        graphics.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId)
                               .withMultipliedAlpha(alpha));
        graphics.strokePath(value, juce::PathStrokeType(4.0f,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    }

    auto innerRadius = radius - 10.0f;
    graphics.setColour(PluginTheme::surfaceRaised.withMultipliedAlpha(alpha));
    graphics.fillEllipse(centre.x - innerRadius, centre.y - innerRadius,
                         innerRadius * 2.0f, innerRadius * 2.0f);
    graphics.setColour((slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true)
                            ? PluginTheme::secondaryText
                            : PluginTheme::border)
                           .withMultipliedAlpha(alpha));
    graphics.drawEllipse(centre.x - innerRadius, centre.y - innerRadius,
                         innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    auto marker = centre.getPointOnCircumference(innerRadius - 5.0f, angle);
    graphics.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId)
                           .withMultipliedAlpha(alpha));
    graphics.fillEllipse(marker.x - 2.5f, marker.y - 2.5f, 5.0f, 5.0f);
}

juce::Label* PluginLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(PluginTheme::makeFont(15.0f, true));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
    label->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    label->setInterceptsMouseClicks(false, true);
    return label;
}

juce::Slider::SliderLayout PluginLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    auto bounds = slider.getLocalBounds();
    layout.sliderBounds = bounds;

    if (slider.getTextBoxPosition() != juce::Slider::NoTextBox)
    {
        auto width = juce::jmin(slider.getTextBoxWidth(), bounds.getWidth() - 4);
        auto height = juce::jmin(slider.getTextBoxHeight(), bounds.getHeight() - 4);
        layout.textBoxBounds = juce::Rectangle<int>(width, height).withCentre(bounds.getCentre());
    }

    return layout;
}

void PluginLookAndFeel::drawComboBox(juce::Graphics& graphics, int width, int height,
                                     bool isButtonDown, int, int, int, int,
                                     juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f,
                                         static_cast<float>(width) - 1.0f,
                                         static_cast<float>(height) - 1.0f);
    graphics.setColour(PluginTheme::surfaceRaised);
    graphics.fillRoundedRectangle(bounds, 7.0f);

    auto outline = box.hasKeyboardFocus(false) || isButtonDown
                       ? PluginTheme::accent
                       : (box.isMouseOver() ? PluginTheme::secondaryText : PluginTheme::border);
    graphics.setColour(outline);
    graphics.drawRoundedRectangle(bounds, 7.0f, 1.0f);

    auto arrowCentreX = static_cast<float>(width - 17);
    auto arrowCentreY = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(arrowCentreX - 4.0f, arrowCentreY - 2.0f);
    arrow.lineTo(arrowCentreX, arrowCentreY + 2.0f);
    arrow.lineTo(arrowCentreX + 4.0f, arrowCentreY - 2.0f);
    graphics.setColour(PluginTheme::secondaryText);
    graphics.strokePath(arrow, juce::PathStrokeType(1.5f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
}

juce::Font PluginLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return PluginTheme::makeFont(15.0f);
}

juce::Font PluginLookAndFeel::getPopupMenuFont()
{
    return PluginTheme::makeFont(15.0f);
}

void PluginLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(12, 1, box.getWidth() - 38, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}
