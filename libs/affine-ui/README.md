# affine-ui

The shared interface kit for Affine plug-ins: a JUCE module (`affine_ui`), its bundled
typefaces, and the [design language](DESIGN_LANGUAGE.md) it implements.

This folder is **vendored identically** into every Affine plug-in repository. Do not make
product-specific edits here; put those in the product's editor. See
[Keeping products in sync](DESIGN_LANGUAGE.md#keeping-products-in-sync).

## Use

```cmake
# After JUCE is available:
include(libs/affine-ui/AffineUI.cmake)
target_link_libraries(MyPlugin PRIVATE affine_ui)
```

```cpp
#include <affine_ui/affine_ui.h>

affine::Theme theme;                              // the release's finish, in product colours
theme.panel.texture = affine::PanelFinish::Texture::anodised;
theme.panel.base = juce::Colour (0xff2a3037);
theme.palette.accent = juce::Colour (0xff4fd6ff);

affine::Knob threshold { apvts, "threshold", "What this control does." };
threshold.setTheme (theme);
threshold.setDiameter (affine::metrics::knobMedium);
threshold.setScale ({ -80, -60, -40, -20, 0 }, [] (double v) { return juce::String (juce::roundToInt (v)); });
```

Requires JUCE 8 and C++17. The module depends on `juce_gui_basics` and
`juce_audio_processors`.

## Contents

| Path | What |
|---|---|
| `affine_ui/render` | Procedural renderers: knob materials, faceplate textures (anodised, hammertone, powder, glass), panel hardware, glow and blur |
| `affine_ui/components` | `Knob`, `KeyButton`, `SelectorKeys`, `NeedleMeter`, `LadderMeter`, `NixieDisplay`, `TuningDial`, `IlluminatedLegends`, `DisplayLabel`, `Faceplate` and silkscreen helpers |
| `affine_ui/theme` | Palette, metrics, fonts, `LookAndFeel` |
| `docs/finishes.png` | Both products in each of the four finishes |
| `fonts` | Barlow Condensed, Michroma, Nixie One, DSEG14 Classic, Share Tech Mono (all SIL OFL 1.1; licences in `fonts/licenses`) |
| `DESIGN_LANGUAGE.md` | Principles, finishes, tokens, anatomy, component contracts, states |

## Rendering notes

Knobs, faceplates and meter faces are shaded per pixel at the display's physical
scale and cached, so they stay sharp at 100–200% and on Retina displays. Only the parts
that move are redrawn: the knob's knurled grip band is re-shaded (about a millisecond at
2x) when its value changes, and meters repaint their own bounds. Nothing here runs on the
audio thread.
