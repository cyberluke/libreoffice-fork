# WRITER 2027 VISUAL BEFORE/AFTER

Status: Phase 2 engineering complete. Screenshots captured from the running product on a real 4K display (3840×2160, Windows scaling in effect). No agent may approve visual quality — this document is the human review package (§87, §84).

Screenshots live in `v271/review/`.

---

## 1. Blank Writer 2027 document (wide)

- **Problem**: legacy Writer defaulted to a physically literal, visually tiny 100% page — on 4K the body text was miserable and the page looked like a small island in an ocean of canvas.
- **Design principle**: screen-first authoring optics; document typography stays semantically real (11 pt body), the *view* magnifies it (§20, §22).
- **Implemented mechanism**: `ComputeInitialAuthoringZoom()` — derives initial zoom from page/stage geometry + golden-ratio stage share (0.62), clamped 100-260%; `ComputeCanvasBorder()` — ramps the canvas gutter to keep the page at ~61.8% of the stage on wide windows.
- **Result**: `01_blank_writer2027_wide.png` — page dominates, body type reads substantially larger than legacy 100%, calm dark workspace.

## 2. Blank Writer 2027 with sidebar

- **Problem**: sidebar decks each chose unrelated widths; the stage shrank arbitrarily.
- **Design principle**: sidebar participates in the composition; panel widths central (§49-50).
- **Implemented mechanism**: `PanelWidth` classes (Compact 220 / Standard 300 / Studio 360) in `writer2027visual.hxx`; sidebar opens by default in the Writer 2027 mode.
- **Result**: `02_blank_writer2027_sidebar.png` — stage + contextual region form a deliberate composition.

## 3. Typography Browser

- **Problem**: the font picker was a tiny dropdown; premium open-source faces looked like plain names in a combo box (§32, §37).
- **Design principle**: specimen-first — the user should appreciate each face.
- **Implemented mechanism**: `Writer2027FontPopup` — 560-760 logical px wide (preferred 640), 72 px specimen-first rows, preview glyph ≈46% of row height, search at top, family + metadata line, height capped to 68vh via `AdaptivePopoverGeometry`.
- **Result**: `03_typography_browser.png` — faces are visually appreciable, not tiny menu items.

## 4. Type System

- **Problem**: Type System was a small text command buried under script buttons (§42).
- **Design principle**: first-class differentiator — shows current system + pairing without opening the popup.
- **Implemented mechanism**: full-width "Type System" row in the Typography block; `Writer2027TypeSystemPopup` with 132 px preset cards rendering heading/body specimens in the resolved families, current-preset checkmark, missing-font reporting.
- **Result**: `04_typesystem.png` — Type System is a deliberate product surface.

## 5. Styles

- **Problem**: a 13-card micro-strip communicates no typographic hierarchy (§41).
- **Design principle**: large screen means room to communicate hierarchy, not more tiny items.
- **Implemented mechanism**: `StylesPreview` gallery integrated into the Home Styles section.
- **Result**: `05_styles.png` — styles as typographic previews.

## 6. Home full

- **Problem**: header read as a legacy office ribbon / icon carpet (§31).
- **Design principle**: font choice is important, Type System is important, formatting actions are secondary — not all controls have equal weight.
- **Implemented mechanism**: Typography block (font name + size + specimen), Type System full-width row, Paragraph semantic clusters (Alignment/Lists/Indent/Spacing), restrained Styles section.
- **Result**: `06_home_full.png` — deliberate hierarchy, no icon carpet.

## 7. Narrow window

- **Problem**: wide-layout chrome must collapse gracefully, no clipping (§70).
- **Design principle**: reduce labels, keep primary tasks intact.
- **Implemented mechanism**: notebookbar responsive sections (priority classes), popover geometry clamps to work area.
- **Result**: `07_narrow_window.png` — graceful collapse, no overlap.

## 8. Story mode

- **Problem**: chrome-heavy editing environment for long-form writing.
- **Design principle**: page is the product.
- **Implemented mechanism**: `FN_WRITER2027_STORY` — draft layout, hidden rulers, page-width fit on the same document (view-only, no content mutation).
- **Result**: `08_story_mode.png` — chrome-reduced authoring view.

## 9. Standard UI after switch

- **Problem**: previously the modified Tabbed UI was the only way to reach Writer 2027 features (UI island).
- **Design principle**: Writer 2027 is a first-class mode; switching away/back must preserve it fully (§93).
- **Implemented mechanism**: Phase 1 ToolbarMode integration — `Writer2027` registered as a real mode with its own resource.
- **Result**: `09_standard_ui_after_switch.png` — standard UI intact.

## 10. Writer 2027 after switch back

- **Result**: `10_writer2027_after_switch_back.png` — full Writer 2027 preserved after switching away and back.

## 11. Typography specimen (§86)

- **Problem**: the "g" acceptance scene — a lowercase g should be large enough to appreciate bowl/ear/link/terminal/contrast/spacing/hinting/curvature (§19, §39).
- **Design principle**: authoring optics, not document metric cheating (§40).
- **Implemented mechanism**: initial authoring zoom + specimen rendered at 96 pt on the page for review.
- **Result**: `11_typography_specimen.png` — `g / ag / Hamburgefontsiv / R&g` at reviewable scale.

---

## Known issue (not Writer 2027)

Reproduced deterministically: opening a **second** document while the sidebar (AI deck, CEF `AIAssistantPanel`) is open crashes the process inside `libcef.dll` (NULL deref in a CEF thread during second WebView creation). This reproduces in **standard UI with sidebar open** as well, so it is a pre-existing OfficeLabs AI/CEF limitation, not a Writer 2027 defect (spec §82 — AI runtime is out of scope). Workaround for review: one document per instance, or close the sidebar (View → Sidebar) before opening a second document.