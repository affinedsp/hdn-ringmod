#include "PluginEditor.h"
#include "NoteNames.h"
#include "ParameterIDs.h"

namespace
{
constexpr float pitchConfidence = 0.1f;
constexpr double freshnessMs = 400.0;

// Layout, in logical pixels. Polished end caps hold an 868 px glass front.
constexpr float capWidth = 46.0f;
const juce::Rectangle<int> modeArea { 716, 18, 170, 80 };
const juce::Rectangle<int> carrierArea { 80, 120, 800, 86 };
const juce::Rectangle<int> inputArea { 80, 234, 280, 100 };
const juce::Rectangle<int> signalArea { 378, 222, 224, 124 };
const juce::Rectangle<int> tuningArea { 614, 222, 224, 124 };
const juce::Rectangle<int> waveformArea { 618, 396, 112, 140 };
constexpr int knobCentreY = 458;

const juce::Colour brandGreen { 0xff54f28c }, legendWhite { 0xffe3ecf2 };

juce::String whole(double value)
{
    return juce::String(juce::roundToInt(value));
}

// A group title printed behind the glass, with a lit rule running to its right.
void groupTitle(juce::Graphics& g, const juce::String& title, juce::Rectangle<float> rule, juce::Colour colour)
{
    const auto font = affine::fonts::label(11.5f, 0.26f);
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, title, 0.0f, 0.0f);
    const auto textWidth = glyphs.getBoundingBox(0, -1, true).getWidth();
    affine::silkscreen::litLegend(g, title, rule.withWidth(textWidth + 8.0f).withHeight(14.0f).translated(0.0f, -7.0f),
                                  colour, font, juce::Justification::centredLeft, 0.35f);
    g.setColour(colour.withAlpha(0.18f));
    g.fillRect(rule.withTrimmedLeft(textWidth + 14.0f).withHeight(1.0f));
}
}

affine::Theme RingmodTheme::theme()
{
    affine::Theme t;
    t.panel.texture = affine::PanelFinish::Texture::glass;
    t.panel.base = juce::Colour(0xff050607);
    t.panel.sheen = 1.0f;

    using Material = affine::KnobFinish::Material;
    t.knob.cap = juce::Colour(0xffd4d7db);
    t.knob.body = juce::Colour(0xffc9ccd0);
    t.knob.capMaterial = Material::spunAluminium;
    t.knob.bodyMaterial = Material::polishedAluminium;
    t.knob.pointer = juce::Colour(0xff17181a);
    t.knob.index = juce::Colour(0xff17181a);

    auto& p = t.palette;
    p.silkscreen = legendWhite;
    p.silkscreenDim = juce::Colour(0xff93a2ad);
    p.accent = juce::Colour(0xff4aa8ff);
    p.attention = juce::Colour(0xffffb238);
    p.danger = juce::Colour(0xffff4a3d);
    p.glass = juce::Colour(0xff030405);
    p.glassTint = juce::Colour(0xff0a0e12);
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
    setTooltip("Tracked input pitch. The carrier runs once a detection reaches the green zone of the signal meter, set by Sensitivity.");
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

float PitchDisplay::getCents() const
{
    if (pitchHz <= 0.0f)
        return 0.0f;
    const auto midi = 69.0f + 12.0f * std::log2(pitchHz / 440.0f);
    return (midi - std::round(midi)) * 100.0f;
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
    // Fluorescent characters behind the glass front: no window of their own.
    const auto unlit = brandGreen.withMultipliedSaturation(0.45f).withMultipliedBrightness(0.13f);
    const auto noteArea = juce::Rectangle<float>(0.0f, 4.0f, 150.0f, 58.0f);
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
    noteGlow.draw(g, note, noteFont, noteArea, juce::Justification::centredLeft, brandGreen.withMultipliedAlpha(strength), 3.0f, 0.9f);
    detailGlow.draw(g, detail, affine::fonts::readout(15.0f, 0.08f), { 2.0f, 70.0f, 190.0f, 22.0f },
                    juce::Justification::centredLeft, brandGreen.withMultipliedAlpha(trackingMode && !live ? 0.5f : 0.9f), 2.2f, 0.6f);

    // Lock lamp: lit only while the tracked carrier is actually running.
    const auto lamp = juce::Point<float>(getWidth() - 70.0f, 18.0f);
    affine::render::lamp(g, lamp, 7.0f, brandGreen, locked ? 1.0f : 0.0f);
    g.setFont(affine::fonts::label(11.5f, 0.24f));
    g.setColour(locked ? legendWhite : juce::Colour(0xff4a525a));
    g.drawText("LOCK", juce::Rectangle<float>(lamp.x + 12.0f, lamp.y - 7.0f, 50.0f, 14.0f), juce::Justification::centredLeft, false);
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
    setTooltip("Frequency the carrier oscillator is running at. The pointer parks at the left stop while the tracker has no pitch and the effect stays dry.");
    dial.setRange(20.0f, 20000.0f);
    addAndMakeVisible(dial);
}

void CarrierDisplay::setTheme(const affine::Theme& t)
{
    theme = t;
    dial.setTheme(t);
    repaint();
}

juce::String CarrierDisplay::formatFrequency(float hz)
{
    return hz > 0.0f ? juce::String(juce::jmin(hz, 99999.9f), 1) : juce::String();
}

void CarrierDisplay::setState(float carrierHz, int waveformIndex)
{
    const auto changed = juce::roundToInt(carrier * 10.0f) != juce::roundToInt(carrierHz * 10.0f);
    dial.setFrequency(carrierHz, carrierHz > 0.0f);
    if (!changed && waveformIndex == waveform)
        return;

    const auto wasRunning = carrier > 0.0f;
    carrier = carrierHz;
    waveform = waveformIndex;
    repaint();

    if (wasRunning != (carrier > 0.0f))
        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
}

void CarrierDisplay::resized()
{
    dial.setBounds(0, 0, getWidth() - 184, getHeight());
}

std::unique_ptr<juce::AccessibilityHandler> CarrierDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

void CarrierDisplay::paint(juce::Graphics& g)
{
    const auto accent = theme.palette.accent.interpolatedWith(juce::Colours::white, 0.25f);
    const auto running = carrier > 0.0f;
    const auto area = getLocalBounds().toFloat().withTrimmedLeft(static_cast<float>(getWidth() - 170));
    const auto digits = area.withHeight(52.0f).withY(10.0f);
    const auto font = affine::fonts::segment(30.0f, 0.04f);
    g.setColour(accent.withMultipliedSaturation(0.4f).withMultipliedBrightness(0.16f));
    g.setFont(font);
    g.drawText("~~~~.~", digits, juce::Justification::centredRight, false);
    readout.draw(g, running ? formatFrequency(carrier) : juce::String("----"), font, digits,
                 juce::Justification::centredRight, accent.withMultipliedAlpha(running ? 1.0f : 0.35f), 2.6f, running ? 0.9f : 0.2f);
    unit.draw(g, "HZ", affine::fonts::label(12.0f, 0.3f), area.withTrimmedTop(62.0f).withHeight(16.0f),
              juce::Justification::centredRight, legendWhite.withMultipliedAlpha(0.8f), 1.6f, 0.3f);
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

    affine::NeedleMeter::Face blue;
    blue.backlight = juce::Colour(0xff3a8ef0);
    blue.ink = juce::Colour(0xff08131f);
    blue.mirror = false;
    blue.vignette = 2.4f;
    blue.twinLamps = true;
    blue.bezel = affine::NeedleMeter::Face::Bezel::flush;

    affine::NeedleMeter::Scale signal;
    signal.toPosition = [](float value) { return value; };
    signal.majors = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    signal.minors = { 0.125f, 0.375f, 0.625f, 0.875f };
    signal.format = [](float value) { return whole(value * 100.0); };
    signal.unit = "%";
    signal.caption = "Signal";
    signalMeter.setTheme(theme);
    signalMeter.setScale(signal);
    signalMeter.setFace(blue);
    signalMeter.setBallistics(4.0f, 0.8f);
    signalMeter.setTitle("Signal");
    addAndMakeVisible(signalMeter);

    affine::NeedleMeter::Scale tuning;
    tuning.toPosition = [](float cents) { return (juce::jlimit(-50.0f, 50.0f, cents) + 50.0f) / 100.0f; };
    tuning.majors = { -50.0f, -25.0f, 0.0f, 25.0f, 50.0f };
    tuning.minors = { -40.0f, -30.0f, -20.0f, -10.0f, 10.0f, 20.0f, 30.0f, 40.0f };
    tuning.format = [](float cents) { return cents > 0.0f ? "+" + whole(cents) : whole(cents); };
    tuning.unit = "cents";
    tuning.caption = "Tuning";
    tuning.restPosition = 0.5f;
    tuningMeter.setTheme(theme);
    tuningMeter.setScale(tuning);
    tuningMeter.setFace(blue);
    tuningMeter.setBallistics(3.0f, 0.85f);
    tuningMeter.setTitle("Tuning");
    addAndMakeVisible(tuningMeter);

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
    modeKeys.setKeySize(50.0f, 34.0f);
    waveformKeys.setLegends({ "Sine", "Tri", "Square", "Saw" });
    waveformKeys.setGlyphs({ affine::glyphs::sine(), affine::glyphs::triangle(),
                             affine::glyphs::square(), affine::glyphs::saw() });
    waveformKeys.setKeySize(44.0f, 32.0f);
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
        keys->setStyle(affine::KeyButton::Style::silverSquare);
        for (int i = 0; i < keys->getNumKeys(); ++i)
            keys->getKey(i)->setLampColour(brandGreen);
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
}

void HdnRingmodAudioProcessorEditor::print(juce::Graphics& g)
{
    using namespace affine::silkscreen;
    const auto& palette = theme.palette;
    const auto w = static_cast<float>(width), h = static_cast<float>(height);

    affine::render::endCap(g, { 0.0f, 0.0f, capWidth, h }, true);
    affine::render::endCap(g, { w - capWidth, 0.0f, capWidth, h }, false);

    litLegend(g, "RING MODULATOR", { 80.0f, 22.0f, 460.0f, 34.0f }, brandGreen, affine::fonts::wordmark(24.0f, 0.22f),
              juce::Justification::centredLeft, 0.95f);
    litLegend(g, "HDN  /  PITCH-TRACKING CARRIER", { 82.0f, 58.0f, 360.0f, 14.0f }, palette.silkscreenDim,
              affine::fonts::label(12.0f, 0.24f), juce::Justification::centredLeft, 0.25f);
    litLegend(g, "SOURCE", modeArea.toFloat().withWidth(70.0f).withX(static_cast<float>(modeArea.getX()) - 78.0f).withHeight(36.0f),
              palette.silkscreenDim, affine::fonts::label(11.5f, 0.24f), juce::Justification::centredRight, 0.25f);

    groupTitle(g, "CARRIER", { 80.0f, 106.0f, 800.0f, 1.0f }, legendWhite);
    groupTitle(g, "TRACKING", { 80.0f, 362.0f, 238.0f, 1.0f }, legendWhite);
    groupTitle(g, "CARRIER", { 334.0f, 362.0f, 400.0f, 1.0f }, legendWhite);
    groupTitle(g, "OUTPUT", { 750.0f, 362.0f, 130.0f, 1.0f }, legendWhite);

    makersMark(g, { 80.0f, h - 12.0f }, palette);
}

void HdnRingmodAudioProcessorEditor::resized()
{
    modeKeys.setBounds(modeArea);
    carrierDisplay.setBounds(carrierArea);
    pitchDisplay.setBounds(inputArea);
    signalMeter.setBounds(signalArea);
    tuningMeter.setBounds(tuningArea);

    const auto place = [](affine::Knob& knob, int centreX)
    {
        knob.setBounds(knob.getBoundsForCentre({ centreX, knobCentreY }));
    };

    place(smoothingKnob, 139);
    place(sensitivityKnob, 257);
    place(rateMultKnob, 404);
    place(manualRateKnob, 544);
    waveformKeys.setBounds(waveformArea);
    place(mixKnob, 814);
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

    pitchDisplay.setState(processorRef.currentPitchHz.load(std::memory_order_relaxed), confidence,
                          pitchTracking, manualRate, sensitivity, live && pitchTracking && carrierHz > 0.0f, live);
    carrierDisplay.setState(pitchTracking ? (live ? carrierHz : 0.0f) : manualRate, waveformKeys.getSelectedIndex());

    // The signal meter's green zone starts at the Sensitivity threshold.
    if (!juce::approximatelyEqual(shownThreshold, sensitivity))
    {
        shownThreshold = sensitivity;
        affine::NeedleMeter::Face blue;
        blue.backlight = juce::Colour(0xff3a8ef0);
        blue.ink = juce::Colour(0xff08131f);
        blue.zone = juce::Colour(0xff1f7a3a);
        blue.zoneFrom = sensitivity;
        blue.mirror = false;
        blue.vignette = 2.4f;
        blue.twinLamps = true;
        blue.bezel = affine::NeedleMeter::Face::Bezel::flush;
        signalMeter.setFace(blue);
    }
    signalMeter.setReading(confidence, live && pitchTracking);
    tuningMeter.setReading(pitchDisplay.getCents(), pitchDisplay.hasPitch());
    updateModePresentation();
}
