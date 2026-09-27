#pragma once

namespace affine
{
/** The anodised faceplate every product is machined from. */
struct PanelFinish
{
    juce::Colour base { 0xff2b3036 };
    float grain = 0.030f;      // bead-blast micro texture
    float mottle = 0.030f;     // slow variation of the anodising
    float brushing = 0.0f;     // 0 = bead blasted, 1 = horizontally brushed
    float sheen = 0.10f;       // soft reflection of the studio softbox
};

namespace render
{
/** Renders a faceplate of the given logical size at a physical pixel scale. Deterministic. */
juce::Image faceplate (juce::Rectangle<int> logicalBounds, float scale, const PanelFinish&);

/** Hex-socket panel screw, drawn in logical coordinates. */
void screw (juce::Graphics&, juce::Point<float> centre, float diameter, float rotation = 0.35f);

/** A soft, analytic shadow for a rounded rectangle, e.g. under a raised control. */
void softShadow (juce::Graphics&, juce::Rectangle<float> area, float corner, float blur,
                 juce::Point<float> offset, float opacity);

/** An opening cut into the faceplate: inner shadow at the top, a lit lip at the bottom. */
void recess (juce::Graphics&, juce::Rectangle<float> area, float corner, float depth = 1.0f);
} // namespace render
} // namespace affine
