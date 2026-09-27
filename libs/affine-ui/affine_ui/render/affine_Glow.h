#pragma once

namespace affine::render
{
/** Three-pass box blur approximating a Gaussian, in place, on a single-channel image. */
void blurAlpha (juce::Image& singleChannel, float radiusPx);

/**
    Caches light-emitting artwork (display segments, traces) with a soft bloom.
    The painter draws white shapes in coordinates local to `area`; the layer is
    rebuilt only when the key, the bounds or the physical scale change.
*/
class GlowLayer
{
public:
    using Painter = std::function<void (juce::Graphics&)>;

    void draw (juce::Graphics&, const juce::String& key, juce::Rectangle<float> area, juce::Colour colour,
               float glowRadius, float glowStrength, const Painter&);

private:
    juce::String cachedKey;
    juce::Rectangle<float> cachedArea;
    float cachedScale = 0.0f, cachedRadius = 0.0f;
    juce::Image crisp, glow;
    int margin = 0;
};

/** Light-emitting text, e.g. a display readout. */
class GlowText
{
public:
    void draw (juce::Graphics&, const juce::String& text, const juce::Font&, juce::Rectangle<float> area,
               juce::Justification, juce::Colour colour, float glowRadius = 3.0f, float glowStrength = 0.9f);

private:
    GlowLayer layer;
};

/** Paints a circular soft light, e.g. the halo of a lit LED, without blurring. */
void halo (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, float intensity);
} // namespace affine::render
