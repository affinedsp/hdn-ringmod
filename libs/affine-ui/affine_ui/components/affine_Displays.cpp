namespace affine
{
namespace render
{
void lamp (juce::Graphics& g, juce::Point<float> centre, float diameter, juce::Colour colour, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    const auto r = diameter * 0.5f;
    const auto lens = juce::Rectangle<float> (diameter, diameter).withCentre (centre);

    // Bezel ring pressed into the plate.
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillEllipse (lens.expanded (r * 0.38f));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (lens.expanded (r * 0.38f).translated (0.0f, 0.6f), 0.8f);

    halo (g, centre, r * 4.2f, colour, 0.34f * level);

    const auto unlit = colour.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.20f);
    const auto body = unlit.interpolatedWith (colour, level);
    juce::ColourGradient dome (body.interpolatedWith (juce::Colours::white, 0.35f * level + 0.08f), centre.x - r * 0.3f, centre.y - r * 0.4f,
                               body.darker (0.5f - 0.35f * level), centre.x + r * 0.5f, centre.y + r * 0.8f, true);
    g.setGradientFill (dome);
    g.fillEllipse (lens);

    // Specular glint on the lens.
    g.setColour (juce::Colours::white.withAlpha (0.45f + 0.25f * level));
    g.fillEllipse (juce::Rectangle<float> (r * 0.62f, r * 0.42f).withCentre (centre.translated (-r * 0.28f, -r * 0.40f)));
}

void indicator (juce::Graphics& g, juce::Point<float> centre, float diameter, juce::Colour colour, float level, const Palette& palette)
{
    if (palette.lamp == Palette::Lamp::jewel)
        jewel (g, centre, diameter, colour, level);
    else
        lamp (g, centre, diameter, colour, level);
}

void glass (juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette, float corner)
{
    juce::ColourGradient body (palette.glassTint, area.getX(), area.getY(),
                               palette.glass, area.getX(), area.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (area, corner);
    recess (g, area, corner, 0.45f);

    // A faint reflection across the upper half of the glass.
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (area, corner);
    g.reduceClipRegion (clip);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.045f), area.getX(), area.getY(),
                                juce::Colours::white.withAlpha (0.0f), area.getX(), area.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRect (area.withHeight (area.getHeight() * 0.5f));
}
} // namespace render

//==============================================================================
DisplayLabel::DisplayLabel() : font (fonts::readout (14.0f, 0.06f))
{
    setBorderSize (juce::BorderSize<int> (0));
    setInterceptsMouseClicks (true, false);
}

void DisplayLabel::setTheme (const Theme& t)
{
    theme = t;
    if (emission.isTransparent())
        emission = theme.palette.accent;
    repaint();
}

void DisplayLabel::setEmission (juce::Colour colour, float level)
{
    if (colour == emission && juce::approximatelyEqual (level, lampLevel))
        return;
    emission = colour;
    lampLevel = level;
    repaint();
}

void DisplayLabel::setLampColour (juce::Colour colour)
{
    lampColour = colour;
    repaint();
}

void DisplayLabel::setShowsLamp (bool shouldShow)
{
    showsLamp = shouldShow;
    repaint();
}

void DisplayLabel::setBacklit (bool shouldBeBacklit)
{
    backlit = shouldBeBacklit;
    repaint();
}

void DisplayLabel::setGlassVisible (bool shouldShow)
{
    glassVisible = shouldShow;
    repaint();
}

void DisplayLabel::setDisplayFont (const juce::Font& f)
{
    font = f;
    repaint();
}

void DisplayLabel::paint (juce::Graphics& g)
{
    if (backlit)
    {
        const auto window = getLocalBounds().toFloat().reduced (2.0f);
        render::litWindow (g, window, emission, lampLevel);
        g.setColour (theme.palette.readoutInk.withMultipliedAlpha (0.35f + 0.55f * lampLevel));
        g.setFont (font);
        g.drawText (getText().toUpperCase(), window.reduced (8.0f, 0.0f), getJustificationType(), false);
        return;
    }

    auto area = getLocalBounds().toFloat();
    if (glassVisible)
        render::glass (g, area, theme.palette);
    area = area.reduced (glassVisible ? 9.0f : 0.0f, 0.0f);

    const auto text = getText().toUpperCase();
    if (text.isEmpty() && ! glassVisible)
        return;

    const auto justification = getJustificationType();
    if (showsLamp)
    {
        // A right-aligned legend keeps its lamp immediately to the left of the text.
        auto lampSlot = area.removeFromLeft (12.0f);
        if (justification.testFlags (juce::Justification::right))
        {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (font, text, 0.0f, 0.0f);
            const auto textWidth = glyphs.getBoundingBox (0, -1, true).getWidth();
            lampSlot = lampSlot.withX (area.getRight() - textWidth - 19.0f);
        }
        render::indicator (g, lampSlot.getCentre(), 6.0f, lampColour.isTransparent() ? emission : lampColour, lampLevel, theme.palette);
        area.removeFromLeft (7.0f);
    }

    glow.draw (g, text, font, area, justification, emission.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f),
               2.4f, 0.55f + 0.45f * lampLevel);
}
} // namespace affine
