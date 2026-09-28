#pragma once

namespace affine::screen
{
/** The screen colour lifted towards white, for lit numerals, traces and markers. */
juce::Colour lit (juce::Colour screen, float level = 1.0f);

/** A caption in the screen colour: a display's title or the name of a reading. */
void caption (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, juce::Colour screen,
              juce::Justification = juce::Justification::centredLeft, float alpha = 0.85f);

/**
    A segmented bar across `area`. A cell lights when `fraction` reaches its centre;
    `colourFor` gives each cell's colour from its centre position (0 to 1) and whether it is lit.
*/
void segments (juce::Graphics&, juce::Rectangle<float> area, int count, float fraction,
               const std::function<juce::Colour (float position, bool lit)>& colourFor);
} // namespace affine::screen
