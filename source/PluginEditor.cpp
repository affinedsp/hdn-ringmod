#include "PluginEditor.h"
#include "NoteNames.h"
#include "ParameterIDs.h"

namespace
{
constexpr float pitchConfidence = 0.1f;
constexpr double freshnessMs = 400.0;

// Layout, in logical pixels. The rack ears frame an 880 px panel.
constexpr float earWidth = 40.0f;
const juce::Rectangle<float> nameplateArea { 64.0f, 22.0f, 300.0f, 42.0f };
const juce::Rectangle<int> modeArea { 716, 18, 180, 66 };
const juce::Rectangle<int> inputArea { 64, 134, 212, 146 };
const juce::Point<float> lockLamp { 316.0f, 146.0f };
const juce::Rectangle<int> confidenceArea { 292, 160, 158, 124 };
const juce::Rectangle<int> carrierArea { 480, 134, 416, 146 };
const juce::Rectangle<float> trackingFrame { 64.0f, 316.0f, 228.0f, 224.0f };
const juce::Rectangle<float> carrierFrame { 304.0f, 316.0f, 408.0f, 224.0f };
const juce::Rectangle<float> outputFrame { 724.0f, 316.0f, 172.0f, 224.0f };
const juce::Rectangle<int> waveformArea { 594, 350, 112, 176 };
constexpr int knobCentreY = 424;

const juce::Colour lockGreen { 0xff49d16b };
const juce::Colour phosphor { 0xff7dffa0 };

juce::String whole(double value)
{
    return juce::String(juce::roundToInt(value));
}

juce::Path twoCycles(int waveform)
{
    const juce::Path shapes[] = { affine::glyphs::sine(), affine::glyphs::triangle(),
                                  affine::glyphs::square(), affine::glyphs::saw() };
    auto cycle = shapes[juce::jlimit(0, 3, waveform)];
    juce::Path path;
    path.addPath(cycle, juce::AffineTransform::scale(0.5f, 1.0f));
    path.addPath(cycle, juce::AffineTransform::scale(0.5f, 1.0f).translated(0.5f, 0.0f));
    return path;
}
}

affine::Theme RingmodTheme::theme()
{
    affine::Theme t;
    t.panel.texture = affine::PanelFinish::Texture::hammertone;
    t.panel.base = juce::Colour(0xffa9b0b5);
    t.panel.grain = 0.035f;
    t.panel.mottle = 0.12f;
    t.panel.sheen = 0.8f;
    t.panel.dimple = 11.0f;

    using Material = affine::KnobFinish::Material;
    t.knob.cap = juce::Colour(0xff141414);
    t.knob.body = juce::Colour(0xff121212);
    t.knob.capMaterial = Material::glossPlastic;
    t.knob.bodyMaterial = Material::glossPlastic;
    t.knob.pointer = juce::Colour(0xfff2efe6);
    t.knob.index = juce::Colour(0xfff2efe6);
    t.knob.capRatio = 0.66f;
    t.knob.gripRatio = 0.86f;
    t.knob.capDome = 0.28f;
    t.knob.ridges = 20;
    t.knob.ridgeDepth = 0.9f;

    auto& p = t.palette;
    p.silkscreen = juce::Colour(0xff1b1a17);
    p.silkscreenDim = juce::Colour(0xff25231f);
    p.accent = juce::Colour(0xffff6a1c);
    p.attention = juce::Colour(0xffffb238);
    p.danger = juce::Colour(0xffe5483b);
    p.glassTint = juce::Colour(0xff111615);
    p.lamp = affine::Palette::Lamp::jewel;
    p.readout = affine::Palette::Readout::backlit;
    p.readoutBacklight = juce::Colour(0xfff1d58c);
    p.readoutInk = juce::Colour(0xff1f1a12);
    return t;
}

//==============================================================================
class PitchDisplay::ValueInterface final : public juce::AccessibilityValueInterface
{
public:
    explicit ValueInterface(PitchDisplay& displayToUse) : display(displayToUse) {}

    bool isReadOnly() const override { return true; }
    double getCurrentValue() const override
    {
        return display.trackingMode ? display.pitchHz : display.manualRate;
    }
    juce::String getCurrentValueAsString() const override
    {
        return display.getAccessibleValueText();
    }
    void setValue(double) override {}
    void setValueAsString(const juce::String&) override {}
    AccessibleValueRange getRange() const override { return {}; }

private:
    PitchDisplay& display;
};

PitchDisplay::PitchDisplay()
{
    setTitle("Input pitch");
    setTooltip("Tracked input pitch. The carrier runs once a detection reaches the green zone of the confidence meter, set by Sensitivity.");
    note.setCharacters(affine::NixieDisplay::Characters::alphanumeric);
    note.setNumTubes(3);
    addAndMakeVisible(note);
}

void PitchDisplay::setTheme(const affine::Theme& t)
{
    theme = t;
    note.setTheme(t);
    repaint();
}

void PitchDisplay::resized()
{
    note.setBounds(16, 8, 180, 94);
}

bool PitchDisplay::hasPitch() const
{
    return trackingMode && live && pitchHz > 0.0f && confidenceValue > pitchConfidence;
}

void PitchDisplay::setState(float detectedPitchHz, float confidence, bool pitchTracking, float manualRateHz,
                            float acceptanceThreshold, bool carrierRunning, bool audioLive)
{
    auto pitchChanged = juce::roundToInt(pitchHz * 10.0f) != juce::roundToInt(detectedPitchHz * 10.0f);
    auto confidenceChanged = juce::roundToInt(confidenceValue * 100.0f) != juce::roundToInt(confidence * 100.0f);
    auto thresholdChanged = juce::roundToInt(threshold * 100.0f) != juce::roundToInt(acceptanceThreshold * 100.0f);
    auto manualRateChanged = juce::roundToInt(manualRate * 10.0f) != juce::roundToInt(manualRateHz * 10.0f);
    auto stateChanged = trackingMode != pitchTracking || locked != carrierRunning || live != audioLive;

    if (!pitchChanged && !confidenceChanged && !thresholdChanged && !manualRateChanged && !stateChanged)
        return;

    auto previousAccessibleValue = getAccessibleValueText();
    pitchHz = detectedPitchHz;
    confidenceValue = confidence;
    threshold = acceptanceThreshold;
    manualRate = manualRateHz;
    trackingMode = pitchTracking;
    locked = carrierRunning;
    live = audioLive;

    note.setText(hasPitch() ? juce::String(NoteNames::fromFrequency(pitchHz).c_str()) : juce::String());
    note.setIntensity(hasPitch() ? (locked ? 1.0f : 0.45f) : 0.0f);

    auto accessibleValue = getAccessibleValueText();
    setDescription(accessibleValue);
    repaint();

    if (accessibleValue != previousAccessibleValue)
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

juce::String PitchDisplay::getAccessibleValueText() const
{
    if (!trackingMode)
        return "Manual carrier, " + juce::String(manualRate, 1) + " hertz";

    if (!live)
        return "No audio";

    if (hasPitch())
        return juce::String(NoteNames::fromFrequency(pitchHz).c_str()) + ", "
             + juce::String(pitchHz, 1) + " hertz, " + (locked ? "locked" : "below sensitivity")
             + ", confidence " + juce::String(juce::roundToInt(confidenceValue * 100.0f)) + " percent";

    return "Listening for input pitch";
}

std::unique_ptr<juce::AccessibilityHandler> PitchDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

void PitchDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    affine::render::glass(g, bounds.withHeight(110.0f), theme.palette);

    juce::String detail;
    float level = 1.0f;
    if (!trackingMode)
    {
        detail = "MANUAL CARRIER";
        level = 0.55f;
    }
    else if (!live)
    {
        detail = "NO AUDIO";
        level = 0.0f;
    }
    else if (hasPitch())
    {
        detail = juce::String(pitchHz, 1) + " HZ";
    }
    else
    {
        detail = "LISTENING";
        level = 0.55f;
    }

    // An amber-lit legend window under the tubes.
    const auto window = juce::Rectangle<float>(0.0f, 118.0f, bounds.getWidth(), 26.0f);
    affine::render::litWindow(g, window, theme.palette.readoutBacklight, level);
    g.setColour(theme.palette.readoutInk.withMultipliedAlpha(0.35f + 0.6f * level));
    g.setFont(affine::fonts::readout(15.0f, 0.08f));
    g.drawText(detail, window, juce::Justification::centred, false);
}

//==============================================================================
class CarrierDisplay::ValueInterface final : public juce::AccessibilityValueInterface
{
public:
    explicit ValueInterface(CarrierDisplay& displayToUse) : display(displayToUse) {}

    bool isReadOnly() const override { return true; }
    double getCurrentValue() const override { return display.carrier; }
    juce::String getCurrentValueAsString() const override
    {
        return display.carrier > 0.0f ? juce::String(display.carrier, 1) + " hertz" : "Carrier off";
    }
    void setValue(double) override {}
    void setValueAsString(const juce::String&) override {}
    AccessibleValueRange getRange() const override { return {}; }

private:
    CarrierDisplay& display;
};

CarrierDisplay::CarrierDisplay()
{
    setTitle("Carrier frequency");
    setTooltip("Frequency the carrier oscillator is running at, with its waveform on the scope. Dark while the tracker has no pitch and the effect stays dry.");
    counter.setNumTubes(6);
    addAndMakeVisible(counter);
}

void CarrierDisplay::setTheme(const affine::Theme& t)
{
    theme = t;
    counter.setTheme(t);
    repaint();
}

juce::String CarrierDisplay::formatFrequency(float hz)
{
    return hz > 0.0f ? juce::String(juce::jmin(hz, 99999.9f), 1) : juce::String();
}

void CarrierDisplay::setState(float carrierHz, int waveformIndex)
{
    const auto changed = juce::roundToInt(carrier * 10.0f) != juce::roundToInt(carrierHz * 10.0f);
    if (!changed && waveformIndex == waveform)
        return;

    const auto wasRunning = carrier > 0.0f;
    carrier = carrierHz;
    waveform = waveformIndex;
    counter.setText(formatFrequency(carrier));
    counter.setIntensity(carrier > 0.0f ? 1.0f : 0.0f);
    repaint();

    if (wasRunning != (carrier > 0.0f))
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

void CarrierDisplay::resized()
{
    counter.setBounds(14, 10, 250, getHeight() - 20);
}

std::unique_ptr<juce::AccessibilityHandler> CarrierDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

void CarrierDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    affine::render::glass(g, bounds, theme.palette);

    const auto neon = theme.palette.accent;
    const auto running = carrier > 0.0f;
    const auto unitArea = juce::Rectangle<float>(268.0f, 94.0f, 30.0f, 24.0f);
    unit.draw(g, "Hz", unitArea, neon.withMultipliedAlpha(running ? 1.0f : 0.25f), 2.5f, running ? 0.8f : 0.0f,
              [&](juce::Graphics& lg)
              {
                  lg.setFont(affine::fonts::wordmark(13.0f, 0.06f));
                  lg.drawText("Hz", unitArea.withZeroOrigin(), juce::Justification::centredLeft, false);
              });

    // A round phosphor scope tracing the selected carrier shape.
    const auto crt = juce::Rectangle<float>(112.0f, 112.0f).withCentre({ bounds.getRight() - 64.0f, bounds.getCentreY() });
    juce::ColourGradient bezel(juce::Colour(0xff3b3f44), crt.getX(), crt.getY(), juce::Colour(0xff0b0c0e), crt.getX(), crt.getBottom(), false);
    g.setGradientFill(bezel);
    g.fillEllipse(crt.expanded(5.0f));
    const auto screen = crt.reduced(4.0f);
    juce::ColourGradient glassFace(juce::Colour(0xff0f2616), screen.getCentreX(), screen.getCentreY(),
                                   juce::Colour(0xff030805), screen.getX(), screen.getY(), true);
    g.setGradientFill(glassFace);
    g.fillEllipse(screen);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addEllipse(screen);
        g.reduceClipRegion(clip);
        g.setColour(phosphor.withAlpha(0.12f));
        for (int i = 1; i < 6; ++i)
        {
            const auto x = screen.getX() + screen.getWidth() * static_cast<float>(i) / 6.0f;
            const auto y = screen.getY() + screen.getHeight() * static_cast<float>(i) / 6.0f;
            g.drawVerticalLine(juce::roundToInt(x), screen.getY(), screen.getBottom());
            g.drawHorizontalLine(juce::roundToInt(y), screen.getX(), screen.getRight());
        }
        const auto traceArea = screen.reduced(20.0f, 30.0f);
        trace.draw(g, "wave" + juce::String(waveform), traceArea, phosphor.withMultipliedAlpha(running ? 1.0f : 0.3f), 3.0f,
                   running ? 1.0f : 0.0f,
                   [&](juce::Graphics& lg)
                   {
                       auto path = twoCycles(waveform);
                       path.applyTransform(path.getTransformToScaleToFit(traceArea.withZeroOrigin(), false));
                       lg.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                   });
        juce::ColourGradient reflection(juce::Colours::white.withAlpha(0.10f), screen.getX(), screen.getY(),
                                        juce::Colours::transparentWhite, screen.getCentreX(), screen.getCentreY(), false);
        g.setGradientFill(reflection);
        g.fillEllipse(screen);
    }
}

//==============================================================================
HdnRingmodAudioProcessorEditor::HdnRingmodAudioProcessorEditor(HdnRingmodAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      processorRef(processor),
      theme(RingmodTheme::theme()),
      lookAndFeel(theme),
      smoothingKnob(processor.apvts, ParameterIDs::smoothing, "How quickly the carrier follows pitch changes."),
      sensitivityKnob(processor.apvts, ParameterIDs::sensitivity,
                      "Minimum detection confidence the tracker accepts; higher values require clearer notes."),
      rateMultKnob(processor.apvts, ParameterIDs::rateMultiplier, "Carrier frequency as a multiple of the tracked pitch."),
      manualRateKnob(processor.apvts, ParameterIDs::manualRate, "Fixed carrier frequency used in Manual mode."),
      mixKnob(processor.apvts, ParameterIDs::mix, "Balance between the dry input and the ring-modulated signal."),
      modeKeys(processor.apvts, ParameterIDs::mode),
      waveformKeys(processor.apvts, ParameterIDs::waveform)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    faceplate.setShowsScrews(false);

    pitchDisplay.setTheme(theme);
    carrierDisplay.setTheme(theme);
    addAndMakeVisible(pitchDisplay);
    addAndMakeVisible(carrierDisplay);

    affine::NeedleMeter::Scale confidence;
    confidence.toPosition = [](float value) { return value; };
    confidence.majors = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    confidence.minors = { 0.125f, 0.375f, 0.625f, 0.875f };
    confidence.format = [](float value) { return whole(value * 100.0); };
    confidence.unit = "%";
    confidence.caption = "Confidence";
    confidenceMeter.setTheme(theme);
    confidenceMeter.setScale(confidence);
    confidenceMeter.setBallistics(4.0f, 0.8f);
    confidenceMeter.setTitle("Confidence");
    addAndMakeVisible(confidenceMeter);

    const auto percent = [](double value) { return whole(value); };
    smoothingKnob.setScale({ 0, 25, 50, 75, 100 }, percent);
    sensitivityKnob.setScale({ 0, 25, 50, 75, 100 }, percent);
    mixKnob.setScale({ 0, 25, 50, 75, 100 }, percent);
    rateMultKnob.setScale({ 0.1, 0.5, 1, 2, 4, 8 },
                          [](double value) { return value < 1.0 ? juce::String(value, 1) : whole(value); });
    manualRateKnob.setScale({ 20, 100, 500, 1000, 5000 },
                            [](double value) { return value >= 1000.0 ? whole(value / 1000.0) + "k" : whole(value); });
    rateMultKnob.setKeyboardStep(0.01);
    manualRateKnob.setKeyboardStep(1.0);

    smoothingKnob.setDiameter(affine::metrics::knobSmall);
    sensitivityKnob.setDiameter(affine::metrics::knobSmall);
    rateMultKnob.setDiameter(affine::metrics::knobLarge);
    manualRateKnob.setDiameter(affine::metrics::knobMedium);
    mixKnob.setDiameter(affine::metrics::knobLarge);

    for (auto* knob : { &smoothingKnob, &sensitivityKnob, &rateMultKnob, &manualRateKnob })
        knob->setShowsActivityLamp(true);

    modeKeys.onChange = [this](int) { updateModePresentation(); };
    modeKeys.setLegends({ "Track", "Manual" });
    modeKeys.setKeySize(52.0f, 30.0f);
    waveformKeys.setLegends({ "Sine", "Tri", "Square", "Saw" });
    waveformKeys.setGlyphs({ affine::glyphs::sine(), affine::glyphs::triangle(),
                             affine::glyphs::square(), affine::glyphs::saw() });
    waveformKeys.setKeySize(44.0f, 34.0f);
    waveformKeys.setColumns(2);

    int order = 1;
    for (juce::Component* control : { static_cast<juce::Component*>(&modeKeys),
                                      static_cast<juce::Component*>(&smoothingKnob),
                                      static_cast<juce::Component*>(&sensitivityKnob),
                                      static_cast<juce::Component*>(&rateMultKnob),
                                      static_cast<juce::Component*>(&manualRateKnob),
                                      static_cast<juce::Component*>(&waveformKeys),
                                      static_cast<juce::Component*>(&mixKnob) })
    {
        control->setExplicitFocusOrder(order++);
        addAndMakeVisible(control);
    }

    for (auto* knob : { &smoothingKnob, &sensitivityKnob, &rateMultKnob, &manualRateKnob, &mixKnob })
        knob->setTheme(theme);
    for (auto* keys : { &modeKeys, &waveformKeys })
    {
        keys->setTheme(theme);
        keys->setStyle(affine::KeyButton::Style::pianoKey);
        keys->setKeyColour(juce::Colour(0xffece5d2));
    }

    lastBlockCount = processorRef.processedBlocks.load(std::memory_order_acquire);
    lastBlockMs = juce::Time::getMillisecondCounterHiRes() - 1000.0;

    setSize(width, height);
    updateModePresentation();
    timerCallback();
    startTimerHz(30);
}

HdnRingmodAudioProcessorEditor::~HdnRingmodAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void HdnRingmodAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    faceplate.paint(graphics, getLocalBounds(), theme, [this](juce::Graphics& plate) { print(plate); });
    affine::render::jewel(graphics, lockLamp, 10.0f, lockGreen, lockLit ? 1.0f : 0.0f);
}

void HdnRingmodAudioProcessorEditor::print(juce::Graphics& g)
{
    using namespace affine::silkscreen;
    const auto& palette = theme.palette;
    const auto w = static_cast<float>(width), h = static_cast<float>(height);

    affine::render::rackEar(g, { 0.0f, 0.0f, earWidth, h }, true);
    affine::render::rackEar(g, { w - earWidth, 0.0f, earWidth, h }, false);
    affine::render::nameplate(g, nameplateArea, "RING MODULATOR", affine::fonts::label(24.0f, 0.36f));
    g.setColour(palette.silkscreenDim);
    g.setFont(affine::fonts::label(12.0f, 0.24f));
    g.drawText("HDN  /  PITCH-TRACKING CARRIER", juce::Rectangle<float>(nameplateArea.getX() + 1.0f, nameplateArea.getBottom() + 6.0f, 320.0f, 14.0f),
               juce::Justification::centredLeft, false);
    legend(g, "Source", modeArea.toFloat().withWidth(70.0f).withX(static_cast<float>(modeArea.getX()) - 76.0f)
                                 .withHeight(32.0f).translated(0.0f, 2.0f),
           palette, juce::Justification::centredRight, true);
    groove(g, earWidth + 12.0f, w - earWidth - 12.0f, 100.0f);

    legend(g, "Input pitch", inputArea.toFloat().withHeight(14.0f).translated(0.0f, -20.0f), palette);
    legend(g, "Carrier frequency", carrierArea.toFloat().withHeight(14.0f).translated(0.0f, -20.0f), palette);
    g.setColour(palette.silkscreen);
    g.setFont(affine::fonts::label(11.5f, 0.24f));
    g.drawText("LOCK", juce::Rectangle<float>(lockLamp.x + 14.0f, lockLamp.y - 7.0f, 60.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setFont(affine::fonts::wordmark(22.0f, 0.0f));
    g.drawText(juce::String(juce::CharPointer_UTF8("\xc3\x97")),
               juce::Rectangle<float>(450.0f, 194.0f, 30.0f, 26.0f), juce::Justification::centred, false);

    frame(g, trackingFrame, "Tracking", palette);
    frame(g, carrierFrame, "Carrier", palette);
    frame(g, outputFrame, "Output", palette);
    legend(g, "Waveform", waveformArea.toFloat().withHeight(14.0f).translated(0.0f, -18.0f), palette,
           juce::Justification::centred, true);

    makersMark(g, { earWidth + 24.0f, h - 12.0f }, palette);
}

void HdnRingmodAudioProcessorEditor::resized()
{
    modeKeys.setBounds(modeArea);
    pitchDisplay.setBounds(inputArea);
    confidenceMeter.setBounds(confidenceArea);
    carrierDisplay.setBounds(carrierArea);

    const auto place = [](affine::Knob& knob, int centreX)
    {
        knob.setBounds(knob.getBoundsForCentre({ centreX, knobCentreY }));
    };

    place(smoothingKnob, 122);
    place(sensitivityKnob, 234);
    place(rateMultKnob, 386);
    place(manualRateKnob, 516);
    waveformKeys.setBounds(waveformArea);
    place(mixKnob, 810);
}

int HdnRingmodAudioProcessorEditor::getControlParameterIndex(juce::Component& component)
{
    for (auto* knob : { &smoothingKnob, &sensitivityKnob, &rateMultKnob, &manualRateKnob, &mixKnob })
        if (&component == knob || knob->isParentOf(&component))
            return knob->parameter.getParameterIndex();

    for (auto* keys : { &modeKeys, &waveformKeys })
        if (&component == keys || keys->isParentOf(&component))
            return keys->parameter.getParameterIndex();

    return -1;
}

void HdnRingmodAudioProcessorEditor::updateModePresentation()
{
    auto modeIndex = modeKeys.getSelectedIndex();
    if (modeIndex == lastModeIndex)
        return;

    lastModeIndex = modeIndex;
    auto pitchTracking = modeIndex != 1;
    rateMultKnob.setInactive(!pitchTracking);
    smoothingKnob.setInactive(!pitchTracking);
    sensitivityKnob.setInactive(!pitchTracking);
    manualRateKnob.setInactive(pitchTracking);
}

void HdnRingmodAudioProcessorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto blocks = processorRef.processedBlocks.load(std::memory_order_acquire);
    if (blocks != lastBlockCount)
    {
        lastBlockCount = blocks;
        lastBlockMs = now;
    }

    const auto live = now - lastBlockMs < freshnessMs;
    const auto pitchTracking = modeKeys.getSelectedIndex() != 1;
    const auto manualRate = static_cast<float>(manualRateKnob.getValue());
    const auto carrierHz = processorRef.currentCarrierHz.load(std::memory_order_relaxed);
    const auto confidence = processorRef.currentConfidence.load(std::memory_order_relaxed);
    const auto sensitivity = processorRef.apvts.getRawParameterValue(ParameterIDs::sensitivity)->load() / 100.0f;
    const auto locked = live && pitchTracking && carrierHz > 0.0f;

    pitchDisplay.setState(processorRef.currentPitchHz.load(std::memory_order_relaxed), confidence,
                          pitchTracking, manualRate, sensitivity, locked, live);
    carrierDisplay.setState(pitchTracking ? (live ? carrierHz : 0.0f) : manualRate, waveformKeys.getSelectedIndex());

    // The green zone of the confidence meter starts at the Sensitivity threshold.
    if (!juce::approximatelyEqual(shownThreshold, sensitivity))
    {
        shownThreshold = sensitivity;
        affine::NeedleMeter::Face face;
        face.backlight = juce::Colour(0xfff2d48a);
        face.ink = juce::Colour(0xff20190f);
        face.zone = juce::Colour(0xff2f9e4f);
        face.zoneFrom = sensitivity;
        face.mirror = false;
        face.bezel = affine::NeedleMeter::Face::Bezel::chrome;
        confidenceMeter.setFace(face);
    }
    confidenceMeter.setReading(confidence, live && pitchTracking);

    if (locked != lockLit)
    {
        lockLit = locked;
        repaint(juce::Rectangle<int>(40, 40).withCentre(lockLamp.toInt()));
    }
    updateModePresentation();
}
