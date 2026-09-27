#pragma once

namespace affine::fonts
{
/** Silkscreen labels: Barlow Condensed SemiBold. `tracking` is a kerning factor. */
juce::Font label (float height, float tracking = 0.06f);

/** Supporting copy: Barlow Condensed Regular. */
juce::Font text (float height, float tracking = 0.0f);

/** Product wordmarks: Michroma, a wide machined grotesque. */
juce::Font wordmark (float height, float tracking = 0.08f);

/** Glass-display numerals in the style of a Nixie tube. */
juce::Font nixie (float height);

/** Fourteen-segment vacuum-fluorescent display characters. */
juce::Font segment (float height, float tracking = 0.0f);

/** Monospaced readouts for small displays. */
juce::Font readout (float height, float tracking = 0.0f);

juce::Typeface::Ptr labelTypeface();
} // namespace affine::fonts
