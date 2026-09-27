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

        // Machined edge of the plate.
        const auto edge = bounds.toFloat();
        layer.setColour (juce::Colours::white.withAlpha (0.10f));
        layer.fillRect (edge.withHeight (1.0f));
        layer.setColour (juce::Colours::black.withAlpha (0.45f));
        layer.fillRect (edge.withTop (edge.getBottom() - 1.0f));

        silkscreen::screws (layer, edge);
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
    g.setFont (fonts::wordmark (21.0f, 0.18f));
    g.drawText (product.toUpperCase(), juce::Rectangle<float> (topLeft.x, topLeft.y, 520.0f, 30.0f),
                juce::Justification::centredLeft, false);
    g.setColour (palette.silkscreenDim);
    g.setFont (fonts::label (11.5f, 0.22f));
    g.drawText (descriptor.toUpperCase(), juce::Rectangle<float> (topLeft.x + 2.0f, topLeft.y + 30.0f, 520.0f, 16.0f),
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

void groove (juce::Graphics& g, float x1, float x2, float y)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (x1, y, x2 - x1, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRect (x1, y + 1.0f, x2 - x1, 1.0f);
}

void frame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const Palette& palette)
{
    const auto font = fonts::label (11.5f, 0.24f);
    const auto text = title.toUpperCase();
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, 0.0f, 0.0f);
    const auto titleWidth = glyphs.getBoundingBox (0, -1, true).getWidth() + 14.0f;
    const auto c = metrics::frameCorner;
    const auto gapStart = r.getX() + 16.0f, gapEnd = gapStart + titleWidth;

    juce::Path path;
    path.startNewSubPath (gapEnd, r.getY());
    path.lineTo (r.getRight() - c, r.getY());
    path.quadraticTo (r.getRight(), r.getY(), r.getRight(), r.getY() + c);
    path.lineTo (r.getRight(), r.getBottom() - c);
    path.quadraticTo (r.getRight(), r.getBottom(), r.getRight() - c, r.getBottom());
    path.lineTo (r.getX() + c, r.getBottom());
    path.quadraticTo (r.getX(), r.getBottom(), r.getX(), r.getBottom() - c);
    path.lineTo (r.getX(), r.getY() + c);
    path.quadraticTo (r.getX(), r.getY(), r.getX() + c, r.getY());
    path.lineTo (gapStart, r.getY());
    g.setColour (palette.silkscreen.withAlpha (0.30f));
    g.strokePath (path, juce::PathStrokeType (metrics::frameLine));

    g.setColour (palette.silkscreen.withAlpha (0.88f));
    g.setFont (font);
    g.drawText (text, juce::Rectangle<float> (gapStart, r.getY() - 8.0f, titleWidth, 16.0f), juce::Justification::centred, false);
}

void legend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const Palette& palette,
             juce::Justification justification, bool dim)
{
    g.setColour (dim ? palette.silkscreenDim : palette.silkscreen);
    g.setFont (fonts::label (11.5f, 0.24f));
    g.drawText (text.toUpperCase(), area, justification, false);
}

void screws (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto inset = metrics::screwInset;
    const juce::Point<float> corners[] {
        { bounds.getX() + inset, bounds.getY() + inset },
        { bounds.getRight() - inset, bounds.getY() + inset },
        { bounds.getX() + inset, bounds.getBottom() - inset },
        { bounds.getRight() - inset, bounds.getBottom() - inset },
    };
    // Each screw was driven home at its own angle.
    const float turns[] { 0.35f, 1.10f, 0.72f, 0.05f };
    for (int i = 0; i < 4; ++i)
        render::screw (g, corners[i], metrics::screwDiameter, turns[i]);
}
} // namespace silkscreen
} // namespace affine
