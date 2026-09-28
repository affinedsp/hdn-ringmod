namespace affine
{
IlluminatedLegends::IlluminatedLegends()
{
    setBorderSize (juce::BorderSize<int> (0));
}

void IlluminatedLegends::setTheme (const Theme& t)
{
    theme = t;
    if (lit.isTransparent())
        lit = theme.palette.accent;
    repaint();
}

void IlluminatedLegends::setLegends (const juce::StringArray& newLegends, std::function<int (const juce::String&)> legendForText)
{
    legends = newLegends;
    legendFor = std::move (legendForText);
    repaint();
}

void IlluminatedLegends::setLitColour (juce::Colour colour)
{
    if (colour == lit)
        return;
    lit = colour;
    repaint();
}

void IlluminatedLegends::paint (juce::Graphics& g)
{
    if (legends.isEmpty())
        return;

    const auto active = legendFor != nullptr ? legendFor (getText()) : -1;
    const auto bounds = getLocalBounds().toFloat();
    const auto font = fonts::label (12.0f, 0.20f);
    g.setFont (font);

    // Each legend takes its own width; the spare space separates them evenly.
    std::vector<float> widths;
    auto total = 0.0f;
    for (const auto& legend : legends)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, legend.toUpperCase(), 0.0f, 0.0f);
        widths.push_back (glyphs.getBoundingBox (0, -1, true).getWidth());
        total += widths.back();
    }
    const auto gap = juce::jmax (8.0f, (bounds.getWidth() - total) / static_cast<float> (juce::jmax (1, legends.size() - 1)));
    auto x = bounds.getX() + juce::jmax (0.0f, (bounds.getWidth() - total - gap * static_cast<float> (legends.size() - 1)) * 0.5f);

    for (int i = 0; i < legends.size(); ++i)
    {
        const auto width = widths[static_cast<size_t> (i)];
        const auto area = juce::Rectangle<float> (x, bounds.getY(), width, bounds.getHeight());
        if (i == active)
        {
            render::halo (g, area.getCentre(), width * 0.75f + 8.0f, lit, 0.30f);
            g.setColour (lit.interpolatedWith (juce::Colours::white, 0.35f));
        }
        else
        {
            // Unlit legends are only just visible through the smoked glass.
            g.setColour (juce::Colour (0xff3a4047));
        }
        g.drawText (legends[i].toUpperCase(), area.expanded (4.0f, 0.0f), juce::Justification::centred, false);
        x += width + gap;
    }
}

//==============================================================================
TuningDial::TuningDial()
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

TuningDial::~TuningDial() { stopTimer(); }

void TuningDial::setTheme (const Theme& t)
{
    theme = t;
    scaleLayer = {};
    repaint();
}

void TuningDial::setRange (float minimumHz, float maximumHz)
{
    minimum = minimumHz;
    maximum = maximumHz;
    scaleLayer = {};
    repaint();
}

void TuningDial::setPointerColour (juce::Colour c)
{
    pointer = c;
    repaint();
}

float TuningDial::positionFor (float hz) const
{
    return juce::jlimit (0.0f, 1.0f, std::log (hz / minimum) / std::log (maximum / minimum));
}

void TuningDial::setFrequency (float hz, bool live)
{
    const auto p = live && hz > 0.0f ? positionFor (hz) : 0.0f;
    const auto l = live && hz > 0.0f ? 1.0f : 0.0f;
    if (juce::approximatelyEqual (p, target) && juce::approximatelyEqual (l, lampTarget))
        return;
    target = p;
    lampTarget = l;
    if (! isTimerRunning())
    {
        lastTick = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }
}

void TuningDial::settle()
{
    stopTimer();
    position = target;
    lamp = lampTarget;
    repaint();
}

void TuningDial::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = static_cast<float> (juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001));
    lastTick = now;
    // The dial cord drags the pointer along: an exponential glide, about 120 ms.
    position += (target - position) * (1.0f - std::exp (-dt / 0.06f));
    lamp += (lampTarget - lamp) * (1.0f - std::exp (-dt / 0.15f));
    repaint();
    if (std::abs (target - position) < 0.0005f && std::abs (lampTarget - lamp) < 0.002f)
    {
        position = target;
        lamp = lampTarget;
        stopTimer();
    }
}

void TuningDial::resized() { scaleLayer = {}; }

void TuningDial::renderScale (float scale)
{
    const auto w = juce::roundToInt (static_cast<float> (getWidth()) * scale);
    const auto h = juce::roundToInt (static_cast<float> (getHeight()) * scale);
    scaleLayer = juce::Image (juce::Image::SingleChannel, juce::jmax (1, w), juce::jmax (1, h), true);
    scaleLayerScale = scale;
    juce::Graphics g (scaleLayer);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.setColour (juce::Colours::white);

    const auto area = getLocalBounds().toFloat().reduced (18.0f, 10.0f);
    const auto baseline = area.getY() + area.getHeight() * 0.50f;
    const auto xFor = [&] (float hz) { return area.getX() + area.getWidth() * positionFor (hz); };

    g.fillRect (area.getX(), baseline, area.getWidth(), 1.2f);
    const float decades[] { 10.0f, 100.0f, 1000.0f, 10000.0f };
    for (auto decade : decades)
    {
        for (int step = 1; step < 10; ++step)
        {
            const auto hz = decade * static_cast<float> (step);
            if (hz < minimum || hz > maximum)
                continue;
            const auto major = step == 1 || step == 2 || step == 5;
            const auto x = xFor (hz);
            g.fillRect (x - 0.6f, baseline - (major ? 11.0f : 6.0f), 1.2f, major ? 11.0f : 6.0f);
            if (major)
            {
                const auto text = hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz));
                g.setFont (fonts::label (12.5f, 0.04f));
                g.drawText (text, juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ x, baseline - 21.0f }), juce::Justification::centred, false);
            }
        }
    }

    // A musical row under the frequencies: the A of every octave.
    g.setFont (fonts::label (10.5f, 0.10f));
    for (int octave = 0; octave <= 9; ++octave)
    {
        const auto hz = 27.5f * static_cast<float> (1 << octave);
        if (hz < minimum || hz > maximum)
            continue;
        const auto x = xFor (hz);
        g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ x, baseline + 7.0f }));
        g.drawText ("A" + juce::String (octave), juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ x, baseline + 17.0f }),
                    juce::Justification::centred, false);
    }
}

void TuningDial::paint (juce::Graphics& g)
{
    const auto scale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto bounds = getLocalBounds().toFloat();
    if (! scaleLayer.isValid() || ! juce::approximatelyEqual (scaleLayerScale, scale))
        renderScale (scale);

    // Dial glass: smoked, lit from behind by the dial lamps.
    g.setColour (juce::Colour (0xff020304));
    g.fillRoundedRectangle (bounds, 4.0f);
    const auto accent = theme.palette.accent;
    const auto glow = 0.25f + 0.75f * lamp;
    juce::ColourGradient backlight (accent.withAlpha (0.22f * glow), bounds.getCentreX(), bounds.getCentreY(),
                                    accent.withAlpha (0.0f), bounds.getX(), bounds.getY(), true);
    g.setGradientFill (backlight);
    g.fillRoundedRectangle (bounds, 4.0f);

    const auto toLogical = juce::AffineTransform::scale (1.0f / scale);
    g.setColour (accent.interpolatedWith (juce::Colours::white, 0.35f).withMultipliedAlpha (0.55f + 0.45f * glow));
    g.drawImageTransformed (scaleLayer, toLogical, true);

    // Pointer.
    const auto area = bounds.reduced (18.0f, 10.0f);
    const auto x = area.getX() + area.getWidth() * position;
    render::halo (g, { x, bounds.getCentreY() }, 16.0f, pointer, 0.35f * (0.3f + 0.7f * lamp));
    g.setColour (pointer.withMultipliedAlpha (0.45f + 0.55f * lamp));
    g.fillRect (juce::Rectangle<float> (x - 1.2f, bounds.getY() + 5.0f, 2.4f, bounds.getHeight() - 10.0f));

    // Glass reflection.
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.07f), bounds.getX(), bounds.getY(),
                                juce::Colours::white.withAlpha (0.0f), bounds.getX(), bounds.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (bounds.withHeight (bounds.getHeight() * 0.5f), 4.0f);
}
} // namespace affine
