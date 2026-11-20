# Design system

The UI is built from three layers:

| Layer | Where | What |
|---|---|---|
| Tokens | `src/app/ui/Theme.qml` (singleton, module `CanUi`) | colors, type, spacing, radius, sizes, elevation, motion |
| Components | `src/app/ui/*.qml` (module `CanUi`) | `Icon`, `Card`, `Chip`, `IconButton`, `SectionHeader`, `EmptyState`, `Kbd`, `Divider`, `ExtTag`, `FocusRing` |
| Controls style | `src/app/style/*.qml` (module `CanStyle`) | `Button`, `ToolButton`, `TextField`, `ComboBox`, `TabBar`, `TabButton`, `ScrollBar`, `Switch`, `ToolTip` |

`CanStyle` is a real Qt Quick Controls style, selected in `main.cpp` with
`QQuickStyle::setStyle("CanStyle")`. Screens therefore just write `Button { }` and get the
custom look; controls the style does not define fall back to Basic.

No design references were present (`design_refs/` does not exist), so the look is derived
from first principles: an instrument-like dark UI, calm surfaces, one strong accent.

## Principles

1. **Data first.** Hex data, IDs and times are the content; chrome stays quiet (muted
   labels, 1 px borders, no gradients).
2. **One accent.** Iris violet marks the primary action, the selection, focus and the active
   tab. Nothing else competes with it.
3. **Numbers never jitter.** All numeric columns use a monospace face; chips use tabular
   figures (`font.features: { "tnum": 1 }`); numbers are right-aligned.
4. **Motion explains state.** 120–200 ms with ease-out curves, only for things that change
   state (theme, tabs, pause, sort, hover). Nothing loops except the "live" dot.
5. **Dark first, light equal.** Both palettes are complete and both pass WCAG AA.

## Color

All public colors are blends of a light and a dark palette (`Theme.blend`) driven by one
animated value `t` (0 = light, 1 = dark). One 200 ms animation cross-fades the whole UI.

| Token | Light | Dark | Use |
|---|---|---|---|
| `bg` | `#F3F4F7` | `#0E1014` | window background |
| `surface` | `#FFFFFF` | `#15181E` | cards, toolbar, status bar |
| `raised` | `#F8F9FB` | `#1B1F27` | keycaps, floating elements |
| `sunken` | `#FBFBFD` | `#111318` | text field wells |
| `popup` | `#FFFFFF` | `#1E222B` | menus, tooltips (dark) |
| `border` / `borderStrong` | `#E1E4EA` / `#C9CED8` | `#242934` / `#343A48` | hairlines / hover outlines |
| `text` | `#151922` | `#E7EAF0` | primary text, data |
| `textMuted` | `#4E5668` | `#A2AABA` | secondary text, times |
| `textFaint` | `#646C7E` | `#848D9E` | captions, headers, hints |
| `accent` | `#5A43D6` | `#A594FF` | primary button, selection, focus |
| `textOnAccent` | `#FFFFFF` | `#14102B` | text on accent fills |
| `ok` / `warning` / `error` | `#16794C` / `#9A5800` / `#C4342F` | `#4CD08A` / `#F2B54B` / `#F47C78` | status semantics |

State layers are translucent overlays of an "ink" color (black in light, white in dark):
`zebra` (≈2 %), `hover` (4.5 %), `pressed` (8 %), `selection` (accent 14–20 %),
`highlightChange` (warning 15–16 %).

### Contrast (WCAG 2.1, computed)

Lowest ratios of each text token against the four surfaces:

| Token | Light (min) | Dark (min) |
|---|---|---|
| `text` | 15.99 | 13.70 |
| `textMuted` | 6.69 | 7.07 |
| `textFaint` | 4.79 | 4.94 |
| `ok` / `warning` / `error` | 4.93 / 5.06 / 4.92 | 8.41 / 9.03 / 6.29 |
| accent text | 5.95 | 7.74 |
| on-accent text | 6.54 | 7.27 |

Everything meets AA (4.5:1) for normal text.

## Typography

| Role | Family | Size (px) | Weight |
|---|---|---|---|
| UI text | Segoe UI Variable Text → Segoe UI | 13 (`type.body`) | Normal / Medium |
| Secondary | same | 12 (`type.small`) | Normal |
| Captions / column headers | same, caps, +0.6 tracking | 11 (`type.caption`) | DemiBold |
| Titles | Segoe UI Variable Display | 15 (`type.title`) | DemiBold |
| Data (IDs, hex, times) | Cascadia Mono → Consolas | 13 (`type.mono`) | Normal (IDs DemiBold) |

Families are picked at runtime from `Qt.fontFamilies()`; all ship with Windows 11 (fallbacks
with Windows 10). Hex bytes are separated by a space with extra `wordSpacing` so 8 bytes read
as 8 distinct tokens.

## Spacing, radius, size

- Spacing scale: 2 · 4 · 8 · 12 · 16 · 24 · 32 (`space.xxs … space.xxl`).
- Radius: 4 (tags, keycaps) · 6 (controls) · 10 (cards) · pill.
- Sizes: control height 32, toolbar 60, status bar 36, table row 26, header 32.
- Page margins 24; gaps between bars and cards 12.

## Elevation

| Level | Surface | Shadow | Used by |
|---|---|---|---|
| 0 | `bg` | none | page |
| 1 | `surface` | soft, 1 px offset | table cards |
| 2 | `raised` | medium | floating pill |
| 3 | `popup` | large, 8 px offset | ComboBox popup |

In dark mode shadows are barely visible, so the surface color steps lighter per level.

## Motion

| Token | Duration | Used for |
|---|---|---|
| `motion.fast` | 120 ms | hover, press, focus |
| `motion.normal` | 160 ms | tab indicator, switch knob, chevrons, pill |
| `motion.slow` | 200 ms | theme cross-fade, page transition, row fade-in |
| `motion.highlight` | 650 ms | decay of the "byte changed" highlight |

Easing: `OutCubic` for movement, `InOutCubic` for the theme cross-fade.

New trace rows fade in only while they arrive slowly enough to be noticed (fewer than about
1.5 screens of rows per second). At high rates every visible row would be mid-fade all the
time and the data would look washed out, so fast streams render directly.

## Components

- **Card**: surface with 1 px border, radius 10, elevation shadow.
- **Chip**: status pill with optional dot (pulsing when live), label and tabular value;
  tones neutral / accent / ok / warning / error.
- **IconButton**: 32 px square, icon only, tooltip, focus ring.
- **SectionHeader**: title + subtitle + action slot.
- **EmptyState**: icon in an accent-tinted circle, title, message, keyboard hint.
- **Kbd**: keycap for shortcuts. **ExtTag**: outlined `EXT` marker for 29-bit IDs.
- **FocusRing**: 2 px accent ring 3 px outside the control, keyboard focus only.

## Icons

18 icons drawn by hand for this project (`src/app/ui/icons/*.svg`): 24 px grid, 1.75 px
stroke, round caps and joins, solid fills only for media glyphs (play, pause, stop). They
are tinted at runtime, so one SVG serves every state and both themes.

## Tables

- Sticky header (outside the scrolling list), caps captions, right-aligned numeric columns.
- Zebra striping at ~2 % ink, keyed to a stable row ordinal so stripes never flicker when
  old rows are evicted.
- Hover = 4.5 % ink; selection = accent tint + 2 px accent bar + accent ID text.
- Messages: changed bytes flash amber and decay over 650 ms; sorting by a live column uses
  coarse keys (2 significant digits) so near-equal rows do not swap on every update.
