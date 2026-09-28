/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 affine_ui
  vendor:             affine
  version:            3.0.0
  name:               Affine instrument UI
  description:        Shared rendered-instrument design language for Affine plug-ins
  license:            GPL-3.0-or-later
  minimumCppStandard: 17

  dependencies:       juce_gui_basics, juce_audio_processors

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define AFFINE_UI_H_INCLUDED

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>

#include "render/affine_Shading.h"
#include "render/affine_KnobRenderer.h"
#include "render/affine_PanelRenderer.h"
#include "theme/affine_Fonts.h"
#include "render/affine_Glow.h"
#include "theme/affine_Theme.h"
#include "components/affine_Knob.h"
#include "components/affine_Displays.h"
#include "components/affine_Keys.h"
#include "components/affine_Faceplate.h"
#include "components/affine_Screen.h"
#include "theme/affine_LookAndFeel.h"
