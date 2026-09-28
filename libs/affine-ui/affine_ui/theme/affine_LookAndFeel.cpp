namespace affine
{
namespace
{
constexpr float tooltipWidth = 280.0f;

juce::TextLayout tooltipLayout (const juce::String& text, juce::Colour colour)
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.append (text, fonts::text (15.0f), colour);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, tooltipWidth);
    return layout;
}
} // namespace

LookAndFeel::LookAndFeel (const Theme& t) { setTheme (t); }

void LookAndFeel::setTheme (const Theme& t)
{
    theme = t;
    const auto& p = theme.palette;
    const auto menu = juce::Colour (0xff121518);
    setColour (juce::PopupMenu::backgroundColourId, menu);
    setColour (juce::PopupMenu::textColourId, p.silkscreen);
    setColour (juce::PopupMenu::headerTextColourId, p.silkscreenDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, menu);
    setColour (juce::TooltipWindow::textColourId, p.silkscreen);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colours::white.withAlpha (0.12f));
    setColour (juce::TextEditor::backgroundColourId, p.glass);
    setColour (juce::TextEditor::textColourId, p.silkscreen);
    setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId, p.silkscreenDim);
    setColour (juce::TextEditor::focusedOutlineColourId, p.accent);
    setColour (juce::CaretComponent::caretColourId, p.accent);
    setColour (juce::Label::textColourId, p.silkscreen);
}

juce::Font LookAndFeel::getPopupMenuFont() { return fonts::text (16.0f); }

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height));
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.04f), 0.0f, 0.0f,
                                juce::Colours::transparentWhite, 0.0f, 40.0f, false);
    g.setGradientFill (sheen);
    g.fillRect (bounds.withHeight (40.0f));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRect (bounds, 1.0f);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                    juce::Rectangle<int> parentArea)
{
    const auto layout = tooltipLayout (tipText, juce::Colours::white);
    const auto w = static_cast<int> (layout.getWidth() + 22.0f);
    const auto h = static_cast<int> (layout.getHeight() + 16.0f);
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height));
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    tooltipLayout (text, findColour (juce::TooltipWindow::textColourId)).draw (g, bounds.reduced (11.0f, 8.0f));
}
} // namespace affine
