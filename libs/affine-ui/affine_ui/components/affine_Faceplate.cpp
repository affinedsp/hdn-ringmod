namespace affine
{
void Faceplate::paint (juce::Graphics& g, juce::Rectangle<int> bounds, const Theme& theme, const Printer& print)
{
    const auto scale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! cache.isValid() || bounds != cachedBounds || ! juce::approximatelyEqual (scale, cachedScale))
    {
        cachedBounds = bounds;
        cachedScale = scale;
        cache = render::faceplate (bounds, scale, theme.panel);
        juce::Graphics layer (cache);
        layer.addTransform (juce::AffineTransform::scale (scale));
        layer.addTransform (juce::AffineTransform::translation (static_cast<float> (-bounds.getX()), static_cast<float> (-bounds.getY())));

        // Machined edge of the panel.
        const auto edge = bounds.toFloat();
        layer.setColour (juce::Colours::white.withAlpha (0.10f));
        layer.fillRect (edge.withHeight (1.0f));
        layer.setColour (juce::Colours::black.withAlpha (0.45f));
        layer.fillRect (edge.withTop (edge.getBottom() - 1.0f));

        if (print != nullptr)
            print (layer);
    }

    g.drawImageTransformed (cache, juce::AffineTransform::scale (1.0f / scale)
                                       .translated (static_cast<float> (bounds.getX()), static_cast<float> (bounds.getY())));
}

namespace silkscreen
{
void wordmark (juce::Graphics& g, const juce::String& product, const juce::String& descriptor,
               juce::Point<float> topLeft, const Palette& palette)
{
    g.setColour (palette.silkscreen);
    g.setFont (fonts::label (30.0f, 0.45f));
    g.drawText (product.toUpperCase(), juce::Rectangle<float> (topLeft.x, topLeft.y, 600.0f, 36.0f),
                juce::Justification::centredLeft, false);
    g.setColour (palette.silkscreenDim);
    g.setFont (fonts::label (11.5f, 0.3f));
    g.drawText (descriptor.toUpperCase(), juce::Rectangle<float> (topLeft.x + 2.0f, topLeft.y + 36.0f, 600.0f, 14.0f),
                juce::Justification::centredLeft, false);
}

void makersMark (juce::Graphics& g, juce::Point<float> baselineLeft, const Palette& palette)
{
    const auto colour = palette.silkscreenDim.withMultipliedAlpha (0.85f);
    // A unit square under a horizontal shear.
    juce::Path mark;
    const auto size = 9.0f, shear = 3.5f;
    const auto x = baselineLeft.x, y = baselineLeft.y;
    mark.startNewSubPath (x + shear, y - size);
    mark.lineTo (x + shear + size, y - size);
    mark.lineTo (x + size, y);
    mark.lineTo (x, y);
    mark.closeSubPath();
    g.setColour (colour);
    g.strokePath (mark, juce::PathStrokeType (1.2f, juce::PathStrokeType::mitered));
    g.setFont (fonts::wordmark (9.5f, 0.32f));
    g.drawText ("affine", juce::Rectangle<float> (x + size + shear + 6.0f, y - size - 2.0f, 120.0f, size + 4.0f),
                juce::Justification::centredLeft, false);
}

void section (juce::Graphics& g, const juce::String& title, juce::Rectangle<float> rule, const Palette& palette)
{
    const auto font = fonts::label (11.5f, 0.26f);
    const auto text = title.toUpperCase();
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, 0.0f, 0.0f);
    const auto textWidth = glyphs.getBoundingBox (0, -1, true).getWidth();
    g.setColour (palette.silkscreenDim);
    g.setFont (font);
    g.drawText (text, rule.withWidth (textWidth + 8.0f).withHeight (14.0f).translated (0.0f, -7.0f),
                juce::Justification::centredLeft, false);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRect (rule.withTrimmedLeft (textWidth + 14.0f).withHeight (1.0f));
}
} // namespace silkscreen
} // namespace affine
