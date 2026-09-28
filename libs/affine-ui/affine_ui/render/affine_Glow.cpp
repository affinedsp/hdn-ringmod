namespace affine::render
{
namespace
{
void boxPass (const juce::uint8* src, juce::uint8* dst, int count, int stride, int radius)
{
    if (radius <= 0)
    {
        for (int i = 0; i < count; ++i)
            dst[i * stride] = src[i * stride];
        return;
    }

    const auto window = radius * 2 + 1;
    int sum = 0;
    for (int i = -radius; i <= radius; ++i)
        sum += src[juce::jlimit (0, count - 1, i) * stride];

    for (int i = 0; i < count; ++i)
    {
        dst[i * stride] = static_cast<juce::uint8> ((sum + window / 2) / window);
        const auto add = juce::jmin (count - 1, i + radius + 1);
        const auto remove = juce::jmax (0, i - radius);
        sum += src[add * stride] - src[remove * stride];
    }
}
} // namespace

void blurAlpha (juce::Image& image, float radiusPx)
{
    jassert (image.getFormat() == juce::Image::SingleChannel);
    const auto width = image.getWidth(), height = image.getHeight();
    if (width == 0 || height == 0 || radiusPx < 0.5f)
        return;

    // Three box passes of this width approximate a Gaussian with sigma ~= radius / 2.
    const auto box = juce::jmax (1, juce::roundToInt (radiusPx * 0.58f));
    juce::Image::BitmapData data (image, juce::Image::BitmapData::readWrite);
    std::vector<juce::uint8> line (static_cast<size_t> (juce::jmax (width, height)));
    std::vector<juce::uint8> temp (line.size());

    for (int pass = 0; pass < 3; ++pass)
    {
        for (int y = 0; y < height; ++y)
        {
            auto* row = data.getLinePointer (y);
            for (int x = 0; x < width; ++x)
                line[static_cast<size_t> (x)] = row[x * data.pixelStride];
            boxPass (line.data(), temp.data(), width, 1, box);
            for (int x = 0; x < width; ++x)
                row[x * data.pixelStride] = temp[static_cast<size_t> (x)];
        }

        for (int x = 0; x < width; ++x)
        {
            for (int y = 0; y < height; ++y)
                line[static_cast<size_t> (y)] = *data.getPixelPointer (x, y);
            boxPass (line.data(), temp.data(), height, 1, box);
            for (int y = 0; y < height; ++y)
                *data.getPixelPointer (x, y) = temp[static_cast<size_t> (y)];
        }
    }
}

void GlowLayer::draw (juce::Graphics& g, const juce::String& key, juce::Rectangle<float> area, juce::Colour colour,
                      float glowRadius, float glowStrength, const Painter& painter)
{
    const auto scale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto localArea = area.withZeroOrigin();

    if (key != cachedKey || localArea != cachedArea || ! juce::approximatelyEqual (scale, cachedScale)
        || ! juce::approximatelyEqual (glowRadius, cachedRadius) || ! crisp.isValid())
    {
        cachedKey = key;
        cachedArea = localArea;
        cachedScale = scale;
        cachedRadius = glowRadius;
        margin = juce::roundToInt (glowRadius * 2.0f * scale) + 2;

        const auto w = juce::roundToInt (area.getWidth() * scale) + margin * 2;
        const auto h = juce::roundToInt (area.getHeight() * scale) + margin * 2;
        crisp = juce::Image (juce::Image::SingleChannel, juce::jmax (1, w), juce::jmax (1, h), true);
        {
            juce::Graphics cg (crisp);
            cg.addTransform (juce::AffineTransform::scale (scale).translated (static_cast<float> (margin), static_cast<float> (margin)));
            cg.setColour (juce::Colours::white);
            painter (cg);
        }
        glow = crisp.createCopy();
        blurAlpha (glow, glowRadius * scale);
    }

    const auto transform = juce::AffineTransform::translation (static_cast<float> (-margin), static_cast<float> (-margin))
                               .scaled (1.0f / scale)
                               .translated (area.getX(), area.getY());
    g.setColour (colour.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, glowStrength)));
    g.drawImageTransformed (glow, transform, true);
    if (glowStrength > 1.0f)
    {
        g.setColour (colour.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, glowStrength - 1.0f)));
        g.drawImageTransformed (glow, transform, true);
    }
    g.setColour (colour.interpolatedWith (juce::Colours::white, 0.25f));
    g.drawImageTransformed (crisp, transform, true);
}

void GlowText::draw (juce::Graphics& g, const juce::String& text, const juce::Font& font, juce::Rectangle<float> area,
                     juce::Justification justification, juce::Colour colour, float glowRadius, float glowStrength)
{
    if (text.isEmpty())
        return;
    const auto key = text + "|" + font.getTypefaceName() + "|" + juce::String (font.getHeight(), 2)
                   + "|" + juce::String (justification.getFlags());
    const auto local = area.withZeroOrigin();
    layer.draw (g, key, area, colour, glowRadius, glowStrength, [&] (juce::Graphics& lg)
    {
        lg.setFont (font);
        lg.drawText (text, local, justification, false);
    });
}

void halo (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, float intensity)
{
    if (intensity <= 0.0f)
        return;
    juce::ColourGradient gradient (colour.withMultipliedAlpha (intensity), centre,
                                   colour.withAlpha (0.0f), centre.translated (radius, 0.0f), true);
    gradient.addColour (0.25, colour.withMultipliedAlpha (intensity * 0.55f));
    gradient.addColour (0.55, colour.withMultipliedAlpha (intensity * 0.16f));
    g.setGradientFill (gradient);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
}
} // namespace affine::render
