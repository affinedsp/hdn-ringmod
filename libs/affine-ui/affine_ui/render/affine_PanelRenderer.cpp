namespace affine::render
{
using namespace shading;

namespace
{
// Hammered enamel: every hammer blow leaves a round, shallow crater of its own size.
// Summing the craters of neighbouring blows gives soft overlapping rims, no creases.
float hammerField (float x, float y, int seed, float& shade) noexcept
{
    const auto cx = static_cast<int> (std::floor (x)), cy = static_cast<int> (std::floor (y));
    auto field = 0.0f, nearest = 8.0f;
    for (int j = -1; j <= 1; ++j)
    {
        for (int i = -1; i <= 1; ++i)
        {
            const auto cellX = cx + i, cellY = cy + j;
            const auto px = static_cast<float> (cellX) + 0.1f + 0.8f * hash (cellX, cellY, seed);
            const auto py = static_cast<float> (cellY) + 0.1f + 0.8f * hash (cellX, cellY, seed + 7);
            const auto radius = 0.42f + 0.30f * hash (cellX, cellY, seed + 11);
            const auto d2 = (px - x) * (px - x) + (py - y) * (py - y);
            field += std::exp (-d2 / (radius * radius)) * (0.6f + 0.4f * hash (cellX, cellY, seed + 19));
            if (d2 < nearest)
            {
                nearest = d2;
                shade = hash (cellX, cellY, seed + 13);
            }
        }
    }
    return field;
}

V3 anodisedPixel (const PanelFinish& finish, V3 base, float u, float v, float lx, float ly, int x, int y) noexcept
{
    // Key light from above: the plate is brighter at the top and falls off gently.
    const auto vertical = mix (1.16f, 0.78f, smoothstep (-0.1f, 1.05f, v));
    const auto side = 1.0f - 0.12f * std::pow (std::abs (u - 0.46f) * 2.0f, 3.0f);
    const auto grain = (valueNoise (lx * 1.35f, ly * 1.35f, 3) - 0.5f) * finish.grain
                     + (hash (x, y, 9) - 0.5f) * finish.grain * 0.55f;
    const auto mottle = (fbm (lx / 95.0f, ly / 95.0f, 3, 17) - 0.5f) * finish.mottle;
    const auto brushed = finish.brushing > 0.0f
        ? ((fbm (lx / 220.0f, ly * 1.9f, 2, 23) - 0.5f) * 0.10f + (hash (0, y, 29) - 0.5f) * 0.04f) * finish.brushing
        : 0.0f;
    const auto sheenX = (u - 0.30f) / 0.62f, sheenY = (v + 0.12f) / 0.50f;
    const auto sheen = std::exp (-(sheenX * sheenX + sheenY * sheenY)) * finish.sheen * 0.060f;
    return base * (vertical * side * (1.0f + grain + mottle + brushed)) + V3 { sheen, sheen, sheen * 1.04f };
}

V3 powderPixel (const PanelFinish& finish, V3 base, float u, float v, float lx, float ly, int x, int y) noexcept
{
    const auto vertical = mix (1.10f, 0.84f, smoothstep (-0.1f, 1.05f, v));
    const auto speckle = (hash (x, y, 41) - 0.5f) * finish.grain + (valueNoise (lx * 2.2f, ly * 2.2f, 43) - 0.5f) * finish.grain * 0.8f;
    const auto mottle = (fbm (lx / 70.0f, ly / 70.0f, 3, 47) - 0.5f) * finish.mottle;
    const auto sheenX = (u - 0.35f) / 0.7f, sheenY = (v + 0.2f) / 0.6f;
    const auto sheen = std::exp (-(sheenX * sheenX + sheenY * sheenY)) * finish.sheen * 0.02f;
    return base * (vertical * (1.0f + speckle + mottle)) + V3 { sheen, sheen, sheen };
}

V3 glassPixel (const PanelFinish& finish, V3 base, float u, float v) noexcept
{
    // Black glass mirrors the room: a faint ceiling at the top edge and one broad
    // diagonal reflection of the softbox.
    const auto ceiling = std::exp (-v * 9.0f) * 0.010f;
    const auto band = (u * 0.55f + v) - 0.30f;
    const auto softbox = std::exp (-band * band / 0.020f) * smoothstep (0.85f, 0.05f, u) * finish.sheen * 0.030f;
    const auto floor = smoothstep (0.55f, 1.0f, v) * 0.004f;
    const auto light = ceiling + softbox + floor;
    return base + V3 { light, light, light * 1.08f };
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

    // Hammertone: dimples of a hammered enamel, as a height field lit from the key light.
    std::vector<float> relief, tint;
    if (finish.texture == PanelFinish::Texture::hammertone)
    {
        relief.assign (static_cast<size_t> (width) * static_cast<size_t> (height), 0.0f);
        tint.assign (relief.size(), 0.0f);
        const auto cell = 1.0f / juce::jmax (1.0f, finish.dimple);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const auto lx = (static_cast<float> (x) + 0.5f) * inverseScale;
                const auto ly = (static_cast<float> (y) + 0.5f) * inverseScale;
                float id = 0.0f;
                const auto craters = hammerField (lx * cell, ly * cell, 61, id);
                const auto index = static_cast<size_t> (y) * static_cast<size_t> (width) + static_cast<size_t> (x);
                relief[index] = -0.40f * craters + (valueNoise (lx * 0.35f, ly * 0.35f, 67) - 0.5f) * 0.08f;
                tint[index] = id;
            }
        }
    }

    const V3 hammerLight = V3 { -0.35f, -0.80f, 0.60f }.normalised();

    for (int y = 0; y < height; ++y)
    {
        const auto v = (static_cast<float> (y) + 0.5f) / static_cast<float> (height);
        const auto ly = (static_cast<float> (y) + 0.5f) * inverseScale;

        for (int x = 0; x < width; ++x)
        {
            const auto u = (static_cast<float> (x) + 0.5f) / static_cast<float> (width);
            const auto lx = (static_cast<float> (x) + 0.5f) * inverseScale;
            V3 colour;

            switch (finish.texture)
            {
                case PanelFinish::Texture::hammertone:
                {
                    const auto at = [&] (int px, int py)
                    {
                        px = juce::jlimit (0, width - 1, px);
                        py = juce::jlimit (0, height - 1, py);
                        return relief[static_cast<size_t> (py) * static_cast<size_t> (width) + static_cast<size_t> (px)];
                    };
                    // Gradients in logical units so the relief looks the same at every scale.
                    const auto dx = (at (x + 1, y) - at (x - 1, y)) * scale * 0.5f;
                    const auto dy = (at (x, y + 1) - at (x, y - 1)) * scale * 0.5f;
                    const auto normal = V3 { -dx * 3.0f, -dy * 3.0f, 1.0f }.normalised();
                    const auto lambert = saturate (normal.dot (hammerLight));
                    const auto spec = std::pow (saturate (normal.dot ((hammerLight + V3 { 0.0f, 0.0f, 1.0f }).normalised())), 18.0f);
                    const auto vertical = mix (1.08f, 0.86f, smoothstep (-0.1f, 1.05f, v));
                    const auto id = tint[static_cast<size_t> (y) * static_cast<size_t> (width) + static_cast<size_t> (x)];
                    const auto cloud = (fbm (lx / 60.0f, ly / 60.0f, 3, 73) - 0.5f) * finish.mottle;
                    const auto flake = 1.0f + (id - 0.5f) * finish.mottle * 0.6f + cloud + (hash (x, y, 71) - 0.5f) * finish.grain;
                    const auto sheen = finish.sheen * spec * 0.10f;
                    colour = base * (vertical * flake * (0.72f + 0.34f * lambert)) + V3 { sheen, sheen, sheen * 0.98f };
                    break;
                }
                case PanelFinish::Texture::powder:
                    colour = powderPixel (finish, base, u, v, lx, ly, x, y);
                    break;
                case PanelFinish::Texture::glass:
                    colour = glassPixel (finish, base, u, v);
                    break;
                case PanelFinish::Texture::anodised:
                default:
                    colour = anodisedPixel (finish, base, u, v, lx, ly, x, y);
                    break;
            }

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

void litWindow (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour backlight, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    const auto corner = 2.5f;
    // Bezel: a thin dark frame pressed into the plate.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (area.expanded (1.6f), corner + 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (area.expanded (1.6f).translated (0.0f, 0.8f), corner + 1.5f, 0.8f);

    // Frosted glass: unlit it is a milky grey, lit it glows brightest in the middle.
    const auto unlit = backlight.withMultipliedSaturation (0.25f).withMultipliedBrightness (0.42f);
    const auto centre = unlit.interpolatedWith (backlight.brighter (0.15f), level);
    const auto edge = unlit.darker (0.25f).interpolatedWith (backlight.darker (0.45f), level);
    juce::ColourGradient body (centre, area.getCentreX(), area.getCentreY(),
                               edge, area.getX(), area.getY(), true);
    g.setGradientFill (body);
    g.fillRoundedRectangle (area, corner);
    if (level > 0.0f)
        halo (g, area.getCentre(), area.getWidth() * 0.6f, backlight, 0.10f * level);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (area.toNearestInt());
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.30f), area.getX(), area.getY(),
                                juce::Colours::transparentBlack, area.getX(), area.getY() + 5.0f, false);
    g.setGradientFill (shade);
    g.fillRect (area.withHeight (5.0f));
}

void jewel (juce::Graphics& g, juce::Point<float> centre, float diameter, juce::Colour colour, float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    const auto r = diameter * 0.5f;

    // Knurled chrome bezel.
    const auto bezel = juce::Rectangle<float> (diameter * 1.55f, diameter * 1.55f).withCentre (centre);
    softShadow (g, bezel, bezel.getWidth() * 0.5f, 3.0f, { 0.0f, 1.5f }, 0.5f);
    juce::ColourGradient chrome (juce::Colour (0xfff3f5f7), bezel.getX(), bezel.getY(),
                                 juce::Colour (0xff4d5157), bezel.getRight(), bezel.getBottom(), false);
    chrome.addColour (0.55, juce::Colour (0xff9ea3a9));
    g.setGradientFill (chrome);
    g.fillEllipse (bezel);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int i = 0; i < 28; ++i)
    {
        const auto a = static_cast<float> (i) * juce::MathConstants<float>::twoPi / 28.0f;
        g.drawLine (juce::Line<float> (centre.getPointOnCircumference (bezel.getWidth() * 0.43f, a),
                                       centre.getPointOnCircumference (bezel.getWidth() * 0.5f, a)), 0.6f);
    }

    if (level > 0.0f)
        halo (g, centre, r * 3.6f, colour, 0.38f * level);

    // Faceted jewel: eight facets around a flat table.
    const auto unlit = colour.withMultipliedSaturation (0.7f).withMultipliedBrightness (0.28f);
    const auto body = unlit.interpolatedWith (colour, level);
    for (int i = 0; i < 8; ++i)
    {
        const auto a0 = static_cast<float> (i) * juce::MathConstants<float>::twoPi / 8.0f;
        const auto a1 = a0 + juce::MathConstants<float>::twoPi / 8.0f;
        juce::Path facet;
        facet.startNewSubPath (centre.getPointOnCircumference (r * 0.45f, a0));
        facet.lineTo (centre.getPointOnCircumference (r, a0));
        facet.lineTo (centre.getPointOnCircumference (r, a1));
        facet.lineTo (centre.getPointOnCircumference (r * 0.45f, a1));
        facet.closeSubPath();
        // Facets that face the key light read brighter.
        const auto facing = 0.5f + 0.5f * std::cos (a0 + 0.39f - (-2.2f));
        g.setColour (body.darker (0.55f - 0.45f * facing).brighter (0.25f * level * facing));
        g.fillPath (facet);
    }
    juce::Path table;
    for (int i = 0; i < 8; ++i)
    {
        const auto p = centre.getPointOnCircumference (r * 0.45f, static_cast<float> (i) * juce::MathConstants<float>::twoPi / 8.0f);
        if (i == 0) table.startNewSubPath (p); else table.lineTo (p);
    }
    table.closeSubPath();
    g.setColour (body.brighter (0.2f + 0.6f * level));
    g.fillPath (table);
    g.setColour (juce::Colours::white.withAlpha (0.55f + 0.35f * level));
    g.fillEllipse (juce::Rectangle<float> (r * 0.42f, r * 0.30f).withCentre (centre.translated (-r * 0.30f, -r * 0.38f)));
}

void rackEar (juce::Graphics& g, juce::Rectangle<float> area, bool leftSide)
{
    // The flange is folded from the same plate: a crease where it meets the panel.
    const auto crease = leftSide ? area.getRight() : area.getX();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (crease - 1.0f, area.getY(), 1.5f, area.getHeight()));
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRect (juce::Rectangle<float> (crease + (leftSide ? 0.5f : -1.5f), area.getY(), 1.0f, area.getHeight()));
    juce::ColourGradient falloff (juce::Colours::black.withAlpha (leftSide ? 0.10f : 0.0f), area.getX(), area.getY(),
                                  juce::Colours::black.withAlpha (leftSide ? 0.0f : 0.10f), area.getRight(), area.getY(), false);
    g.setGradientFill (falloff);
    g.fillRect (area);

    for (auto fraction : { 0.20f, 0.80f })
    {
        const auto slot = juce::Rectangle<float> (area.getWidth() * 0.62f, 12.0f)
                              .withCentre ({ area.getCentreX(), area.getY() + area.getHeight() * fraction });
        g.setColour (juce::Colour (0xff0f0e0c));
        g.fillRoundedRectangle (slot, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (slot.translated (0.0f, 0.8f), 6.0f, 0.8f);
        screw (g, slot.getCentre().translated (slot.getWidth() * 0.12f, 0.0f), 10.0f, fraction * 3.0f);
    }
}

void nameplate (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const juce::Font& font)
{
    softShadow (g, area, 3.0f, 4.0f, { 0.0f, 2.0f }, 0.45f);
    juce::ColourGradient metal (juce::Colour (0xffe9ebec), area.getX(), area.getY(),
                                juce::Colour (0xffa4a8ac), area.getX(), area.getBottom(), false);
    metal.addColour (0.55, juce::Colour (0xffcfd3d6));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (area, 3.0f);
    // Horizontal brushing.
    for (int i = 0; i < static_cast<int> (area.getHeight() * 1.6f); ++i)
    {
        const auto y = area.getY() + 1.0f + static_cast<float> (i) * 0.62f;
        g.setColour ((shading::hash (i, 3, 91) > 0.5f ? juce::Colours::white : juce::Colours::black)
                         .withAlpha (0.035f + 0.04f * shading::hash (i, 4, 91)));
        g.drawHorizontalLine (juce::roundToInt (y), area.getX() + 2.0f, area.getRight() - 2.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawLine (area.getX() + 2.0f, area.getY() + 0.7f, area.getRight() - 2.0f, area.getY() + 0.7f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (area, 3.0f, 0.8f);

    for (auto corner : { area.getTopLeft(), area.getTopRight(), area.getBottomLeft(), area.getBottomRight() })
    {
        const auto inset = juce::Point<float> (corner.x < area.getCentreX() ? 6.0f : -6.0f, corner.y < area.getCentreY() ? 6.0f : -6.0f);
        const auto rivet = corner + inset;
        juce::ColourGradient dome (juce::Colour (0xfffcfdfd), rivet.x - 1.5f, rivet.y - 1.5f,
                                   juce::Colour (0xff6f7378), rivet.x + 2.0f, rivet.y + 2.0f, false);
        g.setGradientFill (dome);
        g.fillEllipse (juce::Rectangle<float> (4.4f, 4.4f).withCentre (rivet));
    }

    // Engraved lettering: dark fill with a lit lower lip.
    g.setFont (font);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawText (text, area.translated (0.0f, 0.8f), juce::Justification::centred, false);
    g.setColour (juce::Colour (0xff1e1e1f));
    g.drawText (text, area, juce::Justification::centred, false);
}

void endCap (juce::Graphics& g, juce::Rectangle<float> area, bool leftSide)
{
    // Polished aluminium block: mirrors the ceiling near the top and the room below.
    juce::ColourGradient block (juce::Colour (0xffdfe3e7), area.getX(), area.getY(),
                                juce::Colour (0xff6c7177), area.getX(), area.getBottom(), false);
    block.addColour (0.18, juce::Colour (0xfff7f8f9));
    block.addColour (0.46, juce::Colour (0xff9aa0a6));
    block.addColour (0.70, juce::Colour (0xffc8cdd2));
    g.setGradientFill (block);
    g.fillRect (area);
    const auto inner = leftSide ? area.getRight() : area.getX();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (inner - (leftSide ? 1.5f : 0.0f), area.getY(), 1.5f, area.getHeight()));

    // Pull handle standing off the cap on two posts.
    const auto handle = juce::Rectangle<float> (area.getWidth() * 0.34f, area.getHeight() * 0.66f).withCentre (area.getCentre());
    softShadow (g, handle.translated (leftSide ? 3.0f : -3.0f, 5.0f), handle.getWidth() * 0.5f, 6.0f, {}, 0.55f);
    for (auto post : { handle.getY() + 8.0f, handle.getBottom() - 8.0f })
    {
        g.setColour (juce::Colour (0xff3e4247));
        g.fillEllipse (juce::Rectangle<float> (handle.getWidth() * 1.1f, 9.0f).withCentre ({ handle.getCentreX(), post }));
    }
    juce::ColourGradient bar (juce::Colour (0xff5b6066), handle.getX(), handle.getY(),
                              juce::Colour (0xff4a4f55), handle.getRight(), handle.getY(), false);
    bar.addColour (0.28, juce::Colour (0xfffdfeff));
    bar.addColour (0.55, juce::Colour (0xffb8bdc3));
    g.setGradientFill (bar);
    g.fillRoundedRectangle (handle, handle.getWidth() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (handle, handle.getWidth() * 0.5f, 0.8f);
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
