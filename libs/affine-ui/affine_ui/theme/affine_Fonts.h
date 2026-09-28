#pragma once

namespace affine::fonts
{
/** Silkscreen labels: Barlow Condensed SemiBold. `tracking` is a kerning factor. */
juce::Font label (float height, float tracking = 0.06f);

/** Supporting copy: Barlow Condensed Regular. */
juce::Font text (float height, float tracking = 0.0f);

/** The Affine wordmark in the maker's mark: Michroma, a wide machined grotesque. */
juce::Font wordmark (float height, float tracking = 0.08f);

/** Readouts and screen numerals: Share Tech Mono. */
juce::Font readout (float height, float tracking = 0.0f);
} // namespace affine::fonts
