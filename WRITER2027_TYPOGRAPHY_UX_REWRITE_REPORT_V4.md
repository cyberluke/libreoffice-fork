# Writer 2027 V4 Remediation Report

## Commit
Pending after push (see git log on `cyberluke/lukas_dev`).

## Runtime build identity
- branch `lukas_dev`
- `schema=4` (logged via `writer2027.build`)
- Executable: `D:\_SATIN_AI\LibreOffice\instdir\program\soffice.exe`
- Active Writer notebookbar variant audited: `notebookbar_writer2027.ui` and `notebookbar.ui` (see Style Gallery provenance).

## Root cause: Liberation survives Type System
Confirmed source/semantics root cause (direct formatting precedence), matching spec V4 §2:
- Writer precedence: direct char formatting overrides style definitions.
- The reviewer's sequence (choose Liberation Serif -> Apply Editorial) leaves the direct family; `ApplyTypeSystem` (V3) only mutated style definitions and intentionally preserved direct formatting.
- Added the missing migration: managed direct family/size override clearing, caret/insertion clearing, paragraph-rhythm clearing for managed semantic paragraph styles, and post-verification, all in one undo transaction (spec V4 §3-13, §38).

Evidence capture implemented via logs (`writer2027typesystem.apply.ok` / `apply.verify_failed` with before/after counts + detected preset). Live four-state (style/direct/effective/toolbar) capture requires the UI runner (§11) and is NOT executed here.

## Managed typography migration
- `ScanManagedTypographyOverrides` / `ClearManagedTypographyOverrides` in the sw typography manager: manage `RES_CHRATR_FONT/CJK_FONT/CTL_FONT`, `RES_CHRATR_FONTSIZE/CJK/CTL_FONTSIZE`, and `RES_PARATR_LINESPACING`/`RES_UL_SPACE` on managed semantic paragraph styles only (custom styles preserved).
- `ClearManagedInsertionAttributes` clears caret/insertion managed attrs so new typing resolves to the semantic style.
- `ApplyPresetTransaction` = one undo group (RAII via `StartUndo`/`EndUndo`) covering style updates + direct cleanup + caret cleanup; ends with binding refresh; reports `success` only when the re-scan is clean and the detected preset matches (§36). Success is never claimed on exception (§58).
- Preserved: bold/italic/underline/strike/superscript/color/highlight/language/hyperlink/bookmarks/fields/tracked changes/comments (only the 6 font + 2 rhythm which-ids are reset, not a broad Clear Direct Formatting).

## Toolbar refresh
- `RefreshTypographyBindings` invalidates and re-queries, on the owning frame/view shell via real `SfxBindings`:
  `SID_ATTR_CHAR_FONT`, `SID_ATTR_CHAR_FONTHEIGHT`, `SID_STYLE_FAMILY2`, `SID_ATTR_CHAR_POSTURE`, `SID_ATTR_CHAR_WEIGHT` (branch slot ids, not guessed), then `Update()` so no document click is required (§12/§37).

## Style Gallery source/runtime provenance
- **Active UI file fixed**: the live stale gallery was a real runtime mismatch. `notebookbar_writer2027.ui` (the active Writer 2027 variant) still used `.uno:StylesPreview`; V3 only changed `notebookbar.ui`. Both Writer notebookbar variants now use `.uno:Writer2027StyleGallery` (§17/§20/§49).
- Installed asset: `instdir/share/config/soffice.cfg/modules/swriter/ui/notebookbar_writer2027.ui` now contains `.uno:Writer2027StyleGallery` (verified after rebuild).
- Controller registered in `Controller.xcu` and `sw.component` for `.uno:Writer2027StyleGallery` → `com.sun.star.comp.sw.Writer2027StyleGalleryToolBoxControl`.
- Installed `swlo.dll` contains `sw_Writer2027StyleGalleryToolBoxControl_get_implementation` and the gallery code (verified by symbol scan).
- Provenance logs added: `writer2027.build`, `writer2027.gallery.controller.initialize`, `writer2027.gallery.controller.createItemWindow`, `writer2027.gallery.model.rebuild` (with itemCount, expect >=9).
- `createItemWindow` runtime confirmation requires the Notebookbar Writer 2027 view to be active; NOT executed here (no UI runner) — see Tests.

## Style Gallery fixes
- **Hit-test coordinate bug fixed** (§24): `IndexAtInternal` now uses ONE convention (Option A: card starts include padding, pointer in content space by `X + scroll`); shared by paint, hit-test, keyboard scroll-to-visible.
- **Empty exception catches replaced** (§25/§58): `MouseButtonDown`, `MouseMove`, `KeyInput`, `Command` now use structured `Writer2027LogException`/`Writer2027LogUnknownException`. Model rebuild failures log and keep the last known good model (no silent one-card collapse).
- **Hover/focus/current separated** (§31): added `mnFocusIndex`; keyboard moves focus (ring), mouse sets hover, current uses pool-id highlight. Distinct rendering per state.
- Frame-bound path audited: controller uses `m_xFrame` (context) only; no `SfxObjectShell::Current()` / `SwModule::GetFirstView()` (spec §26).
- `itemCount < 9` is logged as a failure (spec §27), not silently shown.

## Type System UI fixes
- **Preview clipping** (§32): every section has an explicit measured rectangle; body sample wraps to two lines (`PaintBody`), code one ellipsized line, heading scale-to-fit; each section clipped to its own rect; preview surface height increased to 360lp.
- **Fallback presentation** (§33): structured per-role rows ("Heading\nNeue Montreal -> Inter", "Body\nSAP 72 installed as 72", etc.) instead of one comma line; alias vs substitution distinguished by requested/resolved comparison.
- **Current/selected/hover distinction** (§34): `mbPreviewing` drives a "Previewing X" header when the selected-for-preview differs from applied; CURRENT header stays authoritative.
- Action rail: the popup layout already separates the Apply button; Apply is disabled-equivalent only when Apply would be a no-op (selection resolve + current) at the owner; a same-preset apply is still allowed to clean overrides (spec §16/§35).

## Tests

| Gate | Command / flow | Exit | Result | Artifact |
|---|---|---|---:|---|---|
| Compile | `make svx` (j24 MSVC) | 0 | PASS | build_svx_sw.log |
| Compile | `make sw` (j24 MSVC) | 0 | PASS | build_sw_only.log |
| Compile | `make officecfg` | 0 | PASS | build_officecfg.log |
| Runtime asset | `notebookbar_writer2027.ui` uses Writer2027StyleGallery | — | PASS (verified in instdir) | — |
| Runtime binary | swlo.dll contains gallery ctor | — | PASS (symbol scan) | — |
| Launch | `soffice.exe --writer` clean profile | — | PASS (app stays alive; no AV/abort) | %TEMP%/<crash>.log empty |
| Gallery createItemWindow | Notebookbar view active | — | NOT VERIFIED (UI runner required) | — |
| `make CppunitTest_sw_uibase_uiview` | — | blocked | NOT VERIFIED (external build corruption) | — |
| `make UITest_sw_writer2027` | — | blocked | NOT VERIFIED (external build corruption) | — |
| Golden 100/150/200% | viewer | — | NOT VERIFIED (no pinned runner) | — |

## Runtime logs
- Path: `%TEMP%\writer2027.log` (created when Writer 2027 code paths run). In the clean-profile launch only benign startup ran; the gallery `createItemWindow` log requires the Notebookbar view (see Tests).
- No `writer2027_crash.log` content (no AV/abort captured).

## Visual artifacts
None captured. Real captures require the pinned visual runner; per spec §61 no synthetic mocks are used as evidence. All visual claims are RUNTIME-WIRED only.

## Undo/Redo
Source: managed migration runs in one undo transaction covering style + direct + caret cleanup (§13). Not executed (no UI runner).

## Save/Reopen
Not executed (no UI runner).

## Multi-document
Frame-bound refactor keeps A/B isolated at source level (§26/§47); not executed.

## 100% / 150% / 200%
NOT VERIFIED (no pinned visual runner).

## Status hierarchy
- **COMPILED**: all source builds (`make svx sw officecfg`) green.
- **RUNTIME-WIRED (partial)**: active Writer notebookbar variant now hosts `.uno:Writer2027StyleGallery`; controller is in the installed binary; app launches clean. `createItemWindow` runtime confirmation pending the Notebookbar view.
- **FUNCTIONALLY-VERIFIED**: NOT achieved (no UI runner / blocked harness).
- **VISUALLY-VERIFIED**: NOT achieved (no pinned runner).

## Remaining issues
1. External-build corruption still blocks `make build`, `CppunitTest_sw_uibase_uiview`, `UITest_sw_writer2027` (spec §60/§87) — need a clean worktree/CI runner.
2. Runtime gallery instantiation + the full scenario (§69: apply migration, caret typing, toolbar refresh, gallery apply, undo/redo, save/reopen) must run on the pinned runner; only source + compile + startup-smoke evidence exists here.
3. The `v4-gallery-*` and `v4-typesystem-*` golden captures are not produced locally.

Per spec §68, this is NOT claimed as accepted/verified: precise status is COMPILED + RUNTIME-WIRED(partial), with FUNCTIONAL/VISUAL VERIFICATION pending the pinned runner.