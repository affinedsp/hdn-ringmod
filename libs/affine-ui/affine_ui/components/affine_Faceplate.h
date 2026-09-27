#pragma once

namespace affine
{
/**
    The static layer of an editor: anodised plate, screws and everything
    silkscreened on it. Rendered once per size and physical scale, so editors
    repaint dynamic parts without touching the texture or the typography.
*/
class Faceplate
{
public:
    using Printer = std::function<void (juce::Graphics&)>;

    /** Paints the plate for `bounds` and calls `print` (in logical coordinates) when rebuilding. */
    void paint (juce::Graphics&, juce::Rectangle<int> bounds, const Theme&, const Printer& print);

    /** Forces the next paint to rebuild, e.g. after printed text changed. */
    void invalidate() noexcept { cache = {}; }

private:
    juce::Image cache;
    juce::Rectangle<int> cachedBounds;
    float cachedScale = 0.0f;
};

namespace silkscreen
{
/** Product name in the family wordmark, with its descriptor underneath. */
void wordmark (juce::Graphics&, const juce::String& product, const juce::String& descriptor,
               juce::Point<float> topLeft, const Palette&);

/** The maker's mark: a sheared square and the name, as an affine transform of a square. */
void makersMark (juce::Graphics&, juce::Point<float> baselineLeft, const Palette&);

/** A machined groove across the plate. */
void groove (juce::Graphics&, float x1, float x2, float y);

/** A printed frame grouping controls, with its title interrupting the top rule. */
void frame (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, const Palette&);

/** Small caps legend, e.g. above a display window. */
void legend (juce::Graphics&, const juce::String&, juce::Rectangle<float>, const Palette&,
             juce::Justification = juce::Justification::centredLeft, bool dim = false);

/** Four corner screws. */
void screws (juce::Graphics&, juce::Rectangle<float> bounds);
} // namespace silkscreen
} // namespace affine
