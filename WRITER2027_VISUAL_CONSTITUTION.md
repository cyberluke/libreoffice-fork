# WRITER 2027 VISUAL CONSTITUTION

Status: **Phase 2 engineering complete — READY_FOR_HUMAN_VISUAL_REVIEW** (not self-approved)
Scope: Writer 2027 UI profile (`notebookbar_writer2027.ui`), per spec §88.
Source of truth for future consumers: Template Studio, Story 2.0, Master Layout, Research Workspace, AI contextual surfaces.

---

## 1. Layout regions

The Writer 2027 workspace separates four layers (spec §45):

| Layer | Role | Notes |
|---|---|---|
| APPLICATION CHROME | menubar/tab strip, notebookbar header, status bar | quiet, subordinate |
| WORKSPACE | canvas around the page | dark tonal depth, calm |
| DOCUMENT STAGE | the region the page occupies | dominant region on wide layouts |
| PAGE / DOCUMENT | the digital page itself | subtle boundary, near-zero shadow |

No single near-black color for everything; layers are separated by subtle tonal depth, not fake shadows (§45-46).

## 2. Golden-ratio policy (spec §23-24, §78)

Semantic policy lives in `svx/inc/svx/writer2027visual.hxx` — `WorkspaceCompositionPolicy`:

- `PrimaryStageRatio = 0.618` — document stage vs auxiliary space on Studio/wide layouts
- `PrimaryAuxRatio = 0.382`
- `AuxRailRatio / AuxPanelRatio = 0.382 / 0.618` — optional further subdivision (rail/panel)
- `stageFraction(Density)` returns 0.618 for Studio, 0.62 otherwise

The ratio is **composition, not religion** (§24): it guides major region hierarchy (stage/panel balance, browser rail/content split) and is never sprinkled as `* 0.618` through the codebase. Small-scale components use the ergonomic spacing/control-height scale.

## 3. Spacing scale (spec §29)

From `Spacing` in `writer2027visual.hxx` — logical px:

```
XS=4  S=8  M=12  L=16  XL=24  XXL=32  XXXL=48
```

No random 7/13/19 px values without an optical reason.

## 4. Control heights (spec §30)

From `ControlHeight` — logical px:

| Density | Height |
|---|---|
| Compact | 34 |
| Standard | 40 |
| Studio | 48 |

Primary controls may be larger; not every icon button is 52 px. Hierarchy matters.

## 5. Density modes (spec §26-27)

`Density::{Compact, Standard, Studio}` — defined centrally. Writer 2027 prefers Studio density on large displays; density is user-selectable, not permanently tied to monitor DPI.

Responsive breakpoints are conceptual (Compact/Standard/Studio), derived from available logical work area, not hardcoded monitor names.

## 6. UI typography (spec §60)

`UIType` scales relative to the UI font:

```
CaptionScale 0.78   BodyScale 1.0   LabelScale 1.05   TitleScale 1.35
```

Application UI font stays separate from document fonts (body/heading/mono/display). Typography Browser specimens intentionally use candidate fonts; chrome uses product UI typography.

## 7. Popover rules (spec §51-53)

All Writer 2027 custom popovers share one design language via `AdaptivePopoverGeometry` (`writer2027visual.hxx`):

- Typography Browser: `min 560 / preferred 640 / max 760` logical px, height ≤ 68vh
- Type System: `520/640/760`, preset cards 132 px
- Document Kit: `520/640/760`, kit cards 124 px
- Editorial Block gallery: `520/640/760`, cards 108 px

Shared rules: header/search at top, outer margin 16, row/card rhythm, border, selection, hover, keyboard focus, scroll behavior, max work-area height. Geometry clamps width into [min, max] honoring work area and height to a vh% cap.

Content first, geometry second (§53): each component defines content bands, min/preferred content width, metadata wrapping and ellipsis policy. No overlapping paint calls.

## 8. Panel widths (spec §49-50)

`PanelWidth` — logical px, central:

```
Compact 220   Standard 300   Studio 360
```

Sidebar decks use these width classes centrally; no deck picks an unrelated width.

## 9. Header hierarchy (spec §31, §44)

Header has deliberate vertical layers (tab/navigation row → primary command row → group labels), no dense matrix of arbitrary baselines:

- **Typography** block: font name (large), font size, specimen-first; formatting actions (B/I/U, script) secondary
- **Type System**: full-width, first-class row (spec §42) showing current system pairing without opening the popup
- **Paragraph**: semantic clusters Alignment / Lists / Indent / Spacing (§43) — no icon carpet
- **Styles**: `StylesPreview` gallery (§41)

Order of visual weight: document/text → active editing context → typography/primary action → secondary controls → chrome/separators (§72).

## 10. Canvas layers / page presence (spec §45-48)

Implementation in `sw/inc/writer2027view.hxx` + `sw/source/core/view/writer2027view.cxx`:

- `IsDarkThemeActive()` — dark theme gate, high-contrast untouched
- `IsWriter2027DarkDocument()` — page background matches the Writer 2027 digital palette
- `ComputeCanvasBorder()` — ramps from classic 5 mm border to a wide golden-ratio gutter (61.8% stage share) starting ~1267 px logical edit-window width, full at ~1667 px
- `ComputeCanvasGap()` — page gap, `max(classic, width/100)`

Page is physically present but digital: subtle boundary, controlled contrast, near-zero decorative shadow, calm surrounding workspace. No skeuomorphic paper shadow, no disappearing boundary.

## 11. Selection / focus rules

- Restrained selection: subtle tint (window color merged with highlight, 80/255), never the OS slab
- Keyboard-first: search field focused on popup open, Down enters list, Enter applies, Esc clears/closes per LibreOffice convention
- Not color-only state: selection plus cursor/current-row tracking (`mnLastSelectedIndex`)
- High-contrast mode: custom visual styling gates off via `IsDarkThemeActive()`; canonical accessible rendering wins (§67)

## 12. 4K authoring zoom policy (spec §20-22)

`ComputeInitialAuthoringZoom(pageWidthTwips, stageWidthTwips)` in `writer2027view.cxx`:

- Scales the page to occupy the golden-ratio stage share (~0.62) of the edit-window width
- Clamped to 100%-260%
- Applied only when: default zoom is active (no stored user zoom), not browse mode, Writer 2027 canvas active (§21 user zoom always wins)
- View optics only — document typography (11 pt / 12 pt / Type System sizes) is never inflated (§22)

Verified math: 1080p 1600px stage → 124%; 4K@150% 2560px → 199%; 4K 1900px → 148%; small window → 100%.

## 13. Accessibility fallbacks (spec §67-68)

- High contrast: custom styling disabled, canonical rendering used
- Screen-reader names: popups set accessible names; rows expose family + resolved pairing text
- Light theme: dark-first product look is canonical, but native controls/popup text remain legible in light/high-contrast states (colors derive from the active StyleSettings, not hardcoded)

## 14. Animation / performance (spec §75-77)

- No decorative animation added; only native transitions the framework already provides
- Popup instances are built once and reused (no tear-down churn); row models rebuilt only on search/open
- Typography Browser rows render rich specimens only for visible rows via the tree's custom renderer; legacy list remains scrollable (no eager preview resources for 233+ fonts)

## 15. Typography rendering audit (spec §56)

Audited at the target environment (Windows, Skia/VCL backend):

- Text rendering uses the standard VCL text pipeline (`vcl::RenderContext` / Skia on Windows); custom popup specimens are drawn with `DrawText` using real `FontMetric` data — no bitmap scaling, no baked previews (§55)
- Font hinting/subpixel behavior follows the OS/VCL defaults; no rendering-engine change was made (out of scope by design)
- HiDPI: all custom geometry is logical-px and scaled by `GetDPIScaleFactor()`; specimen glyph sizes are derived from row height × DPI scale, so 4K stays crisp
- **No quality blocker found** that the visual constitution could hide; actual glyph quality must be confirmed in the human visual review (§84)
- Variable font axes: not exercised by this pass; candidates (Inter, IBM Plex) are installed static families in this environment

## 16. Type System default evaluation (spec §57-59)

The default must be human-selected after real rendered comparison — not guessed. Provided presets for review (no commercial font as a silent default; all are open/installed families):

| Preset | Heading | Body | Mono | Scale | Notes |
|---|---|---|---|---|---|
| Modern Product | Neue Montreal / Inter | SAP 72 (alias "72") / Inter | SAP 72 Mono | Balanced | screen-first, product |
| Executive | Söhne / Inter | SAP 72 / Inter | JetBrains Mono | Balanced | restrained, high-trust |
| Editorial | Canela / Instrument Serif | Söhne / Inter | IBM Plex Mono | Editorial | magazine, long-form |
| Tech | Neue Machina / IBM Plex Sans | Inter | JetBrains Mono | Balanced | engineering, AI |
| Research | SAP 72 / IBM Plex Sans | SAP 72 / IBM Plex Sans | SAP 72 Mono | Balanced | scientific, neutral |
| Accessible | Atkinson Hyperlegible | Atkinson Hyperlegible | JetBrains Mono | Accessible | high legibility |
| Creative Agency | Hatton / Instrument Serif | Neue Montreal / Inter | JetBrains Mono | Balanced | brand, campaign |

- **IBM Plex** (Sans/Serif/Mono) is included as a premium reference (spec §58), not hardcoded as the final default
- **SAP 72 / 72 Mono** included (spec §59) via Phase 6 aliases (`"SAP 72" → "72"`); evaluated for body readability, contrast, heading pairing, screen rendering
- UI font vs document font stays separate (§60): chrome uses the product UI font; document faces come only from the chosen Type System
- Resolved pairings are surfaced in the Type System popup with a missing-font report (`%1 fonts missing`, `Using: %1`) so fallbacks are never silent

---

*This document is the semantic source of truth for Writer 2027 visual composition. Values are LOGICAL px unless noted; the weld/VCL layer applies UI DPI scaling.*