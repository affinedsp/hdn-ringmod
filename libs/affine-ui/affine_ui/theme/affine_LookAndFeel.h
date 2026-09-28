#pragma once

namespace affine
{
/** Menus, tooltips and text entry in the family style. Set it on the editor. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (const Theme& = {});
    void setTheme (const Theme&);

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    Theme theme;
};
} // namespace affine
