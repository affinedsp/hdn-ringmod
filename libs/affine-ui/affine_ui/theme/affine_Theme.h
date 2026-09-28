#pragma once

namespace affine
{
/**
    Colours of the family. Products change the faceplate anodising and their
    emission accent; everything else is shared so the instruments sit together.
*/
struct Palette
{
    // Printed on the faceplate.
    juce::Colour silkscreen { 0xffece8df };
    juce::Colour silkscreenDim { 0xffaeaaa1 };

    // Emitted light. `accent` is the product colour: lamps, displays, focus.
    juce::Colour accent { 0xff4fd6ff };
    juce::Colour attention { 0xffffb238 };
    juce::Colour danger { 0xffff4a3d };

    // Display glass and its unlit segments.
    juce::Colour glass { 0xff07090b };
    juce::Colour glassTint { 0xff0d1417 };

    /** Indicator lamps: small LEDs behind lenses, or faceted jewels in chrome bezels. */
    enum class Lamp { led, jewel };
    Lamp lamp = Lamp::led;

    /** How value readouts are lit: glowing characters on glass, or dark ink on a backlit window. */
    enum class Readout { glowing, backlit };
    Readout readout = Readout::glowing;
    juce::Colour readoutBacklight { 0xfff1d58c };  // backlit readouts only
    juce::Colour readoutInk { 0xff1f1a12 };        // backlit readouts only
};

struct Theme
{
    PanelFinish panel;
    KnobFinish knob;
    Palette palette;
};

/** Family geometry, in logical pixels. */
namespace metrics
{
inline constexpr float screwDiameter = 11.0f;
inline constexpr float screwInset = 14.0f;
inline constexpr float displayCorner = 4.0f;
inline constexpr float frameCorner = 7.0f;
inline constexpr float frameLine = 1.1f;

// Knob sizes: one large knob per primary decision, medium for the rest, small for refinements.
inline constexpr float knobLarge = 84.0f;
inline constexpr float knobMedium = 72.0f;
inline constexpr float knobSmall = 58.0f;
} // namespace metrics
} // namespace affine
