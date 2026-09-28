#include "PluginEditor.h"
#include "NoteNames.h"
#include "ParameterIDs.h"

namespace
{
constexpr float pitchConfidence = 0.1f;
constexpr double freshnessMs = 400.0;

// Layout, in logical pixels.
const juce::Rectangle<int> modeArea { 752, 18, 172, 64 };
const juce::Rectangle<int> screenArea { 36, 94, 888, 222 };
const juce::Rectangle<int> inputArea { 59, 113, 250, 185 };
const juce::Rectangle<int> carrierArea { 333, 113, 568, 185 };
const juce::Rectangle<int> waveformArea { 644, 370, 128, 152 };
constexpr int knobCentreY = 452;

const juce::Colour screenAmber { 0xffffb23f }, legendWhite { 0xffd8dbdf };

juce::String whole(double value)
{
    return juce::String(juce::roundToInt(value));
}

juce::Colour lit(float level = 1.0f)
{
    return screenAmber.interpolatedWith(juce::Colours::white, 0.2f).withMultipliedAlpha(level);
}

// A caption on the screen, in the screen's colour.
void caption(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
             juce::Justification justification = juce::Justification::centredLeft, float alpha = 0.85f)
{
    g.setColour(screenAmber.withAlpha(alpha));
    g.setFont(affine::fonts::label(12.0f, 0.22f));
    g.drawText(text, area, justification, false);
}

// One cycle of the carrier's waveform at `phase` in [0, 1), as the oscillator computes it.
float carrierShape(int waveform, double phase)
{
    switch (waveform)
    {
        case 1:  return static_cast<float>(2.0 * std::abs(2.0 * phase - 1.0) - 1.0);
        case 2:  return phase < 0.5 ? 1.0f : -1.0f;
        case 3:  return static_cast<float>(2.0 * phase - 1.0);
        default: return static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase));
    }
}

// A group title printed on the plate, with a hairline running to its right.
void groupTitle(juce::Graphics& g, const juce::String& title, juce::Rectangle<float> rule, const affine::Palette& palette)
{
    const auto font = affine::fonts::label(11.5f, 0.26f);
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, title, 0.0f, 0.0f);
    const auto textWidth = glyphs.getBoundingBox(0, -1, true).getWidth();
    g.setColour(palette.silkscreenDim);
    g.setFont(font);
    g.drawText(title, rule.withWidth(textWidth + 8.0f).withHeight(14.0f).translated(0.0f, -7.0f),
               juce::Justification::centredLeft, false);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(rule.withTrimmedLeft(textWidth + 14.0f).withHeight(1.0f));
}
}

affine::Theme RingmodTheme::theme()
{
    affine::Theme t;
    t.panel.texture = affine::PanelFinish::Texture::powder;
    t.panel.base = juce::Colour(0xff17181b);
    t.panel.grain = 0.10f;
    t.panel.mottle = 0.03f;
    t.panel.sheen = 0.5f;

    using Material = affine::KnobFinish::Material;
    t.knob.cap = juce::Colour(0xff34373c);
    t.knob.body = juce::Colour(0xff141518);
    t.knob.capMaterial = Material::spunAluminium;
    t.knob.bodyMaterial = Material::anodised;
    t.knob.pointer = juce::Colour(0xfff2f4f7);
    t.knob.index = juce::Colour(0xfff2f4f7);

    auto& p = t.palette;
    p.silkscreen = legendWhite;
    p.silkscreenDim = juce::Colour(0xff8a9097);
    p.accent = juce::Colour(0xffeef4ff);
    p.attention = juce::Colour(0xffffb238);
    p.danger = juce::Colour(0xffff4a3d);
    p.glass = juce::Colour(0xff050607);
    p.glassTint = juce::Colour(0xff0c0f12);
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
    setTooltip("Tracked input pitch. The carrier runs once the signal bar reaches the Sensitivity mark.");
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
    auto area = getLocalBounds().toFloat();
    auto header = area.removeFromTop(15.0f);
    caption(g, "INPUT", header);

    // Lock lamp: lit only while the tracked carrier is actually running.
    const auto lamp = juce::Point<float>(header.getRight() - 40.0f, header.getCentreY());
    if (locked)
        affine::render::halo(g, lamp, 9.0f, screenAmber, 0.6f);
    g.setColour(locked ? lit() : screenAmber.withAlpha(0.18f));
    g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre(lamp));
    caption(g, "LOCK", header.withLeft(lamp.x + 8.0f), juce::Justification::centredLeft, locked ? 0.95f : 0.3f);

    juce::String note = "--", detail, cents;
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
        const auto deviation = juce::roundToInt(getCents());
        cents = (deviation > 0 ? "+" : "") + juce::String(deviation) + " CENTS";
    }
    else
    {
        detail = "LISTENING";
    }

    const auto noteLevel = hasPitch() ? (locked ? 1.0f : 0.55f) : 0.3f;
    noteGlow.draw(g, note, affine::fonts::readout(54.0f), area.removeFromTop(56.0f), juce::Justification::centredLeft,
                  lit(noteLevel), 4.0f, 0.4f + 0.5f * noteLevel);

    auto details = area.removeFromTop(16.0f);
    g.setFont(affine::fonts::readout(15.0f, 0.04f));
    g.setColour(hasPitch() ? lit(0.9f) : screenAmber.withAlpha(0.5f));
    g.drawText(detail, details, juce::Justification::centredLeft, false);
    g.drawText(cents, details, juce::Justification::centredRight, false);

    area.removeFromTop(10.0f);
    paintTuner(g, area.removeFromTop(28.0f));
    area.removeFromTop(6.0f);
    paintSignal(g, area);
}

void PitchDisplay::paintTuner(juce::Graphics& g, juce::Rectangle<float> area)
{
    // Cents from the nearest note, -50 to +50, centred.
    const auto line = area.removeFromTop(14.0f);
    const auto y = line.getCentreY();
    const auto xFor = [&](float c) { return line.getX() + line.getWidth() * (juce::jlimit(-50.0f, 50.0f, c) + 50.0f) / 100.0f; };
    for (int c = -50; c <= 50; c += 10)
    {
        const auto major = c % 50 == 0;
        g.setColour(screenAmber.withAlpha(major ? 0.55f : 0.25f));
        const auto half = major ? 6.0f : 3.0f;
        g.fillRect(xFor(static_cast<float>(c)) - 0.5f, y - half, 1.0f, half * 2.0f);
    }
    g.setColour(screenAmber.withAlpha(0.18f));
    g.fillRect(line.getX(), y - 0.5f, line.getWidth(), 1.0f);

    if (hasPitch())
    {
        const auto x = xFor(getCents());
        const auto centre = xFor(0.0f);
        const auto level = locked ? 1.0f : 0.55f;
        g.setColour(screenAmber.withAlpha(0.45f * level));
        g.fillRect(juce::jmin(x, centre), y - 1.5f, std::abs(x - centre), 3.0f);
        affine::render::halo(g, { x, y }, 10.0f, screenAmber, 0.5f * level);
        g.setColour(lit(level));
        g.fillRoundedRectangle(juce::Rectangle<float>(3.0f, 16.0f).withCentre({ x, y }), 1.5f);
    }

    g.setFont(affine::fonts::label(9.5f, 0.04f));
    g.setColour(screenAmber.withAlpha(0.45f));
    g.drawText("-50", area.withWidth(30.0f), juce::Justification::centredLeft, false);
    g.drawText("TUNING", area, juce::Justification::centred, false);
    g.drawText("+50", area.withLeft(area.getRight() - 30.0f), juce::Justification::centredRight, false);
}

void PitchDisplay::paintSignal(juce::Graphics& g, juce::Rectangle<float> area)
{
    // Detection confidence against the Sensitivity threshold that gates the carrier.
    const auto active = trackingMode && live;
    auto header = area.removeFromTop(15.0f);
    caption(g, "SIGNAL", header, juce::Justification::centredLeft, trackingMode ? 0.85f : 0.4f);
    g.setFont(affine::fonts::readout(14.0f));
    g.setColour(active ? lit(0.9f) : screenAmber.withAlpha(0.35f));
    g.drawText(active ? whole(confidenceValue * 100.0f) + " %" : juce::String("--"), header,
               juce::Justification::centredRight, false);

    area.removeFromTop(6.0f);
    const auto bar = area.removeFromTop(10.0f);
    constexpr int segments = 32;
    const auto pitch = bar.getWidth() / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i)
    {
        const auto at = (static_cast<float>(i) + 0.5f) / static_cast<float>(segments);
        const auto on = active && at <= confidenceValue;
        const auto accepted = at >= threshold;
        g.setColour(on ? (accepted ? lit() : screenAmber.withAlpha(0.5f)) : screenAmber.withAlpha(accepted ? 0.14f : 0.08f));
        g.fillRect(bar.getX() + static_cast<float>(i) * pitch, bar.getY(), pitch - 1.0f, bar.getHeight());
    }

    const auto mark = bar.getX() + bar.getWidth() * juce::jlimit(0.0f, 1.0f, threshold);
    g.setColour(juce::Colours::white.withAlpha(trackingMode ? 0.85f : 0.3f));
    g.fillRect(mark - 1.0f, bar.getY() - 4.0f, 2.0f, bar.getHeight() + 8.0f);
    g.setFont(affine::fonts::label(9.5f, 0.12f));
    g.setColour(screenAmber.withAlpha(0.55f));
    g.drawText("SENS", juce::Rectangle<float>(40.0f, 12.0f).withCentre({ juce::jlimit(bar.getX() + 20.0f, bar.getRight() - 20.0f, mark),
                                                                          bar.getBottom() + 9.0f }),
               juce::Justification::centred, false);
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
    setTooltip("The carrier oscillator: its waveform and the frequency it is running at. While tracking, the scope spans two periods of the input pitch; it stays flat while the effect is dry.");
}

void CarrierDisplay::setTheme(const affine::Theme& t)
{
    theme = t;
    repaint();
}

juce::String CarrierDisplay::formatFrequency(float hz)
{
    return hz > 0.0f ? juce::String(juce::jmin(hz, 99999.9f), 1) : juce::String();
}

void CarrierDisplay::setReference(bool pitchTracking, float inputPitchHz)
{
    if (tracking == pitchTracking && juce::roundToInt(reference * 10.0f) == juce::roundToInt(inputPitchHz * 10.0f))
        return;
    tracking = pitchTracking;
    reference = inputPitchHz;
    repaint();
}

void CarrierDisplay::setState(float carrierHz, int waveformIndex)
{
    const auto changed = juce::roundToInt(carrier * 10.0f) != juce::roundToInt(carrierHz * 10.0f);
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

std::unique_ptr<juce::AccessibilityHandler> CarrierDisplay::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::staticText, juce::AccessibilityActions {},
        juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface>(*this) });
}

void CarrierDisplay::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    paintScope(g, area.removeFromLeft(320.0f));
    area.removeFromLeft(12.0f);
    g.setColour(screenAmber.withAlpha(0.10f));
    g.fillRect(area.getX() - 0.5f, area.getY() + 4.0f, 1.0f, area.getHeight() - 8.0f);
    area.removeFromLeft(12.0f);

    const auto running = carrier > 0.0f;
    static const char* names[] = { "SINE", "TRIANGLE", "SQUARE", "SAW" };
    auto header = area.removeFromTop(15.0f);
    caption(g, "CARRIER", header);
    caption(g, names[juce::jlimit(0, 3, waveform)], header, juce::Justification::centredRight, 0.6f);

    area.removeFromTop(8.0f);
    readout.draw(g, running ? formatFrequency(carrier) : juce::String("----"), affine::fonts::readout(44.0f),
                 area.removeFromTop(52.0f), juce::Justification::centredRight, running ? lit() : screenAmber.withAlpha(0.3f),
                 4.0f, running ? 0.9f : 0.3f);
    caption(g, "HZ", area.removeFromTop(14.0f), juce::Justification::centredRight, running ? 0.85f : 0.4f);

    area.removeFromTop(16.0f);
    g.setColour(screenAmber.withAlpha(0.10f));
    g.fillRect(area.getX(), area.getY(), area.getWidth(), 1.0f);
    area.removeFromTop(10.0f);

    // The carrier as a note, and its ratio to the input pitch it follows.
    auto row = [&](const juce::String& name, const juce::String& value)
    {
        auto line = area.removeFromTop(24.0f);
        caption(g, name, line, juce::Justification::centredLeft, 0.6f);
        g.setFont(affine::fonts::readout(17.0f));
        g.setColour(running ? lit(0.9f) : screenAmber.withAlpha(0.35f));
        g.drawText(value, line, juce::Justification::centredRight, false);
    };
    row("NOTE", running ? juce::String(NoteNames::fromFrequency(carrier).c_str()) : juce::String("--"));
    row("RATIO", running && reference > 0.0f ? juce::String(carrier / reference, 2) + "x" : running ? juce::String("MANUAL") : juce::String("--"));
}

void CarrierDisplay::paintScope(juce::Graphics& g, juce::Rectangle<float> area)
{
    // While tracking, the window is two periods of the input; a fixed carrier gets 10 ms.
    const auto periods = tracking && (reference > 0.0f || carrier <= 0.0f);
    auto header = area.removeFromTop(15.0f);
    caption(g, "WAVE", header);
    caption(g, periods ? "TWO INPUT PERIODS" : "10 MS", header, juce::Justification::centredRight, 0.45f);

    area.removeFromTop(12.0f);
    const auto axis = area.removeFromBottom(14.0f);
    const auto plot = area.reduced(0.0f, 4.0f);
    const auto mid = plot.getCentreY();
    const auto amplitude = plot.getHeight() * 0.42f;

    g.setColour(screenAmber.withAlpha(0.07f));
    g.fillRect(plot.getX(), mid - amplitude - 0.5f, plot.getWidth(), 1.0f);
    g.fillRect(plot.getX(), mid + amplitude - 0.5f, plot.getWidth(), 1.0f);
    g.setColour(screenAmber.withAlpha(0.16f));
    g.fillRect(plot.getX(), mid - 0.5f, plot.getWidth(), 1.0f);

    const auto divisions = periods ? 2 : 10;
    for (int i = 0; i <= divisions; ++i)
    {
        const auto x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(divisions);
        const auto strong = periods || i % 5 == 0;
        g.setColour(screenAmber.withAlpha(strong ? 0.16f : 0.07f));
        for (auto y = plot.getY(); y < plot.getBottom(); y += 4.0f)
            g.fillRect(x - 0.5f, y, 1.0f, 1.5f);
        if (strong)
        {
            g.setFont(affine::fonts::label(9.5f, 0.04f));
            g.setColour(screenAmber.withAlpha(0.45f));
            const auto label = periods ? juce::String(i) : juce::String(i) + (i == divisions ? " MS" : "");
            const auto just = i == 0 ? juce::Justification::centredLeft
                            : i == divisions ? juce::Justification::centredRight : juce::Justification::centred;
            const auto box = i == 0 ? axis.withX(x).withWidth(40.0f)
                           : i == divisions ? axis.withRight(x).withLeft(x - 40.0f)
                                            : juce::Rectangle<float>(40.0f, axis.getHeight()).withCentre({ x, axis.getCentreY() });
            g.drawText(label, box, just, false);
        }
    }

    if (carrier <= 0.0f)
    {
        caption(g, "DRY", plot.withBottom(mid - 4.0f), juce::Justification::centredBottom, 0.35f);
        return;
    }

    const auto window = periods ? 2.0 / reference : 0.010;
    const auto cycles = static_cast<double>(carrier) * window;
    const auto points = juce::jlimit(256, 6000, static_cast<int>(cycles * 64.0));
    juce::Path wave;
    for (int i = 0; i <= points; ++i)
    {
        const auto t = static_cast<double>(i) / points;
        const auto phase = std::fmod(t * cycles, 1.0);
        const juce::Point<float> at { plot.getX() + plot.getWidth() * static_cast<float>(t),
                                      mid - amplitude * carrierShape(waveform, phase) };
        if (i == 0)
            wave.startNewSubPath(at);
        else
            wave.lineTo(at);
    }
    g.saveState();
    g.reduceClipRegion(plot.expanded(0.0f, 6.0f).toNearestInt());
    g.setColour(screenAmber.withAlpha(0.16f));
    g.strokePath(wave, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(lit());
    g.strokePath(wave, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();
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
    modeKeys.setKeySize(56.0f, 30.0f);
    waveformKeys.setLegends({ "Sine", "Tri", "Square", "Saw" });
    waveformKeys.setGlyphs({ affine::glyphs::sine(), affine::glyphs::triangle(),
                             affine::glyphs::square(), affine::glyphs::saw() });
    waveformKeys.setKeySize(50.0f, 38.0f);
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
    {
        knob->setTheme(theme);
        knob->setLedRing(true);
    }
    for (auto* keys : { &modeKeys, &waveformKeys })
    {
        keys->setTheme(theme);
        keys->setStyle(affine::KeyButton::Style::softKey);
        for (int i = 0; i < keys->getNumKeys(); ++i)
            keys->getKey(i)->setLampColour(theme.palette.accent);
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
    const auto& palette = theme.palette;
    const auto h = static_cast<float>(height);

    g.setColour(palette.silkscreen);
    g.setFont(affine::fonts::label(30.0f, 0.45f));
    g.drawText("RING MODULATOR", juce::Rectangle<float>(38.0f, 22.0f, 560.0f, 36.0f), juce::Justification::centredLeft, false);
    g.setColour(palette.silkscreenDim);
    g.setFont(affine::fonts::label(11.5f, 0.3f));
    g.drawText("HDN  /  PITCH-TRACKING CARRIER", juce::Rectangle<float>(40.0f, 58.0f, 420.0f, 14.0f),
               juce::Justification::centredLeft, false);
    g.setFont(affine::fonts::label(11.5f, 0.26f));
    g.drawText("SOURCE", modeArea.toFloat().withWidth(70.0f).withX(static_cast<float>(modeArea.getX()) - 78.0f).withHeight(36.0f),
               juce::Justification::centredRight, false);

    const auto glass = screenArea.toFloat().reduced(5.0f);
    affine::render::screenGlass(g, glass);
    g.setColour(screenAmber.withAlpha(0.10f));
    g.fillRect(321.0f, static_cast<float>(inputArea.getY()) + 4.0f, 1.0f, static_cast<float>(inputArea.getHeight()) - 8.0f);

    groupTitle(g, "TRACKING", { 36.0f, 344.0f, 272.0f, 1.0f }, palette);
    groupTitle(g, "CARRIER", { 330.0f, 344.0f, 446.0f, 1.0f }, palette);
    groupTitle(g, "OUTPUT", { 796.0f, 344.0f, 128.0f, 1.0f }, palette);

    affine::silkscreen::makersMark(g, { 40.0f, h - 14.0f }, palette);
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

    place(smoothingKnob, 104);
    place(sensitivityKnob, 240);
    place(rateMultKnob, 406);
    place(manualRateKnob, 562);
    waveformKeys.setBounds(waveformArea);
    place(mixKnob, 860);
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
    const auto pitchHz = processorRef.currentPitchHz.load(std::memory_order_relaxed);
    const auto sensitivity = processorRef.apvts.getRawParameterValue(ParameterIDs::sensitivity)->load() / 100.0f;

    pitchDisplay.setState(pitchHz, processorRef.currentConfidence.load(std::memory_order_relaxed),
                          pitchTracking, manualRate, sensitivity, live && pitchTracking && carrierHz > 0.0f, live);
    carrierDisplay.setReference(pitchTracking, pitchDisplay.hasPitch() ? pitchHz : 0.0f);
    carrierDisplay.setState(pitchTracking ? (live ? carrierHz : 0.0f) : manualRate, waveformKeys.getSelectedIndex());
    updateModePresentation();
}
