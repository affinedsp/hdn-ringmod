#ifdef AFFINE_UI_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of JUCE cpp file"
#endif

#include "affine_ui.h"

#include "render/affine_KnobRenderer.cpp"
#include "render/affine_PanelRenderer.cpp"
#include "theme/affine_Fonts.cpp"
#include "render/affine_Glow.cpp"
#include "components/affine_Knob.cpp"
#include "components/affine_NeedleMeter.cpp"
#include "components/affine_Displays.cpp"
#include "components/affine_LadderMeter.cpp"
#include "components/affine_Keys.cpp"
#include "components/affine_Nixie.cpp"
#include "components/affine_Faceplate.cpp"
#include "theme/affine_LookAndFeel.cpp"
