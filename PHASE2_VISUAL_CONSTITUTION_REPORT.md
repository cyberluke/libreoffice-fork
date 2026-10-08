# PHASE 2 FINAL REPORT — Writer 2027 Visual Constitution

Status: **READY_FOR_HUMAN_VISUAL_REVIEW** (Phase 2 engineering complete; no agent self-approval — spec §84, §93)

## A. Visual Constitution

Implemented as one semantic source of truth: `svx/inc/svx/writer2027visual.hxx` (tokens) + `WRITER2027_VISUAL_CONSTITUTION.md` (documented policy).

- `Spacing` 4/8/12/16/24/32/48 (logical px)
- `ControlHeight` Compact 34 / Standard 40 / Studio 48
- `UIType` caption/body/label/title scales
- `Radius` Small 3 / Medium 6; `Elevation` 1 (near-zero decorative shadow)
- `PanelWidth` Compact 220 / Standard 300 / Studio 360
- `Density` enum Compact/Standard/Studio

## B. Golden-ratio composition

`WorkspaceCompositionPolicy` in `writer2027visual.hxx` — semantic (0.618 stage / 0.382 aux, rail 0.382/0.618), never sprinkled as `*0.618`.

Runtime application:
- Wide stage gutter: `ComputeCanvasBorder()` ramps to a 61.8% stage share starting ~1267 logical px edit-window width, full at ~1667 px
- Popover internal composition: specimen/reserved column + name/metadata band

## C. 4K authoring optics

- `ComputeInitialAuthoringZoom(pageW, stageW)` — page to ~0.62 of stage width, clamp 100-260%, applied only at initial view when no user zoom, not browse mode, Writer 2027 canvas active (§21 user zoom wins)
- Verified: 1080p 1600px → 124%; 4K@150% 2560px → 199%; 4K 1900px → 148%; small → 100%
- `IsWriter2027CanvasActive` gates on dark theme + Writer 2027 dark page (high-contrast untouched)
- Document typography remains real (11/12 pt; Type System sizes) — view optics only (§22)

## D. Typography Browser

- Geometry: `AdaptivePopoverGeometry` 560/640/760 logical px, height ≤ 68vh
- Specimen-first rows: 72 px Studio rows, preview glyph ≈46% row height, reserved specimen column (72 px), family + metadata line
- Search at top ("Search fonts…"), focus-on-open, Esc clears/closes, Enter applies (§35, §66)
- Font measurement: real `FontMetric`/`get_approximate_digit_width`, no `len*8` (§54)
- Metadata descriptors: Editorial · Serif, Modern · Humanist, Display · Brand, Technical · Mono (§36)
- Legacy list stays scrollable; specimens render only for visible rows (custom renderer) (§77)

## E. Header

- Typography block: font name + size + specimen-first; B/I/U/script secondary (§31)
- Type System: full-width first-class row showing pairing (§42)
- Paragraph: semantic clusters Alignment / Lists / Indent / Spacing — no icon carpet (§43)
- Deliberate vertical layers, not arbitrary baselines (§44)

## F. Sidebars

- `PanelWidth` classes defined centrally (§49-50)
- Sidebar opens by default in Writer 2027 mode (`Sidebar=Opened`)
- Known limitation: the AI deck (`AIAssistantPanel`, CEF) crashes the process when a **second** document opens with the sidebar open — reproduced in standard UI too, pre-existing OfficeLabs/CEF issue, out of scope (§82)

## G. DPI/density matrix

| Environment | Density | Result | Known issue |
|---|---|---|---|
| 3840×2160 @125% | Studio (default) | stage ~61.8%, large typography | none observed |
| 1920×1080 | Studio | zoom clamps to 100-260% band | popover width clamps to work area |
| narrow window | Studio | sections collapse (priority classes) | — |
| high contrast | — | custom canvas gates off; canonical rendering | — |
| light theme | — | colors derive from StyleSettings; legible | dark-first is canonical |

## H. Screenshots (actual running product, 4K)

`v271/review/`:
- `01_blank_writer2027_wide.png`
- `02_blank_writer2027_sidebar.png`
- `03_typography_browser.png`
- `04_typesystem.png`
- `05_styles.png`
- `06_home_full.png`
- `07_narrow_window.png`
- `08_story_mode.png`
- `09_standard_ui_after_switch.png`
- `10_writer2027_after_switch_back.png`

## I. Typography specimen

- `v271/review/11_typography_specimen.png` — `g / ag / Hamburgefontsiv / R&g` (§86)
- Fonts reviewed: Liberation Serif (specimen render), IBM Plex family + SAP 72/72 Mono + Inter + Atkinson Hyperlegible as Type System candidates (installed aliases; §57-59)
- Preview environments in Type System popup render heading/body in resolved families

## J. Human review

- **READY_FOR_HUMAN_VISUAL_REVIEW** — pending explicit human approval (§84)
- After explicit human approval: `WRITER_2027_VISUAL_CONSTITUTION_APPROVED`

---

## Engineering regression note (crash fixes)

During this pass, three real defects in the Writer 2027 popup code were found and fixed (all reproducible, all verified fixed):

1. **Type System popup hang on 2nd click** (read as a crash): re-pop of an already-open popover wedged weld. Fix: re-entrancy guard in `Open()` (applied to all 4 popups: typesystem, font, blockgallery, documentkit).
2. **Off-by-one** between `maRowIds` and `maResolved` when the inert "custom" row is present → wrong pairings + last preset skipped. Fix: preset-index translation.
3. **Nullptr risk** in `lcl_PresetMatches` (pool styles) → added guards.

Incremental build green (`[build ALL]`); regression: 3× Type System open, document load, no crash. The remaining reproducible crash (2nd document + open sidebar → libcef) is the pre-existing OfficeLabs AI/CEF limitation, not Writer 2027.