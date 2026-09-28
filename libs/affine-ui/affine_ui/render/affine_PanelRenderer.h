#pragma once

namespace affine
{
/** The faceplate a product is built on. */
struct PanelFinish
{
    enum class Texture
    {
        anodised,    // bead-blasted or brushed dyed aluminium
        hammertone,  // hammered enamel: dimpled metallic paint
        powder,      // fine matte powder coat
        glass        // a black glass front
    };

    Texture texture = Texture::anodised;
    juce::Colour base { 0xff2b3036 };
    float grain = 0.030f;      // micro texture
    float mottle = 0.030f;     // slow variation of the finish
    float brushing = 0.0f;     // anodised only: 0 = bead blasted, 1 = horizontally brushed
    float sheen = 0.10f;       // soft reflection of the studio softbox
    float dimple = 7.0f;       // hammertone only: cell size in logical pixels
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

/** A frosted window lit from behind, as on illuminated legends and counters. `level` 0 is dark. */
void litWindow (juce::Graphics&, juce::Rectangle<float> area, juce::Colour backlight, float level);

/** A faceted glass jewel in a knurled chrome bezel. `level` 0 is dark, 1 fully lit. */
void jewel (juce::Graphics&, juce::Point<float> centre, float diameter, juce::Colour colour, float level);

/** The flange of a rack panel, with its two mounting slots and screws. */
void rackEar (juce::Graphics&, juce::Rectangle<float> area, bool leftSide);

/** A brushed-aluminium name plate riveted to the panel, with engraved lettering. */
void nameplate (juce::Graphics&, juce::Rectangle<float> area, const juce::String& text, const juce::Font&);

/** A polished aluminium end cap with a pull handle, as on the sides of a hi-fi front. */
void endCap (juce::Graphics&, juce::Rectangle<float> area, bool leftSide);

/** A display screen: thin bezel around dark glass. */
void screenGlass (juce::Graphics&, juce::Rectangle<float> area);
} // namespace render
} // namespace affine
