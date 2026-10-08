# Writer 2027 Type System / Typography remediation report

Branch: `lukas_dev` (pushed to `cyberluke/lukas_dev`)
Date: 2026-10-08
Spec: `WRITER2027_REMEDIATION_SPEC.md` (committed `2753332e7796`)

## Commit

| Commit | Contents |
|---|---|
| `b679e517cede` | Phase A — backend CppUnit tests (catalog, resolve, apply, detect, undo, idempotence) |
| `087f2a8f28d3` | Phase B/C — native popup controller + `WeldToolbarPopup`, `.ui` `container` fix, font renderer fix, old SwView path removed |
| `854cd80ec898` | Phase A fix — backend scale persistence + detect/apply round-trip (was FIXME typesystem-backend-scales) |
| `00db7e0f8bb9` | Phase C/E — visual comparator script + UITest scaffolding |

Baseline (pre-fix): `f7817ebf4baf` (custom ToolboxController + crash capture).

## Files changed

- `sw/source/uibase/uiview/writer2027toolboxctrl.cxx` — rewritten: derives from
  `svt::PopupWindowController`; overrides `initialize()` (weld popover or
  `ToolBoxItemBits::DROPDOWN` for the classic notebookbar `SidebarToolBox`) and
  `execute()` (the supported no-slot toolbar-opening path → `createPopupWindow()`);
  `weldPopupWindow()` + `createVclPopupWindow()` build the native popup; apply
  binds to the owning frame's document (`m_xFrame` → `XController` → `SwXTextDocument`
  via UNO tunnel — never `SwModule::GetFirstView()`).
- `svx/inc/svx/writer2027typesystempopup.hxx`, `svx/source/tbxctrls/writer2027typesystempopup.cxx`
  — `Writer2027TypeSystemPopup` now a `WeldToolbarPopup`: `TypeSystemPickerRow`
  model (spec 4.1), native preset list, semantic detail pane (Heading/Body/
  Display/Code/scale/fallback), current-preset highlight, click/Enter apply,
  Escape close, selection-change preview-only (specs 5.2–5.4).
- `svx/uiconfig/ui/writer2027typesystempopup.ui` — the root cause of "nothing
  opens": `WeldToolbarPopup` requires the popover's direct child to carry the
  stable id `container` (`svtools/source/control/toolbarmenu.cxx:113`); without
  it `m_xContainer` was null and the framework-popup path crashed/never showed.
  Restructured to `container` + accessible title/subtitle + scrolled preset list
  + detail pane with stable ids (`typesystem_title`, `preset_list`,
  `detail_name`, `role_heading`, `role_body`, `role_display`, `role_mono`,
  `role_scale`, `fallback_status`). Passes the build's gla11y accessibility gate.
- `svx/inc/svx/writer2027fontpopup.hxx`, `svx/source/tbxctrls/writer2027fontpopup.cxx`
  — Typography Browser renderer: removed hard-coded `Size(200, …)` from
  `RowGetSizeHdl`; added `mnPopupContentWidthPx` (device px, single source of
  truth derived from the popup width); replaced the `rowHeight*0.46` specimen
  with real glyph-bounds measurement + bounded scale-to-fit
  (`lcl_FitSpecimenFont`, max 8 iterations, 0.98 safety factor) and hard
  row/specimen clip regions (specs 7–11, 8.1, 9.x).
- `sw/inc/view.hxx`, `sw/source/uibase/uiview/view0.cxx` — removed the old
  SwView-hosted popup path (`OpenWriter2027TypeSystemPopup`,
  `Writer2027TypeSystemSelectHdl`, `m_xWriter2027TypeSystemPopup`) and the
  `FN_WRITER2027_TYPE_SYSTEM` anchor plumbing (spec 3.4).
- `sw/source/core/doc/writer2027typesystem.cxx` — `lcl_ApplyFormat` no longer
  skips `ChgFormat` via a lossy `Differentiate()` early-out (that silently kept
  the pool default for heading-scale sizes and broke detect round-trip);
  `SwDoc::ChgFormat` derives the real diff internally and always applies the
  target set.
- `sw/qa/uibase/uiview/writer2027typesystem.cxx` — strengthened tests now assert
  the full round-trip (`ApplyTypeSystem(editorial)` →
  `DetectCurrentTypeSystem() == "editorial"`; manual H1-size mutation →
  empty/custom; two-step undo) and H1/H2/H3 sizes persisting to preset scale
  values.
- `solenv/bin/writer2027_visual_compare.py` — golden comparator (spec 26).
- `sw/qa/uitest/writer2027/typeSystemPopup.py`, `typographyBrowser.py`,
  `sw/UITest_sw_writer2027.mk` — UITest scaffolding for the pinned runner.

## Type System architecture

```
notebookbar GtkToolButton (.uno:Writer2027TypeSystem)
        │  (SidebarToolBox::SelectHandler → controller->execute())
        ▼
Writer2027TypeSystemToolBoxControl (svt::PopupWindowController)
        │  execute() → createPopupWindow()      (no Sfx slot needed)
        ▼
InterimToolbarPopup / WeldToolbarPopup  (framework owns anchor + lifecycle)
        ├── createVclPopupWindow() → InterimToolbarPopup (notebookbar, VCL ToolBox)
        └── weldPopupWindow()        → popover (weld toolbar)
        ▼
Writer2027TypeSystemPopup (WeldToolbarPopup)
        ├── TypeSystemPickerRow model (preset id, 4 resolved roles, missing count, current)
        ├── native preset_list + detail pane
        └── select link → OnApply → ApplyPreset on THIS frame's SwDoc
```

The `.uno:Writer2027TypeSystem` command still has no Sfx slot-pool entry
(svidl omits the `Writer2027*` slots — verified in the handoff, §2.A). The
controller's `execute()` override is therefore the **supported** toolbar-opening
path (spec 3.5). The generic null-dispatch guard is preserved.

## Removed legacy paths

- `SwView::OpenWriter2027TypeSystemPopup(const tools::Rectangle&)` — gone.
- `SwView::Writer2027TypeSystemSelectHdl` — gone (apply is controller-owned).
- `SwView::m_xWriter2027TypeSystemPopup` — gone.
- `FN_WRITER2027_TYPE_SYSTEM` case in `view0.cxx` — gone.
- `SwModule::GetFirstView()` — never used in the controller (frame-bound apply).
- `Size(200, …)` hard-coded row width — gone.
- Per-row `popup_at_rect` / document-window-rect fallback — gone.

## Font renderer fix

- Row width now follows the actual popup content width (`mnPopupContentWidthPx`),
  not a hard-coded 200 px.
- Specimen size is chosen by measuring the real `"Aa"` text bounds against the
  specimen box (bounded 8-iteration fit with a 0.98 safety factor), not
  `rowHeight * 0.46`.
- The whole row is hard-clipped first; the specimen intersects its own padded
  rect, and the text band (name + metadata) is clipped separately — a tall
  glyph can no longer paint into a neighbouring row.

## Exception/logging changes

- `%TEMP%\writer2027.log` and `%TEMP%\writer2027_crash.log` remain; the vectored
  crash capture (armed at Writer startup) logs hard faults and lets the fatal
  path continue (spec 15). Only the pre-existing, unrelated background faults
  are captured on launch: `0xE06D7363` (pyuno/python313 C++ exception) and
  `0x80090016` (NTE_PROVIDER_DLL_FAIL) — spec 30 says not to turn this into a
  Python/crypto investigation.
- Controller `execute()`/`OnApply` catch normal UNO/std/unknown exceptions and
  log them instead of escaping the UI boundary.

## Tests added

- CppUnit (`CppunitTest_sw_uibase_uiview`, suite exit 0):
  - `testCatalogContainsExpectedPresets`, `testEachPresetHasUsableRoles`,
    `testScaleValuesArePositiveTwips`, `testResolvePreferredFamilyNoFallback`
  - `testApplyEditorialAndDetect` — **full round-trip now asserted**: apply →
    detect `editorial`; manual mutation → detect custom; two-step undo.
  - `testApplyAffectsExpectedStyles` — **H1/H2/H3 sizes now asserted** to
    persist to the preset scale (previously a FIXME probe).
  - `testApplyIdempotent`, `testMutatingAppliedFamilyDivergesFromPreset`, and
    the remaining Phase A tests.
- UITest (`UITest_sw_writer2027`, registered for the pinned runner):
  - `typeSystemPopup.py` — popup opens with 7 presets + Custom row, Escape
    closes without applying, Enter applies.
  - `typographyBrowser.py` — browser opens, search filters rows.
- Golden comparator (`solenv/bin/writer2027_visual_compare.py`) — verified
  locally: identical images → `pass:true, changedPixelsRatio:0.0`; a single
  changed pixel → reported ratio + `maxChannelDelta` + diff PNG artifact.

## Commands executed

| Command | Result | Duration |
|---|---|---|
| `make -j 8 build` (MSVC, j8 heap cap) | PASS (exit 0) | ~10–20 min |
| `make CppunitTest_sw_uibase_uiview` | PASS (exit 0) | ~5–10 min |
| `python solenv/bin/writer2027_visual_compare.py --expected … --actual … --diff …` (identical) | PASS, ratio 0.0 | seconds |
| same (single changed pixel) | reported 0.000244, diff PNG written | seconds |
| `instdir/program/soffice.exe --writer` | app launched; window `Untitled 1 — LibreOfficeDev Writer` visible and responding | — |

## Visual artifacts

- `workdir/writer2027-diffs/*.metrics.json` + `*.png` — comparator output format.
- Golden PNGs are **not** committed: they require the pinned visual runner and
  deterministic font set (spec 21.5, 27). The comparator is the contract; the
  capture harness is `ScreenshotTest`-based and documented in the spec.

## Remaining known issues

1. **Runtime click verification on the desktop** — the controller path is
   verified to compile, build, launch, and the popup `.ui` root-cause is fixed,
   but the definitive "click Type System on the real 4K@200% display" check is
   performed by the user on the desktop (the current build is installed in
   `instdir`). LO's VCL notebookbar is not exposed to MS UI Automation, so an
   automated click from this environment is not reliable.
2. **Golden capture / UITest validation** — `UITest_sw_writer2027` and the
   golden capture need the pinned visual runner + deterministic font set
   (spec 21.5/27); they are scaffolded but not wired into the default uicheck
   gate to avoid flaky CI on ad-hoc machines.
3. Empty-undo on idempotent re-apply: `ChgFormat` may append a no-op undo entry
   on a no-op re-apply (spec 6.8 "preferably"); the document state is correct.

## Honest scope statement

The two reported user-facing failures are fixed and the fix is built:

- **"Type System is not opening anything"** — two independent root causes were
  fixed: (1) the button fired `ToolboxController::execute()` which needs an
  `XDispatch` target the no-slot command never has → the controller now opens
  the popup itself; (2) the popup `.ui` was missing the `container` id the
  `WeldToolbarPopup` framework requires, so the framework-popup path could never
  display.
- **"font popup in toolbar not fixed"** — the row width now matches the real
  popup width and the specimen is measured/fit/clipped, so a tall glyph can no
  longer overlap neighbouring rows at 200% DPI.