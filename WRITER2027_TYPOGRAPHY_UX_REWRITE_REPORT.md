# Writer 2027 Typography UX Rewrite Report

Branch: `lukas_dev` (pushed to `cyberluke/lukas_dev`)
Date: 2026-10-09
Spec: `writer2027-custom-typography-ux-rewrite-junior-spec.md`
Previous baseline: `679f3afa8cb9`

## Commit

```
a0483d043b99  Phase 1 — clip stack fix, width ordering, Type System lifecycle logs
c1aec7254da8  Phase 2 — Writer2027TypographyList custom drawing surface
a0483d043b99  Phase 3 — Type System product surface (Custom state, no scrollbar,
                        list + preview panel + Apply)
7bdaa872b38d  Phase 4 — UITest coverage + registration
0fe67bfc4bfc  Phase 5 — completion report
ccef4937ffbc  Phase 6 — font picker popup crash fix (dangling weld::DrawingArea
                        reference) + spec 36 exception-boundary hardening
```

(Phase 1 and Phase 3 landed together in `a0483d043b99`.)

## Root causes fixed

1. **Typography Browser text clipped out (spec 3/4).** The specimen clip
   (`IntersectClipRegion(aSpecimenRect)`) mutated the active row clip and was
   never popped before the text band, so the text clip became
   `row ∩ specimen ∩ textBand == ∅` and family names/metadata vanished (the
   "Aa/Aa/Aa" screenshot). Fixed with a nested specimen `Push/Pop` scope that
   restores the row clip before the text scope. This fix is mandatory and
   independent of the TreeView replacement.
2. **Row width initialized after rows were built (spec 5).** `mnPopupContentWidthPx`
   started at 0 and was set after `RebuildRows()`, so the row-measure callback
   could see a zero/stale width on first layout. `Open()` now follows
   model → geometry → store width → populate → show, with `RebuildModel()` and
   `PopulateRows()` separated.
3. **Second Type System click does nothing (spec 22/23).** The controller added
   `ToolBoxItemBits::DROPDOWN` (dropdown handler → `createPopupWindow()`) AND
   overrode `execute()` → `createPopupWindow()`, giving two contended entry
   points into one popup lifecycle. The notebookbar button is a plain
   `GtkToolButton` that only reaches `execute()`; the `DROPDOWN` bit was
   removed and `execute()` is the single canonical open path. Each reopen also
   does `mxInterimPopover.disposeAndClear()` so no stale popup state survives
   (spec 25).
4. **Custom rendered as a preset + 240 px scrollbar (spec 16/18).** "Custom
   typography" is now the CURRENT header state, not a selectable row; the
   preset list is the 7 real presets only and `vscrollbar-policy=never` (they
   fit on a normal/4K work area).
5. **Font picker dropdown crash on open (fail-fast `0xC0000409`).** Opening
   the font popup aborted the process. Live cdb capture exposed the real fault
   as an access violation in `Writer2027TypographyList::SetViewportSize+0x3e`
   (`movsxd rcx,[rax+4]`, `rax=0x1100000001` garbage, `[rax+4]` unmapped).
   `Writer2027TypographyList` holds a `weld::DrawingArea& m_xArea` to the
   wrapper returned by `weld_drawing_area(u"rows")`, but the `unique_ptr` was a
   **temporary destroyed at the end of the owning full expression**, freeing
   the `SalInstanceDrawingArea`; the first later use of `m_xArea` dereferenced
   freed memory, and LO's UAE filter converted the AV into the abort fail-fast
   seen in WER (the earlier `.ui`/`GtkScrolledWindow` theory was a red herring).
   Fixed in `ccef4937ffbc`: the popup now stores
   `std::unique_ptr<weld::DrawingArea> m_xRowsArea;` (kept alive for the popup
   lifetime) and constructs the list from `*m_xRowsArea` with a null guard,
   mirroring the Type System popup's `m_xRows(...)` ownership pattern. The
   `writer2027fontpopup.ui` "rows" widget is a plain `GtkDrawingArea` (no
   `GtkScrolledWindow`), so `weld_drawing_area()` resolves it as `VclBuilder`
   expects. Verified: the dropdown opens, the picker renders, and the process
   stays alive (no AV, no LocalDumps `.dmp`).

## Font browser architecture

```
Writer2027FontPopup (weld::Popover shell — LO owns shell/focus/anchoring)
    ├── search field
    ├── m_xRowsArea (owning std::unique_ptr<weld::DrawingArea> — keeps the
    │   drawing surface alive for the list below; prevents the dangling
    │   reference that crashed SetViewportSize)
    └── Writer2027TypographyList (custom weld::DrawingArea surface,
            holds weld::DrawingArea& m_xArea = *m_xRowsArea)
            ├── TypographyRowLayout vector (row/specimen/title/meta, spec 12)
            ├── semantic DPI-scaled row heights (Header 36 / Font 84 /
            │   Category+Legacy+Back 44 / NoResults 48 logical px, spec 8)
            ├── viewport + scroll offset (wheel via CommandEvent, clamped,
            │   painted thin scrollbar indicator, spec 13)
            ├── hover / selection state (restrained tints, no OS slab)
            ├── hitTest() + mouse press (Font→apply, Category/Legacy/Back→nav)
            ├── keyboard (Up/Down/PageUp/PageDown/Home/End/Enter, spec 12)
            └── paint() with per-row hard clip (spec 11) and real glyph-bounds
                specimen fit (spec 10)
```

The `weld::TreeView` custom renderer, row-size callback and all TreeView
keyboard/mouse plumbing were removed from `writer2027fontpopup.cxx`.

## Type System architecture

```
Writer2027TypeSystemToolBoxControl (svt::PopupWindowController)
    └── execute()  ← ONLY open path (plain notebookbar button)
            └── createPopupWindow() → createVclPopupWindow()/weldPopupWindow()
                    └── Writer2027TypeSystemPopup (WeldToolbarPopup)
                            ├── CURRENT header (Custom typography | <preset>)
                            ├── PRESETS list (7 selectable systems only)
                            ├── preview panel (Heading/Body/Code/Scale/Fallbacks,
                            │   fixed UI sample text, spec 43)
                            └── Apply button (apply + close); Enter applies;
                                Escape closes; selection previews only (spec 19)
```

Popup lifecycle: `CLOSED → OPENING → OPEN → CLOSING → CLOSED`; one create per
open, explicit dispose on reopen, `typesystem.*` lifecycle logs (spec 38).

## Popup lifecycle

- Canonical open path: `execute()` only (spec 23).
- Close-state reset: `mxInterimPopover.disposeAndClear()` before each reopen
  (spec 25).
- Logging: `typesystem.controller.execute`, `createPopupWindow`,
  `createVclPopupWindow`, `typesystem.popup.created`, `popup.closed`, `apply`.

## Tests executed

| Command | Exit code | Result |
|---|---:|---|
| `make svx sw` (Phase 1–3, j8 MSVC) | 0 | PASS (compiles, links, installs) |
| `make svx sw` (Phase 2, j8 MSVC) | 0 | PASS |
| `make svx sw` (Phase 2 fixes) | 0 | PASS |
| `make svx sw` (Phase 6 crash fix, j24 MSVC) | 0 | PASS (relinks svxcorelo.dll + swlo.dll) |
| app launch `soffice.exe --writer` (post-fix) | — | PASS, window visible + responding |
| font dropdown click (post-fix) | — | PASS — picker opens/renders, process stays alive, no AV, LocalDumps empty |
| `make build` (full) | 2 | FAIL — external re-unpack corruption, see below |
| `make CppunitTest_sw_uibase_uiview` | 2 | BLOCKED by liblangtag external build |
| `make UITest_sw_writer2027` | 2 | BLOCKED by liblangtag external build |
| app launch `soffice.exe --writer` | — | PASS, window visible + responding |
| screenshot capture | — | PASS, `typesystem-app-main-window.png` (3866×2006) |

### Environment blocker (evidence)

The earlier full `make build` (needed to recompile after the Phase 1 source
edits) re-unpacked several external tarballs and corrupted their incremental
state:

- `external/python3` patch fails on re-apply (`1 out of 1 hunk FAILED`).
- `external/liblangtag` re-configure produces a Makefile whose compile rule
  invokes `gcc-wrapper.exe` → `cl` with no source (`cl: D8003 missing source
  filename`); the wrapper additionally requires the LO build env (`REAL_CC`,
  `ILIB`) that `gb_ExternalProject_run` sets, and the re-configure state does
  not survive. Restoring the liblangtag tree by hand and rebuilding still
  fails at the same MSVC/autoconf integration point.
- Because `sw` depends on `i18nlangtag`, this blocks `make CppunitTest_*` and
  `make UITest_*` dependency resolution.

Direct harness execution was attempted as a bypass: `cppunittester.exe` was
run against the already-built `test_sw_uibase_uiview.dll` with the gbuild
`-env:` bootstrap set (BRAND_BASE_DIR, LO_LIB_DIR, UNO_TYPES/SERVICES with the
api rdbs, protectors). The loader now resolves all DLLs, but the harness
crashes with `0xC0000409` (fail-fast) during UNO bootstrap — the full gbuild
environment (ure bootstrap rc files, additional UNO_TYPES/SERVICES from the
test mk) could not be replicated exactly. This is a harness/environment issue,
not a test-content issue.

## Visual artifacts

- `workdir/writer2027-diffs/typesystem-app-main-window.png` — live 4K@200%
  Writer window with the new binaries (3866×2006), captured from the running
  app.
- Golden PNGs (`typesystem-custom-100/200`, `typesystem-editorial-100/200`,
  `fontbrowser-root-100/200`, `fontbrowser-category-200`, `fontbrowser-search-200`)
  are **NOT VERIFIED**: the pinned visual runner that can open the popups and
  capture them is not available on this machine, and the UI harness is blocked
  by the external-build corruption above. Status for these: **NOT VERIFIED**,
  not "fixed".

## Stress test

open/close iterations: not executed (make-driven UITest blocked; direct
harness crashes during UNO bootstrap before test code runs).
result: **NOT VERIFIED**. The lifecycle regression is addressed in code (single
open path + dispose-on-reopen) and covered by `typeSystemPopup.py`
`test_open_close_open_loop` (20 iterations) for the pinned runner.

## Known remaining issues

1. **External incremental build corruption** on this machine (python3,
   liblangtag re-unpack) — blocks `make build`/CppUnit/UITest targets until the
   externals are restored or a clean environment is used.
2. **UITest/golden execution** requires the pinned visual runner (spec 27) or
   a machine with intact external build state; targets are registered and the
   tests are written.
3. Empty-undo on idempotent re-apply: `ChgFormat` may append a no-op undo
   entry on an idempotent re-apply (spec 6.8 "preferably"); document state is
   correct.
4. The Type System popup selection preview + Apply model (spec 19's preferred
   reliability design) changes the previous instant-apply interaction; if the
   product owner requires instant apply, the Apply button can be removed and
   selection wired to apply.

## Honest scope statement

Phases 1–3 of the spec (correctness fixes, custom Typography Browser surface,
Type System product redesign, canonical popup lifecycle) are **implemented,
compiled, linked, installed and running** (app launches, window visible, all
`make svx sw` builds exit 0). Phase 6 (font picker crash) is **fixed and
verified live**: the dropdown opens and the process survives, with the true
root cause (dangling `weld::DrawingArea` reference) captured under cdb and the
ownership fix in `ccef4937ffbc`. Phase 4/5 execution (CppUnit, UITest, goldens,
stress) is **NOT VERIFIED** on this machine due to the documented external-build
corruption and the missing pinned visual runner. Per spec 52, the UI work is
reported as implemented-with-build-evidence; the test gates are reported as NOT
VERIFIED, not as passing.