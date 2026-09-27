#pragma once

namespace affine
{
/** Materials of the family knob. Only the paint colours differ between products. */
struct KnobFinish
{
    juce::Colour cap { 0xffc3c7cc };      // spun aluminium top
    juce::Colour body { 0xff1b1e22 };     // anodised grip and skirt
    juce::Colour pointer { 0xffe8e4da };  // paint-filled engraving on the cap
    juce::Colour index { 0xffe8e4da };    // printed index on the skirt
};

/**
    Renders the Affine knob with a fixed studio light, so highlights stay put
    while the knob turns: exactly what real hardware does.

    The rotation-invariant body and the shadow are rendered once per physical
    size. Only the knurled grip band is re-shaded when the angle changes.
*/
class KnobRenderer
{
public:
    explicit KnobRenderer (KnobFinish finishToUse = {}) : finish (finishToUse) {}

    /** Rebuilds the caches if the physical diameter changed. */
    void prepare (int diameterPx);

    int getDiameter() const noexcept { return diameter; }
    const juce::Image& getBody() const noexcept { return body; }

    /** Shadow image; its top-left sits at (-getShadowMargin(), -getShadowMargin()) from the body. */
    const juce::Image& getShadow() const noexcept { return shadow; }
    int getShadowMargin() const noexcept { return shadowMargin; }

    /** The grip band for a knob angle in radians (0 = pointing up, clockwise positive). */
    const juce::Image& getGrip (float angle);

    const KnobFinish& getFinish() const noexcept { return finish; }

    /** Radii of the rendered geometry as fractions of the knob radius. */
    static constexpr float capRadius = 0.600f;
    static constexpr float skirtRadius = 0.800f;

private:
    void renderBody();
    void renderShadow();
    void renderGrip (float angle);

    KnobFinish finish;
    int diameter = 0, shadowMargin = 0, ridges = 36;
    float gripAngle = std::numeric_limits<float>::quiet_NaN();
    juce::Image body, shadow, grip;
};
} // namespace affine
