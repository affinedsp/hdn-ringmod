#pragma once

namespace affine
{
/**
    Colours of the family. A product sets its screen colour; everything else is
    shared so the instruments sit together.
*/
struct Palette
{
    // Printed on the panel.
    juce::Colour silkscreen { 0xffd8dbdf };
    juce::Colour silkscreenDim { 0xff8a9097 };

    // Emitted light. `accent` lights LED rings, keys and focus; `screen` is the product's display colour.
    juce::Colour accent { 0xffeef4ff };
    juce::Colour screen { 0xff35e0c8 };
    juce::Colour attention { 0xffffb238 };
    juce::Colour danger { 0xffff4a3d };

    // Display glass.
    juce::Colour glass { 0xff050607 };
    juce::Colour glassTint { 0xff0c0f12 };
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
inline constexpr float displayCorner = 4.0f;

// Knob sizes: one large knob per primary decision, medium for the rest, small for refinements.
inline constexpr float knobLarge = 84.0f;
inline constexpr float knobMedium = 72.0f;
inline constexpr float knobSmall = 58.0f;
} // namespace metrics
} // namespace affine
