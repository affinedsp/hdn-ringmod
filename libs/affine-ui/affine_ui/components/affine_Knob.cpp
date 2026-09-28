namespace affine
{
namespace
{
constexpr float labelHeight = 16.0f, scaleMargin = 19.0f, readoutGap = 11.0f, readoutHeight = 20.0f;

void strokeRadial (juce::Graphics& g, juce::Point<float> centre, float angle, float from, float to, float width)
{
    juce::Path p;
    p.startNewSubPath (centre.getPointOnCircumference (from, angle));
    p.lineTo (centre.getPointOnCircumference (to, angle));
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID, const juce::String& help)
    : parameter (*state.getParameter (parameterID)), attachment (state, parameterID, *this)
{
    setComponentID (parameterID);
    setName (parameter.getName (64));
    setTitle (getName());
    label = getName();
    setDescription (help);
    setTooltip ((help.isNotEmpty() ? help + " " : juce::String())
                + "Double-click or Return to type. Shift-drag for fine adjustment. "
                  "Alt/Option-click to reset; right-click for the menu.");
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    if (parameter.getLabel().isNotEmpty())
        setTextValueSuffix (" " + parameter.getLabel());
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setScrollWheelEnabled (false);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setDoubleClickReturnValue (false, 0.0);
    setOpaque (false);

    addChildComponent (entry);
    entry.setName (getName() + " exact value");
    entry.setJustification (juce::Justification::centred);
    entry.setFont (fonts::readout (15.0f));
    entry.setIndents (4, 2);
    entry.onReturnKey = [this] { finishEntry (true); };
    entry.onEscapeKey = [this] { finishEntry (false); };
    entry.onFocusLost = [this] { finishEntry (false); };
    setTheme ({});
}

Knob::~Knob() { cancelInteraction(); }

void Knob::setTheme (const Theme& newTheme)
{
    theme = newTheme;
    renderer = KnobRenderer (theme.knob);
    const auto& p = theme.palette;
    entry.setColour (juce::TextEditor::backgroundColourId, p.glass);
    entry.setColour (juce::TextEditor::textColourId, p.accent.interpolatedWith (juce::Colours::white, 0.3f));
    entry.setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    entry.setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    entry.setColour (juce::TextEditor::outlineColourId, p.accent.withAlpha (0.8f));
    entry.setColour (juce::TextEditor::focusedOutlineColourId, p.accent);
    entry.setColour (juce::CaretComponent::caretColourId, p.accent);
    repaint();
}

void Knob::setDiameter (float d)
{
    diameter = juce::jmax (24.0f, d);
    resized();
    repaint();
}

void Knob::setLabel (const juce::String& text)
{
    label = text;
    repaint();
}

void Knob::setScale (std::vector<double> plainValues, std::function<juce::String (double)> formatter)
{
    scaleValues = std::move (plainValues);
    scaleFormatter = std::move (formatter);
    repaint();
}

void Knob::setInactive (bool shouldBeInactive)
{
    if (inactive == shouldBeInactive)
        return;
    inactive = shouldBeInactive;
    repaint();
}

void Knob::setShowsActivityLamp (bool shouldShow)
{
    activityLamp = shouldShow;
    repaint();
}

void Knob::setLedRing (bool shouldShow)
{
    ledRing = shouldShow;
    repaint();
}

juce::Point<int> Knob::getPreferredSize() const
{
    // An LED ring pushes the printed numbers further out.
    return { juce::roundToInt (diameter + (ledRing ? 76.0f : 60.0f)),
             juce::roundToInt (labelHeight + scaleMargin + diameter + readoutGap + readoutHeight + 2.0f) };
}

juce::Rectangle<int> Knob::getBoundsForCentre (juce::Point<int> knobCentre) const
{
    const auto size = getPreferredSize();
    const auto axis = juce::roundToInt (labelHeight + scaleMargin + diameter * 0.5f);
    return { knobCentre.x - size.x / 2, knobCentre.y - axis, size.x, size.y };
}

Knob::Geometry Knob::getGeometry() const
{
    const auto bounds = getLocalBounds().toFloat();
    Geometry geometry;
    geometry.radius = diameter * 0.5f;
    geometry.label = bounds.withHeight (labelHeight);
    geometry.centre = { bounds.getCentreX(), labelHeight + scaleMargin + geometry.radius };
    const auto readoutWidth = juce::jmin (bounds.getWidth() - 2.0f, juce::jmax (78.0f, diameter + 14.0f));
    geometry.readout = juce::Rectangle<float> (readoutWidth, readoutHeight)
                           .withCentre ({ bounds.getCentreX(),
                                          geometry.centre.y + geometry.radius + readoutGap + readoutHeight * 0.5f });
    return geometry;
}

juce::Point<float> Knob::getKnobCentre() const { return getGeometry().centre; }

void Knob::resized()
{
    juce::Slider::resized();
    entry.setBounds (getGeometry().readout.getSmallestIntegerContainer());
}

void Knob::paint (juce::Graphics& g)
{
    const auto geometry = getGeometry();
    const auto& palette = theme.palette;
    const auto enabled = isEnabled();
    const auto live = enabled && ! inactive;
    const auto scale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());

    // Label, with an optional activity lamp. Hovering lifts the label so the target is clear.
    {
        g.setFont (fonts::label (12.5f, 0.16f));
        const auto text = label.toUpperCase();
        const auto hover = enabled && isMouseOverOrDragging (true);
        g.setColour ((hover ? juce::Colours::white : palette.silkscreen).withMultipliedAlpha (live ? 1.0f : 0.62f));
        g.drawText (text, geometry.label, juce::Justification::centred, false);

        if (activityLamp)
        {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (g.getCurrentFont(), text, 0.0f, 0.0f);
            const auto textWidth = glyphs.getBoundingBox (0, -1, true).getWidth();
            render::indicator (g, { geometry.label.getCentreX() - textWidth * 0.5f - 9.0f, geometry.label.getCentreY() - 0.5f },
                               4.5f, palette.accent, live ? 1.0f : 0.0f, palette);
        }
    }

    paintScale (g, geometry);

    // The knob itself.
    const auto physical = juce::roundToInt (diameter * scale);
    renderer.prepare (physical);
    const auto inverse = 1.0f / scale;
    const auto size = static_cast<float> (physical) * inverse;
    const auto origin = geometry.centre - juce::Point<float> (size, size) * 0.5f;
    const auto margin = static_cast<float> (renderer.getShadowMargin()) * inverse;
    const auto rotary = getRotaryParameters();
    const auto proportion = static_cast<float> (valueToProportionOfLength (getValue()));
    const auto angle = rotary.startAngleRadians + proportion * (rotary.endAngleRadians - rotary.startAngleRadians);
    const auto place = [&] (float x, float y) { return juce::AffineTransform::scale (inverse).translated (x, y); };

    g.drawImageTransformed (renderer.getShadow(), place (origin.x - margin, origin.y - margin));
    g.drawImageTransformed (renderer.getBody(), place (origin.x, origin.y));
    g.drawImageTransformed (renderer.getGrip (angle), place (origin.x, origin.y));

    const auto r = geometry.radius;
    const auto& finish = theme.knob;
    const auto pointerEnd = r * (finish.capRatio - 0.07f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    strokeRadial (g, geometry.centre.translated (0.0f, 0.45f), angle, r * 0.15f, pointerEnd, r * 0.070f + 0.8f);
    g.setColour (finish.pointer);
    strokeRadial (g, geometry.centre, angle, r * 0.15f, pointerEnd, r * 0.052f);
    g.setColour (finish.index);
    strokeRadial (g, geometry.centre, angle, r * (finish.gripRatio + 0.05f), r * 0.955f, juce::jmax (1.2f, r * 0.05f));

    if (! enabled)
    {
        g.setColour (theme.panel.base.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (geometry.centre).expanded (1.0f));
    }

    if (hasKeyboardFocus (true))
    {
        // An illuminated ring at the foot of the skirt, inside the printed scale.
        const auto ring = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (geometry.centre).expanded (1.6f);
        g.setColour (palette.accent.withAlpha (0.25f));
        g.drawEllipse (ring.expanded (0.8f), 2.6f);
        g.setColour (palette.accent);
        g.drawEllipse (ring, 1.3f);
    }

    paintReadout (g, geometry);
}

void Knob::paintScale (juce::Graphics& g, const Geometry& geometry)
{
    const auto& palette = theme.palette;
    const auto rotary = getRotaryParameters();
    const auto r = geometry.radius;
    const auto enabled = isEnabled();
    const auto angleFor = [&] (double plain)
    {
        const auto p = static_cast<float> (valueToProportionOfLength (plain));
        return rotary.startAngleRadians + p * (rotary.endAngleRadians - rotary.startAngleRadians);
    };

    if (ledRing)
    {
        // LEDs light from the start of the sweep up to the value; the last one fades in.
        constexpr int count = 23;
        const auto proportion = static_cast<float> (valueToProportionOfLength (getValue()));
        const auto live = enabled && ! inactive;
        const auto colour = inactive ? palette.accent.withMultipliedSaturation (0.2f).withMultipliedBrightness (0.7f) : palette.accent;
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (count - 1);
            const auto angle = rotary.startAngleRadians + t * (rotary.endAngleRadians - rotary.startAngleRadians);
            const auto at = geometry.centre.getPointOnCircumference (r + 7.5f, angle);
            const auto lit = enabled ? juce::jlimit (0.0f, 1.0f, (proportion - t) * static_cast<float> (count - 1) + 1.0f) : 0.0f;
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillEllipse (juce::Rectangle<float> (4.4f, 4.4f).withCentre (at));
            if (lit > 0.0f)
            {
                if (live)
                    render::halo (g, at, 6.5f, colour, 0.5f * lit);
                g.setColour (colour.withAlpha (live ? lit : lit * 0.55f));
            }
            else
            {
                g.setColour (colour.withMultipliedSaturation (0.3f).withMultipliedBrightness (0.18f));
            }
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (at));
        }
    }

    if (scaleValues.empty())
        return;

    const auto majorColour = palette.silkscreen.withMultipliedAlpha (enabled ? 0.92f : 0.45f);
    const auto minorColour = palette.silkscreen.withMultipliedAlpha (enabled ? 0.50f : 0.25f);

    if (! ledRing)
    {
        for (size_t i = 0; i + 1 < scaleValues.size(); ++i)
        {
            const auto a = scaleValues[i], b = scaleValues[i + 1];
            g.setColour (minorColour);
            for (int k = 1; k < 4; ++k)
                strokeRadial (g, geometry.centre, angleFor (a + (b - a) * k / 4.0), r + 3.6f, r + 6.0f, 0.9f);
        }
    }

    g.setFont (fonts::label (10.0f, 0.02f));
    for (auto value : scaleValues)
    {
        const auto angle = angleFor (value);
        if (! ledRing)
        {
            g.setColour (majorColour);
            strokeRadial (g, geometry.centre, angle, r + 3.2f, r + 7.6f, 1.2f);
        }

        const auto text = scaleFormatter ? scaleFormatter (value) : juce::String (value);
        const auto at = geometry.centre.getPointOnCircumference (ledRing ? r + 17.0f : r + 14.5f, angle);
        g.setColour (palette.silkscreenDim.withMultipliedAlpha (enabled ? 1.0f : 0.5f));
        g.drawText (text, juce::Rectangle<float> (34.0f, 12.0f).withCentre (at), juce::Justification::centred, false);
    }
}

void Knob::paintReadout (juce::Graphics& g, const Geometry& geometry)
{
    const auto& palette = theme.palette;
    const auto area = geometry.readout;
    const auto live = isEnabled() && ! inactive;
    const auto text = getTextFromValue (getValue());

    if (palette.readout == Palette::Readout::backlit)
    {
        const auto level = ! isEnabled() ? 0.0f : (inactive ? 0.35f : 1.0f);
        render::litWindow (g, area, palette.readoutBacklight, level);
        if (entry.isVisible())
            return;
        g.setColour (palette.readoutInk.withMultipliedAlpha (isEnabled() ? 0.92f : 0.45f));
        g.setFont (fonts::readout (14.5f, 0.02f));
        g.drawText (text, area.reduced (4.0f, 0.0f), juce::Justification::centred, false);
        return;
    }

    g.setColour (palette.glass);
    g.fillRoundedRectangle (area, metrics::displayCorner);
    render::recess (g, area, metrics::displayCorner, 0.45f);

    if (entry.isVisible())
        return;

    auto colour = palette.accent;
    if (! isEnabled())
        colour = palette.silkscreenDim.withMultipliedBrightness (0.45f);
    else if (inactive)
        colour = palette.accent.withMultipliedSaturation (0.25f).withMultipliedBrightness (0.62f);

    valueGlow.draw (g, text, fonts::readout (14.5f, 0.02f), area.reduced (4.0f, 0.0f),
                    juce::Justification::centred, colour, 2.2f, live ? 0.85f : 0.35f);
}

void Knob::setWithGesture (double plainValue)
{
    juce::Slider::ScopedDragNotification gesture (*this);
    setValue (plainValue, juce::sendNotificationSync);
}

double Knob::defaultValue() const
{
    return parameter.convertFrom0to1 (parameter.getDefaultValue());
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;
    grabKeyboardFocus();
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }
    if (e.mods.isAltDown())
    {
        setWithGesture (defaultValue());
        return;
    }
    lastDrag = e.position;
    dragProportion = valueToProportionOfLength (getValue());
    drag = std::make_unique<juce::Slider::ScopedDragNotification> (*this);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == nullptr)
        return;
    const auto delta = e.position - lastDrag;
    const double sensitivity = e.mods.isShiftDown() ? 2400.0 : 240.0;
    // Accumulate sub-step movement so fine drags work on coarse parameter grids.
    dragProportion = juce::jlimit (0.0, 1.0, dragProportion + (delta.x - delta.y) / sensitivity);
    setValue (proportionOfLengthToValue (dragProportion), juce::sendNotificationSync);
    lastDrag = e.position;
}

void Knob::mouseUp (const juce::MouseEvent&) { drag.reset(); }

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isAltDown() && ! e.mods.isPopupMenu())
        beginEntry();
}

void Knob::cancelInteraction()
{
    drag.reset();
    finishEntry (false);
}

void Knob::enablementChanged()
{
    if (! isEnabled())
        cancelInteraction();
    repaint();
}

bool Knob::keyPressed (const juce::KeyPress& key)
{
    if (! isEnabled())
        return false;
    if (key == juce::KeyPress::returnKey)
    {
        beginEntry();
        return true;
    }

    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey
        || code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
    {
        auto step = keyboardStep;
        if (step <= 0.0)
            step = getInterval() > 0.0 ? getInterval() : (getMaximum() - getMinimum()) / 100.0;
        const auto direction = code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey ? 1 : -1;
        const auto fine = key.getModifiers().isShiftDown() && getInterval() <= 0.0 ? 0.1 : 1.0;
        setWithGesture (getValue() + direction * step * fine);
        return true;
    }
    if (code == juce::KeyPress::homeKey)
    {
        setWithGesture (defaultValue());
        return true;
    }
    return juce::Slider::keyPressed (key);
}

void Knob::beginEntry()
{
    if (! isEnabled())
        return;
    drag.reset();
    entry.setText (getTextFromValue (getValue()), false);
    entry.setVisible (true);
    entry.grabKeyboardFocus();
    entry.selectAll();
    repaint();
}

void Knob::finishEntry (bool commit)
{
    if (! entry.isVisible())
        return;
    const auto typed = entry.getText().trim();
    entry.setVisible (false);

    if (commit)
    {
        // Accept one finite number plus an optional matching unit. Do not let a
        // permissive parser turn malformed input into zero.
        auto number = typed;
        const auto unit = parameter.getLabel();
        if (unit.isNotEmpty() && number.endsWithIgnoreCase (unit))
            number = number.dropLastCharacters (unit.length()).trimEnd();
        const auto utf8 = number.toRawUTF8();
        char* end = nullptr;
        const auto value = std::strtod (utf8, &end);
        if (number.containsOnly ("0123456789.+-") && end != utf8 && *end == '\0' && std::isfinite (value))
            setWithGesture (parameter.convertFrom0to1 (parameter.getValueForText (number)));
        grabKeyboardFocus();
    }
    repaint();
}

void Knob::showMenu()
{
    juce::PopupMenu menu;
    const juce::Component::SafePointer<Knob> safe (this);
    menu.addItem ("Enter value...", [safe] { if (safe != nullptr) safe->beginEntry(); });
    menu.addItem ("Reset to default", [safe]
    {
        if (safe != nullptr)
            safe->setWithGesture (safe->defaultValue());
    });
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* host = editor->getHostContext())
            if (auto hostMenu = host->getContextMenuForParameter (&parameter))
                menu.addSubMenu ("Host", hostMenu->getEquivalentPopupMenu());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}
} // namespace affine
