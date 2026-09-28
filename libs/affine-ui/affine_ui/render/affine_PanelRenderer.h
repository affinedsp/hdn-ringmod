#pragma once

namespace affine
{
/** The panel: a fine matte powder coat. */
struct PanelFinish
{
    juce::Colour base { 0xff17181b };
    float grain = 0.10f;   // speckle of the powder
    float mottle = 0.03f;  // slow variation of the coat
    float sheen = 0.5f;    // soft reflection of the studio softbox
};

namespace render
{
/** Renders a panel of the given logical size at a physical pixel scale. Deterministic. */
juce::Image faceplate (juce::Rectangle<int> logicalBounds, float scale, const PanelFinish&);

/** A soft, analytic shadow for a rounded rectangle, e.g. under a raised control. */
void softShadow (juce::Graphics&, juce::Rectangle<float> area, float corner, float blur,
                 juce::Point<float> offset, float opacity);

/** An opening cut into the panel: inner shadow at the top, a lit lip at the bottom. */
void recess (juce::Graphics&, juce::Rectangle<float> area, float corner, float depth = 1.0f);

/** A display screen: thin bezel around dark glass. */
void screenGlass (juce::Graphics&, juce::Rectangle<float> area);
} // namespace render
} // namespace affine
