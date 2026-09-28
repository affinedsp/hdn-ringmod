namespace affine::screen
{
juce::Colour lit (juce::Colour screen, float level)
{
    return screen.interpolatedWith (juce::Colours::white, 0.2f).withMultipliedAlpha (level);
}

void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour screen,
              juce::Justification justification, float alpha)
{
    g.setColour (screen.withAlpha (alpha));
    g.setFont (fonts::label (12.0f, 0.22f));
    g.drawText (text, area, justification, false);
}

void segments (juce::Graphics& g, juce::Rectangle<float> area, int count, float fraction,
               const std::function<juce::Colour (float, bool)>& colourFor)
{
    const auto pitch = area.getWidth() / static_cast<float> (count);
    for (int i = 0; i < count; ++i)
    {
        const auto position = (static_cast<float> (i) + 0.5f) / static_cast<float> (count);
        g.setColour (colourFor (position, position <= fraction));
        g.fillRect (area.getX() + static_cast<float> (i) * pitch, area.getY(), pitch - 1.0f, area.getHeight());
    }
}
} // namespace affine::screen
