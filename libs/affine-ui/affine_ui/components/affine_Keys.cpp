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

    if (! glyph.isEmpty())
    {
        const auto area = cap.withTrimmedTop (cap.getHeight() * 0.50f).reduced (cap.getWidth() * 0.26f, cap.getHeight() * 0.12f);
        auto shape = glyph;
        shape.applyTransform (shape.getTransformToScaleToFit (area, false));
        g.setColour (palette.silkscreen.withAlpha (engaged ? 0.95f : 0.72f));
        g.strokePath (shape, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (hasKeyboardFocus (true))
    {
        g.setColour (palette.accent.withAlpha (0.9f));
        g.drawRoundedRectangle (well.expanded (2.5f), 8.0f, 1.5f);
    }

    // Printed legend under the key.
    auto legend = getLocalBounds().toFloat();
    legend = legend.removeFromBottom (legendTwo.isNotEmpty() ? 30.0f : 18.0f);
    g.setColour (palette.silkscreen);
    g.setFont (fonts::label (12.0f, 0.16f));
    g.drawText (legendOne.toUpperCase(), legend.removeFromTop (15.0f), juce::Justification::centred, false);
    if (legendTwo.isNotEmpty())
    {
        g.setColour (palette.silkscreenDim);
        g.setFont (fonts::label (10.0f, 0.18f));
        g.drawText (legendTwo.toUpperCase(), legend.removeFromTop (13.0f), juce::Justification::centred, false);
    }
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
