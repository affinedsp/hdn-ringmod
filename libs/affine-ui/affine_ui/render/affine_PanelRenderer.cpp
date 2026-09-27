namespace affine::render
{
using namespace shading;

juce::Image faceplate (juce::Rectangle<int> logicalBounds, float scale, const PanelFinish& finish)
{
    const auto width = juce::jmax (1, juce::roundToInt (static_cast<float> (logicalBounds.getWidth()) * scale));
    const auto height = juce::jmax (1, juce::roundToInt (static_cast<float> (logicalBounds.getHeight()) * scale));
    juce::Image image (juce::Image::ARGB, width, height, false);
    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

    const auto base = linear (finish.base);
    const auto inverseScale = 1.0f / scale;

    for (int y = 0; y < height; ++y)
    {
        const auto v = (static_cast<float> (y) + 0.5f) / static_cast<float> (height);
        const auto ly = (static_cast<float> (y) + 0.5f) * inverseScale;
        // Key light from above: the plate is brighter at the top and falls off gently.
        const auto vertical = mix (1.16f, 0.78f, smoothstep (-0.1f, 1.05f, v));

        for (int x = 0; x < width; ++x)
        {
            const auto u = (static_cast<float> (x) + 0.5f) / static_cast<float> (width);
            const auto lx = (static_cast<float> (x) + 0.5f) * inverseScale;
            const auto side = 1.0f - 0.12f * std::pow (std::abs (u - 0.46f) * 2.0f, 3.0f);
            const auto grain = (valueNoise (lx * 1.35f, ly * 1.35f, 3) - 0.5f) * finish.grain
                             + (hash (x, y, 9) - 0.5f) * finish.grain * 0.55f;
            const auto mottle = (fbm (lx / 95.0f, ly / 95.0f, 3, 17) - 0.5f) * finish.mottle;
            const auto brushed = finish.brushing > 0.0f
                ? ((fbm (lx / 220.0f, ly * 1.9f, 2, 23) - 0.5f) * 0.10f + (hash (0, y, 29) - 0.5f) * 0.04f) * finish.brushing
                : 0.0f;
            const auto sheenX = (u - 0.30f) / 0.62f, sheenY = (v + 0.12f) / 0.50f;
            const auto sheen = std::exp (-(sheenX * sheenX + sheenY * sheenY)) * finish.sheen * 0.060f;
            const auto light = vertical * side * (1.0f + grain + mottle + brushed);
            const auto colour = base * light + V3 { sheen, sheen, sheen * 1.04f };

            auto* pixel = reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y));
            pixel->setARGB (255,
                            static_cast<juce::uint8> (juce::roundToInt (toSrgb (colour.x) * 255.0f)),
                            static_cast<juce::uint8> (juce::roundToInt (toSrgb (colour.y) * 255.0f)),
                            static_cast<juce::uint8> (juce::roundToInt (toSrgb (colour.z) * 255.0f)));
        }
    }

    return image;
}

void screw (juce::Graphics& g, juce::Point<float> centre, float diameter, float rotation)
{
    const auto r = diameter * 0.5f;
    const auto head = juce::Rectangle<float> (diameter, diameter).withCentre (centre);

    // Countersink: the plate is cut slightly larger than the head.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (head.expanded (r * 0.14f).translated (0.0f, r * 0.05f));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (head.expanded (r * 0.16f).translated (0.0f, r * 0.10f), juce::jmax (0.6f, r * 0.08f));

    juce::ColourGradient dome (juce::Colour (0xffd9dcdf), centre.x - r * 0.45f, centre.y - r * 0.55f,
                               juce::Colour (0xff4a4f55), centre.x + r * 0.55f, centre.y + r * 0.75f, true);
    dome.addColour (0.45, juce::Colour (0xff9aa0a6));
    g.setGradientFill (dome);
    g.fillEllipse (head);

    // Hex socket, oriented per screw so a row of screws does not look stamped.
    juce::Path hex;
    const auto socket = r * 0.46f;
    for (int i = 0; i < 6; ++i)
    {
        const auto a = rotation + static_cast<float> (i) * juce::MathConstants<float>::twoPi / 6.0f;
        const auto p = centre + juce::Point<float> (std::cos (a), std::sin (a)) * socket;
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    juce::ColourGradient pit (juce::Colour (0xff0c0d0f), centre.x, centre.y - socket,
                              juce::Colour (0xff3b3f44), centre.x, centre.y + socket, false);
    g.setGradientFill (pit);
    g.fillPath (hex);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.strokePath (hex, juce::PathStrokeType (juce::jmax (0.5f, r * 0.05f)),
                  juce::AffineTransform::translation (0.0f, r * 0.04f));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (head, juce::jmax (0.5f, r * 0.06f));
}

void softShadow (juce::Graphics& g, juce::Rectangle<float> area, float corner, float blur,
                 juce::Point<float> offset, float opacity)
{
    juce::Path shape;
    shape.addRoundedRectangle (area.translated (offset.x, offset.y), corner);
    juce::DropShadow (juce::Colours::black.withAlpha (opacity), juce::jmax (1, juce::roundToInt (blur)), {})
        .drawForPath (g, shape);
}

void recess (juce::Graphics& g, juce::Rectangle<float> area, float corner, float depth)
{
    // Lit lower lip of the cut: the light comes from above.
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (area.expanded (0.5f).translated (0.0f, 0.8f), corner + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawRoundedRectangle (area.expanded (0.5f), corner + 0.5f, 1.0f);

    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (area, corner);
    g.reduceClipRegion (clip);

    // Inner shadow cast by the upper wall of the opening.
    const auto h = 10.0f * depth;
    juce::ColourGradient top (juce::Colours::black.withAlpha (0.55f), area.getX(), area.getY(),
                              juce::Colours::transparentBlack, area.getX(), area.getY() + h, false);
    g.setGradientFill (top);
    g.fillRect (area.withHeight (h));
    juce::ColourGradient left (juce::Colours::black.withAlpha (0.25f), area.getX(), area.getY(),
                               juce::Colours::transparentBlack, area.getX() + h * 0.6f, area.getY(), false);
    g.setGradientFill (left);
    g.fillRect (area.withWidth (h * 0.6f));
}
} // namespace affine::render
