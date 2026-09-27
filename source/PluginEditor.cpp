#include "PluginEditor.h"
#include "NoteNames.h"
#include "ParameterIDs.h"

namespace
{
constexpr float pitchConfidence = 0.1f;
constexpr double freshnessMs = 400.0;

const juce::Rectangle<int> modeArea { 690, 22, 170, 64 };
const juce::Rectangle<int> inputArea { 40, 118, 330, 124 };
const juce::Rectangle<int> carrierArea { 412, 118, 448, 124 };
const juce::Rectangle<float> trackingFrame { 32.0f, 274.0f, 240.0f, 194.0f };
const juce::Rectangle<float> carrierFrame { 284.0f, 274.0f, 424.0f, 194.0f };
const juce::Rectangle<float> outputFrame { 720.0f, 274.0f, 148.0f, 194.0f };
const juce::Rectangle<int> waveformArea { 578, 306, 124, 150 };
constexpr int knobCentreY = 374;

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
    t.panel.base = juce::Colour(0xff1f3538);
    t.panel.brushing = 0.45f;
    t.knob.pointer = juce::Colour(0xff1c1f22);
    t.palette.accent = juce::Colour(0xffff6a1c);
    t.palette.glassTint = juce::Colour(0xff111615);
    return t;
}

affine::Theme RingmodTheme::inputDisplayTheme()
{
    auto t = theme();
    t.palette.accent = juce::Colour(0xff5cf2d6);
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
    setTooltip("Tracked input pitch. The carrier runs once a detection clears the Sensitivity mark on the confidence bar.");
}

void PitchDisplay::setTheme(const affine::Theme& t)
{
    theme = t;
    repaint();
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
    affine::render::glass(g, bounds, theme.palette);

    const auto vfd = theme.palette.accent;
    const auto unlit = vfd.withMultipliedSaturation(0.45f).withMultipliedBrightness(0.12f);
    const auto legendColour = [&](bool lit) { return lit ? vfd.withAlpha(0.9f) : unlit.brighter(0.35f); };

    // Fourteen-segment note, its unlit segments faintly visible behind it.
    const auto noteArea = juce::Rectangle<float>(20.0f, 16.0f, 150.0f, 58.0f);
    const auto noteFont = affine::fonts::segment(44.0f, 0.05f);
    g.setColour(unlit);
    g.setFont(noteFont);
    g.drawText("~~~", noteArea, juce::Justification::centredLeft, false);

    juce::String note;
    juce::String detail;
    if (!trackingMode)
    {
        note = "OFF";
        detail = "MANUAL CARRIER";
    }
    else if (!live)
    {
        detail = "NO AUDIO";
    }
    else if (hasPitch())
    {
        note = NoteNames::fromFrequency(pitchHz).c_str();
        detail = juce::String(pitchHz, 1) + " HZ";
    }
    else
    {
        detail = "LISTENING";
    }

    const auto strength = hasPitch() && !locked ? 0.45f : 1.0f;
    noteGlow.draw(g, note, noteFont, noteArea, juce::Justification::centredLeft, vfd.withMultipliedAlpha(strength), 3.0f, 0.9f);
    detailGlow.draw(g, detail, affine::fonts::readout(15.0f, 0.08f), { 22.0f, 82.0f, 190.0f, 22.0f },
                    juce::Justification::centredLeft, vfd.withMultipliedAlpha(trackingMode && !live ? 0.55f : 0.95f), 2.2f, 0.6f);

    // Lock lamp: lit only while the tracked carrier is actually running.
    const auto column = juce::Rectangle<float>(214.0f, 20.0f, 100.0f, 84.0f);
    affine::render::lamp(g, { column.getX() + 6.0f, column.getY() + 8.0f }, 7.0f, vfd, locked ? 1.0f : 0.0f);
    g.setFont(affine::fonts::label(11.0f, 0.22f));
    g.setColour(legendColour(locked));
    g.drawText("LOCK", juce::Rectangle<float>(column.getX() + 18.0f, column.getY() + 1.0f, 70.0f, 14.0f),
               juce::Justification::centredLeft, false);

    g.setColour(legendColour(trackingMode && live));
    g.drawText("CONFIDENCE", juce::Rectangle<float>(column.getX(), column.getY() + 30.0f, 100.0f, 14.0f),
               juce::Justification::centredLeft, false);

    const auto bar = juce::Rectangle<float>(column.getX(), column.getY() + 48.0f, 96.0f, 12.0f);
    const auto segments = 10;
    const auto gap = 2.4f;
    const auto segmentWidth = (bar.getWidth() - gap * static_cast<float>(segments - 1)) / static_cast<float>(segments);
    const auto level = trackingMode && live ? juce::jlimit(0.0f, 1.0f, confidenceValue) : 0.0f;
    for (int i = 0; i < segments; ++i)
    {
        const auto segment = juce::Rectangle<float>(bar.getX() + static_cast<float>(i) * (segmentWidth + gap), bar.getY(),
                                                    segmentWidth, bar.getHeight());
        const auto lit = juce::jlimit(0.0f, 1.0f, level * static_cast<float>(segments) - static_cast<float>(i));
        if (lit > 0.0f)
        {
            g.setColour(vfd.withAlpha(0.16f * lit));
            g.fillRoundedRectangle(segment.expanded(2.0f), 2.5f);
        }
        g.setColour(unlit.interpolatedWith(vfd, lit));
        g.fillRoundedRectangle(segment, 1.2f);
    }

    // The Sensitivity threshold a detection must clear before the carrier follows it.
    if (trackingMode)
    {
        const auto x = bar.getX() + bar.getWidth() * juce::jlimit(0.0f, 1.0f, threshold);
        juce::Path marker;
        marker.addTriangle(x, bar.getBottom() + 3.0f, x - 3.5f, bar.getBottom() + 9.0f, x + 3.5f, bar.getBottom() + 9.0f);
        g.setColour(vfd.withAlpha(live ? 0.85f : 0.4f));
        g.fillPath(marker);
    }
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
    setTooltip("Frequency the carrier oscillator is running at. Dark while the tracker has no pitch and the effect stays dry.");
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
    counter.setBounds(16, 10, 262, getHeight() - 20);
}

std::unique_ptr<juce::AccessibilityHandler> CarrierDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

void CarrierDisplay::paint(juce::Graphics& g)
{
    affine::render::glass(g, getLocalBounds().toFloat(), theme.palette);

    const auto neon = theme.palette.accent;
    const auto running = carrier > 0.0f;
    const auto unitArea = juce::Rectangle<float>(286.0f, 74.0f, 40.0f, 26.0f);
    unit.draw(g, "Hz", unitArea, neon.withMultipliedAlpha(running ? 1.0f : 0.25f), 2.5f, running ? 0.8f : 0.0f,
              [&](juce::Graphics& lg)
              {
                  lg.setFont(affine::fonts::wordmark(14.0f, 0.06f));
                  lg.drawText("Hz", unitArea.withZeroOrigin(), juce::Justification::centredLeft, false);
              });

    // A small scope screen tracing the selected carrier shape.
    const auto scope = juce::Rectangle<float>(338.0f, 20.0f, 94.0f, getHeight() - 40.0f);
    g.setColour(juce::Colour(0xff0a0e0d));
    g.fillRoundedRectangle(scope, 3.0f);
    g.setColour(neon.withAlpha(0.10f));
    for (int i = 1; i < 4; ++i)
    {
        const auto x = scope.getX() + scope.getWidth() * static_cast<float>(i) / 4.0f;
        g.drawVerticalLine(juce::roundToInt(x), scope.getY() + 2.0f, scope.getBottom() - 2.0f);
    }
    for (int i = 1; i < 4; ++i)
    {
        const auto y = scope.getY() + scope.getHeight() * static_cast<float>(i) / 4.0f;
        g.drawHorizontalLine(juce::roundToInt(y), scope.getX() + 2.0f, scope.getRight() - 2.0f);
    }
    const auto traceArea = scope.reduced(9.0f, 16.0f);
    trace.draw(g, "wave" + juce::String(waveform), traceArea, neon.withMultipliedAlpha(running ? 1.0f : 0.3f), 2.5f,
               running ? 0.9f : 0.0f,
               [&](juce::Graphics& lg)
               {
                   auto path = twoCycles(waveform);
                   path.applyTransform(path.getTransformToScaleToFit(traceArea.withZeroOrigin(), false));
                   lg.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
               });
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

    pitchDisplay.setTheme(RingmodTheme::inputDisplayTheme());
    carrierDisplay.setTheme(theme);
    addAndMakeVisible(pitchDisplay);
    addAndMakeVisible(carrierDisplay);

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
    modeKeys.setKeySize(46.0f, 28.0f);
    waveformKeys.setLegends({ "Sine", "Tri", "Square", "Saw" });
    waveformKeys.setGlyphs({ affine::glyphs::sine(), affine::glyphs::triangle(),
                             affine::glyphs::square(), affine::glyphs::saw() });
    waveformKeys.setKeySize(40.0f, 30.0f);
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
    modeKeys.setTheme(theme);
    waveformKeys.setTheme(theme);

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
}

void HdnRingmodAudioProcessorEditor::print(juce::Graphics& g)
{
    using namespace affine::silkscreen;
    const auto& palette = theme.palette;

    wordmark(g, "Ring Modulator", "HDN  /  Pitch-tracking carrier", { 40.0f, 26.0f }, palette);
    legend(g, "Source", modeArea.toFloat().withWidth(76.0f).withX(static_cast<float>(modeArea.getX()) - 80.0f)
                                 .withHeight(30.0f).translated(0.0f, 4.0f),
           palette, juce::Justification::centredRight, true);
    groove(g, 32.0f, static_cast<float>(width) - 32.0f, 94.0f);

    legend(g, "Input pitch", inputArea.toFloat().withHeight(14.0f).translated(0.0f, -18.0f), palette);
    legend(g, "Carrier frequency", carrierArea.toFloat().withHeight(14.0f).translated(0.0f, -18.0f), palette);

    g.setColour(palette.silkscreen);
    g.setFont(affine::fonts::wordmark(22.0f, 0.0f));
    g.drawText(juce::String(juce::CharPointer_UTF8("\xc3\x97")),
               juce::Rectangle<float>(static_cast<float>(inputArea.getRight()), 164.0f,
                                      static_cast<float>(carrierArea.getX() - inputArea.getRight()), 26.0f),
               juce::Justification::centred, false);

    frame(g, trackingFrame, "Tracking", palette);
    frame(g, carrierFrame, "Carrier", palette);
    frame(g, outputFrame, "Output", palette);
    legend(g, "Waveform", waveformArea.toFloat().withHeight(14.0f).translated(0.0f, -16.0f), palette,
           juce::Justification::centred, true);

    makersMark(g, { 34.0f, static_cast<float>(height) - 10.0f }, palette);
}

void HdnRingmodAudioProcessorEditor::resized()
{
    modeKeys.setBounds(modeArea);
    pitchDisplay.setBounds(inputArea);
    carrierDisplay.setBounds(carrierArea);

    const auto place = [](affine::Knob& knob, int centreX)
    {
        knob.setBounds(knob.getBoundsForCentre({ centreX, knobCentreY }));
    };

    place(smoothingKnob, 92);
    place(sensitivityKnob, 212);
    place(rateMultKnob, 358);
    place(manualRateKnob, 500);
    waveformKeys.setBounds(waveformArea);
    place(mixKnob, 794);
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
    const auto sensitivity = processorRef.apvts.getRawParameterValue(ParameterIDs::sensitivity)->load() / 100.0f;

    pitchDisplay.setState(processorRef.currentPitchHz.load(std::memory_order_relaxed),
                          processorRef.currentConfidence.load(std::memory_order_relaxed),
                          pitchTracking, manualRate, sensitivity, live && pitchTracking && carrierHz > 0.0f, live);
    carrierDisplay.setState(pitchTracking ? (live ? carrierHz : 0.0f) : manualRate, waveformKeys.getSelectedIndex());
    updateModePresentation();
}
