namespace affine
{
namespace
{
constexpr float tubeGap = 5.0f;

juce::Rectangle<float> glyphAreaIn (juce::Rectangle<float> tube)
{
    return tube.reduced (tube.getWidth() * 0.17f, 0.0f)
               .withTrimmedTop (tube.getHeight() * 0.20f)
               .withTrimmedBottom (tube.getHeight() * 0.19f);
}

int spriteIndex (juce::juce_wchar c)
{
    if (c >= '0' && c <= '9')
        return static_cast<int> (c - '0');
    if (c == '-')
        return 10;
    return -1;
}

juce::Path rawGlyph (juce::juce_wchar c)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (fonts::nixie (100.0f), juce::String::charToString (c), 0.0f, 0.0f);
    juce::Path path;
    glyphs.createPath (path);
    return path;
}

// Numeral cathodes share one height and baseline; each is centred in the tube.
juce::Path cathode (juce::juce_wchar c, juce::Rectangle<float> area)
{
    static const auto reference = rawGlyph ('0').getBounds();
    auto path = rawGlyph (c);
    const auto bounds = path.getBounds();
    const auto sy = area.getHeight() / reference.getHeight();
    const auto sx = juce::jmin (sy, area.getWidth() / reference.getWidth());
    path.applyTransform (juce::AffineTransform::translation (-bounds.getCentreX(), -reference.getY())
                             .scaled (sx, sy)
                             .translated (area.getCentreX(), area.getY()));
    return path;
}

void drawGlyph (juce::Graphics& g, juce::juce_wchar c, juce::Rectangle<float> area)
{
    g.fillPath (cathode (c, area));
}
} // namespace

NixieDisplay::NixieDisplay()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

void NixieDisplay::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void NixieDisplay::setNumTubes (int n)
{
    tubes = juce::jmax (1, n);
    glassLayer = {};
    sprites = {};
    repaint();
}

void NixieDisplay::setText (const juce::String& newText)
{
    if (newText == text)
        return;
    text = newText;
    repaint();
}

void NixieDisplay::setIntensity (float i)
{
    i = juce::jlimit (0.0f, 1.0f, i);
    if (juce::approximatelyEqual (i, intensity))
        return;
    intensity = i;
    repaint();
}

void NixieDisplay::resized()
{
    glassLayer = {};
    sprites = {};
}

juce::Rectangle<float> NixieDisplay::tubeBounds (int index) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto width = (bounds.getWidth() - tubeGap * static_cast<float> (tubes - 1)) / static_cast<float> (tubes);
    return { bounds.getX() + static_cast<float> (index) * (width + tubeGap), bounds.getY(), width, bounds.getHeight() };
}

std::vector<NixieDisplay::Cell> NixieDisplay::layoutCells() const
{
    std::vector<Cell> cells;
    for (auto c : text)
    {
        if (c == '.')
        {
            if (! cells.empty())
                cells.back().point = true;
            continue;
        }
        cells.push_back ({ c, false });
    }
    // Right-align into the tubes, dropping anything that does not fit.
    std::vector<Cell> placed (static_cast<size_t> (tubes));
    const auto count = juce::jmin (static_cast<int> (cells.size()), tubes);
    for (int i = 0; i < count; ++i)
        placed[static_cast<size_t> (tubes - count + i)] = cells[cells.size() - static_cast<size_t> (count) + static_cast<size_t> (i)];
    return placed;
}

void NixieDisplay::renderTubes (float scale)
{
    const auto w = juce::roundToInt (static_cast<float> (getWidth()) * scale);
    const auto h = juce::roundToInt (static_cast<float> (getHeight()) * scale);
    glassLayer = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    glassScale = scale;
    juce::Graphics g (glassLayer);
    g.addTransform (juce::AffineTransform::scale (scale));

    for (int i = 0; i < tubes; ++i)
    {
        const auto tube = tubeBounds (i);
        const auto glyphs = glyphAreaIn (tube);
        const auto radius = tube.getWidth() * 0.5f;

        juce::Path envelope;
        envelope.addRoundedRectangle (tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight(),
                                      radius, radius, true, true, false, false);
        juce::ColourGradient glass (juce::Colour (0xff1c1612), tube.getX(), tube.getY(),
                                    juce::Colour (0xff0b0908), tube.getX(), tube.getBottom(), false);
        g.setGradientFill (glass);
        g.fillPath (envelope);

        // Ghost cathodes: every numeral, stacked, faintly visible through the mesh.
        g.setColour (juce::Colour (0xff8a705a).withAlpha (0.09f));
        for (auto c : juce::String ("0123456789"))
            drawGlyph (g, c, glyphs);
        const auto dot = juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ glyphs.getRight() + tube.getWidth() * 0.05f, glyphs.getBottom() - 2.0f });
        g.fillEllipse (dot);

        // Metal base and pins.
        const auto base = tube.withTop (tube.getBottom() - tube.getHeight() * 0.06f);
        juce::ColourGradient metal (juce::Colour (0xff4a4744), base.getX(), base.getY(),
                                    juce::Colour (0xff121110), base.getX(), base.getBottom(), false);
        g.setGradientFill (metal);
        g.fillRect (base);
    }
}

void NixieDisplay::renderFront (float scale)
{
    const auto w = juce::roundToInt (static_cast<float> (getWidth()) * scale);
    const auto h = juce::roundToInt (static_cast<float> (getHeight()) * scale);
    frontLayer = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Graphics g (frontLayer);
    g.addTransform (juce::AffineTransform::scale (scale));

    for (int i = 0; i < tubes; ++i)
    {
        const auto tube = tubeBounds (i);
        const auto glyphs = glyphAreaIn (tube);

        // Honeycomb anode in front of the cathodes.
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (glyphs.expanded (tube.getWidth() * 0.08f, 4.0f).getSmallestIntegerContainer());
            juce::Path mesh;
            const auto pitch = 3.4f;
            for (float y = glyphs.getY() - 4.0f; y < glyphs.getBottom() + 4.0f; y += pitch * 0.866f)
            {
                const auto row = static_cast<int> ((y - glyphs.getY()) / (pitch * 0.866f));
                const auto offset = (row % 2 == 0) ? 0.0f : pitch * 0.5f;
                for (float x = glyphs.getX() - tube.getWidth() * 0.1f + offset; x < glyphs.getRight() + tube.getWidth() * 0.1f; x += pitch)
                    mesh.addEllipse (x - 1.1f, y - 1.1f, 2.2f, 2.2f);
            }
            g.setColour (juce::Colour (0xff0c0907).withAlpha (0.35f));
            g.strokePath (mesh, juce::PathStrokeType (0.45f));
        }

        // Glass highlights.
        juce::ColourGradient left (juce::Colours::white.withAlpha (0.0f), tube.getX(), tube.getY(),
                                   juce::Colours::white.withAlpha (0.0f), tube.getRight(), tube.getY(), false);
        left.addColour (0.10, juce::Colours::white.withAlpha (0.13f));
        left.addColour (0.22, juce::Colours::white.withAlpha (0.02f));
        left.addColour (0.82, juce::Colours::white.withAlpha (0.0f));
        left.addColour (0.90, juce::Colours::white.withAlpha (0.05f));
        g.setGradientFill (left);
        juce::Path envelope;
        const auto radius = tube.getWidth() * 0.5f;
        envelope.addRoundedRectangle (tube.getX(), tube.getY(), tube.getWidth(), tube.getHeight() * 0.93f,
                                      radius, radius, true, true, false, false);
        g.fillPath (envelope);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.strokePath (envelope, juce::PathStrokeType (0.8f));
    }
}

void NixieDisplay::renderSprites (float scale, juce::Rectangle<float> glyphArea)
{
    spriteScale = scale;
    spriteArea = glyphArea.withZeroOrigin();
    spriteMargin = juce::roundToInt (8.0f * scale) + 2;
    const auto w = juce::roundToInt (glyphArea.getWidth() * scale) + spriteMargin * 2;
    const auto h = juce::roundToInt (glyphArea.getHeight() * scale) + spriteMargin * 2;
    const juce::String chars ("0123456789-");

    for (int i = 0; i < chars.length(); ++i)
    {
        auto& sprite = sprites[static_cast<size_t> (i)];
        sprite.crisp = juce::Image (juce::Image::SingleChannel, w, h, true);
        {
            juce::Graphics g (sprite.crisp);
            g.addTransform (juce::AffineTransform::scale (scale).translated (static_cast<float> (spriteMargin), static_cast<float> (spriteMargin)));
            g.setColour (juce::Colours::white);
            drawGlyph (g, chars[i], spriteArea);
        }
        sprite.glow = sprite.crisp.createCopy();
        render::blurAlpha (sprite.glow, 4.5f * scale);
    }
}

void NixieDisplay::paint (juce::Graphics& g)
{
    const auto scale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! glassLayer.isValid() || ! frontLayer.isValid() || ! juce::approximatelyEqual (glassScale, scale))
    {
        renderTubes (scale);
        renderFront (scale);
    }
    const auto firstGlyphs = glyphAreaIn (tubeBounds (0));
    if (! sprites[0].crisp.isValid() || ! juce::approximatelyEqual (spriteScale, scale))
        renderSprites (scale, firstGlyphs);

    g.drawImageTransformed (glassLayer, juce::AffineTransform::scale (1.0f / scale));

    const auto neon = theme.palette.accent;
    const auto core = neon.interpolatedWith (juce::Colour (0xfffff3df), 0.58f);
    const auto cells = layoutCells();

    for (int i = 0; i < tubes; ++i)
    {
        const auto tube = tubeBounds (i);
        const auto glyphs = glyphAreaIn (tube);
        const auto& cell = cells[static_cast<size_t> (i)];
        const auto index = spriteIndex (cell.digit);

        if (index >= 0 && intensity > 0.0f)
        {
            const auto& sprite = sprites[static_cast<size_t> (index)];
            const auto transform = juce::AffineTransform::translation (static_cast<float> (-spriteMargin), static_cast<float> (-spriteMargin))
                                       .scaled (1.0f / scale)
                                       .translated (glyphs.getX(), glyphs.getY());
            // Neon discharge: a wide orange bloom, a tighter halo, then the hot cathode.
            render::halo (g, glyphs.getCentre(), tube.getWidth() * 1.05f, neon, 0.16f * intensity);
            g.setColour (neon.withAlpha (intensity));
            g.drawImageTransformed (sprite.glow, transform, true);
            g.drawImageTransformed (sprite.glow, transform, true);
            g.drawImageTransformed (sprite.glow, transform, true);
            g.setColour (core.withAlpha (intensity));
            g.drawImageTransformed (sprite.crisp, transform, true);
        }

        if (cell.point && intensity > 0.0f)
        {
            const auto dot = juce::Point<float> (glyphs.getRight() + tube.getWidth() * 0.05f, glyphs.getBottom() - 2.0f);
            render::halo (g, dot, 7.0f, neon, 0.6f * intensity);
            g.setColour (core.withAlpha (intensity));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (dot));
        }
    }

    g.drawImageTransformed (frontLayer, juce::AffineTransform::scale (1.0f / scale));
}
} // namespace affine
