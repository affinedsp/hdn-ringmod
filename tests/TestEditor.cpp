#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace
{
float relativeLuminance(juce::Colour colour)
{
    auto linearise = [](float value)
    {
        return value <= 0.04045f ? value / 12.92f
                                 : std::pow((value + 0.055f) / 1.055f, 2.4f);
    };

    return 0.2126f * linearise(colour.getFloatRed())
         + 0.7152f * linearise(colour.getFloatGreen())
         + 0.0722f * linearise(colour.getFloatBlue());
}

float contrastRatio(juce::Colour first, juce::Colour second)
{
    auto lighter = std::max(relativeLuminance(first), relativeLuminance(second));
    auto darker = std::min(relativeLuminance(first), relativeLuminance(second));
    return (lighter + 0.05f) / (darker + 0.05f);
}

struct GestureCounter final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { starting ? ++starts : ++ends; }
    int starts = 0;
    int ends = 0;
};

juce::MouseEvent mouseEvent(juce::Component& component, juce::Point<float> position, juce::ModifierKeys modifiers)
{
    auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position, modifiers,
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, now, position, now, 1, false };
}

void setPlain(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    auto* parameter = state.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void process(HdnRingmodAudioProcessor& processor, int blocks, float frequency);

// Feeds audio at roughly real-time pace. The pitch detector analyses on its own
// thread, and a host never delivers blocks faster than real time.
void processInRealTime(HdnRingmodAudioProcessor& processor, double milliseconds, float frequency)
{
    constexpr int blocksPerStep = 2;
    constexpr double stepMs = 1000.0 * blocksPerStep * 512 / 48000.0;
    for (double elapsed = 0.0; elapsed < milliseconds; elapsed += stepMs)
    {
        process(processor, blocksPerStep, frequency);
        juce::Thread::sleep(static_cast<int>(stepMs));
    }
}

bool waitForSteadyCarrier(HdnRingmodAudioProcessor& processor, float frequency)
{
    auto deadline = juce::Time::getMillisecondCounterHiRes() + 8000.0;
    int steady = 0;
    while (juce::Time::getMillisecondCounterHiRes() < deadline && steady < 12)
    {
        processInRealTime(processor, 20.0, frequency);
        auto locked = processor.currentCarrierHz.load() > 0.0f && processor.currentConfidence.load() > 0.5f;
        steady = locked ? steady + 1 : 0;
    }
    return steady >= 12;
}

void process(HdnRingmodAudioProcessor& processor, int blocks, float frequency)
{
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    static double phase = 0.0;
    for (int block = 0; block < blocks; ++block)
    {
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            auto sample = frequency > 0.0f ? static_cast<float>(0.5 * std::sin(phase)) : 0.0f;
            phase += juce::MathConstants<double>::twoPi * frequency / 48000.0;
            buffer.setSample(0, n, sample);
            buffer.setSample(1, n, sample);
        }
        processor.processBlock(buffer, midi);
    }
}
}

TEST_CASE("Ring modulator theme: printed and emitted colours keep readable contrast")
{
    auto theme = RingmodTheme::theme();
    const auto& palette = theme.palette;

    for (auto plate : { theme.panel.base, theme.panel.base.darker(0.25f) })
    {
        REQUIRE(contrastRatio(palette.silkscreen, plate) >= 4.5f);
        REQUIRE(contrastRatio(palette.silkscreenDim, plate) >= 4.5f);
    }

    REQUIRE(contrastRatio(palette.accent, palette.glass) >= 4.5f);
    REQUIRE(contrastRatio(palette.readoutInk, palette.readoutBacklight) >= 4.5f);
}

TEST_CASE("Family typefaces are bundled rather than taken from the system")
{
    REQUIRE(affine::fonts::label(18.0f).getTypefaceName() == "Barlow Condensed");
    REQUIRE(affine::fonts::wordmark(18.0f).getTypefaceName() == "Michroma");
    REQUIRE(affine::fonts::nixie(18.0f).getTypefaceName() == "Nixie One");
    REQUIRE(affine::fonts::readout(18.0f).getTypefaceName() == "Share Tech Mono");
    REQUIRE(affine::fonts::segment(18.0f).getTypefaceName().startsWith("DSEG14"));
}

TEST_CASE("Editor: every parameter is a focusable control mapped to its host parameter")
{
    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    HdnRingmodAudioProcessorEditor editor(processor);

    REQUIRE(editor.getWidth() == HdnRingmodAudioProcessorEditor::width);
    REQUIRE(editor.getHeight() == HdnRingmodAudioProcessorEditor::height);
    REQUIRE_FALSE(editor.isResizable());

    const char* ids[] = { ParameterIDs::mix, ParameterIDs::rateMultiplier, ParameterIDs::manualRate,
                          ParameterIDs::mode, ParameterIDs::smoothing, ParameterIDs::sensitivity,
                          ParameterIDs::waveform };
    std::set<int> focusOrders;

    for (auto* id : ids)
    {
        auto* control = editor.findChildWithID(id);
        REQUIRE(control != nullptr);
        auto index = processor.apvts.getParameter(id)->getParameterIndex();
        REQUIRE(editor.getControlParameterIndex(*control) == index);
        REQUIRE(control->getExplicitFocusOrder() > 0);
        REQUIRE(focusOrders.insert(control->getExplicitFocusOrder()).second);
        REQUIRE(control->getTitle().isNotEmpty());
        REQUIRE(control->isAccessible());

        if (auto* keys = dynamic_cast<affine::SelectorKeys*>(control))
        {
            for (int i = 0; i < keys->getNumKeys(); ++i)
            {
                auto* key = keys->getKey(i);
                REQUIRE(editor.getControlParameterIndex(*key) == index);
                REQUIRE(key->getWantsKeyboardFocus());
                REQUIRE(key->getWidth() >= 32);
                REQUIRE(key->getHeight() >= 32);
            }
        }
        else
        {
            REQUIRE(control->getWantsKeyboardFocus());
            REQUIRE(control->getWidth() >= 32);
            REQUIRE(control->getHeight() >= 32);
        }
    }
}

TEST_CASE("Knob: exact entry commits valid text and rejects malformed text and cancels")
{
    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    HdnRingmodAudioProcessorEditor editor(processor);
    auto* knob = dynamic_cast<affine::Knob*>(editor.findChildWithID(ParameterIDs::mix));
    REQUIRE(knob != nullptr);

    auto enter = [&](const juce::String& text, bool commit)
    {
        REQUIRE(knob->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        auto* entry = dynamic_cast<juce::TextEditor*>(knob->getChildComponent(0));
        REQUIRE(entry != nullptr);
        REQUIRE(entry->isVisible());
        REQUIRE(editor.getControlParameterIndex(*entry) == knob->parameter.getParameterIndex());
        entry->setText(text);
        if (commit)
            entry->onReturnKey();
        else
            entry->onEscapeKey();
        REQUIRE_FALSE(entry->isVisible());
    };

    enter("25 %", true);
    REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(25.0, 1.0e-6));
    enter("80", false);
    REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(25.0, 1.0e-6));
    for (auto* malformed : { "abc", "", "--3", "1e2", "12 garbage", "nan" })
    {
        enter(malformed, true);
        REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(25.0, 1.0e-6));
    }
    enter("250", true);
    REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(100.0, 1.0e-6));

    auto* rate = dynamic_cast<affine::Knob*>(editor.findChildWithID(ParameterIDs::rateMultiplier));
    REQUIRE(rate != nullptr);
    REQUIRE(rate->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
    auto* rateEntry = dynamic_cast<juce::TextEditor*>(rate->getChildComponent(0));
    REQUIRE(rateEntry != nullptr);
    rateEntry->setText("2.5x");
    rateEntry->onReturnKey();
    REQUIRE_THAT(rate->getValue(), Catch::Matchers::WithinAbs(2.5, 1.0e-6));
}

TEST_CASE("Knob: drags and keys and resets are each one delimited host gesture")
{
    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    HdnRingmodAudioProcessorEditor editor(processor);
    auto* knob = dynamic_cast<affine::Knob*>(editor.findChildWithID(ParameterIDs::smoothing));
    REQUIRE(knob != nullptr);
    GestureCounter gestures;
    knob->parameter.addListener(&gestures);

    auto centre = knob->getKnobCentre();
    const juce::ModifierKeys left(juce::ModifierKeys::leftButtonModifier);
    knob->mouseDown(mouseEvent(*knob, centre, left));
    knob->mouseDrag(mouseEvent(*knob, centre.translated(0.0f, -24.0f), left));
    knob->mouseUp(mouseEvent(*knob, centre.translated(0.0f, -24.0f), left));
    REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(60.0, 1.0e-6));
    REQUIRE(gestures.starts == 1);
    REQUIRE(gestures.ends == 1);

    REQUIRE(knob->keyPressed(juce::KeyPress(juce::KeyPress::upKey)));
    REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(60.1, 1.0e-4));
    REQUIRE(gestures.starts == 2);
    REQUIRE(gestures.ends == 2);

    knob->mouseDown(mouseEvent(*knob, centre, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier
                                                                 | juce::ModifierKeys::altModifier)));
    REQUIRE_THAT(knob->getValue(), Catch::Matchers::WithinAbs(50.0, 1.0e-6));
    REQUIRE(gestures.starts == gestures.ends);

    knob->mouseDown(mouseEvent(*knob, centre, left));
    knob->cancelInteraction();
    REQUIRE(gestures.starts == gestures.ends);
    knob->parameter.removeListener(&gestures);
}

TEST_CASE("Selector keys: a press selects its choice as one gesture and arrows move the selection")
{
    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    HdnRingmodAudioProcessorEditor editor(processor);
    auto* keys = dynamic_cast<affine::SelectorKeys*>(editor.findChildWithID(ParameterIDs::waveform));
    REQUIRE(keys != nullptr);
    REQUIRE(keys->getNumKeys() == 4);
    REQUIRE(keys->getSelectedIndex() == 0);
    REQUIRE(keys->getKey(0)->getToggleState());

    GestureCounter gestures;
    keys->parameter.addListener(&gestures);
    keys->getKey(2)->onClick();
    REQUIRE(keys->getSelectedIndex() == 2);
    REQUIRE(keys->getKey(2)->getToggleState());
    REQUIRE_FALSE(keys->getKey(0)->getToggleState());
    REQUIRE(gestures.starts == 1);
    REQUIRE(gestures.ends == 1);

    REQUIRE(keys->getKey(2)->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)));
    REQUIRE(keys->getSelectedIndex() == 3);
    REQUIRE(static_cast<int>(processor.apvts.getRawParameterValue(ParameterIDs::waveform)->load()) == 3);

    setPlain(processor.apvts, ParameterIDs::waveform, 1.0f);
    REQUIRE(keys->getSelectedIndex() == 1);
    REQUIRE(keys->getKey(1)->getToggleState());
    keys->parameter.removeListener(&gestures);
}

TEST_CASE("Mode: controls that do not act in the current source are shown inactive but stay editable")
{
    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    HdnRingmodAudioProcessorEditor editor(processor);
    auto knob = [&](const char* id) { return dynamic_cast<affine::Knob*>(editor.findChildWithID(id)); };

    REQUIRE_FALSE(knob(ParameterIDs::rateMultiplier)->isInactive());
    REQUIRE(knob(ParameterIDs::manualRate)->isInactive());

    setPlain(processor.apvts, ParameterIDs::mode, 1.0f);
    REQUIRE(knob(ParameterIDs::rateMultiplier)->isInactive());
    REQUIRE(knob(ParameterIDs::smoothing)->isInactive());
    REQUIRE(knob(ParameterIDs::sensitivity)->isInactive());
    REQUIRE_FALSE(knob(ParameterIDs::manualRate)->isInactive());
    for (auto* id : { ParameterIDs::rateMultiplier, ParameterIDs::smoothing, ParameterIDs::sensitivity })
        REQUIRE(knob(id)->isEnabled());
}

TEST_CASE("Pitch display: tracking states are reported truthfully")
{
    juce::ScopedJuceInitialiser_GUI gui;
    PitchDisplay display;

    display.setState(0.0f, 0.0f, false, 440.0f, 0.5f, false, true);
    REQUIRE(display.getAccessibleValueText() == "Manual carrier, 440.0 hertz");

    display.setState(110.0f, 0.82f, true, 440.0f, 0.5f, true, false);
    REQUIRE(display.getAccessibleValueText() == "No audio");

    display.setState(110.0f, 0.82f, true, 440.0f, 0.5f, true, true);
    REQUIRE(display.getAccessibleValueText() == "A2, 110.0 hertz, locked, confidence 82 percent");

    display.setState(110.0f, 0.3f, true, 440.0f, 0.5f, false, true);
    REQUIRE(display.getAccessibleValueText() == "A2, 110.0 hertz, below sensitivity, confidence 30 percent");

    display.setState(0.0f, 0.0f, true, 440.0f, 0.5f, false, true);
    REQUIRE(display.getAccessibleValueText() == "Listening for input pitch");
}

TEST_CASE("Carrier display: the counter lights only while the carrier runs")
{
    juce::ScopedJuceInitialiser_GUI gui;
    CarrierDisplay display;

    display.setState(220.0f, 0);
    REQUIRE(display.getCounter().getText() == "220.0");
    display.setState(0.0f, 0);
    REQUIRE(display.getCounter().getText().isEmpty());

    REQUIRE(CarrierDisplay::formatFrequency(40000.0f) == "40000.0");
    REQUIRE(CarrierDisplay::formatFrequency(1234.56f) == "1234.6");
    REQUIRE(CarrierDisplay::formatFrequency(0.0f).isEmpty());
}

TEST_CASE("Processor: publishes the carrier it actually runs and counts processed blocks")
{
    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    setPlain(processor.apvts, ParameterIDs::mode, 1.0f);
    setPlain(processor.apvts, ParameterIDs::manualRate, 440.0f);
    auto blocksBefore = processor.processedBlocks.load();
    process(processor, 20, 220.0f);
    REQUIRE(processor.processedBlocks.load() == blocksBefore + 20);
    REQUIRE_THAT(processor.currentCarrierHz.load(), Catch::Matchers::WithinAbs(440.0, 0.01));

    setPlain(processor.apvts, ParameterIDs::mode, 0.0f);
    process(processor, 40, 0.0f);
    REQUIRE(processor.currentCarrierHz.load() == 0.0f);

    setPlain(processor.apvts, ParameterIDs::rateMultiplier, 2.0f);
    setPlain(processor.apvts, ParameterIDs::sensitivity, 0.0f);
    REQUIRE(waitForSteadyCarrier(processor, 220.0f));
    REQUIRE_THAT(processor.currentCarrierHz.load(), Catch::Matchers::WithinRel(440.0f, 0.02f));
    processor.releaseResources();
}

TEST_CASE("Editor: renders every display state for review")
{
    auto directory = juce::SystemStats::getEnvironmentVariable("HDN_UI_CAPTURE_DIR", {});
    if (directory.isEmpty())
        return;

    juce::ScopedJuceInitialiser_GUI gui;
    HdnRingmodAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    HdnRingmodAudioProcessorEditor editor(processor);
    auto folder = juce::File(directory);
    REQUIRE(folder.createDirectory().wasOk());

    auto refresh = [&]
    {
        juce::Thread::sleep(40);
        juce::Timer::callPendingTimersSynchronously();
    };
    auto capture = [&](const juce::String& name, float scale)
    {
        // Show the steady state rather than a frame of a needle's or pointer's movement.
        for (auto* child : editor.getChildren())
        {
            if (auto* meter = dynamic_cast<affine::NeedleMeter*>(child))
                meter->settle();
            if (auto* dial = dynamic_cast<affine::TuningDial*>(child))
                dial->settle();
        }
        auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
        auto file = folder.getChildFile(name + ".png");
        file.deleteFile();
        juce::FileOutputStream stream(file);
        REQUIRE(stream.openedOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(image, stream));
    };

    setPlain(processor.apvts, ParameterIDs::rateMultiplier, 2.0f);
    REQUIRE(waitForSteadyCarrier(processor, 110.0f));
    refresh();
    capture("hdn-ring-modulator-tracking", 1.0f);
    capture("hdn-ring-modulator-tracking@2x", 2.0f);

    processInRealTime(processor, 300.0, 0.0f);
    refresh();
    capture("hdn-ring-modulator-listening", 1.0f);

    setPlain(processor.apvts, ParameterIDs::mode, 1.0f);
    setPlain(processor.apvts, ParameterIDs::waveform, 2.0f);
    processInRealTime(processor, 100.0, 110.0f);
    refresh();
    capture("hdn-ring-modulator-manual", 1.0f);

    juce::Thread::sleep(450);
    setPlain(processor.apvts, ParameterIDs::mode, 0.0f);
    refresh();
    capture("hdn-ring-modulator-no-audio", 1.0f);

    for (auto scale : { 1.25f, 1.5f, 1.75f, 2.0f })
        capture("hdn-ring-modulator-scale-" + juce::String(juce::roundToInt(scale * 100.0f)), scale);
    processor.releaseResources();
}
