namespace affine::render
{
using namespace shading;

namespace
{
V3 powderPixel (const PanelFinish& finish, V3 base, float u, float v, float lx, float ly, int x, int y) noexcept
{
    const auto vertical = mix (1.10f, 0.84f, smoothstep (-0.1f, 1.05f, v));
    const auto speckle = (hash (x, y, 41) - 0.5f) * finish.grain + (valueNoise (lx * 2.2f, ly * 2.2f, 43) - 0.5f) * finish.grain * 0.8f;
    const auto mottle = (fbm (lx / 70.0f, ly / 70.0f, 3, 47) - 0.5f) * finish.mottle;
    const auto sheenX = (u - 0.35f) / 0.7f, sheenY = (v + 0.2f) / 0.6f;
    const auto sheen = std::exp (-(sheenX * sheenX + sheenY * sheenY)) * finish.sheen * 0.02f;
    return base * (vertical * (1.0f + speckle + mottle)) + V3 { sheen, sheen, sheen };
}
} // namespace

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

        for (int x = 0; x < width; ++x)
        {
            const auto u = (static_cast<float> (x) + 0.5f) / static_cast<float> (width);
            const auto lx = (static_cast<float> (x) + 0.5f) * inverseScale;
            const auto colour = powderPixel (finish, base, u, v, lx, ly, x, y);
            auto* pixel = reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y));
            pixel->setARGB (255,
                            static_cast<juce::uint8> (juce::roundToInt (toSrgb (colour.x) * 255.0f)),
                            static_cast<juce::uint8> (juce::roundToInt (toSrgb (colour.y) * 255.0f)),
                            static_cast<juce::uint8> (juce::roundToInt (toSrgb (colour.z) * 255.0f)));
        }
    }

    return image;
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

void screenGlass (juce::Graphics& g, juce::Rectangle<float> area)
{
    softShadow (g, area, 6.0f, 6.0f, { 0.0f, 2.0f }, 0.6f);
    g.setColour (juce::Colour (0xff0b0c0e));
    g.fillRoundedRectangle (area.expanded (4.0f), 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (area.expanded (4.0f), 7.0f, 1.0f);
    juce::ColourGradient glass (juce::Colour (0xff0a0d10), area.getX(), area.getY(),
                                juce::Colour (0xff040506), area.getX(), area.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (area, 3.0f);
}
} // namespace affine::render
