namespace affine
{
namespace
{
constexpr float housingInset = 6.0f, bezel = 11.0f, sweep = 0.78f; // sweep: half-angle in radians
const juce::Colour ink { 0xff1d1b18 };
} // namespace

NeedleMeter::NeedleMeter()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

NeedleMeter::~NeedleMeter() { stopTimer(); }

void NeedleMeter::setTheme (const Theme& t)
{
    theme = t;
    face = {};
    repaint();
}

void NeedleMeter::setScale (Scale s)
{
    scale = std::move (s);
    face = {};
    repaint();
}

void NeedleMeter::setCaption (const juce::String& caption)
{
    if (caption == scale.caption)
        return;
    scale.caption = caption;
    face = {};
    repaint();
}

void NeedleMeter::setBacklight (juce::Colour c)
{
    backlight = c;
    face = {};
    repaint();
}

void NeedleMeter::setReading (float value, bool live)
{
    const auto p = live && scale.toPosition ? juce::jlimit (0.0f, 1.0f, scale.toPosition (value)) : 0.0f;
    lampTarget = live ? 1.0f : 0.0f;
    if (juce::approximatelyEqual (p, target) && juce::approximatelyEqual (lamp, lampTarget)
        && juce::approximatelyEqual (position, target))
        return;
    target = p;
    if (! isTimerRunning())
    {
        lastTick = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }
}

void NeedleMeter::settle()
{
    stopTimer();
    position = target;
    velocity = 0.0f;
    lamp = lampTarget;
    repaint();
}

void NeedleMeter::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = static_cast<float> (juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001));
    lastTick = now;

    // Moving-coil ballistics: a lightly under-damped spring, about 300 ms to settle.
    constexpr float omega = juce::MathConstants<float>::twoPi * 2.1f, zeta = 0.74f;
    constexpr int steps = 4;
    const auto h = dt / static_cast<float> (steps);
    for (int i = 0; i < steps; ++i)
    {
        const auto acceleration = omega * omega * (target - position) - 2.0f * zeta * omega * velocity;
        velocity += acceleration * h;
        position += velocity * h;
    }
    position = juce::jlimit (-0.02f, 1.03f, position);
    lamp += (lampTarget - lamp) * (1.0f - std::exp (-dt / 0.18f));

    repaint();
    if (std::abs (target - position) < 0.0005f && std::abs (velocity) < 0.001f
        && std::abs (lampTarget - lamp) < 0.002f)
    {
        position = target;
        velocity = 0.0f;
        lamp = lampTarget;
        stopTimer();
    }
}

juce::Rectangle<float> NeedleMeter::faceBounds() const
{
    return getLocalBounds().toFloat().reduced (housingInset + bezel);
}

juce::Point<float> NeedleMeter::pivot() const
{
    const auto f = faceBounds();
    return { f.getCentreX(), f.getBottom() + f.getHeight() * 0.14f };
}

float NeedleMeter::arcRadius() const
{
    const auto f = faceBounds();
    return juce::jmin (f.getHeight() * 0.90f, (f.getWidth() * 0.5f - 24.0f) / std::sin (sweep));
}

float NeedleMeter::angleFor (float p) const { return -sweep + 2.0f * sweep * p; }

void NeedleMeter::resized() { face = {}; }

void NeedleMeter::renderFace (float physicalScale)
{
    const auto area = faceBounds();
    const auto w = juce::roundToInt (area.getWidth() * physicalScale), h = juce::roundToInt (area.getHeight() * physicalScale);
    face = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
    faceScale = physicalScale;
    juce::Graphics g (face);
    // Draw in component coordinates from here on.
    g.addTransform (juce::AffineTransform::scale (physicalScale));
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()));

    const auto centre = pivot();
    const auto radius = arcRadius();

    // Backlit card: the lamp sits low behind the scale, so light falls off upwards and outwards.
    juce::ColourGradient light (backlight.brighter (0.12f), area.getCentreX(), area.getBottom() - area.getHeight() * 0.10f,
                                backlight.darker (1.1f), area.getX() - area.getWidth() * 0.12f, area.getY() - area.getHeight() * 0.25f, true);
    light.addColour (0.30, backlight);
    light.addColour (0.62, backlight.darker (0.30f));
    g.setGradientFill (light);
    g.fillRect (area);

    // Paper fibre.
    for (int i = 0; i < 1400; ++i)
    {
        const auto x = area.getX() + shading::hash (i, 1, 41) * area.getWidth();
        const auto y = area.getY() + shading::hash (i, 2, 41) * area.getHeight();
        g.setColour (juce::Colours::black.withAlpha (0.012f + 0.018f * shading::hash (i, 3, 41)));
        g.fillRect (x, y, 1.4f, 0.5f);
    }

    const auto arcPath = [&] (float r, float from, float to)
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, r, r, 0.0f, from, to, true);
        return p;
    };

    // Anti-parallax mirror band.
    {
        juce::ColourGradient mirror (juce::Colour (0xffe4e7ea), centre.x - radius, centre.y - radius,
                                     juce::Colour (0xff5f666d), centre.x + radius * 0.7f, centre.y - radius * 0.3f, false);
        mirror.addColour (0.40, juce::Colour (0xff8d959c));
        mirror.addColour (0.58, juce::Colour (0xfff2f4f6));
        mirror.addColour (0.75, juce::Colour (0xff7c848b));
        g.setGradientFill (mirror);
        g.strokePath (arcPath (radius - 10.0f, -sweep, sweep), juce::PathStrokeType (5.5f));
        g.setColour (ink.withAlpha (0.6f));
        g.strokePath (arcPath (radius - 12.9f, -sweep, sweep), juce::PathStrokeType (0.7f));
        g.strokePath (arcPath (radius - 7.1f, -sweep, sweep), juce::PathStrokeType (0.7f));
    }

    g.setColour (ink);
    g.strokePath (arcPath (radius, -sweep, sweep), juce::PathStrokeType (1.4f));

    const auto tick = [&] (float value, float length, float width)
    {
        const auto a = angleFor (juce::jlimit (0.0f, 1.0f, scale.toPosition (value)));
        juce::Path p;
        p.startNewSubPath (centre.getPointOnCircumference (radius - 0.6f, a));
        p.lineTo (centre.getPointOnCircumference (radius + length, a));
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    if (scale.toPosition)
    {
        g.setColour (ink.withAlpha (0.85f));
        for (auto v : scale.minors)
            tick (v, 5.0f, 1.0f);
        g.setColour (ink);
        g.setFont (fonts::label (15.5f, 0.0f));
        for (auto v : scale.majors)
        {
            tick (v, 9.5f, 1.8f);
            const auto a = angleFor (juce::jlimit (0.0f, 1.0f, scale.toPosition (v)));
            const auto at = centre.getPointOnCircumference (radius + 19.0f, a);
            g.drawText (scale.format ? scale.format (v) : juce::String (v),
                        juce::Rectangle<float> (40.0f, 18.0f).withCentre (at), juce::Justification::centred, false);
        }
    }

    g.setColour (ink);
    g.setFont (fonts::wordmark (12.0f, 0.22f));
    g.drawText (scale.unit, juce::Rectangle<float> (area.getWidth() * 0.5f, 16.0f).withCentre ({ area.getCentreX(), area.getY() + area.getHeight() * 0.58f }),
                juce::Justification::centred, false);
    g.setColour (ink.withAlpha (0.8f));
    g.setFont (fonts::label (11.0f, 0.26f));
    g.drawText (scale.caption.toUpperCase(),
                juce::Rectangle<float> (area.getWidth(), 14.0f).withCentre ({ area.getCentreX(), area.getY() + area.getHeight() * 0.71f }),
                juce::Justification::centred, false);

    // The card is recessed behind the bezel: soft shadow along the top and sides.
    const auto edge = [&] (juce::Rectangle<float> r, juce::Point<float> from, juce::Point<float> to, float alpha)
    {
        juce::ColourGradient gr (juce::Colours::black.withAlpha (alpha), from, juce::Colours::transparentBlack, to, false);
        g.setGradientFill (gr);
        g.fillRect (r);
    };
    edge (area.withHeight (18.0f), area.getTopLeft(), area.getTopLeft().translated (0.0f, 18.0f), 0.50f);
    edge (area.withWidth (14.0f), area.getTopLeft(), area.getTopLeft().translated (14.0f, 0.0f), 0.28f);
    edge (area.withLeft (area.getRight() - 14.0f), area.getTopRight(), area.getTopRight().translated (-14.0f, 0.0f), 0.28f);
}

void NeedleMeter::paint (juce::Graphics& g)
{
    const auto physicalScale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto housing = getLocalBounds().toFloat().reduced (housingInset);
    const auto area = faceBounds();

    // Housing: a black moulded bezel standing slightly proud of the faceplate.
    render::softShadow (g, housing, 9.0f, 9.0f, { 0.0f, 4.0f }, 0.6f);
    {
        juce::ColourGradient body (juce::Colour (0xff30343a), housing.getX(), housing.getY(),
                                   juce::Colour (0xff0a0b0d), housing.getX(), housing.getBottom(), false);
        body.addColour (0.08, juce::Colour (0xff24272c));
        g.setGradientFill (body);
        g.fillRoundedRectangle (housing, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawRoundedRectangle (housing.reduced (0.6f), 9.0f, 1.0f);

        // Inner chamfer down to the glass: lit from above, so its lower edge catches light.
        const auto chamfer = area.expanded (4.0f);
        juce::ColourGradient bevel (juce::Colour (0xff050506), chamfer.getX(), chamfer.getY(),
                                    juce::Colour (0xff3a3e44), chamfer.getX(), chamfer.getBottom(), false);
        g.setGradientFill (bevel);
        g.fillRoundedRectangle (chamfer, 4.0f);
    }

    if (! face.isValid() || ! juce::approximatelyEqual (faceScale, physicalScale))
        renderFace (physicalScale);

    g.drawImageTransformed (face, juce::AffineTransform::scale (1.0f / physicalScale).translated (area.getX(), area.getY()));

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (area.getSmallestIntegerContainer());

        // Dim the lamp when the reading is not live.
        const auto dark = (1.0f - lamp) * 0.74f;
        if (dark > 0.001f)
        {
            g.setColour (juce::Colour (0xff050608).withAlpha (dark));
            g.fillRect (area);
        }

        const auto centre = pivot();
        const auto radius = arcRadius();
        const auto a = angleFor (position);
        const auto needle = [&] (juce::Point<float> offset, float width, juce::Colour colour)
        {
            juce::Path p;
            p.startNewSubPath (centre.getPointOnCircumference (radius * 0.18f, a) + offset);
            p.lineTo (centre.getPointOnCircumference (radius + 10.0f, a) + offset);
            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        needle ({ 3.5f, 4.5f }, 3.6f, juce::Colours::black.withAlpha (0.07f * (0.3f + lamp)));
        needle ({ 2.2f, 2.8f }, 2.0f, juce::Colours::black.withAlpha (0.13f * (0.3f + lamp)));
        needle ({}, 1.6f, juce::Colour (0xff121314));

        // Pivot cover.
        const auto coverRadius = area.getHeight() * 0.20f;
        const auto cover = juce::Rectangle<float> (coverRadius * 2.4f, coverRadius * 2.0f)
                               .withCentre ({ centre.x, area.getBottom() + coverRadius * 0.25f });
        juce::ColourGradient dome (juce::Colour (0xff40454b), cover.getCentreX(), cover.getY(),
                                   juce::Colour (0xff0b0c0e), cover.getCentreX(), cover.getCentreY(), false);
        g.setGradientFill (dome);
        g.fillEllipse (cover);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawEllipse (cover.reduced (0.6f), 1.0f);

        // Glass: a broad reflection and a narrow glint.
        juce::ColourGradient glare (juce::Colours::white.withAlpha (0.09f), area.getX(), area.getY(),
                                    juce::Colours::white.withAlpha (0.0f), area.getX() + area.getWidth() * 0.5f, area.getY() + area.getHeight() * 0.75f, false);
        g.setGradientFill (glare);
        g.fillRect (area);
        juce::Path glint;
        glint.startNewSubPath (area.getX() + area.getWidth() * 0.64f, area.getY());
        glint.lineTo (area.getX() + area.getWidth() * 0.71f, area.getY());
        glint.lineTo (area.getX() + area.getWidth() * 0.50f, area.getBottom());
        glint.lineTo (area.getX() + area.getWidth() * 0.45f, area.getBottom());
        glint.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.022f));
        g.fillPath (glint);
    }
}
} // namespace affine
