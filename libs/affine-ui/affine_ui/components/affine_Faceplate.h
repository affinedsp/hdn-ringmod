#pragma once

namespace affine
{
/**
    The static layer of an editor: the powder-coated panel and everything
    printed on it. Rendered once per size and physical scale, so editors
    repaint dynamic parts without touching the texture or the typography.
*/
class Faceplate
{
public:
    using Printer = std::function<void (juce::Graphics&)>;

    /** Paints the panel for `bounds` and calls `print` (in logical coordinates) when rebuilding. */
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
/** The product name, wide-tracked, with its descriptor underneath. */
void wordmark (juce::Graphics&, const juce::String& product, const juce::String& descriptor,
               juce::Point<float> topLeft, const Palette&);

/** The maker's mark: a sheared square and the name, as an affine transform of a square. */
void makersMark (juce::Graphics&, juce::Point<float> baselineLeft, const Palette&);

/** A section title over a group of controls, with a hairline running from it to the end of `rule`. */
void section (juce::Graphics&, const juce::String& title, juce::Rectangle<float> rule, const Palette&);
} // namespace silkscreen
} // namespace affine
