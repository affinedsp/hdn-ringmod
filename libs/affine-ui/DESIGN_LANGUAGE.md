# Affine design language

**Version 3.0** · shared by every Affine plug-in · implemented by the `affine_ui` JUCE module in this folder

Affine plug-ins are **rendered instruments** built as modern matte hardware: a black
powder-coated panel, dark knobs inside rings of white LEDs, rubber soft keys, and one
colour screen per product. Colour lives on the screen; everything else is monochrome, so
what the processor is doing is the brightest thing on the panel. The hardware is a
vehicle for clarity, not decoration: every LED, plot and digit reports real state.

<p align="center">
  <img src="docs/family.png" alt="Suppressor with its teal screen and HDN Ring Modulator with its amber screen" width="860">
</p>

## Principles

1. **One light.** A single softbox sits above and slightly left of the player. Every
   highlight, reflection and shadow in every product agrees with it, so the instruments
   read as physical objects and sit together on screen.
2. **Real materials, rendered.** Powder coat, spun-aluminium caps on anodised grips,
   rubber keys, LEDs and screen glass, shaded procedurally at the display's physical
   resolution, never stretched bitmaps.
3. **Truth before spectacle.** An instrument only shows what the processor actually did.
   Screens plot stored readings and break where there were none; stale readings go dark;
   nothing is interpolated, extrapolated or animated for effect.
4. **Colour means information.** The screen is the only coloured surface. LEDs are white;
   amber and red are reserved for attention and clipping, and every coloured state is
   also spelled out in text.
5. **Printed like an instrument.** Labels are printed on the panel, scale numbers are
   placed through the parameter's own mapping (a log control gets a log scale), and
   controls are grouped by stage in signal-flow order, left to right.
6. **One hardware kit.** Products share the panel, knobs, keys, LEDs, screen glass and
   typography. They differ only in their screen colour and layout.
7. **Native behaviour is part of the design.** Every control has the same drag, fine,
   exact-entry, reset, keyboard and host-menu behaviour, and every edit is one host gesture.
8. **Accessible by construction.** Every control has a role, name, formatted value and
   keyboard path; critical state is never carried by colour alone; text meets 4.5:1.
9. **Cheap to keep on screen.** Static layers are cached per physical scale; only
   dynamic parts repaint, and only when their value changes. The audio thread publishes
   atomics and never waits for the UI.

## Anatomy of an instrument

```
┌──────────────────────────────────────────────────────────────────────┐
│  W O R D M A R K                              ● status  /  mode keys │  header
│  DESCRIPTOR                                                          │
│ ╭──────────────────────────────────────────────────────────────────╮ │
│ │  SCREEN: what the processor is doing, in the product's colour    │ │  the hero
│ │  captions · plots and scopes · numerals · segmented bars         │ │
│ ╰──────────────────────────────────────────────────────────────────╯ │
│  STAGE ─────────────────   STAGE ───────────────────   OUTPUT ────── │  what you can change,
│   ◎ knob   ◎ knob            ◎ knob   ◎ knob   ▭ ▭        ◎ knob    │  in signal-flow order
│ ▱ affine                                                             │  maker's mark
└──────────────────────────────────────────────────────────────────────┘
```

- **Header:** the wide-tracked product name and its descriptor on the left; the product's
  status or primary mode on the right.
- **Screen:** the largest area; it shows what the plug-in is doing. See *The screen*.
- **Controls:** grouped by stage under section titles with a hairline, in signal-flow
  order. Knob axes in a row share one horizontal line, as they would on a real panel.
- **Maker's mark** bottom left. No screws or other fixings are visible.

## Tokens

### Palette

| Role | Value | Use |
|---|---|---|
| Panel | `#17181B` | Matte powder coat |
| Silkscreen | `#D8DBDF` | Product name, labels, key legends |
| Silkscreen dim | `#8A9097` | Descriptor, section titles, scale numbers |
| Accent | `#EEF4FF` | LED rings, key strips, readouts under knobs, focus |
| Screen | per product | Everything on the screen |
| Attention | `#FFB238` | Engaged monitoring or learning, hot segments |
| Danger | `#FF4A3D` | Clip cells and bypass only |
| Glass | `#050607` | Screen and readout glass |

| Product | Screen |
|---|---|
| Suppressor | Teal `#35E0C8` |
| HDN Ring Modulator | Amber `#FFB23F` |

A new product picks a screen hue distinct from the others and from attention and danger,
at least 4.5:1 on glass. Printed text is at least 4.5:1 against the panel (checked
against its darker lower edge too), and emitted colours are at least 4.5:1 against
glass; both are unit-tested per product.

### Typography

| Role | Face | Size |
|---|---|---|
| Product name | Barlow Condensed SemiBold, tracked 0.45 | 30 px, capitals |
| Descriptor | Barlow Condensed SemiBold, tracked 0.30 | 11.5 px, capitals |
| Control labels, section titles, captions | Barlow Condensed SemiBold, tracked 0.16–0.26 | 11.5–12.5 px, capitals |
| Scale numbers, axis labels | Barlow Condensed SemiBold | 9.5–10 px |
| Readouts and screen numerals | Share Tech Mono | 14–19 px; screen numerals 32–54 px |
| Maker's mark | Michroma, tracked 0.32 | 9.5 px |

All faces are bundled under the SIL Open Font License; see `fonts/licenses`.

### Geometry

| Token | Value |
|---|---|
| Knob sizes | large 84, medium 72, small 58 px (`metrics::knobLarge` …) |
| Knob sweep | 270°, 7 o'clock to 5 o'clock, up/right increases |
| LED ring | 23 LEDs, 7.5 px outside the knob; scale numbers 17 px outside |
| Screen | 4 px bezel, 3 px glass corners; 14–18 px inner margin |
| Display corners | 4 px |
| Outer margin | 36–40 px |

## The screen

Each product draws its own screen on `render::screenGlass` in its screen colour, with the
helpers in `affine::screen`:

- **Captions** name each area and reading in capitals at 85% (`screen::caption`); units
  and axis labels are smaller and dimmer.
- **Numerals** that report a reading are lit: the screen colour lifted towards white
  (`screen::lit`), with a soft glow, and dimmed to `--` without a reading.
- **Plots and scopes** draw a 1.6 px trace over a 5 px halo, with a faint grid. They show
  only stored readings: a history breaks wherever there was no reading, and an empty one
  says so (`NO AUDIO`).
- **Bars** are segmented (`screen::segments`); a cell lights when the level reaches its
  centre. Hot cells are amber and clip cells red, held for as long as the processor holds
  the clip.
- **Dividers** between areas are 1 px rules at 10% of the screen colour.

## Components

| Component | Contract |
|---|---|
| `Knob` | Rendered knob inside an LED ring lit up to its value, with printed scale numbers, a label and a glass readout. Drag anywhere (up/right increases), Shift for fine, double-click or Return for exact entry (Return commits, Escape cancels, malformed text never changes the value), Alt/Option-click or Home resets, arrows step, right-click opens the parameter menu with host items. Optional activity lamp for *inactive but editable* states. |
| `KeyButton` | Latching rubber key with an LED strip, an optional glyph and a printed legend; Space and Return operate it. |
| `SelectorKeys` | One key per choice of a choice parameter, in a row or grid; each press is one complete gesture; arrows move the selection. |
| `DisplayLabel` | A `juce::Label` shown as glowing characters with a status LED; the accessible text stays the label text. |
| `Faceplate` | Caches the panel texture, its edge and everything printed on it per physical scale. |
| `silkscreen::*` | Product name and descriptor, section titles, maker's mark. |
| `screen::*` | Captions, lit colour and segmented bars for product screens. |
| `render::*` | Panel texture, soft shadows, recesses, screen glass, LEDs, glow and halos. |
| `LookAndFeel` | Menus, tooltips and text entry in the family style. |

## States

| State | Presentation |
|---|---|
| Live | LEDs lit, numerals glow, plots advance |
| No data or stale (no processed audio for 400–500 ms) | Status LED off, numerals `--`, bars dark, plots break |
| Inactive but editable (the control does not act in the current mode) | Activity lamp off, label, LED ring and readout dimmed; the control stays fully operable |
| Disabled | Knob veiled, ring dark; not operable |
| Attention | Amber LED plus explicit text (e.g. `LISTENING TO DIFFERENCE`) |
| Clip | Red clip cell, held by the processor for one second, plus accessible text |
| Keyboard focus | White ring at the foot of the knob, outline around keys |

## Adding a product

1. Copy this folder unchanged into the plug-in as `libs/affine-ui`, `include()` its
   `AffineUI.cmake` after JUCE and link `affine_ui`.
2. Start from the default `affine::Theme` and set `palette.screen` to the product's colour.
3. Lay out the header, the screen and the control stages as above.
4. Publish telemetry from the processor through atomics with a freshness signal (a block
   counter or sequence) and present stale data as dark.
5. Add tests for contrast, control mapping and focus order, and a capture test that writes
   PNGs of every state for review.

## Keeping products in sync

This folder is identical in every Affine plug-in repository. Change it in one place, bump
the version in `affine_ui/affine_ui.h` and this document, and copy the folder to every
product in the same change set. `diff -r` between two checkouts must be empty.
