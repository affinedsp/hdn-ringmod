namespace affine
{
LadderMeter::LadderMeter()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

void LadderMeter::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void LadderMeter::setRange (float minimumDb, float maximumDb, int segmentCount)
{
    minimum = minimumDb;
    maximum = maximumDb;
    segments = juce::jmax (2, segmentCount);
    repaint();
}

void LadderMeter::setZones (float hotDb, float dangerDb)
{
    hot = hotDb;
    danger = dangerDb;
    repaint();
}

void LadderMeter::setLevel (float decibels, bool isLive)
{
    if (juce::approximatelyEqual (decibels, level) && isLive == live)
        return;
    level = decibels;
    live = isLive;
    repaint();
}

void LadderMeter::setClip (bool clipped)
{
    if (clip == clipped)
        return;
    clip = clipped;
    repaint();
}

juce::Colour LadderMeter::colourFor (float segmentDb) const
{
    if (segmentDb >= danger)
        return theme.palette.danger;
    if (segmentDb >= hot)
        return theme.palette.attention;
    return theme.palette.accent;
}

namespace
{
constexpr float segmentGap = 2.0f;
}

LadderMeter::Layout LadderMeter::getLayout() const
{
    // The clip cell sits apart at the top; the level segments fill the rest of the column.
    auto inner = getLocalBounds().toFloat().reduced (4.0f, 5.0f);
    Layout layout;
    const auto cell = (inner.getHeight() - segmentGap * static_cast<float> (segments)) / static_cast<float> (segments + 1);
    layout.clipCell = inner.removeFromTop (cell);
    inner.removeFromTop (segmentGap * 2.0f);
    layout.column = inner;
    layout.segmentHeight = (inner.getHeight() - segmentGap * static_cast<float> (segments - 1)) / static_cast<float> (segments);
    return layout;
}

void LadderMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    render::glass (g, bounds, theme.palette);

    const auto layout = getLayout();
    const auto step = (maximum - minimum) / static_cast<float> (segments);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (bounds.reduced (1.0f).getSmallestIntegerContainer());

    const auto paintSegment = [&] (juce::Rectangle<float> segment, juce::Colour colour, float lit)
    {
        const auto unlit = colour.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.16f);
        g.setColour (unlit);
        g.fillRoundedRectangle (segment, 1.2f);
        if (lit <= 0.0f)
            return;
        // Light scatters in the diffuser: a wide faint bloom, a tight halo, then the lit lens.
        g.setColour (colour.withAlpha (0.07f * lit));
        g.fillRoundedRectangle (segment.expanded (4.0f, 4.0f), 5.0f);
        g.setColour (colour.withAlpha (0.16f * lit));
        g.fillRoundedRectangle (segment.expanded (2.0f, 1.6f), 3.0f);
        juce::ColourGradient diffuser (unlit.interpolatedWith (colour.brighter (0.25f), lit), segment.getCentreX(), segment.getY(),
                                       unlit.interpolatedWith (colour.darker (0.15f), lit), segment.getCentreX(), segment.getBottom(), false);
        g.setGradientFill (diffuser);
        g.fillRoundedRectangle (segment, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.35f * lit));
        g.fillRoundedRectangle (segment.withHeight (juce::jmin (1.2f, segment.getHeight() * 0.4f)).reduced (1.0f, 0.0f), 0.6f);
    };

    paintSegment (layout.clipCell, theme.palette.danger, clip ? 1.0f : 0.0f);

    for (int i = 0; i < segments; ++i)
    {
        const auto lower = minimum + step * static_cast<float> (i);
        const auto y = layout.column.getBottom() - static_cast<float> (i + 1) * layout.segmentHeight - static_cast<float> (i) * segmentGap;
        const auto lit = live ? juce::jlimit (0.0f, 1.0f, (level - lower) / step) : 0.0f;
        paintSegment ({ layout.column.getX(), y, layout.column.getWidth(), layout.segmentHeight }, colourFor (lower + step * 0.5f), lit);
    }
}

juce::Rectangle<float> LadderMeter::getScaleSpan() const
{
    return getLayout().column.translated (static_cast<float> (getX()), static_cast<float> (getY()));
}

juce::Point<float> LadderMeter::getClipCentre() const
{
    return getLayout().clipCell.getCentre().translated (static_cast<float> (getX()), static_cast<float> (getY()));
}
} // namespace affine
