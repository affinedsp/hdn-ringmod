namespace affine
{
KeyButton::KeyButton (const juce::String& name) : juce::Button (name)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    lampColour = theme.palette.attention;
}

void KeyButton::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void KeyButton::setStyle (Style s)
{
    style = s;
    repaint();
}

void KeyButton::setKeyColour (juce::Colour c)
{
    keyColour = c;
    repaint();
}

void KeyButton::setLampColour (juce::Colour c)
{
    lampColour = c;
    repaint();
}

void KeyButton::setLegend (const juce::String& lineOne, const juce::String& lineTwo)
{
    legendOne = lineOne;
    legendTwo = lineTwo;
    repaint();
}

void KeyButton::setGlyph (const juce::Path& p)
{
    glyph = p;
    repaint();
}

void KeyButton::setKeySize (float width, float height)
{
    keyWidth = width;
    keyHeight = height;
    repaint();
}

juce::Rectangle<float> KeyButton::getKeyBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    const auto legend = legendTwo.isNotEmpty() ? 30.0f : (legendOne.isNotEmpty() ? 18.0f : 0.0f);
    bounds.removeFromBottom (legend);
    const auto width = juce::jmin (bounds.getWidth() - 8.0f, keyWidth);
    const auto height = juce::jmin (bounds.getHeight() - 8.0f, keyHeight);
    return juce::Rectangle<float> (width, height).withCentre (bounds.getCentre());
}

bool KeyButton::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    if (onArrow != nullptr && isEnabled())
    {
        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::upKey)
        {
            onArrow (-1);
            return true;
        }
        if (code == juce::KeyPress::rightKey || code == juce::KeyPress::downKey)
        {
            onArrow (1);
            return true;
        }
    }
    if (isEnabled() && (key.isKeyCode (juce::KeyPress::spaceKey) || key.isKeyCode (juce::KeyPress::returnKey)))
    {
        if (getClickingTogglesState())
            setToggleState (! getToggleState(), juce::sendNotificationSync);
        else
            triggerClick();
        return true;
    }
    return juce::Button::keyPressed (key);
}

void KeyButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    switch (style)
    {
        case Style::pianoKey:     paintPianoKey (g, highlighted, down); break;
        case Style::toggle:       paintToggle (g, highlighted, down); break;
        case Style::silverSquare: paintSilverSquare (g, highlighted, down); break;
        case Style::softKey:      paintSoftKey (g, highlighted, down); break;
        case Style::cap:
        default:                  paintCap (g, highlighted, down); break;
    }
}

void KeyButton::paintGlyph (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float width)
{
    if (glyph.isEmpty())
        return;
    auto shape = glyph;
    shape.applyTransform (shape.getTransformToScaleToFit (area, false));
    g.setColour (colour);
    g.strokePath (shape, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void KeyButton::paintLegend (juce::Graphics& g, bool lit)
{
    const auto& palette = theme.palette;
    auto legend = getLocalBounds().toFloat();
    legend = legend.removeFromBottom (legendTwo.isNotEmpty() ? 30.0f : 18.0f);
    if (lit)
    {
        // Legend printed on a lamp window: it glows while the key is engaged.
        render::halo (g, legend.withHeight (15.0f).getCentre(), legend.getWidth() * 0.45f, lampColour, 0.22f);
        g.setColour (lampColour.interpolatedWith (juce::Colours::white, 0.25f));
    }
    else
    {
        g.setColour (palette.silkscreen);
    }
    g.setFont (fonts::label (12.0f, 0.16f));
    g.drawText (legendOne.toUpperCase(), legend.removeFromTop (15.0f), juce::Justification::centred, false);
    if (legendTwo.isNotEmpty())
    {
        g.setColour (palette.silkscreenDim);
        g.setFont (fonts::label (10.0f, 0.18f));
        g.drawText (legendTwo.toUpperCase(), legend.removeFromTop (13.0f), juce::Justification::centred, false);
    }
}

void KeyButton::paintCap (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& palette = theme.palette;
    const auto key = getKeyBounds();
    const auto engaged = getToggleState();
    const auto travel = down ? 1.6f : (engaged ? 0.8f : 0.0f);

    // Well cut into the plate.
    const auto well = key.expanded (3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (well, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (well.translated (0.0f, 0.8f).expanded (0.5f), 6.5f, 0.9f);

    // Key cap: anodised aluminium, lit from above.
    const auto cap = key.translated (0.0f, travel);
    render::softShadow (g, cap.reduced (2.0f), 4.0f, 4.0f, { 0.0f, 2.2f - travel }, 0.6f);
    juce::ColourGradient body (juce::Colour (0xff4a5057), cap.getX(), cap.getY(),
                               juce::Colour (0xff1d2024), cap.getX(), cap.getBottom(), false);
    body.addColour (0.12, juce::Colour (0xff3a3f45));
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, 4.5f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.26f : 0.18f));
    g.drawLine (cap.getX() + 4.0f, cap.getY() + 0.8f, cap.getRight() - 4.0f, cap.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (cap, 4.5f, 0.9f);

    // LED window in the key.
    const auto windowY = cap.getY() + cap.getHeight() * (glyph.isEmpty() ? 0.34f : 0.25f);
    const auto window = juce::Rectangle<float> (cap.getWidth() * 0.42f, 4.0f).withCentre ({ cap.getCentreX(), windowY });
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (window.expanded (1.0f), 2.5f);
    const auto unlit = lampColour.withMultipliedSaturation (0.5f).withMultipliedBrightness (0.22f);
    g.setColour (engaged ? lampColour : unlit);
    g.fillRoundedRectangle (window, 2.0f);
    if (engaged)
    {
        render::halo (g, window.getCentre(), window.getWidth() * 0.95f, lampColour, 0.45f);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (window.reduced (window.getWidth() * 0.2f, 1.2f), 1.0f);
    }

    paintGlyph (g, cap.withTrimmedTop (cap.getHeight() * 0.50f).reduced (cap.getWidth() * 0.26f, cap.getHeight() * 0.12f),
                palette.silkscreen.withAlpha (engaged ? 0.95f : 0.72f), 1.4f);

    if (hasKeyboardFocus (true))
    {
        g.setColour (palette.accent.withAlpha (0.9f));
        g.drawRoundedRectangle (well.expanded (2.5f), 8.0f, 1.5f);
    }
    paintLegend (g, false);
}

void KeyButton::paintPianoKey (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& palette = theme.palette;
    const auto key = getKeyBounds();
    const auto engaged = getToggleState();
    // An interlocking key stays down while engaged; the others stand proud.
    const auto travel = down ? 4.0f : (engaged ? 3.0f : 0.0f);
    const auto lip = 5.0f - travel;

    // Slot in the plate.
    const auto slot = key.expanded (2.0f, 2.0f).withTrimmedBottom (-4.0f);
    g.setColour (juce::Colour (0xff0c0b0a).withAlpha (0.85f));
    g.fillRoundedRectangle (slot, 2.5f);

    const auto top = key.translated (0.0f, travel).withTrimmedBottom (juce::jmax (0.0f, lip));
    const auto front = juce::Rectangle<float> (top.getX(), top.getBottom(), top.getWidth(), juce::jmax (0.0f, lip));
    if (! engaged)
        render::softShadow (g, top.withTrimmedTop (top.getHeight() * 0.5f), 2.0f, 5.0f, { 0.0f, 5.0f }, 0.45f);

    // Front face of the key, then the rounded top.
    g.setColour (keyColour.darker (0.9f));
    g.fillRoundedRectangle (front.expanded (0.0f, 1.5f), 2.0f);
    const auto shade = engaged ? 0.22f : 0.0f;
    juce::ColourGradient body (keyColour.brighter (0.10f).darker (shade), top.getX(), top.getY(),
                               keyColour.darker (0.18f + shade), top.getX(), top.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (top, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.45f : 0.28f));
    g.drawLine (top.getX() + 2.0f, top.getY() + 0.8f, top.getRight() - 2.0f, top.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (top, 2.5f, 0.8f);

    const auto ink = keyColour.getPerceivedBrightness() > 0.5f ? juce::Colour (0xff1b1a18) : juce::Colour (0xffece8df);
    if (! glyph.isEmpty())
        paintGlyph (g, top.reduced (top.getWidth() * 0.22f, top.getHeight() * 0.30f), ink.withAlpha (0.85f), 1.5f);

    if (hasKeyboardFocus (true))
    {
        g.setColour (palette.accent.withAlpha (0.9f));
        g.drawRoundedRectangle (slot.expanded (2.5f), 4.0f, 1.5f);
    }
    paintLegend (g, false);
}

void KeyButton::paintToggle (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& palette = theme.palette;
    auto bounds = getLocalBounds().toFloat();
    const auto upLegend = bounds.removeFromTop (16.0f);
    const auto downLegend = bounds.removeFromBottom (16.0f);
    const auto centre = bounds.getCentre();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight() * 0.62f, 56.0f);
    const auto engaged = getToggleState();

    // Printed positions: up is the engaged state.
    g.setFont (fonts::label (11.5f, 0.18f));
    g.setColour (palette.silkscreen.withMultipliedAlpha (engaged ? 1.0f : 0.72f));
    g.drawText (legendOne.toUpperCase(), upLegend, juce::Justification::centred, false);
    g.setColour (palette.silkscreen.withMultipliedAlpha (engaged ? 0.72f : 1.0f));
    g.drawText (legendTwo.toUpperCase(), downLegend, juce::Justification::centred, false);

    // Washer and hex nut holding the switch to the plate.
    const auto washer = juce::Rectangle<float> (size * 0.62f, size * 0.62f).withCentre (centre);
    render::softShadow (g, washer, washer.getWidth() * 0.5f, 3.0f, { 0.0f, 1.5f }, 0.45f);
    juce::ColourGradient ring (juce::Colour (0xffe9ecef), washer.getX(), washer.getY(),
                               juce::Colour (0xff5a5e64), washer.getRight(), washer.getBottom(), false);
    g.setGradientFill (ring);
    g.fillEllipse (washer);
    juce::Path nut;
    const auto nutRadius = size * 0.24f;
    for (int i = 0; i < 6; ++i)
    {
        const auto a = static_cast<float> (i) * juce::MathConstants<float>::twoPi / 6.0f + 0.26f;
        const auto p = centre + juce::Point<float> (std::cos (a), std::sin (a)) * nutRadius;
        if (i == 0) nut.startNewSubPath (p); else nut.lineTo (p);
    }
    nut.closeSubPath();
    juce::ColourGradient nutShade (juce::Colour (0xfff7f8f9), centre.x - nutRadius, centre.y - nutRadius,
                                   juce::Colour (0xff484c52), centre.x + nutRadius, centre.y + nutRadius, false);
    nutShade.addColour (0.5, juce::Colour (0xffa9aeb4));
    g.setGradientFill (nutShade);
    g.fillPath (nut);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));

    // Bat handle: a tapered chrome lever, seen from the front, thrown up or down.
    const auto direction = engaged ? -1.0f : 1.0f;
    const auto press = down ? 0.9f : 1.0f;
    const auto length = size * 0.62f * press;
    const auto tip = centre.translated (0.0f, direction * length);
    const auto baseWidth = size * 0.17f, tipWidth = size * 0.24f;
    juce::Path bat;
    bat.startNewSubPath (centre.x - baseWidth * 0.5f, centre.y);
    bat.lineTo (tip.x - tipWidth * 0.36f, tip.y);
    bat.lineTo (tip.x + tipWidth * 0.36f, tip.y);
    bat.lineTo (centre.x + baseWidth * 0.5f, centre.y);
    bat.closeSubPath();
    bat.addEllipse (juce::Rectangle<float> (tipWidth, tipWidth * 0.82f).withCentre (tip));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (bat, juce::AffineTransform::translation (size * 0.05f, size * 0.10f));
    juce::ColourGradient chrome (juce::Colour (0xff6d7177), tip.x - tipWidth * 0.5f, tip.y,
                                 juce::Colour (0xff4a4e53), tip.x + tipWidth * 0.5f, tip.y, false);
    chrome.addColour (0.30, juce::Colour (0xfffbfcfd));
    chrome.addColour (0.55, juce::Colour (0xffaeb3b9));
    g.setGradientFill (chrome);
    g.fillPath (bat);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.9f : 0.7f));
    g.fillEllipse (juce::Rectangle<float> (tipWidth * 0.36f, tipWidth * 0.22f).withCentre (tip.translated (-tipWidth * 0.14f, -tipWidth * 0.12f)));
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.strokePath (bat, juce::PathStrokeType (0.7f));

    if (hasKeyboardFocus (true))
    {
        g.setColour (palette.accent.withAlpha (0.9f));
        g.drawEllipse (washer.expanded (4.0f), 1.5f);
    }
}

void KeyButton::paintSilverSquare (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& palette = theme.palette;
    const auto key = getKeyBounds();
    const auto engaged = getToggleState();
    const auto cap = key.reduced (down ? 1.5f : (engaged ? 0.8f : 0.0f));

    render::softShadow (g, cap, 3.0f, 5.0f, { 0.0f, engaged ? 1.5f : 3.0f }, 0.7f);
    // Polished aluminium: bright where it faces the softbox, a dark band where it mirrors the room.
    juce::ColourGradient metal (juce::Colour (0xfff2f4f6), cap.getX(), cap.getY(),
                                juce::Colour (0xff7b8087), cap.getX(), cap.getBottom(), false);
    metal.addColour (0.45, juce::Colour (0xffc7ccd1));
    metal.addColour (0.62, juce::Colour (0xff8f949a));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (cap, 2.5f);
    // Fine vertical brushing.
    g.setColour (juce::Colours::black.withAlpha (0.05f));
    for (float x = cap.getX() + 1.5f; x < cap.getRight() - 1.0f; x += 1.7f)
        g.drawVerticalLine (juce::roundToInt (x), cap.getY() + 1.0f, cap.getBottom() - 1.0f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.95f : 0.75f));
    g.drawLine (cap.getX() + 1.5f, cap.getY() + 0.7f, cap.getRight() - 1.5f, cap.getY() + 0.7f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (engaged ? 0.55f : 0.35f));
    g.drawRoundedRectangle (cap, 2.5f, 0.8f);
    if (engaged)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (cap, 2.5f);
    }

    paintGlyph (g, cap.reduced (cap.getWidth() * 0.24f, cap.getHeight() * 0.30f), juce::Colour (0xff1c1d1f).withAlpha (0.8f), 1.4f);

    if (hasKeyboardFocus (true))
    {
        g.setColour (palette.accent.withAlpha (0.9f));
        g.drawRoundedRectangle (cap.expanded (4.0f), 5.0f, 1.5f);
    }
    paintLegend (g, engaged);
}

void KeyButton::paintSoftKey (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& palette = theme.palette;
    const auto key = getKeyBounds();
    const auto engaged = getToggleState();
    const auto cap = key.reduced (down ? 1.0f : 0.0f);

    render::softShadow (g, cap, 6.0f, 6.0f, { 0.0f, 2.5f }, 0.65f);
    juce::ColourGradient rubber (juce::Colour (0xff34363b), cap.getX(), cap.getY(),
                                 juce::Colour (0xff1c1d20), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (rubber);
    g.fillRoundedRectangle (cap, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.14f : 0.08f));
    g.drawRoundedRectangle (cap.reduced (0.5f), 6.0f, 1.0f);

    // LED strip along the top edge of the key.
    const auto strip = juce::Rectangle<float> (cap.getWidth() * 0.5f, 3.0f).withCentre ({ cap.getCentreX(), cap.getY() + 6.0f });
    g.setColour (engaged ? lampColour : lampColour.withMultipliedSaturation (0.4f).withMultipliedBrightness (0.25f));
    g.fillRoundedRectangle (strip, 1.5f);
    if (engaged)
        render::halo (g, strip.getCentre(), strip.getWidth() * 0.9f, lampColour, 0.5f);

    paintGlyph (g, cap.withTrimmedTop (12.0f).reduced (cap.getWidth() * 0.26f, cap.getHeight() * 0.14f),
                juce::Colour (0xffeceef0).withAlpha (engaged ? 0.95f : 0.65f), 1.4f);

    if (hasKeyboardFocus (true))
    {
        g.setColour (palette.accent.withAlpha (0.9f));
        g.drawRoundedRectangle (cap.expanded (3.0f), 8.0f, 1.5f);
    }
    paintLegend (g, false);
}

//==============================================================================
SelectorKeys::SelectorKeys (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
    : parameter (*state.getParameter (parameterID)),
      attachment (parameter, [this] (float value) { select (juce::roundToInt (value)); }, state.undoManager)
{
    setComponentID (parameterID);
    setTitle (parameter.getName (64));
    setName (parameter.getName (64));
    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);

    const auto choices = parameter.getAllValueStrings();
    const auto groupId = 0x5e1ec700 + parameter.getParameterIndex();
    for (int i = 0; i < choices.size(); ++i)
    {
        auto* key = keys.add (std::make_unique<KeyButton> (choices[i]));
        key->setTitle (choices[i]);
        key->setDescription (parameter.getName (64));
        key->setRadioGroupId (groupId);
        key->setClickingTogglesState (false);
        key->setLegend (choices[i]);
        key->setKeySize (40.0f, 30.0f);
        key->onClick = [this, i] { attachment.setValueAsCompleteGesture (static_cast<float> (i)); };
        key->onArrow = [this, i] (int direction)
        {
            const auto next = juce::jlimit (0, keys.size() - 1, i + direction);
            if (next != i)
            {
                attachment.setValueAsCompleteGesture (static_cast<float> (next));
                keys[next]->grabKeyboardFocus();
            }
        };
        addAndMakeVisible (key);
    }
    attachment.sendInitialUpdate();
}

SelectorKeys::~SelectorKeys() = default;

void SelectorKeys::setTheme (const Theme& t)
{
    for (auto* key : keys)
    {
        key->setTheme (t);
        key->setLampColour (t.palette.accent);
    }
}

void SelectorKeys::setColumns (int count)
{
    columns = juce::jmax (0, count);
    resized();
}

void SelectorKeys::setLegends (const juce::StringArray& legends)
{
    for (int i = 0; i < keys.size() && i < legends.size(); ++i)
        keys[i]->setLegend (legends[i]);
}

void SelectorKeys::setGlyphs (const std::vector<juce::Path>& paths)
{
    for (int i = 0; i < keys.size() && i < static_cast<int> (paths.size()); ++i)
        keys[i]->setGlyph (paths[static_cast<size_t> (i)]);
}

void SelectorKeys::setKeySize (float width, float height)
{
    for (auto* key : keys)
        key->setKeySize (width, height);
}

void SelectorKeys::setStyle (KeyButton::Style style)
{
    for (auto* key : keys)
        key->setStyle (style);
}

void SelectorKeys::setKeyColour (juce::Colour colour)
{
    for (auto* key : keys)
        key->setKeyColour (colour);
}

void SelectorKeys::select (int index)
{
    index = juce::jlimit (0, keys.size() - 1, index);
    for (int i = 0; i < keys.size(); ++i)
        keys[i]->setToggleState (i == index, juce::dontSendNotification);
    if (index != selected)
    {
        selected = index;
        if (onChange != nullptr)
            onChange (index);
    }
}

std::unique_ptr<juce::AccessibilityHandler> SelectorKeys::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
}

void SelectorKeys::resized()
{
    const auto bounds = getLocalBounds();
    const auto cols = columns > 0 ? juce::jmin (columns, keys.size()) : keys.size();
    const auto rows = (keys.size() + cols - 1) / juce::jmax (1, cols);
    const auto width = bounds.getWidth() / juce::jmax (1, cols);
    const auto height = bounds.getHeight() / juce::jmax (1, rows);
    for (int i = 0; i < keys.size(); ++i)
        keys[i]->setBounds (bounds.getX() + (i % cols) * width, bounds.getY() + (i / cols) * height, width, height);
}

//==============================================================================
namespace glyphs
{
juce::Path sine()
{
    juce::Path p;
    for (int i = 0; i <= 48; ++i)
    {
        const auto x = static_cast<float> (i) / 48.0f;
        const auto y = 0.5f - 0.5f * std::sin (x * juce::MathConstants<float>::twoPi);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

juce::Path triangle()
{
    juce::Path p;
    p.startNewSubPath (0.0f, 0.5f);
    p.lineTo (0.25f, 0.0f);
    p.lineTo (0.75f, 1.0f);
    p.lineTo (1.0f, 0.5f);
    return p;
}

juce::Path square()
{
    juce::Path p;
    p.startNewSubPath (0.0f, 1.0f);
    p.lineTo (0.0f, 0.0f);
    p.lineTo (0.5f, 0.0f);
    p.lineTo (0.5f, 1.0f);
    p.lineTo (1.0f, 1.0f);
    p.lineTo (1.0f, 0.0f);
    return p;
}

juce::Path saw()
{
    juce::Path p;
    p.startNewSubPath (0.0f, 1.0f);
    p.lineTo (0.5f, 0.0f);
    p.lineTo (0.5f, 1.0f);
    p.lineTo (1.0f, 0.0f);
    p.lineTo (1.0f, 1.0f);
    return p;
}
} // namespace glyphs
} // namespace affine
