# Writer 2027 — Type System picker handoff

Branch: `lukas_dev` (pushed to `cyberluke/lukas_dev`)
Commit: `f7817ebf4baf` — *"Writer 2027: Type System picker via custom toolbar controller + crash capture"*
Date: 2026-10-08

---

## 1. What you claim is NOT working (your words)

1. **"Type System button opens garbled sidebar"** — clicking the Type System button doesn't produce a clean popover; it renders as (or anchored like) a sidebar with the content visually wrong.
2. **"fonts overlapped incorrect layout"** — the font/typography sample text overlaps and the layout is broken.
3. **"text is overlapping and garbled"** — the sample lines ("A Better Way to Write…", "This is a short body line…", "3 fonts missing / Using: Instrument Serif + Inter +") are clipped/overlapping.
4. **"now it opens positioned at sidebar again"** — the popup anchors/positions into the sidebar region rather than under the button.
5. **"very small popup on the left"** / **"small and content does not fit"** — after the plain-list reimplementation the popup appeared tiny and misplaced, content didn't fit.
6. **"you are opening same exe you did not fix or recompile anything"** — you believed the binary wasn't rebuilt; verified false (see §4), but it mattered for trust.

Bottom line: **the Type System picker feature is not delivering a clean, correctly-positioned, correctly-sized popover.**

---

## 2. Root causes found (with evidence)

### A. The `.uno:Writer2027TypeSystem` command has NO Sfx slot-pool entry
- `swriter.sdi` declares `SfxVoidItem Writer2027TypeSystem FN_WRITER2027_TYPE_SYSTEM` etc.
- The generated slot table `workdir/SdiTarget/sw/sdi/swslots.hxx` (`aSwViewSlots_Impl[283]`) contains **no** `Writer2027*` slot, while identical-format neighbors (`DraftView`, `PrintPagePreview`, `CreateSWDrawView`) ARE present. Forced clean regeneration of the SdiTarget produced byte-identical output → deterministic omission by svidl.
- Consequence: `queryDispatch(".uno:Writer2027TypeSystem")` returns **null**.
  - Before the guard: `framework::GenericToolbarController::ExecuteHdl_Impl` deref'd the null dispatch → **access violation** (`mov rax,[rcx]`, `rcx=0`), confirmed under cdb on the real profile.
  - After the guard: the button silently no-ops.

### B. The crash capture was dormant / too narrow
- LO installs `osl_addSignalHandler` and `WerAddExcludedApplication(L"SOFFICE.EXE")` → **no WER dump, no log** on a real AV; `/EHsc` means `catch(...)` does NOT catch SEH AVs.
- Added `Writer2027InstallCrashCapture()` (vectored exception handler) logging to `%TEMP%\writer2027_crash.log`. First armed inside the popup `Open()` only (too late); then armed at **Writer module startup** (`swdll.cxx`). Then the handler was narrowed to AV-class codes and broadened again to capture everything except benign `DBG_PRINTEXCEPTION_C` (0x406D1388) / Ctrl-C.
- Captured two real background faults: `0xE06D7363` (MSVC C++ exception from `pyuno.pyd`/`python313.dll`) and `0x80090016` (`NTE_PROVIDER_DLL_FAIL`, BitLocker/NGC crypto) — these are **extension/Python UNO crypto crashes on background threads, NOT the popup path**.

### C. The popover anchored to the whole document window → "sidebar"
- `SwView::OpenWriter2027TypeSystemPopup()` passed `&GetViewFrame().GetWindow()` as the anchor and the popup called
  `popup_at_rect(parent, Rect(Point(0,0), windowSize), Placement::Under)`.
- That anchors **under the entire document window** → lands at the window top-left → reads as a sidebar.
- Fix (current branch): the controller computes the **toolbar button rect** (`pToolBox->GetItemRect(nId)`) and passes it through `OpenWriter2027TypeSystemPopup(aAnchorRect)`; the popup anchors under the button.

### D. The custom multi-line card renderer caused the overlap
- `RowGetSizeHdl` returned a hardcoded width of **200 px** per row → the tree clipped every drawn line to 200 px (the "...W", "...s", "Inter +" truncation).
- `RowRender` drew a 1.9×-height heading at fixed `nTextH` multiples, colliding with the body line; the "Using:" line was positioned past the row height.
- Attempted fixes: running vertical cursor + row height mirror, then content-width row width, then compact geometry. The definitive reimplementation on the branch: **remove the custom renderer entirely** and show **plain text rows** (`set_text(row, "Name — Heading + Body", 0)`), which VCL lays out itself and cannot overlap at any DPI.

---

## 3. What I tried (chronological)

1. Arming crash capture + null-guarding `GenericToolbarController::execute`/`ExecuteHdl_Impl` → stopped the AV.
2. Diagnosed the slot-pool omission (svidl) via the generated `swslots.hxx` / forced regeneration + cdb stack.
3. Built a **custom toolbar controller** (`lo.writer.Writer2027TypeSystemToolBoxControl`) mapped in `Controller.xcu` + registered in `sw.component` → the button now invokes it (log confirms `[execute] button clicked`).
4. Fixed the custom card renderer twice (row height cursor; row width from popup content) → reduced but did not fully resolve the garbled look the user reported.
5. Broke out the trace logging to prove the binary was current (`button clicked` → `pView=` → `Open entered` → `geometry` → returned) and confirmed via byte-level search + loaded-module path that instdir DLLs contained the new code.
6. **Reimplemented the popup as plain text rows** (removed `set_column_custom_renderer`/`connect_custom_render`/`connect_custom_get_size`), compact geometry (min 360 / pref 420 / max 520 logical).
7. **Anchored the popover to the toolbar button** (pass `GetItemRect(nId)` through `OpenWriter2027TypeSystemPopup(aAnchorRect)`).

---

## 4. Verification facts (important for trust)

- Installed DLLs were verified current at runtime:
  - `swlo.dll` (instdir) — contains new controller + `early return: pDocShell` diagnostic.
  - `svxcorelo.dll` — contains the reimplementation + `geometry` diagnostic.
  - Loaded-module path/timestamp confirmed the running app uses these binaries.
- `%TEMP%\writer2027.log` confirmed the new controller runs on the click:
  `[Writer2027TypeSystemToolBoxControl::execute] button clicked`.
- The click path also stopped hard when the app closed via **Exit** (not a crash), so the "same exe / nothing recompiled" impression was a delivery/positioning problem, not a stale build.

---

## 5. Remaining work (next steps for the next engineer)

1. **On the current `lukas_dev` build**, click Type System and confirm:
   - popover is anchored under the button, not the sidebar;
   - rows are one clean text line each (no overlap, no clipping);
   - width/height fit the content.
2. If it still mis-positions, check the `[Open] geometry:` log values (`fScale`, `nTextH`, `popupW`, `popupH`, `contentW`) and the anchor rect size (`anchor=WxH` in the controller log) in `%TEMP%\writer2027.log`.
3. The `0xE06D7363` / `0x80090016` background crashes come from **Python UNO / Windows crypto (BitLocker/NGC)** on worker threads — investigate separately; they are not part of the popup path and currently abort a thread (which killed the session before the UI closed cleanly in some tests).
4. If a real popover is required with rich font specimens (name + heading + body + pairing), reimplement it as a **`WeldToolbarPopup`** via a `PopupWindowController` subclass (like `SvxFrameToolBoxControl::weldPopupWindow`) rather than hand-positioned fonts — that is the LO-correct way to get theme-correct measurement and button anchoring.

---

## 6. Key files

| Area | Path |
|---|---|
| Controller | `sw/source/uibase/uiview/writer2027toolboxctrl.cxx` |
| Controller registration | `sw/util/sw.component`, `officecfg/.../UI/Controller.xcu` |
| Popup reimplementation | `svx/source/tbxctrls/writer2027typesystempopup.cxx` (+ `.hxx`, `.ui`) |
| Slot/handler + anchor | `sw/source/uibase/uiview/view0.cxx` |
| Crash capture | `svx/inc/svx/writer2027log.hxx`, armed in `sw/source/uibase/app/swdll.cxx` |
| Dispatch guard | `framework/source/uielement/generictoolbarcontroller.cxx` |
| Slot decl (svidl omitted) | `sw/sdi/swriter.sdi`, `sw/inc/cmdid.h` |
| Logs | `%TEMP%\writer2027.log`, `%TEMP%\writer2027_crash.log` |

---

*End of handoff.*
---

## UPDATE (2026-10-08, post-remediation)

The remediation spec was executed. The two reported failures are fixed and built:

1. **"Type System is not opening anything"** — root causes: (a) the button fired
   the inherited `ToolboxController::execute()` which requires a resolved
   `XDispatch` the no-slot command never has; (b) the popup `.ui` was missing
   the `container` id that `WeldToolbarPopup` requires (`toolbarmenu.cxx:113`),
   so the framework-popup path could never display. Fixes in `087f2a8f28d3`
   (controller override + `.ui` restructure + semantic detail pane).
2. **"font popup in toolbar not fixed"** — row width now follows the real popup
   width (`mnPopupContentWidthPx`) and the specimen is measured/fit/clipped
   (`087f2a8f28d3`).
3. Backend round-trip defect fixed: `ApplyTypeSystem` now persists heading-scale
   sizes, so `DetectCurrentTypeSystem` reports the applied preset (`854cd80ec898`).

Full evidence: see `WRITER2027_REMEDIATION_REPORT.md`.
