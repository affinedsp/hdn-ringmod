#include "AffineFonts.h"

namespace affine::fonts
{
namespace
{
juce::Typeface::Ptr load (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (size));
}

juce::Font make (const juce::Typeface::Ptr& typeface, float height, float tracking)
{
    return juce::Font (juce::FontOptions (typeface).withHeight (height).withKerningFactor (tracking));
}

const juce::Typeface::Ptr& semibold()
{
    static auto t = load (AffineFonts::BarlowCondensedSemiBold_ttf, AffineFonts::BarlowCondensedSemiBold_ttfSize);
    return t;
}

const juce::Typeface::Ptr& regular()
{
    static auto t = load (AffineFonts::BarlowCondensedRegular_ttf, AffineFonts::BarlowCondensedRegular_ttfSize);
    return t;
}

const juce::Typeface::Ptr& michroma()
{
    static auto t = load (AffineFonts::MichromaRegular_ttf, AffineFonts::MichromaRegular_ttfSize);
    return t;
}

const juce::Typeface::Ptr& mono()
{
    static auto t = load (AffineFonts::ShareTechMonoRegular_ttf, AffineFonts::ShareTechMonoRegular_ttfSize);
    return t;
}
} // namespace

juce::Font label (float height, float tracking) { return make (semibold(), height, tracking); }
juce::Font text (float height, float tracking) { return make (regular(), height, tracking); }
juce::Font wordmark (float height, float tracking) { return make (michroma(), height, tracking); }
juce::Font readout (float height, float tracking) { return make (mono(), height, tracking); }
} // namespace affine::fonts
