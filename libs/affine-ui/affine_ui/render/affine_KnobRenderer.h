#pragma once

namespace affine
{
/** Materials and proportions of the family knob. */
struct KnobFinish
{
    enum class Material
    {
        spunAluminium,     // lathe-turned metal with an anisotropic highlight
        anodised,          // satin dyed metal
        glossPlastic,      // moulded phenolic or bakelite with a clear gloss
        polishedAluminium  // bright metal that mirrors the studio
    };

    juce::Colour cap { 0xffc3c7cc };
    juce::Colour body { 0xff1b1e22 };
    juce::Colour pointer { 0xffe8e4da };  // paint-filled engraving on the cap
    juce::Colour index { 0xffe8e4da };    // printed index on the skirt
    Material capMaterial = Material::spunAluminium;
    Material bodyMaterial = Material::anodised;
    float capRatio = 0.60f;               // cap radius as a fraction of the knob radius
    float gripRatio = 0.80f;              // outer radius of the knurled or fluted grip
    float capDome = 0.06f;                // slope of the cap at its edge, in radians
    int ridges = 0;                       // grip ridges; 0 picks a pitch for the size
    float ridgeDepth = 0.62f;             // 0 gives a smooth grip
};

/**
    Renders the Affine knob with a fixed studio light, so highlights stay put
    while the knob turns: exactly what real hardware does.

    The rotation-invariant body and the shadow are rendered once per physical
    size. Only the grip band is re-shaded when the angle changes.
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
